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
# 2020-07-07 MJW Extensive modifications to allow USB and UARTs to be used
#                General cleanups
# 2020-07-08 ELM Some minor fixups, version number changed to 0.1.6
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

VERSION = '0.1.8'
AUTHORS = 'Michael Wouters,Louis Marais'

# File formats
OPENTTP_FORMAT=0
NATIVE_FORMAT=1

# Receiver comm interfaces
USB = 0
UART1 = 1
UART2 = 2

BaudRates = [9600,19200,38400,57600,115200,230400,460800,921600]


# Maximum buffer length
# The longest message that we ask for is UBX-RXM-RAWX
# the payload of which is 16+32*numMeas
# numMeas could be as many as 184
# This is about 6K
# So take maximum buffer length to be 30 K
# (this is roughly 10 s of messages 'normally')

MAXBUFLEN = 30000

# Globals
debug = False
killed = False
ubxMsgs = set() # the set of messages we want to log

# This is not perfect
# Reboot behaviour seems to vary. You may not get SIGTERM. YMMV
#
# ------------------------------------------
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
	fout.flush()
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

# ------------------------------------------------------------------------------
# Verify the checksum
# Note that a full message, including the UBX header, is assumed
#
def CheckSumOK(msg):
	cka=0
	ckb=0
	l = len(msg)
	for i in range(2,l-2): # skip the two bytes of the header (start) and the checksum (end) 
		cka = cka + msg[i]
		cka = cka & 0xff
		ckb = ckb + cka
		ckb = ckb & 0xff
	
	return ((cka == msg[l-2]) and (ckb == msg[l-1]))

	
# ---------------------------------------------------------------------------
def SendCommand(serport,cmd):
	cksum = Checksum(cmd)
	msg = b'\xb5\x62' + cmd + cksum
	serport.write(msg)

