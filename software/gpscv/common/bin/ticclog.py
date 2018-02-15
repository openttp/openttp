#!/usr/bin/python
#

#
# The MIT License (MIT)
#
# Copyright (c) 2017 Michael J. Wouters
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
# ticclog.py - a logging script for the TAPR TICC
#
# Modification history
# 2017-03-02 MJW First version
# 2018-02-15 MJW Specify path for lock file. Use gpscv.conf as default config file
#								 Bug fix. Arguments to subprocess() constructed incorrectly.
#

import argparse
import os
import re
import select
import serial
import signal
import subprocess
import sys
# This is where ottplib is installed
sys.path.append("/usr/local/lib/python2.7/site-packages")
import time

import ottplib

VERSION = "0.1.1"
AUTHORS = "Michael Wouters"

# Time stamp formats
TS_UNIX = 0
TS_TOD  = 1

# Counter mode
TIC_MODE_TS=0 # time stamp mode
TIC_MODE_TI=1 # time interval mode


# Globals
debug = False
killed = False

# ------------------------------------------
def SignalHandler(signal,frame):
	global killed
	killed=True
	return

# ------------------------------------------
def ShowVersion():
	print  os.path.basename(sys.argv[0])," ",VERSION
	print "Written by",AUTHORS
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		print msg
	return

# ------------------------------------------
def ErrorExit(msg):
	print msg
	sys.exit(0)
	
# ------------------------------------------
def Initialise(configFile):
	cfg=ottplib.LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit("Error loading " + configFile)
		
	# Check for required arguments
	reqd = ['counter:port','paths:counter data','counter:file extension','counter:lock file']
	for k in reqd:
		if (not cfg.has_key(k)):
			ErrorExit("The required configuration entry " + k + " is undefined")
		
	return cfg

# ------------------------------------------
# Main 
# ------------------------------------------

tsformat = TS_TOD
tic_mode = TIC_MODE_TS

home =os.environ['HOME'] + '/';
configFile = os.path.join(home,'etc/gpscv.conf');

parser = argparse.ArgumentParser(description='Log a TAPR TICC in TI mode')
parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--settings','-s',help='show settings',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')
args = parser.parse_args()

debug = args.debug

if (args.version):
	ShowVersion()
	exit()

configFile = args.config;

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + " not found")
	
logPath = os.path.join(home,'logs')
if (not os.path.isdir(logPath)):
	ErrorExit(logPath + "not found")

cfg=Initialise(configFile)

#for key,val in cfg.items():
#	print key,"=>",val
	
port = cfg['counter:port']
dataPath= ottplib.MakeAbsolutePath(cfg['paths:counter data'], home)

ctrExt = cfg['counter:file extension']
if (None == re.search(r'\.$',ctrExt)):
	ctrExt = '.' + ctrExt 

headerGen=''
if (cfg.has_key('counter:header generator')):
	headerGen = ottplib.MakeAbsoluteFilePath(cfg['counter:header generator'],home,home+'/bin')
print headerGen

if (cfg.has_key('counter:mode')):
	if (cfg['counter:mode'] == 'timestamp'):
		tic_mode = TIC_MODE_TS
	elif (cfg['counter:mode'] == 'time interval'):
		tic_mode = TIC_MODE_TI
		
if (cfg.has_key('counter:timestamp format')):
	if (cfg['counter:timestamp format'] == 'unix'):
		tsformat = TS_UNIX
	elif (cfg['counter:timestamp format'] == 'time of day'):
		tsformat = TS_TOD
		


# Create the process lock		
lockFile=ottplib.MakeAbsoluteFilePath(cfg['counter:lock file'],home,home + '/etc')
Debug("Creating lock " + lockFile)
if (not ottplib.CreateProcessLock(lockFile)):
	ErrorExit("Couldn't create a lock")

# Create UUCP lock for the serial port
uucpLockPath="/var/lock";
if (cfg.has_key('paths:uucp lock')):
	uucpLockPath = cfg['paths:uucp lock']

ret = subprocess.check_output(['/usr/local/bin/lockport','-d',uucpLockPath,'-p',str(os.getpid()),port,sys.argv[0]])

if (re.match('1',ret)==None):
	ottplib.RemoveProcessLock(lockFile)
	ErrorExit('Could not obtain a lock on ' + port + '.Exiting.')

signal.signal(signal.SIGINT,SignalHandler)
signal.signal(signal.SIGTERM,SignalHandler)

Debug("Opening " + port)

try:
	ser = serial.Serial(port,115200,timeout=2)
except:
	print "Error on device" + port
	ottplib.RemoveProcessLock(lockFile)
	subprocess.check_output(['/usr/local/bin/lockport','-r',port]) # but ignore return value anyway
	exit()

state = 1 # state machine for parsing the startup information
				# note that this is output when a serial connection is initiated

