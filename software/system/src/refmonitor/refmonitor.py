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

import glob
import argparse
#import gpiozero
import os
import re
import signal
import shutil
import subprocess
import sys
import time

# This is where ottplib is installed
sys.path.append("/usr/local/lib/python3.8/site-packages")  # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22.04
sys.path.append("/usr/local/lib/python3.12/site-packages") # Ubuntu 24.04

try: 
	import ottplib as ottp
except ImportError:
	sys.exit('ERROR: Must install ottplib\n eg openttp/software/system/installsys.py -i ottplib')

VERSION = "0.1.2"
AUTHORS = "Michael Wouters, Louis Marais"

STARTUP_WINDOW = 300
GPSDO_STATUS_UPDATE_PERIOD = 10

FURUNO_LOCK_TIME = 300

GPSCV_USER = "cvgps"
KICKSTART = "/usr/local/bin/kickstart.py"

killed = False

#-----------------------------------------------------------------------------
def SignalHandler(signal,frame):
	global killed
	killed = True
	return

#-----------------------------------------------------------------------------
def Log(logFile,msg):
	ottp.Debug(msg)
	try:
		flog = open(logFile,'a')
		flog.write('{} {}\n'.format(time.strftime('%Y-%02m-%02d %H:%M:%S',time.gmtime()),msg))
		flog.close()
		flog.close()
	except:
		ottp.Debug('Unable to log message')
	return

# -----------------------------------------------
def GetUptime():
	with open("/proc/uptime", "r") as f: # Linux only
		up = int(float(f.readline().split()[0]))
	return up

# -----------------------------------------------
def GetFileAge(f):
	return time.time() - os.path.getmtime(f)

# -----------------------------------------------
def GetGoodStatusFile(statusFile,maxAge):
	# If there is no status file, give up
	# We have already waited STARTUP_WINDOW since bootup
	# so something is wrong
	if not os.path.exists(statusFile):
		ottp.Debug(f'Status file {statusFile} is missing')
		return None
	
	# Check whether the information in the status file is fresh.
	# Again, the presumption is that the gpsdo logging process should be running by now
	# This could be a stray file from the last boot
	fileAge = GetFileAge(statusFile)
	if fileAge > maxAge:
		ottp.Debug(f'GPSDO status file is {fileAge} s old - not fresh')
		return None
	
	# Dunno why we might not be able to open the file
	try:
		fin= open(statusFile,'r')
		return fin
	except:
		ottp.Debug(f'Unable to open {statusFile}')
		return None

# -----------------------------------------------
def GetFurunoStatus(fin):
	
	ottp.Debug('Check Furuno')
	for l in fin:

		m = re.search(r'Frequency mode:\s+(Warm up|Pull-in|Coarse lock|Fine lock|Holdover)',l)
		if m:
			freqMode = m.group(1)
			ottp.Debug(f'Frequency mode = {freqMode}')
			if freqMode == 'Warm up':
				return 0
			elif freqMode == 'Pull-in':
				return 1
			elif freqMode == 'Coarse lock':
				return 2
			elif freqMode == 'Fine lock':
				return 3
			elif freqMode == 'Holdover': #TODO investigate holdover behaviour
				return 4
	
	return -1
					
# -----------------------------------------------
def RefOK(refManufacturer,statusFile):
	
	fin =  GetGoodStatusFile(statusFile,GPSDO_STATUS_UPDATE_PERIOD)

	if not fin:
		return False

	if refManufacturer == 'furuno':
		status = GetFurunoStatus(fin)
		return (status >= 3)
	
	return True # TODO
	
# -----------------------------------------------
def ReadGPIO(gpioName):
	fgpio = open(os.path.join(gpioFS,gpioName), 'r')
	val = int(fgpio.readline().strip())
	fgpio.close()
	ottp.Debug(f'READ {gpioName} {val}')
	return val

# -----------------------------------------------
def WriteGPIO(gpioName,val):
	fgpio = open(os.path.join(gpioFS,gpioName), 'w')
	fgpio.write(f'{val}\n')
	fgpio.close()
	ottp.Debug(f'WRITE {gpioName} {val}')

