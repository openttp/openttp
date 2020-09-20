#!/usr/bin/python3
#
# The MIT License (MIT)
#
# Copyright (c) 2020 Michael J. Wouters
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
# prs10log.py a logging script for the SRS PRS10 rubidium frequency standard
#

import argparse
import os
import re
import serial
import signal
import subprocess
import sys
import time

sys.path.append("/usr/local/lib/python3.6/site-packages")
import ottplib

VERSION = "0.0.3"
AUTHORS = "Michael Wouters"

# Globals
debug = False
killed = False

# ------------------------------------------
def SignalHandler(signal,frame):
	global killed
	killed=True
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		print(msg)
	return

# ------------------------------------------
def ErrorExit(msg):
	print(msg)
	sys.exit(0)

# ------------------------------------------
def Initialise(configFile):
	cfg=ottplib.LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit("Error loading " + configFile)
		
	# Check for required arguments
	reqd = ['counter:port','reference:log path'] # require this for the moment
	for k in reqd:
		if (not k in cfg):
			ErrorExit('The required configuration entry'  + '"' + k +'"' + ' is undefined')
		
	return cfg

# ------------------------------------------
def GetResponse(ser,cmd):
	Debug('GetResponse %f' % time.time())
	
	ser.write((cmd + '\r').encode('utf-8'))
	ret = ser.read_until(b'\r') # The io.TextWrapper solution does not seem to be a rock solid one
	Debug('(%f) Got %s' % (time.time(),ret))
	return ret.decode('utf-8').rstrip()

# -------------------------------------------------------------------------
def GetStatus(ser):
	s = GetResponse(ser,'ST?')
	
	# The valid response is six comma-separated integers
	m = re.match(r'(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)',s)
	if m:
		return [m.group(1),m.group(2),m.group(3),m.group(4),m.group(5),m.group(6)]
	return ['-1','-1','-1','-1','-1','-1']
	
# -------------------------------------------------------------------------
def GetAD(ser,index):
	rdg = GetResponse(ser,'AD ' + str(index) + '?')
	m = re.match(r'\d{1}\.\d{3}',rdg)
	if m:
		return rdg # don't convert to float since we're just convert to convert back to a string anyway
	return '-1'
		
# ------------------------------------------
# The main 
# ------------------------------------------


maxTimeouts = 5

home =os.environ['HOME'] + os.sep
configFile = os.path.join(home,'etc','gpscv.conf')

parser = argparse.ArgumentParser(description='Log a SRS PRS10 (status only)',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--settings','-s',help='show settings',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

configFile = args.config;

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + " not found")
	
logPath = os.path.join(home,'logs')
if (not os.path.isdir(logPath)):
	ErrorExit(logPath + 'not found')

cfg=Initialise(configFile)

port = cfg['counter:port'] # required
refLogPath= ottplib.MakeAbsolutePath(cfg['reference:log path'], home) # required

statusExtension = '.rb'  
if 'reference:file extension' in cfg:
	statusExtension=cfg['reference:file extension']

logInterval = 60
if 'reference:log interval' in cfg:
	logInterval=int(cfg['reference:log interval'])

statusFile = 'prs10.status'
if 'reference:status file' in cfg:
	statusFile = cfg['reference:status file']
statusFile = ottplib.MakeAbsoluteFilePath(statusFile,home,home + 'logs')

powerFlag = 'prs10.pwr'
if 'reference:power flag' in cfg:
	powerFlag = cfg['reference:power flag']
powerFlag = ottplib.MakeAbsoluteFilePath(powerFlag,home,home + 'logs')

# Create the process lock		
lockFile=ottplib.MakeAbsoluteFilePath(cfg['counter:lock file'],home,home + '/etc')
Debug('Creating lock ' + lockFile)
if (not ottplib.CreateProcessLock(lockFile)):
	ErrorExit("Couldn't create a lock")

signal.signal(signal.SIGINT,SignalHandler)
signal.signal(signal.SIGTERM,SignalHandler)

