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
# cnt9xlog.py a logging script for the pendulum CNT9x counter
#

import argparse
import os
import re
import signal
import sys
import time
import usbtmc

sys.path.append("/usr/local/lib/python3.6/site-packages") # Ubuntu 18.04
sys.path.append("/usr/local/lib/python3.8/site-packages") # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22.04

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
	reqd = ['paths:counter data','counter:file extension','counter:lock file']
	for k in reqd:
		if (not k in cfg):
			ErrorExit("The required configuration entry " + k + " is undefined")
		
	return cfg

# ------------------------------------------
# The main 
# ------------------------------------------

defaultCntrCfg = [':SENS:TINT:AUTO OFF',
		':SENS:FUNC \'TINT 1,2\'',
		':INIT:CONT ON',
		':TRIG:SOUR IMM',  # disable pacing (defined trigger rate)
		':INP1:COUP DC',':INP2:COUP DC',
		':INP1:IMP 50',':INP2:IMP 50',
		':INP1:LEVEL 1.0',':INP2:LEVEL 1.0',
		':INP1:SLOPE POS',':INP2:SLOPE POS']

maxTimeouts = 5

home =os.environ['HOME'] + os.sep
configFile = os.path.join(home,'etc','gpscv.conf')

parser = argparse.ArgumentParser(description='Log a Pendulum CNT9x in TI mode',
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

try:
	instr =  usbtmc.Instrument(0x14eb, 0x91)
except:
	ErrorExit('Can\'t open the device')

try:
	instr.write('*RST;*CLS')
except:
	ErrorExit('Can\'t write to the device. Check your permissions')
	
id = instr.ask("*IDN?")
Debug('Opened ' + id)

cntrCfg = defaultCntrCfg

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
	instr.write(c)

errcode,errtxt = instr.ask(':SYST:ERR?').split(',')
if not('0'==errcode):
	ErrorExit('SCPI error ' + errcode + ',' + errtxt)

oldmjd=-1

instr.timeout = 5*1000 # this stops a hang if the counter stops triggering

instr.write(':SYST:TOUT ON;TOUT:TIME 2.0')

nTimeouts = 0
while (not killed):
	tt = time.time()
	mjd = ottplib.MJD(tt)
	if (not( mjd == oldmjd)):
		oldmjd = mjd
		fnout = dataPath + str(mjd) + ctrExt
		if (not os.path.isfile(fnout)):
			fout = open(fnout,'w') # unbuffered output is not allowed (but we don't care really)q
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
		rdg = float(instr.ask(':READ?'))
		tstr = time.strftime('%H:%M:%S',time.gmtime())
		fout.write(tstr + ' ' + str(rdg) + '\n')
		Debug(tstr + ' ' + str(rdg))
		nTimeouts = 0 # reset the count 
	except:
		Debug('Timeout')
		nTimeouts += 1
		if (nTimeouts == maxTimeouts):
			fout.close()
			ottplib.RemoveProcessLock(lockFile)
			ErrorExit('Too many timeouts')
	
# All done - cleanup		
fout.close()
ottplib.RemoveProcessLock(lockFile)
