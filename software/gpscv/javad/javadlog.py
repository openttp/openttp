#!/usr/bin/python3

#
# The MIT License (MIT)
#
# Copyright (c) 2026 Michael J. Wouters, Louis Marais
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
                  
import argparse
import binascii
import datetime
import os
import re
import select
import serial
import signal
import socket
import string
import struct
import subprocess
import sys

# This is where ottplib is installed
sys.path.append('/usr/local/lib/python3.6/site-packages')
sys.path.append('/usr/local/lib/python3.8/site-packages')
sys.path.append('/usr/local/lib/python3.10/site-packages')
sys.path.append('/usr/local/lib/python3.12/dist-packages')

import time

import ottplib as ottp

VERSION = '0.0.2'
AUTHORS = 'Michael Wouters,Louis Marais'

# Globals
debug = False
killed = False

NLEAP = 18 # no more leap seconds!
GPS_EPOCH = 315964800 # GPS epoch in the Unix time scale
TOW_INVALID = 4294967295
WNC_INVALID  = 65535


# FIXME This is not perfect
# Reboot behaviour seems to vary. You may not get SIGTERM. YMMV
#

#-----------------------------------------------------------------------------
def SignalHandler(signal,frame):
	#global fdata
	#msg = '# {} {} killed SIG {}\n'.format( \
			#time.strftime('%Y-%m-%d %H:%M:%S',time.gmtime()), \
			#os.path.basename(sys.argv[0]),signal)
	#if (dataFormat == OPENTTP_FORMAT):
		#fdata.write(msg)
	#Cleanup()
	## If you try to print to the console after SIGHUP, it stops further execution 
	#print (msg)
	#sys.exit(0)
	global killed
	killed = True
	return

#-----------------------------------------------------------------------------
def Cleanup():
	# Hmm ugly globals
	ottp.RemoveProcessLock(lockFile)
	if (not serport==None):
		# TODO # turn off all  output
		serport.close()
		subprocess.check_output(['/usr/local/bin/lockport','-r',port])

#-----------------------------------------------------------------------------
def OpenDataFile(mjd):

	fname = dataPath + str(mjd) + dataExt;
	appending = os.path.isfile(fname)
	
	ottp.Debug('Opening ' + fname);
	
	try:
		fout = open(fname,'ab')
	except:
		Cleanup()
		ottp.ErrorExit('Failed to open data file ' + fname)
		
	fout.flush()
	return fout


#----------------------------------------------------------------------------
def ConfigureReceiver(rxcfg):
	pass
		
#-----------------------------------------------------------------------------
# Main 
#-----------------------------------------------------------------------------

home =os.environ['HOME'] + os.sep
configFile = os.path.join(home,'etc','gpscv.conf')
nLeap = NLEAP
checkSync = True

