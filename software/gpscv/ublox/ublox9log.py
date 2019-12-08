#!/usr/bin/python3
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
# ublox9log.py - a logging script for the ublox series 9 GNSS receiver
#
# Modification history
#

import argparse
import binascii
import os
import re
import select
import serial
import signal
import string
import struct
import subprocess
import sys
# This is where ottplib is installed
sys.path.append('/usr/local/lib/python3.6/site-packages')
import time

import ottplib

VERSION = '0.0.1'
AUTHORS = 'Michael Wouters,Louis Marais'

# File formats
OPENTTP_FORMAT=0;
NATIVE_FORMAT=1;

# Globals
debug = False
killed = False
ubxMsgs = set() # the set of messages we want to log

# ------------------------------------------
def SignalHandler(signal,frame):
	global killed
	killed=True
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		print (time.strftime('%H:%M:%S ',time.gmtime()) + msg)
	return

# ------------------------------------------
def ErrorExit(msg):
	print (msg)
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
		if (not k in cfg):
			ErrorExit('The required configuration entry "' + k + '" is undefined')
		
	return cfg

#-----------------------------------------------------------------------------
def Cleanup():
	# Hmm ugly globals
	ottplib.RemoveProcessLock(lockFile)
	if (not serport==None):
		serport.close()
		subprocess.check_output(['/usr/local/bin/lockport','-r',port])
	
#-----------------------------------------------------------------------------
def OpenDataFile(mjd):

	fname = dataPath + str(mjd) + dataExt;
	appending = os.path.isfile(fname)
	
	Debug('Opening ' + fname);
	
	if (dataFormat == OPENTTP_FORMAT):
		try:
			fout = open(fname,'a')
		except:
			Cleanup()
			ErrorExit('Failed to open data file ' + fname)
			
		fout.write('# {} {} (version {})\n'.format( \
			time.strftime('%H:%M:%S',time.gmtime()),
			os.path.basename(sys.argv[0]),VERSION, \
			'continuing' if appending  else 'beginning'))
		fout.write('# {} {}\n'.format('Appending to ' if appending  else 'Beginning new',fname))
		fout.write('@ MJD={}\n'.format(mjd))
	else: # native mode UNTESTED
		try:
			fout = open(fname,'ab')
		except:
			Cleanup()
			ErrorExit('Failed to open data file ' + fname)
			
	return fout

#----------------------------------------------------------------------------
# Calculate the UBX checksum - needed for sending UBX messages
# TESTED 2019-12-07
def Checksum(msg):
	ba = bytearray()
	ba.extend(msg)
	cka = 0
	ckb = 0
	
	for b in ba:
		cka = cka + b
		ckb = ckb + cka
		cka = cka & 0xff
		ckb = ckb & 0xff
		
	return struct.pack('2B',cka,ckb);
	
# ---------------------------------------------------------------------------
def SendCommand(serport,cmd):
	cksum = Checksum(cmd)
	msg = b'\xb5\x62' + cmd + cksum
	serport.write(msg)

#----------------------------------------------------------------------------
def ConfigureReceiver(serport):

	# Note that reset causes a USB disconnect
	# so it's a good idea to use udev to map the device to a 
	# fixed name
	if (args.reset): # hard reset
		Debug('Resetting ')
		cmd = b'\x06\x04\x04\x00\xff\xff\x00\x00'
		SendCommand(serport,cmd)
		time.sleep(5)

	Debug('Configuring receiver')
	
	# Navigation/measurement rate settings
	ubxMsgs.add(b'\x06\x08')
	#$msg = "\x06\x08\x06\x00\xe8\x03\x01\x00\x01\x00"; # CFG_RATE
	#SendCommand($msg);
	
	# Configure various messages for 1 Hz output
	
	# RXM-RAWX raw data message
	ubxMsgs.add(b'\x06\x01')
	ubxMsgs.add(b'\x02\x15')
	#$msg="\x06\x01\x03\x00\x02\x15\x01"; #CFG-MSG 0x02 0x15
	#SendCommand($msg);
	
	# TIM-TP time pulse message (contains sawtooth error)
	ubxMsgs.add(b'\x0d\x01')
	#$msg="\x06\x01\x03\x00\x0d\x01\x01"; #CFG-MSG 0x0d 0x01
	#SendCommand($msg);
	
	# Satellite information
	ubxMsgs.add(b'\x01\x35')
	#$msg="\x06\x01\x03\x00\x01\x35\x01"; #CFG-MSG 0x01 0x35
	#SendCommand($msg); 
	
	# NAV-TIMEUTC UTC time solution 
	ubxMsgs.add(b'\x01\x21')
	#$msg="\x06\x01\x03\x00\x01\x21\x01"; #CFG-MSG 0x01 0x21
	#SendCommand($msg); 
	
	# NAV-CLOCK clock solution (contains clock bias)
	ubxMsgs.add(b'\x01\x22')
	#$msg="\x06\x01\x03\x00\x01\x22\x01"; #CFG-MSG 0x01 0x22
	#SendCommand($msg); 
	
	PollVersionInfo(serport);
	PollChipID(serport);
	
	ubxMsgs.add(b'\x05\x00') # ACK-NAK 
	ubxMsgs.add(b'\x05\x01') # ACK_ACK
	ubxMsgs.add(b'\x27\x03') # chip ID
	
	Debug('Done configuring')

	
# ---------------------------------------------------------------------------
# Gets receiver/software version
def PollVersionInfo(serport):
	cmd = b'\x0a\x04\x00\x00'
	SendCommand(serport,cmd)

# ---------------------------------------------------------------------------
def PollChipID(serport):
	cmd = b'\x27\x03\x00\x00'
	SendCommand(serport,cmd)