#----------------------------------------------------------------------------
def ConfigureReceiver(serport):

	# Note that UBX is LITTLE-ENDIAN
	# Haven't thought about ARM yet
	
	# Note that reset causes a USB disconnect
	# so it's a good idea to use udev to map the device to a 
	# fixed name
	if (args.reset): # hard reset
		Debug('Resetting ')
		cmd = b'\x06\x04\x04\x00\xff\xff\x00\x00'
		SendCommand(serport,cmd)
		time.sleep(5)

	Debug('Configuring receiver')
	
	# Note that all configuration is only done in RAM
	# It is not saved
	#                 Class + ID      Length      Version+Layers+Reserved
	UBX_CFG_VAL_SET = b'\x06\x8a' + b'\x09\x00' + b'\x00\x01\x00\x00' # NOTE: 9 byte payload only
	UBX_ON = b'\x01'
	UBX_OFF = b'\x00'
	
	# Disable NMEA output on interfaces
	
	msg=  UBX_CFG_VAL_SET + b'\x02\x00\x74\x10' + UBX_OFF # CFG-UART1OUTPROT-NMEA 0x10740002
	SendCommand(serport,msg)
	
	msg=  UBX_CFG_VAL_SET + b'\x02\x00\x76\x10' + UBX_OFF  # CFG-UART2OUTPROT-NMEA 0x10760002
	SendCommand(serport,msg)
	
	msg=  UBX_CFG_VAL_SET + b'\x02\x00\x78\x10' + UBX_OFF # CFG-USBOUTPROT-NMEA 0x10780002
	SendCommand(serport,msg)
	
	# GNSS tracking configuration 
	# First, query the current configuration
	# If the configuration is not being changed, then don't reconfigure
	
	# Set which GNSS are enabled
	# All GNSS are enabled by default
	
	BDS_ENA  = b'\x22\x00\x31\x10' # CFG-SIGNAL-BDS_ENA  0x10310022
	GAL_ENA  = b'\x21\x00\x31\x10' # CFG-SIGNAL-GAL_ENA  0x10310021
	GLO_ENA  = b'\x25\x00\x31\x10' # CFG-SIGNAL-GLO_ENA  0x10310025
	GPS_ENA  = b'\x1f\x00\x31\x10' # CFG-SIGNAL-GPS_ENA  0x1031001f
	QZSS_ENA = b'\x24\x00\x31\x10' # CFG-SIGNAL-QZSS_ENA 0x10310024
	
	enabled = [BDS_ENA, GAL_ENA, GLO_ENA , GPS_ENA, QZSS_ENA]
	disabled = []
	
	if ('receiver:observations' in cfg):
		obsv = cfg['receiver:observations'].lower()
	
		if ('all' == obsv):
			pass
		else:
			enabled = []
			
			if ('beidou' in obsv):
				enabled.append(BDS_ENA)  
			else:
				disabled.append(BDS_ENA)
				
			if ('galileo' in obsv):
				enabled.append(GAL_ENA) 
			else:
				disabled.append(GAL_ENA)
				
			if ('glonass' in obsv): 
				enabled.append(GLO_ENA) 
			else:
				disabled.append(GLO_ENA)
			
			# The docs recommend that GPS+QZSS be both on or both off to
			# avoid cross-correlation issues. QZSS is therefore implicitly
			# controlled by whether or not GPS is enabled
			if ('gps' in obsv):
				enabled.append(GPS_ENA) 
				enabled.append(QZSS_ENA)
			else:
				disabled.append(GPS_ENA)
				disabled.append(QZSS_ENA)
	
	if not enabled:
		Cleanup();
		ErrorExit('No valid GNSS have beeen configured in receiver:observations')
	
	# Set which signals are tracked for each constellation
	# The default is dual frequency for all constellations
	
	GPS_L1CA_ENA = b'\x01\x00\x31\x10' # CFG-SIGNAL-GPS_L1CA_ENA 0x10310001
	GPS_L2C_ENA  = b'\x03\x00\x31\x10' # CFG-SIGNAL-GPS_L2C_ENA  0x10310003
	
	GAL_E1_ENA   = b'\x07\x00\x31\x10' # CFG-SIGNAL-GAL_E1_ENA  0x10310007
	GAL_E5B_ENA  = b'\x0a\x00\x31\x10' # CFG-SIGNAL-GAL_E5B_ENA 0x1031000a
	
	BDS_B1_ENA   = b'\x0d\x00\x31\x10' # CFG-SIGNAL-BDS_B1_ENA 0x1031000d
	BDS_B2_ENA   = b'\x0e\x00\x31\x10' # CFG-SIGNAL-BDS_B2_ENA 0x1031000e
	
	GLO_L1_ENA   = b'\x18\x00\x31\x10' # CFG-SIGNAL-GLO_L1_ENA 0x10310018
	GLO_L2_ENA   = b'\x1a\x00\x31\x10' # CFG-SIGNAL-GLO_L2_ENA 0x1031001a
	
	QZSS_L1CA_ENA = b'\x12\x00\x31\x10' # CFG-SIGNAL-QZSS_L1CA_ENA 0x10310012
	QZSS_L2_ENA   = b'\x15\x00\x31\x10' # CFG-SIGNAL-QZSS_L2C_ENA  0x10310015
	
	enabledSigs = [GPS_L1CA_ENA, GPS_L2C_ENA, GAL_E1_ENA, GAL_E5B_ENA,
								BDS_B1_ENA, BDS_B2_ENA, GLO_L1_ENA, GLO_L2_ENA, QZSS_L1CA_ENA, QZSS_L2_ENA]
	disabledSigs = []
	
	for gnss in enabled:
		msg = UBX_CFG_VAL_SET + gnss +  UBX_ON
		SendCommand(serport,msg)
	
	for gnss in disabled:
		msg = UBX_CFG_VAL_SET + gnss +  UBX_OFF
		SendCommand(serport,msg)
	
	if ('receiver:beidou' in cfg):
		sig = cfg['receiver:beidou'].lower()
		if (sig == 'single'):
			enabledSigs.remove(BDS_B2_ENA )
			disabledSigs.append(BDS_B2_ENA)
	
	if ('receiver:galileo' in cfg):
		sig = cfg['receiver:galileo'].lower()
		if (sig == 'single'):
			enabledSigs.remove(GAL_E5B_ENA)
			disabledSigs.append(GAL_E5B_ENA)
	
	if ('receiver:glonass' in cfg):
		sig = cfg['receiver:glonass'].lower()
		if (sig == 'single'):
			enabledSigs.remove(GPS_L1CA_ENA)
			disabledSigs.append(GPS_L2C_ENA)
			
	if ('receiver:gps' in cfg):
		sig = cfg['receiver:gps'].lower()
		if (sig == 'single'):
			enabledSigs.remove(GPS_L2C_ENA)
			disabledSigs.append(GPS_L2C_ENA)
	
	# FIXME temporary hack to enable BeiDou dual frequency
	#    CLASS + ID    BYTE COUNT    VERSION + LAYER + reserved  payload 
	msg = UBX_CFG_VAL_SET + BDS_B2_ENA  +  UBX_ON
	SendCommand(serport,msg)
	
	# The time scale for the output 1 pps needs to be set to GPS
	# UTC=0, GPS=1, GLO=2, BDS=3,GAL=4
	CFG_TP_TIMEGRID_TP1 = b'\x0c\x00\x05\x20'; # CFG-TP-TIMEGRID_TP1 0x2005000c
	msg = UBX_CFG_VAL_SET + CFG_TP_TIMEGRID_TP1 + b'\x01';
	SendCommand(serport,msg)
	
	# And the other one, just in case
	CFG_TP_TIMEGRID_TP2 = b'\x17\x00\x05\x20'; # CFG-TP-TIMEGRID_TP1 0x20050017
	msg = UBX_CFG_VAL_SET + CFG_TP_TIMEGRID_TP2 + b'\x01';
	SendCommand(serport,msg)
	
	# Navigation/measurement rate settings
	ubxMsgs.add(b'\x06\x08')
	#$msg = "\x06\x08\x06\x00\xe8\x03\x01\x00\x01\x00"; # CFG_RATE
	#SendCommand($msg);
	
	# Configure various messages for 1 Hz output
	# Note that the configuration database token has its bytes reversed
	# (wrt what's written in the docs)
	
	# The Key ID for UART1 sets the base key id
	# UART2 = UART1 + 1
	# USB   = UART1 + 2
	
	commOffset = 0
	if (commInterface == UART2):
		commOffset = 1
	elif (commInterface == USB):
		commOffset = 2
	
	# CFG-MSGOUT-UBX_RXM_RAWX_ 0x209102a5+x raw data message
	ubxMsgs.add(b'\x02\x15')
	cmd = struct.pack('B',0xa5 + commOffset) 
	msg = UBX_CFG_VAL_SET + cmd + b'\x02\x91\x20' + UBX_ON 
	SendCommand(serport,msg);
	
	# CFG-MSGOUT-UBX_TIM_TP 0x2091017e+x time pulse message (contains sawtooth error)
	ubxMsgs.add(b'\x0d\x01')
	cmd = struct.pack('B',0x7e + commOffset)
	msg = UBX_CFG_VAL_SET + cmd + b'\x01\x91\x20' + UBX_ON 
	SendCommand(serport,msg)
	
	# CFG-MSGOUT-UBX_NAV_SAT 0x20910016+x satellite information
	ubxMsgs.add(b'\x01\x35')
	cmd = struct.pack('B',0x16 + commOffset)
	msg = UBX_CFG_VAL_SET + cmd + b'\x00\x91\x20' + UBX_ON
	SendCommand(serport,msg)
	
	# CFG-MSGOUT-UBX_NAV_TIMEUTC 0x2091005c+x UTC time solution 
	ubxMsgs.add(b'\x01\x21')
	cmd = struct.pack('B',0x5c + commOffset)
	msg = UBX_CFG_VAL_SET + cmd + b'\x00\x91\x20' + UBX_ON 
	SendCommand(serport,msg)
	
	# CFG-MSGOUT-UBX_NAV_CLOCK 0x20910066+x clock solution (contains clock bias)
	ubxMsgs.add(b'\x01\x22')
	cmd = struct.pack('B',0x66 + commOffset)
	msg = UBX_CFG_VAL_SET  + cmd + b'\x00\x91\x20' + UBX_ON 
	SendCommand(serport,msg)
	
	# CFG-MSGOUT-UBX_RXM_SFBRX 0x20910232+x broadcast navigation data subframe
	ubxMsgs.add(b'\x02\x13')
	cmd = struct.pack('B',0x32 + commOffset)
	msg = UBX_CFG_VAL_SET  + cmd + b'\x02\x91\x20' + UBX_ON 
	SendCommand(serport,msg)
	
	ubxMsgs.add(b'\x05\x00') # ACK-NAK 
	ubxMsgs.add(b'\x05\x01') # ACK_ACK
	ubxMsgs.add(b'\x27\x03') # chip ID
	ubxMsgs.add(b'\x0a\x04') # version info
	
	PollVersionInfo(serport)
	PollChipID(serport)
	
	Debug('Done configuring')
	PollVersionInfo(serport)
	