parser = argparse.ArgumentParser(description='Log a Javad GREIS receiver',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--nosynccheck','-n',help='disable pps and ext ref sync check',action='store_true')
#parser.add_argument('--reset','-r',help='reset receiver and exit',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

ottp.SetDebugging(args.debug)

if args.nosynccheck:
	checkSync = False
	
configFile = args.config;

if (not os.path.isfile(configFile)):
	ottp.ErrorExit(configFile + ' not found')
	
logPath = os.path.join(home,'log')
if (not os.path.isdir(logPath)):
	logPath = os.path.join(home,'logs')
if (not os.path.isdir(logPath)):
	ottp.ErrorExit(logPath + "not found")

cfg=ottp.Initialise(configFile,['paths:receiver data','receiver:port','receiver:file extension', \
		'receiver:lock file','receiver:model', \
		'receiver:configuration'])

rxTimeout = 600
if ('receiver:timeout' in cfg):
	rxTimeout = int(cfg['receiver:timeout'])

port = cfg['receiver:port']

dataPath = ottp.MakeAbsolutePath(cfg['paths:receiver data'], home)

dataExt = cfg['receiver:file extension']
if (None == re.search(r'\.$',dataExt)): # add a '.' separator if needed
	dataExt = '.' + dataExt 

# 
syncAlarmTimeout = 60

# Create the process lock		
lockFile = ottp.MakeAbsoluteFilePath(cfg['receiver:lock file'],home,home + '/etc')
ottp.Debug('Creating lock ' + lockFile)
if (not ottp.CreateProcessLock(lockFile)):
	ottp.ErrorExit("Couldn't create a lock")

signal.signal(signal.SIGINT,SignalHandler) 
signal.signal(signal.SIGTERM,SignalHandler) 
signal.signal(signal.SIGHUP,SignalHandler) # not usually run with a controlling TTY, but handle it anyway


# Create UUCP lock for the serial port
uucpLockPath = '/var/lock';
if ('paths:uucp lock' in cfg):
	uucpLockPath = cfg['paths:uucp lock']

ottp.Debug('Creating uucp lock in ' + uucpLockPath)
ret = subprocess.check_output(['/usr/local/bin/lockport','-d',uucpLockPath,'-p',str(os.getpid()),port,sys.argv[0]])

if (re.match(rb'1',ret)==None):
	ottp.RemoveProcessLock(lockFile)
	ottp.ErrorExit('Could not obtain a lock on ' + port + '.Exiting.')

ottp.Debug('Opening ' + port)

serport=None # so that this can flag failure to open the port
try:
	serport = serial.Serial(port,115200,timeout=0.2)
	ottp.Debug('Port open')
except:
	Cleanup()
	ottp.ErrorExit('Failed to open ' + port)
	
#ConfigureReceiver(rxCfg)

tNow = time.time()
tGPSNow = tNow + nLeap - GPS_EPOCH # best guess, until we get something from the receiver
rolloverValid = False 
tGPSNextRollover = 86400*int(tGPSNow/86400) + 86400        # again, our best guess

mjd = ottp.MJD(tNow)

fdata = OpenDataFile(mjd)

tLastStatusUpdate=0
tLastSyncOK = time.time() # assume OK on startup

tLastMsg=tNow
inp = b''   # buffer for reading incoming messages

killed = False

ottp.Debug('Starting ..')

while (not killed):
	
	# Check for timeout
	tNow = time.time()
	tGPSNow = tNow + nLeap - GPS_EPOCH
	if (tNow - tLastMsg > rxTimeout):
		msg = time.strftime('%Y-%m-%d %H:%M:%S',time.gmtime()) + ' no response from receiver'
		print ('# ' + msg + '\n')
		break
		killed = True

	# The guts
	select.select([serport],[],[],0.2)
	if (serport.in_waiting == 0):
		#Debug('Timeout') # not very informative
		continue
	
	newinp = serport.read(serport.in_waiting)
	
	tLastMsg = time.time()  # got a message, so reset the timeout
			
	#if (tGPSNow > 0 and not(rolloverValid)): # once we get valid time from the receiver, update the rollover time 
		#tGPSNextRollover = 86400*int(tGPSNow/86400) + 86400
		#rolloverValid = True
		#ottp.Debug(str(datetime.datetime.now(datetime.timezone.utc)) + ' Updated: tGPSNextRollover = '
					#+ str(tGPSNextRollover))
		## The initial guess for GPS time may have been bad so rollover the file
		#fdata.close()
		#mjd=ottp.MJD(tGPSNow + GPS_EPOCH)
		#fdata = OpenDataFile(mjd)
	
	if (tGPSNow >= tGPSNextRollover): # invalid tGPSNow == -1 so this will fail 
		ottp.Debug(str(datetime.datetime.now(datetime.timezone.utc))+' tGPSNow = ' + str(tGPSNow))
		fdata.close()
		mjd=ottp.MJD(tGPSNow + GPS_EPOCH)
		ottp.Debug(str(datetime.datetime.now(datetime.timezone.utc))+' Next MJD = ' + str(mjd))
		fdata = OpenDataFile(mjd)
		tGPSNextRollover = 86400*int(tGPSNow/86400) + 86400
		ottp.Debug(str(datetime.datetime.now(datetime.timezone.utc))+' tGPSNextRollover = ' + str(tGPSNextRollover))
	
	fdata.write(newinp)
	
Cleanup()