# ------------------------------------------
# Main 
# ------------------------------------------

#print(binascii.hexlify(Checksum(b"\x06\x01\x03\x00\x01\x35\x01\x06\x01\x03\x00\x01\x35\x01\xfa")))
#exit(0)
			
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
	ErrorExit(configFile + ' not found')
	
logPath = os.path.join(home,'logs')
if (not os.path.isdir(logPath)):
	ErrorExit(logPath + "not found")

cfg=Initialise(configFile)

# Check the receiver model is supported
rxModel = cfg['receiver:model'].lower()
if (None == re.search('zed-f9',rxModel)):
	ErrorExit('Receiver model ' + rxModel + ' is not supported')

rxTimeout = 600
if ('receiver:timeout' in cfg):
	rxTimeout = int(cfg['receiver:timeout'])

port = cfg['receiver:port']
dataPath = ottplib.MakeAbsolutePath(cfg['paths:receiver data'], home)
rxStatus = ottplib.MakeAbsolutePath(cfg['receiver:status file'], home)

dataExt = cfg['receiver:file extension']
if (None == re.search(r'\.$',dataExt)): # add a '.' separator if needed
	dataExt = '.' + dataExt 

dataFormat = OPENTTP_FORMAT
if (not 'receiver:file format' in cfg):
	ff = cfg['receiver:file format'].lower()
	if (ff == 'native'):
		dataFormat = NATIVE_FORMAT;
		cfg['receiver:file extension']= '.ubx'
	
# Create the process lock		
lockFile = ottplib.MakeAbsoluteFilePath(cfg['receiver:lock file'],home,home + '/etc')
Debug('Creating lock ' + lockFile)
if (not ottplib.CreateProcessLock(lockFile)):
	ErrorExit("Couldn't create a lock")

# Create UUCP lock for the serial port
uucpLockPath = '/var/lock';
if ('paths:uucp lock' in cfg):
	uucpLockPath = cfg['paths:uucp lock']
Debug('Creating uucp lock in ' + uucpLockPath)
ret = subprocess.check_output(['/usr/local/bin/lockport','-d',uucpLockPath,'-p',str(os.getpid()),port,sys.argv[0]])

if (re.match(rb'1',ret)==None):
	ottplib.RemoveProcessLock(lockFile)
	ErrorExit('Could not obtain a lock on ' + port + '.Exiting.')

signal.signal(signal.SIGINT,SignalHandler)
signal.signal(signal.SIGTERM,SignalHandler)

Debug('Opening ' + port)

serport=None # so that this can flag failure to open the port
try:
	serport = serial.Serial(port,115200,timeout=0.2)
except:
	Cleanup()
	ErrorExit('Failed to open ' + port)

ConfigureReceiver(serport)

tt = time.time()
tStr = time.strftime('%H:%M:%S',time.gmtime(tt))
mjd = ottplib.MJD(tt)
fdata = OpenDataFile(mjd)
tNext=(mjd-40587+1)*86400
tThen = 0

tLastMsg=time.time()
inp = b''

# Since we like using regexes, pre-compile the main ones
ubxre = re.compile(rb'\xb5\x62(..)(..)([\s\S]*)')

while (not killed):
	
	# Check for timeout
	if (time.time() - tLastMsg > rxTimeout):
		msg = time.strftime('%Y-%m-%d %H:%M:%S',time.gmtime()) + ' no response from receiver'
		if (dataFormat == OPENTTP_FORMAT):
			flog.write('# ' + msg + '\n')
		else:
			print ('# ' + msg + '\n')
		break

	# The guts
	select.select([serport],[],[],0.2)
	if (serport.in_waiting == 0):
		continue
	
	newinp = serport.read(serport.in_waiting)
	inp = inp + newinp
	
	# Header structure for UBX packets is 
	# Sync char 1 | Sync char 2| Class (1 byte) | ID (1 byte) | payload length (2 bytes) | payload | cksum_a | cksum_b
	
	matches = ubxre.search(inp)
	if (matches): # UBX header
		inp = inp[matches.start():] # discard the prematch string
		classid = matches.group(1)
		payloadLength, = struct.unpack_from('<H',matches.group(2)); # ushort, little endian FIXME what about ARM?
		data = matches.group(3)
		
		#PREMATCH: match.string[:match.start()]
		#MATCH: match.group()
		#POSTMATCH: match.string[match.end():]

		packetLength = payloadLength + 8;
		inputLength = len(inp);
		if (packetLength <= inputLength): # it's all there ! yay !
			tNow = time.time()	# got one - tag the time
			tLastMsg = tNow # resets the timeout
			
			if (tNow >= tNext):
				# (this way is safer than just incrementing mjd)
				fdata.close()
				mjd=int(tNow/86400) + 40587	
				fdata = OpenDataFile(mjd)
				
				PollVersionInfo()
				PollChipID()
				tNext=(mjd-40587+1)*86400	# seconds at next MJD
			
			if (tNow > tThen):
				tThen = tNow
				tStr = time.strftime('%H:%M:%S',time.gmtime(tNow))
				
			# Parse messages
			if (classid in ubxMsgs):
				pass
			
			# Tidy up the input buffer - remove what we just parsed
			if (packetLength == inputLength):
				inp='' # we ate the lot
			else:
				inp = inp[packetLength:] # still some chewy bits 
			
		else:
			pass # nuffink to do
			
	
# Do what you gotta do
msg = '# {} {} killed\n'.format( \
			time.strftime('%Y-%m-%d %H:%M:%S',time.gmtime()), \
			os.path.basename(sys.argv[0]))
print (msg)
if (dataFormat == OPENTTP_FORMAT):
	fdata.write(msg)

Cleanup()
