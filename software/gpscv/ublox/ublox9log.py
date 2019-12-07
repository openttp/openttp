#!/usr/bin/python
#

#
# The MIT License (MIT)
#
# Copyright (c) 2019 Michael J. Wouters
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
# ublox9log.py - a logging script for the TAPR TICC
#
# Modification history
#

import argparse
import os
import re
import select
import serial
import signal
import string
import subprocess
import sys
# This is where ottplib is installed
sys.path.append("/usr/local/lib/python2.7/site-packages")
import time

import ottplib

VERSION = "0.0.1"
AUTHORS = "Michael Wouters,Louis Marais"

# File formats
OPENTTP_FORMAT=0;
NATIVE_FORMAT=1;

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
	reqd = ['paths:receiver data','receiver:port','receiver:file extension', \
		'receiver:lock file', 'receiver:pps offset','receiver:model', \
		'receiver:status file']
	
	for k in reqd:
		if (not cfg.has_key(k)):
			ErrorExit("The required configuration entry '" + k + "' is undefined")
		
	return cfg

#-----------------------------------------------------------------------------
def Cleanup():
	# Hmm ugly globals
	ottplib.RemoveProcessLock(lockFile)
	if (not ser==None):
		ser.close()
		subprocess.check_output(['/usr/local/bin/lockport','-r',port])
	
#-----------------------------------------------------------------------------
def OpenDataFile(mjd):

	fname = dataPath + str(mjd) + dataExt;
	appending = os.path.isfile(fname)
	
	Debug('Opening ' + fname);

	if (dataFormat == OPENTTP_FORMAT):
		try:
			fout = open(fname,'a',0)
		except:
			Cleanup()
			ErrorExit('Failed to open data file ' + fname)
			
		fout.write('# {} (version {}) {}\n'.format(os.path.basename(sys.argv[0]),VERSION, \
			'continuing' if appending  else 'beginning'))
		fout.write('# {} {}\n'.format('Appending to ' if appending  else 'Beginning new',fname))
		fout.write('@ MJD={}\n'.format(mjd))
	else: # native mode UNTESTED
		try:
			fout = open(fname,'ab',0)
		except:
			Cleanup()
			ErrorExit('Failed to open data file ' + fname)
			
	return fout

# ------------------------------------------
# Main 
# ------------------------------------------

home =os.environ['HOME'] + os.sep
configFile = os.path.join(home,'etc','gpscv.conf')


parser = argparse.ArgumentParser(description='Log ublox 9 GNSS receivers',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--reset','-r',help='reset receiver',action='store_true')
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

# Check for receiver model
rxModel = string.lower(cfg['receiver:model'])
if (None == re.search('zed-f9',rxModel)):
	ErrorExit('Receiver model ' + rxModel + ' is not supported')
	
port = cfg['receiver:port']
dataPath = ottplib.MakeAbsolutePath(cfg['paths:receiver data'], home)
rxStatus = ottplib.MakeAbsolutePath(cfg['receiver:status file'], home)

dataExt = cfg['receiver:file extension']
if (None == re.search(r'\.$',dataExt)):
	dataExt = '.' + dataExt 

dataFormat = OPENTTP_FORMAT
if (cfg.has_key('receiver:file format')):
	ff = string.lower(cfg['receiver:file format'])
	if (ff == 'native'):
		dataFormat = NATIVE_FORMAT;
		cfg['receiver:file extension']= '.ubx'
	
# Create the process lock		
lockFile = ottplib.MakeAbsoluteFilePath(cfg['receiver:lock file'],home,home + '/etc')
Debug("Creating lock " + lockFile)
if (not ottplib.CreateProcessLock(lockFile)):
	ErrorExit("Couldn't create a lock")

# Create UUCP lock for the serial port
uucpLockPath = "/var/lock";
if (cfg.has_key('paths:uucp lock')):
	uucpLockPath = cfg['paths:uucp lock']

ret = subprocess.check_output(['/usr/local/bin/lockport','-d',uucpLockPath,'-p',str(os.getpid()),port,sys.argv[0]])

if (re.match('1',ret)==None):
	ottplib.RemoveProcessLock(lockFile)
	ErrorExit('Could not obtain a lock on ' + port + '.Exiting.')

signal.signal(signal.SIGINT,SignalHandler)
signal.signal(signal.SIGTERM,SignalHandler)

Debug("Opening " + port)

ser=None # so that this can flag failure to open the port
try:
	ser = serial.Serial(port,115200,timeout=2)
except:
	Cleanup()
	ErrorExit('Failed to open ' + port)

tt = time.time()
mjd = ottplib.MJD(tt)
OpenDataFile(mjd)
		
while (not killed):
	#
	pass

Cleanup()