# ---------------------------------------------------------------------------
# Gets receiver/software version
def PollVersionInfo(serport):
	cmd = b'\x0a\x04\x00\x00'
	SendCommand(serport,cmd)

# ---------------------------------------------------------------------------
def PollChipID(serport):
	cmd = b'\x27\x03\x00\x00'
	SendCommand(serport,cmd)

# ---------------------------------------------------------------------------
def UpdateStatus(rxStatus,msg):
	
	try:
		fstat = open(rxStatus,'w')
	except:
		return
	
	numSVs=int((len(msg)-8-2)/12)
	bds = ''
	gal = ''
	glo = ''
	gps = ''
	qzss = ''
	
	ngood = 0
	
	for i in range(0,numSVs):
		#gnssID,svID,cno,elev,azim=unpack("CCCcssI",substr $data,8+12*$i,12);
		gnssID,svID,cno,elev,azim,prRes,flags =struct.unpack_from('BBBbhhI',msg,8+12*i)
		#print(gnssID,svID,cno,elev,azim,flags & 0x07)
		flags = flags & 0x07
		if (flags < 0x04): # code and time synchronized at least ?
			continue
		ngood = ngood + 1
		if   (0 == gnssID ):
			gps = gps + str(svID) + ','
		elif (2 == gnssID):
			gal = gal + str(svID) + ',' 
		elif (3 == gnssID):
			bds = bds + str(svID) + ','
		elif (5 == gnssID):
			qzss = qzss + str(svID) + ','
		elif (6 == gnssID):
			glo = glo + str(svID) + ','
			
	if (len(gps) > 0):
		gps = gps[:-1]
	if (len(gal) > 0):
		gal = gal[:-1]
	if (len(bds) > 0):
		bds = bds[:-1]
	if (len(qzss) > 0):
		qzss = qzss[:-1]
	if (len(glo) > 0):
		glo = glo[:-1]

	fstat.write('nsats = ' + str(ngood) + '\n')
	fstat.write('BDS = ' + bds + '\n')
	fstat.write('GAL = ' + gal + '\n')
	fstat.write('GLO = ' + glo + '\n')
	fstat.write('GPS = ' + gps + '\n')
	fstat.write('QZSS = ' + qzss + '\n')
	fstat.close()
	
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
	ErrorExit(configFile + ' not found')
	
