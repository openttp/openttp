#!/usr/bin/python3
#

#
# The MIT License (MIT)
#
# Copyright (c) 2025 Michael J. Wouters
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
#

# This script assumes that required processes (eg logging of the Furuno) have all been started at boot
# This can be conveniently accelerated using @reboot in the crontab
# It then waits 5 minutes before checking that it is OK to synchronize the time transfer receiver
#

# Modification record
# ~~~~~~~~~~~~~~~~~~~
# 2025-11-19 ELM Added additional checks for correct operation of chrony
# 2026-10-07 ELM Rewrote code for checking the reference status
# 2026-10-07 ELM Replaced time.sleep(x) instructions with non-blocking code
# 2026-10-19 ELM Application now terminates in signal handler; removed code around sleeps
# 2026-10-19 ELM Check that chrony NMEA refclock is reachable before declaring chrony OK

import glob
import argparse
import os
import re
import signal
import shutil
import subprocess
import sys
import time

# This is where ottplib is installed
sys.path.append("/usr/local/lib/python3.8/site-packages")  # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/dist-packages") # Ubuntu 22.04
sys.path.append("/usr/local/lib/python3.12/dist-packages") # Ubuntu 24.04
sys.path.append("/usr/local/lib/python3.14/dist-packages") # Ubuntu 26.04

try: 
	import ottplib as ottp
except ImportError:
	sys.exit('ERROR: Must install ottplib\n eg openttp/software/system/installsys.py -i ottplib')

VERSION = "0.1.5"
AUTHORS = "Michael Wouters, Louis Marais"

STARTUP_WINDOW = 300
GPSDO_STATUS_UPDATE_PERIOD = 10

FURUNO_LOCK_TIME = 300

GPSCV_USER = "cvgps"
KICKSTART = "/usr/local/bin/kickstart.py"

# -----------------------------------------------------------------------------
def ts():
	return(time.strftime("%Y-%m-%d %H:%M:%S ",time.gmtime()))

#-----------------------------------------------------------------------------
def SignalHandler(signal,frame):
	if os.path.isfile(logFile):
		Log(logFile,'killed')
	if os.path.isfile(lockFile):
		ottp.RemoveProcessLock(lockFile)
	sys.exit(0)

#-----------------------------------------------------------------------------
def Log(logFile,msg):
	ottp.Debug(f"{ts()} {msg}")
	try:
		flog = open(logFile,'a')
		flog.write('{} {}\n'.format(time.strftime('%Y-%02m-%02d %H:%M:%S',time.gmtime()),msg))
		flog.close()
		flog.close()
	except:
		ottp.Debug(f'{ts()} Unable to log message')
	return

# -----------------------------------------------
def GetUptime():
	with open("/proc/uptime", "r") as f: # Linux only
		up = int(float(f.readline().split()[0]))
	return up

