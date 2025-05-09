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

import argparse
import glob
import os
import re
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

KICKSTART_PERIOD = 300
STARTUP_WINDOW = 2*KICKSTART_PERIOD
GPSDO_STATUS_UPDATE_PERIOD = 10

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
def GPSDOOK(gpsdo,statusFile):
	
	fin =  GetGoodStatusFile(statusFile,GPSDO_STATUS_UPDATE_PERIOD)

	if not fin:
		return False

	if gpsdo == 'furuno':
		for l in fin:
			m = re.match(r'FREQMODE\s+=\s+(\d)',l)
			if m:
				freqMode = int(m.group(1))
				ottp.Debug(f'FREQMODE = {freqMode}')
				if freqMode < 3:
					pass
				else:
					break
		
	
	fin.close()
	return False
		
# -----------------------------------------------

home = os.environ['HOME'] 
root = home 
configFile = os.path.join(root,'etc','refmonitor.conf')
gpscvConfigFile = os.path.join(root,'etc','gpscv.conf')
gpsdo = 'furuno'

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

if (not os.path.isfile(gpscvConfigFile)):
	ottp.ErrorExit(gpscvConfigFile + ' not found')
	
cfg=ottp.Initialise(configFile,[])

gpscvCfg=ottp.Initialise(gpscvConfigFile,['reference:status file','reference:model'])
gpsdo = gpscvCfg['reference:model'].lower()
gpsdoStatusFile = ottp.MakeAbsoluteFilePath( gpscvCfg['reference:status file'],root,root + 'etc')

# If the system is being operated as a frequency standard, then we don't care too much about
# pps synchronization but it's nice to have small numbers in time transfer files

up = GetUptime()
up = 30
ottp.Debug(f'Uptime = {up}')
if (up < STARTUP_WINDOW):
	ottp.Debug('System has rebooted')
	# TODO Check what the reference source is
	# If external reference is in use, then it may have lost power too
	# if we've been up less than XX minutes, then wait
	tUp = GetUptime()
	if tUp < STARTUP_WINDOW:
		ottp.Debug('Waiting {:d} s'.format(STARTUP_WINDOW - tUp))
		time.sleep(STARTUP_WINDOW - tUp)
	# Else, if system GPSDO is the reference, then check the GPSDO 
	if GPSDOOK(gpsdo,gpsdoStatusFile):
		pass

ottp.Debug('Boot checks completed')

while True:
	time.sleep(1)
