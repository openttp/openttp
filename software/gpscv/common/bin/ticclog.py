#!/usr/bin/python3
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

# WARNING!!!!
# Note that TOD timestamps are rounded!

import argparse
import os
import re
import select
import serial
import signal
import subprocess
import sys
# This is where ottplib is installed
sys.path.append("/usr/local/lib/python3.6/site-packages")
sys.path.append("/usr/local/lib/python3.8/site-packages")
sys.path.append("/usr/local/lib/python3.10/dist-packages")
import time

import ottplib

VERSION = "0.1.6"
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
	reqd = ['counter:port','paths:counter data','counter:file extension','counter:lock file']
	for k in reqd:
		if not(k in cfg):
			ErrorExit("The required configuration entry " + k + " is undefined")
		
	return cfg

# ------------------------------------------
# Main 
# ------------------------------------------

tsformat = TS_TOD
tic_mode = TIC_MODE_TS

home =os.environ['HOME'] + os.sep
configFile = os.path.join(home,'etc','gpscv.conf')

parser = argparse.ArgumentParser(description='Log a TAPR TICC in TI mode',
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
if ('counter:header generator' in cfg):
	headerGen = ottplib.MakeAbsoluteFilePath(cfg['counter:header generator'],home,home+'/bin')

if ('counter:mode' in cfg):
	cm = cfg['counter:mode'].lower()
	if (cm == 'timestamp'):
		tic_mode = TIC_MODE_TS
	elif (cm == 'time interval'):
		tic_mode = TIC_MODE_TI
		
if ('counter:timestamp format' in cfg):
	tsf = cfg['counter:timestamp format'] 
	if (tsf == 'unix'):
		tsformat = TS_UNIX
	elif (tsf == 'time of day'):
		tsformat = TS_TOD
		
# Create the process lock		
lockFile=ottplib.MakeAbsoluteFilePath(cfg['counter:lock file'],home,home + '/etc')
Debug("Creating lock " + lockFile)
if (not ottplib.CreateProcessLock(lockFile)):
	ErrorExit("Couldn't create a lock")

# Create UUCP lock for the serial port
uucpLockPath="/var/lock";
if ('paths:uucp lock' in cfg):
	uucpLockPath = cfg['paths:uucp lock']

ret = subprocess.check_output(['/usr/local/bin/lockport','-d',uucpLockPath,'-p',str(os.getpid()),port,sys.argv[0]])

if (re.match(b'1',ret)==None):
	ottplib.RemoveProcessLock(lockFile)
	ErrorExit('Could not obtain a lock on ' + port + '.Exiting.')

signal.signal(signal.SIGINT,SignalHandler)
signal.signal(signal.SIGTERM,SignalHandler)
signal.signal(signal.SIGHUP,SignalHandler) # not usually run with a controlling TTY but handle it anyway

Debug("Opening " + port)

try:
	ser = serial.Serial(port,115200,timeout=2)
except:
	print("Error on device" + port)
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
	except select.error as err:
		print(err)
		sys.exit(0)
	if (len(l) == 0):
		print('Timeout')
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
				print(ctrcfg)
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
for i in range(20):
	try:
		l = ser.readline()
	except select.error as err: # CTRL-C returns code=4
		print(err)
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
			foutA = open(fnoutA,'w')
			if (os.path.isfile(headerGen) and os.access(headerGen,os.X_OK)):
				header=subprocess.check_output([headerGen,'-c',configFile])
				foutA.write(header.rstrip()+'\n') # make sure there is just one linefeed
			foutA.write(ctrcfg) # Configuration after the header
		else:
			foutA = open(fnoutA,'a')
			
		Debug('Opened '+fnoutA)
		
		if (tic_mode == TIC_MODE_TS):
			if (not os.path.isfile(fnoutB)):
				foutB = open(fnoutB,'w')
				if (os.path.isfile(headerGen) and os.access(headerGen,os.X_OK)):
					header=subprocess.check_output([headerGen])
					foutB.write(header.rstrip()+'\n')
				foutB.write(ctrcfg) # Configuration after the header
			else:
				foutB = open(fnoutB,'a')
			Debug('Opened '+fnoutB)
			
	try:
		l = ser.readline()[:-2]
	except select.error as err: # CTRL-C returns code=4
		print(err)
		break;
	
	lstr = l.decode('utf-8')
	
	tt = time.time() # floating point!
	ttround = round(tt) # round to take care of system time being close to the epoch
											# of course, if the stop pulse is 0.5 s off, this all falls over
											
	Debug(lstr)
	Debug(str(tt) + ' ' + str(ttround))
	if (tic_mode == TIC_MODE_TI): 
		if (lstr.find('TI(A->B)') != -1):
			if (tsformat == TS_UNIX):
				foutA.write(str(tt) + ' ' + lstr.split()[0]+'\n')
			elif (tsformat == TS_TOD):
				foutA.write(time.strftime('%H:%M:%S',time.gmtime(ttround)) + ' ' + lstr.split()[0]+'\n')
			foutA.flush()
			nbad = nbad - 1
		else:
			nbad = nbad + 1
	elif (tic_mode == TIC_MODE_TS):
		if (lstr.find('chA') != -1):
			ts = lstr.split()[0]
			if (None == re.match('^\d+\.\d{12}',ts)):
				nbad =nbad + 1
			else:
				if (tsformat == TS_UNIX):
					foutA.write(str(tt) + ' ' + ts +'\n')
				elif (tsformat == TS_TOD):
					foutA.write(time.strftime('%H:%M:%S',time.gmtime(ttround)) + ' ' + ts +'\n')
				foutA.flush()
				nbad =nbad -1
		elif (lstr.find('chB') != -1):
			ts = lstr.split()[0]
			if (None == re.match('^\d+\.\d{12}',ts)):
				nbad =nbad + 1
			else:
				if (tsformat == TS_UNIX):
					foutB.write(str(tt) + ' ' + ts + '\n')
				elif (tsformat == TS_TOD):
					foutB.write(time.strftime('%H:%M:%S',time.gmtime(ttround)) + ' ' + ts +'\n')
				foutB.flush()
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