# -----------------------------------------------
def RefOK(refManufacturer,statusFile,logFile):
	nofile = 0
	stalefile = 0
	badstatus = 0
	statusnotfound = 0
	mode = -1
	for i in range(0,3): # Multiple attempts allowed.
		if not os.path.exists(statusFile):
			ottp.Debug(f'{ts()} Status file {statusFile} is missing')
			nofile += 1
			time.sleep(1)
			continue  # We try again.
		fileAge = time.time() - os.path.getmtime(statusFile)
		if fileAge > GPSDO_STATUS_UPDATE_PERIOD:
			ottp.Debug(f'{ts()} GPSDO status file is {fileAge:0.2f} s old - not fresh')
			stalefile += 1
			time.sleep(1)
			continue # We try again.
		if refManufacturer == "furuno":  # For now if the refManufacturer != furuno, return True.
			                               # When we have other manufacturers move the reading of
			                               # the status file to before the 'if' statement.
			# Wait for GPSDO status file update time to not be close to now
			while True: # not killed:
				fileAge = time.time() - os.path.getmtime(statusFile)
				if (fileAge > 1) and (fileAge + 1 < GPSDO_STATUS_UPDATE_PERIOD):
					break
				time.sleep(0.1)
			with open(statusFile,'r') as f:
				lines = f.readlines()
				f.close()
			for line in lines:
				m = re.search(r'Frequency mode:\s+(Warm up|Pull-in|Coarse lock|Fine lock|Holdover)',line)
				if m:
					freqMode = m.group(1)
					ottp.Debug(f'{ts()} Frequency mode = {freqMode}')
					if freqMode == 'Warm up':
						mode = 0
					elif freqMode == 'Pull-in':
						mode = 1
					elif freqMode == 'Coarse lock':
						mode = 2
					elif freqMode == 'Fine lock':
						mode = 3
					elif freqMode == 'Holdover': # TODO investigate holdover behaviour
						mode = 4
					else:
						badstatus += 1
					break # No need to check rest of the lines
			if mode >= 3:
				return True   # NOTE: 'Fine lock' and 'Holdover' means the GNSSDO is OK.
			statusnotfound += 1
		else:
			return True # Not a Furuno GNSSDO...
		time.sleep(1) # We failed but will try again.
	# If the code reaches this point multiple attempts failed. Log the issue.
	if mode <= 2:
		Log(logFile,f"Either could not find or have a bad frequency mode in {statusFile}, mode = {mode}")
	if nofile > 0:
		Log(logFile,f"Could not find {statusFile}")
	if stalefile > 0:
		Log(logFile,f"{statusFile} is stale.")
	if badstatus > 0:
		Log(logFile,f"{statusFile} had a bad status.") # NOTE: this message should never appear.
	if statusnotfound > 0:
		Log(logFile,f"{statusFile} is available but did not contain a valid 'Frequency mode' message.")
	Log(logFile,f"During the check of {statusFile} something went wrong. Hopefully there is a message above this one explaining it.")
	return True # TODO: make sure this is the correct thing to do.

# -----------------------------------------------
def ReadGPIO(gpioName):
	fgpio = open(os.path.join(gpioFS,gpioName), 'r')
	val = int(fgpio.readline().strip())
	fgpio.close()
	ottp.Debug(f'{ts()} READ {gpioName} {val}')
	return val

# -----------------------------------------------
def WriteGPIO(gpioName,val):
	fgpio = open(os.path.join(gpioFS,gpioName), 'w')
	fgpio.write(f'{val}\n')
	fgpio.close()
	ottp.Debug(f'{ts()} WRITE {gpioName} {val}')

# -----------------------------------------------
def restartChrony():
	ottp.Debug(f'{ts()} Restarting chrony')
	try:
		x = subprocess.check_output(['systemctl','restart','chrony']) # eat the output
	except Exception as e:
		Log(logFile,'Failed to restart chrony')
		print(e)
		ottp.ErrorExit('Failed to restart chrony')
	ottp.Debug(ts() +' '+x.decode('utf-8'))
	
	Log(logFile,"chrony restarted")

# -----------------------------------------------
def restartGpsd(): # gpsd must be restarted AFTER starting chrony
	ottp.Debug(f'{ts()} Restarting gpsd')
	try:
		x = subprocess.check_output(['systemctl','restart','gpsd']) # eat the output
	except Exception as e:
		Log(logFile,'Failed to restart gpsd')
		print(e)
		ottp.ErrorExit('Failed to restart gpsd')
	ottp.Debug(ts()+' '+x.decode('utf-8'))
	Log(logFile,"gpsd restarted")

# -----------------------------------------------
# Check that Chrony reports a stratum number > 0
# and that the NMEA refclock is reachable
def checkChrony():
	ottp.Debug(f"{ts()} Check chrony")
	cmd = ['chronyc','tracking']
	retval = subprocess.run(cmd,capture_output=True)
	if retval.returncode != 0:  # chrony is not running.
		restartChrony()
		return(False)
	lines = retval.stdout.decode('ascii').split('\n')
	p = re.compile(r'Stratum\s+:\s+(\d+)')
	for line in lines:
		if not line.startswith('Stratum'):
			continue
		m = re.match(p,line)
		if m:
			stratum = int(m.groups()[0])
			ottp.Debug(f"{ts()} chrony reports stratum {stratum}")
			if stratum > 0 and stratum < 10:  # TODO: Find good value for max stratum
				stratumOK = True
			break
	if not stratumOK:
		return(False)
	cmd = ['chronyc','sources']
	retval = subprocess.run(cmd,capture_output=True)
	if retval.returncode != 0:  # chrony is not running.
		restartChrony()
		return(False)
	lines = retval.stdout.decode('ascii').split('\n')
	for line in lines:
		if "NMEA" in line:
			v = line.split()
			idx = v.index('NMEA')
			reach = int(v[idx+3])
			ottp.Debug(f"{ts()} chrony reports NMEA reach = {reach}")
			if reach > 0:
				return(True)
	return(False)

