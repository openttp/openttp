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

import argparse
import glob
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

VERSION = "0.0.0"
AUTHORS = "Michael Wouters"

STARTUP_WINDOW = 300
GPSDO_STATUS_UPDATE_PERIOD = 10

FURUNO_LOCK_TIME = 300

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
		if status >= 3:
			return True
		# If we got here, then the GPSDO is not in fine lock/holdover
		# So wait until this is true, within limits
		# Spec is lock in < 5 mins and testing agrees with this

		# We'll use our own timer - no need for SIGTIME
		Log(logFile,'waiting for lock')
		timerStart = time.time()
		while (time.time() - timerStart < FURUNO_LOCK_TIME):
			time.sleep(30)
			fin =  GetGoodStatusFile(statusFile,GPSDO_STATUS_UPDATE_PERIOD)
			if fin:
				status = GetFurunoStatus(fin)
				if status >= 3:
					Log(logFile,f'OK status={status}')
					return True
		
	return False


# -----------------------------------------------
def RestartReceiver(rxScript):
	pass

# -----------------------------------------------

home = os.environ['HOME'] 
root = home 
configFile = os.path.join(root,'etc','refmonitor.conf')
refManufacturer = 'furuno'

parser = argparse.ArgumentParser(description='')

examples =  'Usage examples\n'
examples += ''

parser = argparse.ArgumentParser(description='Monitor and control the system reference clock and its measurement systems, in particular,the GNSS receiver',
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
refConfigFile = ottp.MakeAbsoluteFilePath( cfg['reference:config file'],root,root + 'etc')
rxScript = cfg['receiver:logging script']

if (not os.path.isfile(refConfigFile)):
	ottp.ErrorExit(refConfigFile + ' not found')
	
refCfg = ottp.Initialise(refConfigFile,['status:path','status:file name'])
refStatusFile = ottp.MakeAbsoluteFilePath( os.path.join(refCfg['status:path'],refCfg['status:file name']),root,root + 'status')


# Create the process lock		
lockFile = ottp.MakeAbsoluteFilePath(cfg['paths:lock file'],home,home + '/lock')
ottp.Debug('Creating lock ' + lockFile)
if (not ottp.CreateProcessLock(lockFile)):
	ottp.ErrorExit("Couldn't create a lock")

signal.signal(signal.SIGINT,SignalHandler) # Note that CTRL-C will not interrupt a sleep()
signal.signal(signal.SIGTERM,SignalHandler) 
signal.signal(signal.SIGHUP,SignalHandler) # not usually run with a controlling TTY, but handle it anyway

logFile = ottp.MakeAbsoluteFilePath(cfg['paths:log file'],home,home + '/logs')
Log(logFile,'started')
# If the system is being operated as a frequency standard, then we don't care too much about
# pps synchronization but it's nice to have small numbers in time transfer files

up = GetUptime()
#up = 30 # FIXME
ottp.Debug(f'Uptime = {up}')
if (up < STARTUP_WINDOW):
	Log(logFile,'system has rebooted')
	# TODO Check what the reference source is
	# If external reference is in use, then it may have lost power too
	# if we've been up less than XX minutes, then wait
	tUp = GetUptime()
	#tUp = 598 #FIXME
	if tUp < STARTUP_WINDOW:
		ottp.Debug('Waiting {:d} s'.format(STARTUP_WINDOW - tUp))
		time.sleep(STARTUP_WINDOW - tUp)
	# Else, if system GPSDO is the reference, then check the GPSDO 
	if RefOK(refManufacturer,refStatusFile):
		Log(logFile,'Restarting time-transfer receiver')
		RestartReceiver(rxScript)
		
ottp.Debug('Boot checks completed')

while not killed:
	time.sleep(30)
	if RefOK(refManufacturer,refStatusFile):
		pass

# Clean up		
Log(logFile,'killed')
ottp.RemoveProcessLock(lockFile)