# Eat junk and startup information
ctrcfg=''
oldmjd=-1
while (state < 4):
	try:
		l = ser.readline()
	except select.error as (code,msg):
		sys.exit(0)
	if (len(l) == 0):
		print 'Timeout'
		sys.exit(0)
	# If already connected then we won't see the startup bumpf
	l=l[:-2]
	if (len(l) == 0):
		continue
	# If the first character is not # then break out of this completely
	if (not (l[0] == '#')):
		state=4
		
	if (state == 1):
		if (l.find('TICC Configuration') != -1):
			state = 2
	elif (state == 2):
		ctrcfg=ctrcfg  + l + '\n'
		if (l.find('FUDGE0') != -1):
			state = 3
			if (args.settings): # this is here so we don't get stuck on a TICC not configured for TI yet
				print ctrcfg
				ottplib.RemoveProcessLock(lockFile)
				ser.close()
				subprocess.check_output(['/usr/local/bin/lockport','-r',port]) 
				sys.exit(0)
	elif (state == 3):
		if ((tic_mode == TIC_MODE_TI) and (l.find('time interval A->B (seconds)') != -1)):
			state = 4
		elif ((tic_mode == TIC_MODE_TS) and (l.find('Timestamp') != -1)):
			state = 4

nbad = 0

# Can get some buffered stuff at the beginning if already running so toss it
for i in xrange(20):
	try:
		l = ser.readline()
	except select.error as (code,msg): # CTRL-C returns code=4
		print msg
		break;
	
while (not killed):
	
	# Check whether we need to create a new log file
	tt = time.time()
	mjd = ottplib.MJD(tt)
	if (not( mjd == oldmjd)):
		oldmjd = mjd
		if (tic_mode == TIC_MODE_TI):
			fnoutA = dataPath + str(mjd) + ctrExt
		elif (tic_mode == TIC_MODE_TS):
			fnoutA = dataPath + str(mjd) + '.A' + ctrExt
			fnoutB = dataPath + str(mjd) + '.B' + ctrExt
		if (not os.path.isfile(fnoutA)):
			foutA = open(fnoutA,'w',0)
			if (os.path.isfile(headerGen) and os.access(headerGen,os.X_OK)):
				header=subprocess.check_output([headerGen])
				foutA.write(header)
			foutA.write(ctrcfg) # Configuration after the header
		else:
			foutA = open(fnoutA,'a',0)
			
		Debug('Opened '+fnoutA)
		
		if (tic_mode == TIC_MODE_TS):
			if (not os.path.isfile(fnoutB)):
				foutB = open(fnoutB,'w',0)
				if (os.path.isfile(headerGen) and os.access(headerGen,os.X_OK)):
					header=subprocess.check_output([headerGen])
					foutB.write(header)
				foutB.write(ctrcfg) # Configuration after the header
			else:
				foutB = open(fnoutB,'a',0)
			Debug('Opened '+fnoutB)
			
	try:
		l = ser.readline()[:-2]
	except select.error as (code,msg): # CTRL-C returns code=4
		print msg
		break;
	
	if (tic_mode == TIC_MODE_TI): 
		if (l.find('TI(A->B)') != -1):
			if (tsformat == TS_UNIX):
				foutA.write(str(time.time())+' '+l.split()[0]+'\n')
			elif (tsformat == TS_TOD):
				foutA.write(time.strftime('%H:%M:%S',time.gmtime()) + ' ' + l.split()[0]+'\n')
			nbad = nbad - 1
		else:
			nbad = nbad + 1
	elif (tic_mode == TIC_MODE_TS):
		if (l.find('chA') != -1):
			ts = l.split()[0]
			if (None == re.match('^\d+\.\d{12}',ts)):
				nbad =nbad + 1
			else:
				if (tsformat == TS_UNIX):
					foutA.write(str(time.time())+' '+ts+'\n')
				elif (tsformat == TS_TOD):
					foutA.write(time.strftime('%H:%M:%S',time.gmtime()) + ' ' + ts +'\n')
				nbad =nbad -1
		elif (l.find('chB') != -1):
			ts = l.split()[0]
			if (None == re.match('^\d+\.\d{12}',ts)):
				nbad =nbad + 1
			else:
				if (tsformat == TS_UNIX):
					foutB.write(str(time.time())+' '+ts+'\n')
				elif (tsformat == TS_TOD):
					foutB.write(time.strftime('%H:%M:%S',time.gmtime()) + ' ' + ts +'\n')
				nbad =nbad -1
				
	if (nbad < 0):
		nbad =0
					
	if (nbad >= 30):
		foutA.close()
		ottplib.RemoveProcessLock(lockFile)
		ser.close()
		subprocess.check_output(['/usr/local/bin/lockport','-r',port])
		ErrorExit("Too many bad readings - no signal?")

# All done - cleanup		
foutA.close()
ottplib.RemoveProcessLock(lockFile)
ser.close()
subprocess.check_output(['/usr/local/bin/lockport','-r',port])