# -----------------------------------------------
def terminateApp(script):
	app = os.path.basename(script)
	ottp.Debug(f"{ts()} Check if {app} is running")
	cmd = ['ps','ax']
	retval = subprocess.run(cmd,capture_output=True)
	if not retval.returncode == 0:
		return(False)
	v = list(filter(None,[s.strip() for s in retval.stdout.decode('ascii').split('\n')]))
	for s in v:
		if app in s:
			p = re.compile(r'(\d+)')
			m = re.match(p,s)
			if m:
				ottp.Debug(f"{ts()} {app} is still running.")
				pid = m.groups()[0]
				cmd = ['kill',pid]
				retval = subprocess.run(cmd,capture_output=True)
				if not retval.returncode == 0:
					return(False)
				Log(logFile,f"{os.path.basename(script)} terminated")
			break
	return(True) # Either killed successfully, or not running.

# -----------------------------------------------
# TODO: Consider creating separate terminateChrony and terminateGpsd routines
# chrony: systemctl stop chrony
# gpsd: systemctl stop gpsd.service gpsd.socket 
# Check they are really gone with pgrep gpsd | pgrep chrony
# Then update the code that checks chrony to stop both of these, and restart
# them in sequence. It currently takes about 12 minutes before chrony is
# happy, which is how long it takes the receiver to start transmitting the
# NMEA data stream.

# -----------------------------------------------
def RestartReceiver():
	
	# Reset the receiver
	# This is most easily done via the GPIO
	# If we try killall, restart then there is a small risk that 
	# the user crontab will restart the receiver between kill and restart
	
	ottp.Debug(f'{ts()} Resetting the receiver')
	WriteGPIO('MOS_T_RESET.txt',0)
	time.sleep(3) # only checked once per second
	WriteGPIO('MOS_T_RESET.txt',1)
	
	Log(logFile,"receiver reset")
	
	# Wait a bit for MOS-T to become ready
	ottp.Debug(f'{ts()} Waiting for MOS_T to assert READY')
	rdy = ReadGPIO('MOST_T_READY.txt')
						 
	while rdy == 0:
		time.sleep(1)
		rdy = ReadGPIO('MOST_T_READY.txt')
	ottp.Debug(f'{ts()} READY')
	
	# First make sure plrxlog is dead... 
	terminateApp(rxScript) # kill command successfull, but
	time.sleep(2) # plrxlog.py can take a second or two to exit.
	
	# Kick it back into life
	ottp.Debug(f'{ts()} Restarting receiver')
	try:
		x = subprocess.check_output(['su','-l',gpscvUser,'-c',KICKSTART]) # eat the output
	except Exception as e:
		Log(logFile,'Failed to run kickstart')
		ottp.ErrorExit('Failed to run kickstart')
	ottp.Debug(ts()+' '+x.decode('utf-8'))
	Log(logFile,"receiver logging started")
	
	ottp.Debug(f'{ts()} Waiting ...')
	time.sleep(60) # time to first fix is 45 so wait a minute 
	
	restartChrony()
	ottp.Debug(f'{ts()} Waiting ...')
	
	time.sleep(5)
	
	restartGpsd()
	
	return

# -----------------------------------------------

root = '/usr/local' 
configFile = os.path.join(root,'etc','refmonitor.conf')
refManufacturer = 'furuno'
gpscvUser = GPSCV_USER

parser = argparse.ArgumentParser(description='')

examples =  'Usage examples\n'
examples += ''