logPath = os.path.join(home,'logs')
if (not os.path.isdir(logPath)):
	ErrorExit(logPath + "not found")

cfg=Initialise(configFile)

# Check the receiver model is supported
rxModel = cfg['receiver:model'].lower()

if ('zed-f9p' == rxModel):
	commInterface= USB # Sparkfun board
elif ('zed-f9t' == rxModel):
	commInterface = UART1 # ublox board
else:
	ErrorExit('Receiver model ' + rxModel + ' is not supported')

# Receiver comms interface now set to defaults 
if ('receiver:communication interface' in cfg):
	newCommInterface = cfg['receiver:communication interface'].lower()
	if ('usb' == newCommInterface):
		commInterface = USB
	elif ('uart1'== newCommInterface):
		commInterface = UART1
	elif (re.search('uart2',newCommInterface)):
		commInterface = UART2
	else:
		ErrorExit('Invalid communication interface: ' + newCommInterface)
		
portSpeed = 460800
if ('zed-f9t' == rxModel):
	portSpeed = 115200
if ('receiver:baud rate' in cfg):
	newSpeed = cfg['receiver:baud rate']
	try:
		portSpeed = int(newSpeed)
	except:
		ErrorExit('Invalid baud rate:' + newSpeed)
	if (not portSpeed in BaudRates):
		ErrorExit('Invalid baud rate')
		
rxTimeout = 600
if ('receiver:timeout' in cfg):
	rxTimeout = int(cfg['receiver:timeout'])

port = cfg['receiver:port']

dataPath = ottplib.MakeAbsolutePath(cfg['paths:receiver data'], home)
rxStatus = ottplib.MakeAbsoluteFilePath(cfg['receiver:status file'], home,home + '/log')
statusUpdateInterval = 30
 
dataExt = cfg['receiver:file extension']
if (None == re.search(r'\.$',dataExt)): # add a '.' separator if needed
	dataExt = '.' + dataExt 

dataFormat = OPENTTP_FORMAT
if ('receiver:file format' in cfg):
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
signal.signal(signal.SIGHUP,SignalHandler) # not usually run with a controlling TTY, but handle it anyway

