#!/usr/bin/python
#

#
# The MIT License (MIT)
#
# Copyright (c) 2018 Michael J. Wouters
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

# To be used with udev to create uniques device names for each attached ublox device
# Sample rule
# SUBSYSTEM=="tty", ATTRS{idVendor}=="1546", ATTRS{idProduct}=="01a8",  RUN+="/usr/local/sbin/ubloxmkdev.py '%E{DEVNAME}'"
#

import argparse
import binascii
import os
import re
import select
import serial
import signal
import struct
import subprocess
import sys
import time
import threading

# Globals
debug = False
killed = False

VERSION = '0.1'
AUTHORS = "Michael Wouters"

NO_MSG=0
BIN_MSG=1
NMEA_MSG=2

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
# Calculate the checksum of a string (simple XOR)
# This needs to be the payload for binary messages
def CheckSum(msg):
	cka=0
	ckb=0
	l=len(msg)
	for i in range(0,l):
		cka += ord(msg[i])
		ckb += cka
		cka &= 0xff
		ckb &= 0xff
	return (cka,ckb)

# ------------------------------------------
# Send a command to the receiver and get the response
def GetResponse(serport,cmd,classID,payloadLength):
	WriteCmd(serport,cmd)
	(msg,msgOK) = ReadResponse(serport,classID,payloadLength)
	return (msg,msgOK)

# ------------------------------------------
# Write a command to the serial port
def WriteCmd(serport,cmd):
	(cka,ckb) = CheckSum(cmd)
	cmd='\xb5\x62'+cmd+chr(cka)+chr(ckb)
	serport.write(cmd)
	Debug('Sent ' + binascii.hexlify(cmd))

# -------------------------------------------
# Read response to command
# This will discard any unwanted messages
# Returns the entire message

def ReadTimeout():
	global waiting
	waiting = False
	Debug('Timeout')
	
	
def ReadResponse(serport,classID,payloadLength):
	global waiting
	waiting = True
	msgbuf = ''
	msg = ''
	l=''

	Debug('Looking for msg with Class:ID {:02x}:{:02x}'.format(ord(classID[0]),ord(classID[1])))
	
	t = threading.Timer(3, ReadTimeout)
	t.start()
	while (waiting):
		try:
			l = ser.read(8) # minimum packet length is 8 bytes (no payload)
		except select.error as (code,msg): # FIXME need to ignore a timeout since this may be OK
			print msg
			break;
		msgbuf += l
		# print binascii.hexlify(b)
		startIndex = msgbuf.find('\xb5\x62')
		if (startIndex >= 0): # possible UBX message
			bytesLeft = len(msgbuf)-startIndex-2
			if (bytesLeft >= 4): # can get the length
				plen = struct.unpack('<H',msgbuf[startIndex+4:startIndex+6])[0]
				if (bytesLeft >= 4+plen+2): # have a full message
					# Is this the one we want ?
					if ((msgbuf[startIndex+2] == classID[0]) and (msgbuf[startIndex+3] == classID[1])):
						(cka,ckb) = CheckSum(msgbuf[startIndex+2:startIndex+6+plen])
						if (cka == ord(msgbuf[startIndex+6+plen]) and ckb == ord(msgbuf[startIndex+7+plen])):
							t.cancel()
							return (msgbuf[startIndex+6:startIndex+6+plen],True)
						else: # bad checksum
							msgbuf = msgbuf[startIndex+plen+8:] # fuggedda bout it
					else: # don't care about this one
						msgbuf = msgbuf[startIndex+plen+8:] # chop off everything up to the beginning of the next message
						
	t.cancel()	
	return ('',False)

# -------------------------------------------	
# Parses the input stream, returning any valid message and the unparsed part of the stream
def ParseInput(inp):
	startIndex = inp.find('\xb5\x62') # finds first possible sync character
	msg=''
	msgType=NO_MSG
	if (startIndex >= 0): # have a possible message start
		print 'Hi'
	return (inp,msg,msgType)
	
# ------------------------------------------

parser = argparse.ArgumentParser(description='Detects a ublox NEO8 device and makes a device node based on the serial number')
parser.add_argument('devname',nargs='?',help='device name',type=str,default='')
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

debug = args.debug
port  = args.devname

if (args.version):
	ShowVersion()
	sys.exit(0)

if port == '':
	parser.print_help()
	sys.exit(1)
	
signal.signal(signal.SIGINT,SignalHandler)
signal.signal(signal.SIGTERM,SignalHandler)

Debug("Opening " + port)

try:
	ser = serial.Serial(port,115200,timeout=2)
except:
	print 'Error on device ' + port
	sys.exit(1)

try:
	portfile = open('/usr/local/etc/ublox.conf','r')
except:
	print 'Unable to open'
	sys.exit(1)

cfg={}
for line in portfile:
	line=line.rstrip('\r\n') # chomp
	lfields = line.split()
	if not (len(lfields)==2):
		continue
	cfg[lfields[0]]=lfields[1];
	
if (len(cfg)==0):
	print 'Empty config file'
	sys.exit(1)

for t in range(1,6): # try five times
	(msg,msgOK) = GetResponse(ser,'\x27\x03\x00\x00','\x27\x03',8)
	if (msgOK):
		Debug('Got ' + binascii.hexlify(msg))
		# The last five bytes are the chip ID
		chipID = binascii.hexlify(msg[4:])
		Debug('Chip ID is ' + chipID)
		if ( chipID in cfg):
			Debug("Making /dev/" + cfg[chipID])
			devname = '/dev/' + cfg[chipID]
			if (os.path.exists(devname)):
				# we have to handle the case where udev has been triggered again after creating a valid device
				# we don't want to remove it
				Debug("Device already there")
				if  (os.readlink(devname) == port):
					Debug("Already symlinked")
				else:
					Debug("Removing stale symlink")
					os.remove(devname)
					os.symlink(port,devname)
			else:
				os.symlink(port,devname)
			ser.close()
			sys.exit(0)
		else:
			Debug("Chip ID is not in ublox.conf")
		# if it's not in the table then keep trying, in case the message was corrupted
sys.exit(1) # failed