# -----------------------------------------------
def restartChrony():
	# Restart chronyd
	ottp.Debug('Restarting chrony')
	try:
		x = subprocess.check_output(['systemctl','restart','chrony']) # eat the output
	except Exception as e:
		Log(logFile,'Failed to restart chrony')
		print(e)
		ottp.ErrorExit('Failed to restart chrony')
	ottp.Debug(x.decode('utf-8'))
	
	Log(logFile,"chrony restarted")
	ottp.Debug('Waiting ...')

# -----------------------------------------------
def restartGpsd():
	# Restart gpsd - this must be done AFTER starting chrony
	ottp.Debug('Restarting gpsd')
	try:
		x = subprocess.check_output(['systemctl','restart','gpsd']) # eat the output
	except Exception as e:
		Log(logFile,'Failed to restart gpsd')
		print(e)
		ottp.ErrorExit('Failed to restart gpsd')
	ottp.Debug(x.decode('utf-8'))
	Log(logFile,"gpsd restarted")

# -----------------------------------------------
# For now just a simple check that Chrony reports a stratum number > 0
def checkChrony():
	ottp.Debug("Check chrony")
	cmd = ['chronyc','tracking']
	retval = subprocess.run(cmd,capture_output=True)
	if retval.returncode != 0:
		return(False)
	lines = retval.stdout.decode('ascii').split('\n')
	p = re.compile(r'Stratum\s+:\s+(\d+)')
	for line in lines:
		if not line.startswith('Stratum'):
			continue
		m = re.match(p,line)
		if m:
			stratum = int(m.groups()[0])
			ottp.Debug(f"chrony reports stratum {stratum}")
			if stratum > 0 and stratum < 10:  # TODO: Find good value for max stratum
				return(True)
			break
	return(False)

# -----------------------------------------------
def terminateApp(script):
	app = os.path.basename(script)
	ottp.Debug(f"Check if {app} is running")
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
				ottp.Debug(f"{app} is still running.")
				pid = m.groups()[0]
				cmd = ['kill',pid]
				retval = subprocess.run(cmd,capture_output=True)
				if not retval.returncode == 0:
					return(False)
				Log(logFile,f"{os.path.basename(script)} terminated.")
			break
	return(True) # Either killed successfully, or not running.

# -----------------------------------------------
def RestartReceiver():
	
	# Reset the receiver
	# This is most easily done via the GPIO
	# If we try killall,restart then there is a small risk that 
	# the user crontab will restart the receiver between kill and restart
	
	ottp.Debug('Resetting the receiver')
	WriteGPIO('MOS_T_RESET.txt',0)
	time.sleep(3) # only checked once per second
	WriteGPIO('MOS_T_RESET.txt',1)
	
	Log(logFile,"receiver reset")
	
	# Wait a bit for MOS-T to become ready
	ottp.Debug('{} Waiting for MOS_T to assert READY\n'.format(time.strftime('%Y-%02m-%02d %H:%M:%S',time.gmtime())))
	rdy = ReadGPIO('MOST_T_READY.txt')
						 
	while rdy == 0:
		time.sleep(1)
		rdy = ReadGPIO('MOST_T_READY.txt')
	ottp.Debug('{} READY\n'.format(time.strftime('%Y-%02m-%02d %H:%M:%S',time.gmtime())))
	
	# First make sure plrxlog is dead... for s/n 6 it just did not die when it should have,
	# so kickstart did not actually restart it! NOTE: This seems to be the silver bullet
	# for s/n 6.
	terminateApp(rxScript)
	
	# Kick it back into life
	ottp.Debug('Restarting receiver')
	try:
		x = subprocess.check_output(['su','-l',gpscvUser,'-c',KICKSTART]) # eat the output
	except Exception as e:
		Log(logFile,'Failed to run kickstart')
		ottp.ErrorExit('Failed to run kickstart')
	ottp.Debug(x.decode('utf-8'))
	Log(logFile,"receiver logging started")
	
	ottp.Debug('Waiting ...')
	time.sleep(60); # time to first fix is 45 so wait a minute 
	
	# LM, 2025-11-19
	# s/n 6 does not recover from an antenna disconnect. All of s/n 1..5 do. What is up?
	#
	# Just waiting may not be enough to ensure the receiver is up, what if TTFF is > 60 s? 
	#
	# Maybe rather check that the receiver is OK, check receiver status file(?) > 4 GPS SVs?
	
	restartChrony()
	
	time.sleep(5);
	
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
ottp.Debug('Creating lock ' + lockFile)
if (not ottp.CreateProcessLock(lockFile)):
	ottp.ErrorExit("Couldn't create a lock")