# Create UUCP lock for the serial port
uucpLockPath='/var/lock'
if ('paths:uucp lock' in cfg):
	uucpLockPath = cfg['paths:uucp lock']

ret = subprocess.check_output(['/usr/local/bin/lockport','-d',uucpLockPath,'-p',str(os.getpid()),port,sys.argv[0]]).decode('utf-8')

if (re.match('1',ret)==None):
	ottplib.RemoveProcessLock(lockFile)
	ErrorExit('Could not obtain a lock on ' + port + '.Exiting.')

signal.signal(signal.SIGINT,SignalHandler)  # can also catch as  KeyboardInterrupt exception
signal.signal(signal.SIGTERM,SignalHandler)
signal.signal(signal.SIGHUP,SignalHandler)  # processes are not usually run with a controlling TTY, but handle it anyway

Debug('Opening ' + port)

try:
	ser = serial.Serial(port,9600,timeout=2.0)
	
except Exception as ex:
	print('Error on device ' + port)
	
	#template = "An exception of type {0} occurred. Arguments:\n{1!r}"
	#message = template.format(type(ex).__name__, ex.args)
	#print(message)
	
	ottplib.RemoveProcessLock(lockFile)
	subprocess.check_output(['/usr/local/bin/lockport','-r',port]) # but ignore return value anyway
	exit()
	
oldmjd = -1
nTimeouts = 0
lastLog = -1

ser.reset_input_buffer() # eat junk

# Preliminaries over
while (not killed):
	tt = time.time()
	mjd = ottplib.MJD(tt)
	if (not( mjd == oldmjd)):
		oldmjd = mjd
		fnlog = refLogPath + str(mjd) + statusExtension
		if (not os.path.isfile(fnlog)):
			flog = open(fnlog,'w') # unbuffered output is not allowed (but we don't care really)
			id = GetResponse(ser,'ID?')
			flog.write('# prs10log.py version %s\n' % VERSION)
			flog.write('# %s\n' % id)
			flog.flush()
		else:
			flog = open(fnlog,'a')
		Debug('Opened ' + fnlog)
	
	if (tt - lastLog >= logInterval):
		
		fstatus = open(statusFile,'w')
		
		statusBytes = GetStatus(ser)
		advals = []
		timestr = time.strftime('%H:%M:%S',time.gmtime(tt))
		
		# Check for power loss
		if (129 <= int(statusBytes[5])):
			Debug('Power lost')
			fpwr = open(powerFlag,'w')
			fpwr.write('%d %s power loss detected\n' % (mjd,timestr))
			fpwr.close()
			
			for i in range(0,16):
				advals.append(GetAD(ser,i))
			
			# Write the status data to the log so we have a record that power was lost
			outstr = timestr 
			for sb in statusBytes:
				outstr += ' ' + sb
			for i in range(0,16):
				outstr += ' ' + advals[i]
			outstr += '\n'
			flog.write(outstr)
		
			statusBytes = GetStatus(ser) # get the updated status 
			timestr = time.strftime('%H:%M:%S',time.gmtime(tt))
		
		# Get AD values
		for i in range(0,16):
			advals.append(GetAD(ser,i))
		# Write the status data to the log so we have a record that power was lost
		outstr = timestr 
		for sb in statusBytes:
			outstr += ' ' + sb
		for i in range(0,16):
			outstr += ' ' + advals[i]
		outstr += '\n'
		
		flog.write(outstr)
		flog.flush()
		
		fstatus.write(outstr)
		fstatus.close()
		
		lastLog = tt
		
	# Don't try to be fancy
	time.sleep(logInterval) 
	
	#try:
	#	time.sleep(1)
	#except:
	#	Debug('Timeout')
	#	nTimeouts += 1
	#	if (nTimeouts == maxTimeouts):
	#		flog.close()
	#		ottplib.RemoveProcessLock(lockFile)
	#		ErrorExit('Too many timeouts')
	
# All done - cleanup		
flog.close()
ottplib.RemoveProcessLock(lockFile)