Debug('Opening ' + port)

serport=None # so that this can flag failure to open the port
try:
	serport = serial.Serial(port,portSpeed,timeout=0.2)
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
tLastStatusUpdate=0

tLastMsg=time.time()
inp = b''

# Since we like using regexes, pre-compile the main ones
ubxre = re.compile(rb'\xb5\x62(..)(..)([\s\S]*)')

while (not killed):
	
	# Check for timeout
	if (time.time() - tLastMsg > rxTimeout):
		msg = time.strftime('%Y-%m-%d %H:%M:%S',time.gmtime()) + ' no response from receiver'
		if (dataFormat == OPENTTP_FORMAT):
			#flog.write('# ' + msg + '\n') # flog not defined, changed to fdata
			fdata.write('# ' + msg + '\n')
		else:
			print ('# ' + msg + '\n')
		break
		killed = True
    
	# The guts
	select.select([serport],[],[],0.2)
	if (serport.in_waiting == 0):
		# Debug('Timeout')
		continue
	
	newinp = serport.read(serport.in_waiting)
	inp = inp + newinp
	
	# A reasonable limit on the size of the buffered data needs to be set
	# Without this, the situation has been observed where the script was using a 
	# large amount of memory 
	
	if len(inp) > MAXBUFLEN:
		print ('Buffer too big',len(inp),'bytes') # FIXME remove this one day
		inp=b'' # empty the buffer AFTER reporting its size
		continue
	
	# Header structure for UBX packets is 
	# Sync char 1 | Sync char 2| Class (1 byte) | ID (1 byte) | payload length (2 bytes) | payload | cksum_a | cksum_b
	
	matches = ubxre.search(inp)
	while (matches): # UBX header
		
		inp = inp[matches.start():] # discard the prematch string
		classid = matches.group(1)
		payloadLength, = struct.unpack_from('<H',matches.group(2)); # ushort, little endian FIXME what about ARM?
		data = matches.group(3)
		
		#PREMATCH: match.string[:match.start()]
		#MATCH: match.group()
		#POSTMATCH: match.string[match.end():]

		packetLength = payloadLength + 8
		inputLength = len(inp)
		if (packetLength <= inputLength): # it's all there ! yay !
			tNow = time.time()	# got one - tag the time
			tLastMsg = tNow # resets the timeout
			
			if (tNow >= tNext):
				# (this way is safer than just incrementing mjd)
				fdata.close()
				mjd=int(tNow/86400) + 40587	
				fdata = OpenDataFile(mjd)
				
				PollVersionInfo(serport)
				PollChipID(serport)
				tNext=(mjd-40587+1)*86400	# seconds at next MJD
			
			if (tNow > tThen):
				tThen = tNow
				tStr = time.strftime('%H:%M:%S',time.gmtime(tNow))
				
			# Parse messges
			if (classid in ubxMsgs):
				#print(binascii.hexlify(classid),packetLength,inputLength)
				
				ubxClass,ubxID=struct.unpack('2B',classid)
				
				if (not (ubxClass == 0x01 and ubxID == 0x35)): # log it ?
					if (dataFormat  == OPENTTP_FORMAT):
						fdata.write('{:02x}{:02x} {} {}\n'.format(ubxClass,ubxID,tStr,str(binascii.hexlify(data[:payloadLength+2]))[2:-1]))
						fdata.flush()
						
				if (ubxClass == 0x01 and ubxID == 0x35 and tNow - tLastStatusUpdate >= statusUpdateInterval):
					UpdateStatus(rxStatus,data[:payloadLength+2])
					tLastStatusUpdate = tNow
			else:
				Debug('Unhandled')
			# Tidy up the input buffer - remove what we just parsed
			if (packetLength == inputLength):
				inp=b'' # we ate the lot
			else:
				inp = inp[packetLength:] # still some chewy bits 
			matches = ubxre.search(inp) # have another go
		else:
			break # need a bit more 
			
	
# Do what you gotta do
msg = '# {} {} timeout/killed\n'.format( \
			time.strftime('%Y-%m-%d %H:%M:%S',time.gmtime()), \
			os.path.basename(sys.argv[0]))

if (dataFormat == OPENTTP_FORMAT):
	fdata.write(msg)

Cleanup()

# This is here because if the process gets SIGHUP
# and STDOUT is not piped to a file, execution will stop here 
print (msg)