parser = argparse.ArgumentParser(description='Monitor and control the system reference clock and its dependent systems, in particular,the GNSS receiver',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug
ottp.SetDebugging(debug)

configFile = args.config
	
if (not os.path.isfile(configFile)):
	ottp.ErrorExit(configFile + ' not found')

cfg=ottp.Initialise(configFile,['reference:manufacturer','reference:config file','receiver:logging script'])
refManufacturer = cfg['reference:manufacturer'].lower()
refConfigFile = ottp.MakeAbsoluteFilePath( cfg['reference:config file'],root,os.path.join(root,'etc'))
rxScript = cfg['receiver:logging script']

if (not os.path.isfile(refConfigFile)):
	ottp.ErrorExit(refConfigFile + ' not found')
	
gpscvUserHome = os.path.join('/home',gpscvUser)

refCfg = ottp.Initialise(refConfigFile,['status:path','status:file name'])
refStatusFile = ottp.MakeAbsoluteFilePath( os.path.join(refCfg['status:path'],refCfg['status:file name']),gpscvUserHome,os.path.join(gpscvUserHome,'var'))

gpioFS = os.path.join(gpscvUserHome,'gpios')

# Create the process lock
lockFile = ottp.MakeAbsoluteFilePath(cfg['paths:lock file'],root,os.path.join(root,'log'))
ottp.Debug(f'{ts()} Creating lock {lockFile}')
if (not ottp.CreateProcessLock(lockFile)):
	ottp.ErrorExit("Couldn't create a lock")

# Application termination is handled in the 'SignalHandler' routine.
signal.signal(signal.SIGINT,SignalHandler)  # Note that CTRL-C will not interrupt a sleep() which
signal.signal(signal.SIGTERM,SignalHandler) # is why there are no excessive sleeps in the code.
signal.signal(signal.SIGHUP,SignalHandler)  # Not usually run with a controlling TTY, but handle it anyway.

logFile = ottp.MakeAbsoluteFilePath(cfg['paths:log file'],root,os.path.join(root,'log'))
Log(logFile,'started')

# If the system is being operated as a frequency standard, then we don't care too much about
# pps synchronization but it's nice to have small numbers in time transfer files

up = GetUptime()
ottp.Debug(f'{ts()} Uptime = {up}')

# We detect reboots and handle them as a special case
# where we always restart receiver, gpsd, chrony ..
# Could detect warm boots but I think a user would expect everything to be restarted

refLocked = True # may not have rebooted - may have eg restarted service

if (up < STARTUP_WINDOW):
	Log(logFile,'system has rebooted')
	
	# TODO Check what the reference source is
	# If external reference is in use, then it may have lost power too
	
	# If we've been up less than XX minutes, then wait unless we are killed
	tUp = GetUptime()
	if tUp < STARTUP_WINDOW:
		ottp.Debug(f'{ts()} Waiting {STARTUP_WINDOW - tUp:d} s')
		time.sleep(STARTUP_WINDOW - tUp)
		
	# TODO else, if system GPSDO is the reference, then check the GPSDO
	
	# Spec is lock in < 5 mins and testing agrees with this

	# We'll use our own timer - no need for SIGTIME
	Log(logFile,'waiting for fine lock')
	timerStart = GetUptime() # better than using the system time 
	while (GetUptime() - timerStart < FURUNO_LOCK_TIME):
		time.sleep(GPSDO_STATUS_UPDATE_PERIOD)
		if RefOK(refManufacturer,refStatusFile,logFile):
			refLocked = True
			break
	
	time.sleep(GPSDO_STATUS_UPDATE_PERIOD) # allow status file to update
	
	# check again
	if RefOK(refManufacturer,refStatusFile,logFile):
		Log(logFile,'GNSSDO locked - restarting receiver')
		refLocked = True
		RestartReceiver()
	else:
		Log(logFile,'GNSSDO still unlocked')
		refLocked = False
		
Log(logFile, 'boot checks completed')

# We might get to this point without the reference being locked 
# - but that's OK
# the main loop will take care of that 

ncycle = 0
checkcycle = 10            # TODO: Check if n == 10 is the right choice.
chronyOK = False
            
while True: # Termination of application happens in 'SignalHandler' routine
	time.sleep(GPSDO_STATUS_UPDATE_PERIOD) # this needs to be sufficiently frequent that we catch a short 'unlock'
	if RefOK(refManufacturer,refStatusFile,logFile):
		if not refLocked:
			Log(logFile,"GNSSDO locked")
			RestartReceiver()
			refLocked = True
			ncycle = 0
		# Check status of chrony every 'checkcycle' cycles
		if ncycle >= checkcycle:
			time.sleep(5)
			if not checkChrony():
				chronyOK = False
				restartGpsd()      
			elif not chronyOK:
				chronyOK = True
				Log(logFile,"chrony OK")
			ncycle = 0
		ncycle += 1
	else:
		if refLocked:
			Log(logFile,"GNSSDO unlocked")
		refLocked = False
