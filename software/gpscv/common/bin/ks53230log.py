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
# ks53230log.py a logging script for the Keysight 53230 counter
#

import argparse
import os
import re
import signal
import sys
import time
import usbtmc

sys.path.append("/usr/local/lib/python3.6/site-packages")
import ottplib

VERSION = "0.0.1"
AUTHORS = "Michael Wouters"

# Globals
debug = False
killed = False
cntrCfg = []

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
	reqd = ['paths:counter data','counter:file extension','counter:lock file']
	for k in reqd:
		if (not k in cfg):
			ErrorExit("The required configuration entry " + k + " is undefined")
		
	return cfg

# --------------------------------------------
# Keysight functions
# Don't call these directly
# --------------------------------------------

def _Keysight53230Open(cfg):
	global ks53230
	try:
		ks53230 =  usbtmc.Instrument(0x0957,usbModelID)
	except:
		ErrorExit("Can\'t open the counter")
	
def _Keysight53230Reset():
	try:
		ks53230.write('*RST;*CLS')
	except:
		ErrorExit("Can't write to the counter. Check your USB device permissions (nb raw USB so you need a udev rule for it)")
	
	id = ks53230.ask("*IDN?")
	Debug('Opened ' + id)
	return id

def _Keysight53230Configure(cfg):
	global cntrCfg
	defaultCfg = [
			'CONF:TINT (@1),(@2)',             # set counter to time-interval mode
			'TRIG:SLOP POS',
			'SYSTEM:TIMEOUT 3',
			':INP1:PROB 1',                    # input attenuation x1
			':INP2:PROB 1',                    # input attenuation x1
			':INP1:COUP DC',                   # coupling DC
			':INP2:COUP DC',                   # coupling DC
			':INP1:IMP 50',                    # input impedance 50 ohms
			':INP2:IMP 50',                    # input impedance 50 ohms
			':INP1:SLOP POS',
			':INP2:SLOP POS',
			':INP1:LEV 1',
			':INP2:LEV 1'
		]
	
	cntrCfg = defaultCfg

	if ('counter:configuration' in cfg):
		cntrCfgFile = ottplib.MakeAbsoluteFilePath(cfg['counter:configuration'],home,home+'/etc')
		if (os.path.isfile(cntrCfgFile)):
			cntrCfg = []
			fin = open(cntrCfgFile,'r')
			for l in fin:
				l = l.strip() # trim leading and trailing whitespace
				if (0==len(l)):
					continue
				if (not None == re.match('#',l)): 
					continue
				l = re.sub('#.*$','',l)
				Debug('Read command: ' + l)
				cntrCfg.append(l)
		else:
			ErrorExit('Can\'t open ' + cntrCfgFile)

	for c in cntrCfg:
		Debug(c)
		ks53230.write(c)
	
	# Check for errors
	errcode,errtxt = ks53230.ask(':SYST:ERR?').split(',')
	if not('+0'==errcode):
		ErrorExit('SCPI error ' + errcode + ',' + errtxt)

	ks53230.timeout = 5*1000
	
def _Keysight53230Arm():
	pass
	
def _Keysight53230Read():
	try:
		rdg = ks53230.ask('READ?') 
	except:
		errcode,errtxt = ks53230.ask(':SYST:ERR?').split(',')
		Debug('SCPI error ' + errcode + ',' + errtxt)
		return '-1'
	return rdg

def _Keysight53230Close():
	ks53230.close()

# ------------------------------------------
# Counter wrapper functions
#

def CounterOpen(cfg):
	_Keysight53230Open(cfg)

def CounterReset():
	return _Keysight53230Reset()
	
def CounterConfigure(cfg):
	_Keysight53230Configure(cfg)

def CounterArm():
	_Keysight53230Arm()
	
def CounterRead():
	rdg = _Keysight53230Read()
	return rdg

def CounterClose():
	_Keysight53230Close()
		
# ------------------------------------------
# The main 
# ------------------------------------------

maxTimeouts = 5
usbModelID = 0x1907

home =os.environ['HOME'] + os.sep
configFile = os.path.join(home,'etc','gpscv.conf')

parser = argparse.ArgumentParser(description='Log a Keysight 53230 counter in TI mode',
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

dataPath= ottplib.MakeAbsolutePath(cfg['paths:counter data'], home)

ctrExt = cfg['counter:file extension']
if (None == re.search(r'\.$',ctrExt)):
	ctrExt = '.' + ctrExt 

headerGen=''
if ('counter:header generator' in cfg):
	headerGen = ottplib.MakeAbsoluteFilePath(cfg['counter:header generator'],home,home+'/bin')

# Create the process lock		
lockFile=ottplib.MakeAbsoluteFilePath(cfg['counter:lock file'],home,home + '/etc')
Debug("Creating lock " + lockFile)
if (not ottplib.CreateProcessLock(lockFile)):
	ErrorExit("Couldn't create a lock")

signal.signal(signal.SIGINT,SignalHandler)
signal.signal(signal.SIGTERM,SignalHandler)
signal.signal(signal.SIGHUP,SignalHandler) # not usually run with a controlling TTY, but handle it anyway

# Preliminaries over

CounterOpen(cfg)
id = CounterReset()
CounterConfigure(cfg)

oldmjd=-1

# instr.timeout = 5*1000 # this stops a hang if the counter stops triggering

nTimeouts = 0
while (not killed):
	tt = time.time()
	mjd = ottplib.MJD(tt)
	if (not( mjd == oldmjd)):
		oldmjd = mjd
		fnout = dataPath + str(mjd) + ctrExt
		if (not os.path.isfile(fnout)):
			fout = open(fnout,'w') # unbuffered output is not allowed (but we don't care really)
			if (os.path.isfile(headerGen) and os.access(headerGen,os.X_OK)):
				header=subprocess.check_output([headerGen,'-c',configFile])
				fout.write(header.rstrip()+'\n') # make sure there is just one linefeed
			fout.write('# ' + id + '\n')
			for c in cntrCfg:
				fout.write('# ' + c + '\n')
			fout.flush()
		else:
			fout = open(fnout,'a')
		Debug('Opened ' + fnout)
	try:
		# Get a reading from the counter

		CounterArm()
		rdg = CounterRead() # nb string
		tt = round(time.time())  # round the timestamp 
		tstamp = time.strftime('%H:%M:%S',time.gmtime(tt))
		fout.write(tstamp + ' ' + str(rdg) + '\n')
		fout.flush()
		Debug(tstamp + ' ' + str(rdg))
		nTimeouts = 0 # reset the count 
	except:
		Debug('Timeout')
		nTimeouts += 1
		if (nTimeouts == maxTimeouts):
			CounterClose()
			fout.close()
			ottplib.RemoveProcessLock(lockFile)
			ErrorExit('Too many timeouts')
	
# All done - cleanup
CounterClose()
fout.close()
ottplib.RemoveProcessLock(lockFile)