signal.signal(signal.SIGINT,SignalHandler) # Note that CTRL-C will not interrupt a sleep()
signal.signal(signal.SIGTERM,SignalHandler) 
signal.signal(signal.SIGHUP,SignalHandler) # not usually run with a controlling TTY, but handle it anyway

logFile = ottp.MakeAbsoluteFilePath(cfg['paths:log file'],root,os.path.join(root,'log'))
Log(logFile,'started')
# If the system is being operated as a frequency standard, then we don't care too much about
# pps synchronization but it's nice to have small numbers in time transfer files

up = GetUptime()
ottp.Debug(f'Uptime = {up}')

# We detect reboots and handle them as a special case
# where we always restart receiver, gpsd, chrony ..
# Could detect warm boots but I think a user would expect everything to be restarted

refLocked = True # may not have rebooted - may have eg restarted service



#
# LM, 2025-11-19 It may not be a good idea to wait without checking 'killed'.
# The 'killed' variable should be checked in the loop below.
# For example if the system is shut down within a few minutes of starting up
# this application may prevent the shut down while it does its thing.
# A possible scenario for this to happen is when a UPS is used and it is close
# to running out of power after a brown-out or similar that caused the TTS
# to shut down and then start up again - the UPS may force a shut down which 
# may not happen in time due to this application holding up the shutdown.
#



if (up < STARTUP_WINDOW):
	Log(logFile,'system has rebooted')
	
	# TODO Check what the reference source is
	# If external reference is in use, then it may have lost power too
	
	# If we've been up less than XX minutes, then wait
	tUp = GetUptime()
	if tUp < STARTUP_WINDOW:
		ottp.Debug('Waiting {:d} s'.format(STARTUP_WINDOW - tUp))
		time.sleep(STARTUP_WINDOW - tUp)
		
	# TODO else, if system GPSDO is the reference, then check the GPSDO
	
	# Spec is lock in < 5 mins and testing agrees with this

	# We'll use our own timer - no need for SIGTIME
	Log(logFile,'waiting for fine lock')
	timerStart = GetUptime() # better than using the system time 
	while (GetUptime() - timerStart < FURUNO_LOCK_TIME):
		time.sleep(GPSDO_STATUS_UPDATE_PERIOD)
		if RefOK(refManufacturer,refStatusFile):
			refLocked = True
			break
					
	# check again
	if RefOK(refManufacturer,refStatusFile):
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
checkcycle = 10            # TODO: is n == 10 the right choice?
            
while not killed:
	time.sleep(GPSDO_STATUS_UPDATE_PERIOD) # this needs to be sufficiently frequent that we catch a short 'unlock'
	if RefOK(refManufacturer,refStatusFile):
		if not refLocked:
			Log(logFile,"GNSSDO locked")
			RestartReceiver()
			refLocked = True
			ncycle = 0
		# Check status of chrony every n cycles
		if ncycle >= checkcycle:  
			time.sleep(5)
			#print(time.strftime("%Y-%m-%d %H:%M:%S ",time.gmtime()))
			if not checkChrony():
				restartGpsd()
			ncycle = 0
		ncycle += 1
	else:
		if refLocked:
			Log(logFile,"GNSSDO unlocked")
		refLocked = False

# Clean up
Log(logFile,'killed')
ottp.RemoveProcessLock(lockFile)
