#!/usr/bin/python3

#
# The MIT License (MIT)
#
# Copyright (c) 2021 Michael J. Wouters, Louis Marais
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
# plrxlog.py - a logging script for the ublox series 9 GNSS receiver
# For configuration and logging of a Septentrio PolaRx or Mosaic-T receiver
# 

# Extra configuration options
# receiver:broadcast status        [enable UDP broadacst of SV data for eg gnssview]
# receiver:communication interface [USB1,COM1,...]
# gnssview:address                 [UDP broadcast address]
# gnssview:port                    [UDP broadcast port]

import argparse
import binascii
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
import time

import ottplib

VERSION = '0.0.1'
AUTHORS = 'Michael Wouters,Louis Marais'

# Globals
debug = False
killed = False

MAXBUFLEN = 30000

STATUS_LOGGING_INTERVAL=5

BEIDOU=0
GPS=1
GALILEO=2
GLONASS=3
QZSS=4
SBAS=5 # note that this is the maximum value accepted by gnssview (2021-07-17)

MAXSVID=245  # maximum SVID in SBF as at 2021-07-01

MCAST_GRP = '226.1.1.37'
MCAST_PORT = 14544
MULTICAST_TTL = 2

#-----------------------------------------------------------------------------
# This is adapted from crc.c as used in sbf2asc
CRC_SBF_LOOKUP = [
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
        ]

def CRC_CCITT(data):
	crc = 0x0000
	for byte in data:
		crc = ((crc<<8) & 0xff00) ^ CRC_SBF_LOOKUP[((crc>>8) & 0xff)^byte]
	return (crc & 0xffff)


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
def Debug(msg):
	if (debug):
		print (time.strftime('%H:%M:%S ',time.gmtime()) + msg)
	return

#-----------------------------------------------------------------------------
def ErrorExit(msg):
	print (msg)
	sys.exit(0)
	
#-----------------------------------------------------------------------------
def Initialise(configFile):
	cfg=ottplib.LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit("Error loading " + configFile)
		
	# Check for required arguments
	reqd = ['paths:receiver data','receiver:port','receiver:file extension', \
		'receiver:lock file','receiver:model', \
		'receiver:configuration']
	
	for k in reqd:
		if (not k in cfg):
			ErrorExit('The required configuration entry "' + k + '" is undefined')
	
	return cfg

#-----------------------------------------------------------------------------
def Cleanup():
	# Hmm ugly globals
	ottplib.RemoveProcessLock(lockFile)
	if (not serport==None):
		SendCommand('SetSBFoutput,Stream1,' + commInterface + ',none') # turn off output
		serport.close()
		subprocess.check_output(['/usr/local/bin/lockport','-r',port])

#-----------------------------------------------------------------------------
def OpenDataFile(mjd):

	fname = dataPath + str(mjd) + dataExt;
	appending = os.path.isfile(fname)
	
	Debug('Opening ' + fname);
	
	try:
		fout = open(fname,'ab')
	except:
		Cleanup()
		ErrorExit('Failed to open data file ' + fname)
		
	fout.flush()
	return fout

#----------------------------------------------------------------------------
def SendCommand(cmd):
	
	Debug('SendCommand: ' + cmd)
	bcmd = cmd.encode('utf-8') + b'\r'
	serport.write(bcmd)
	
	# Replies with $R: <command> for set,get,exe
	#              $R; <command> for lst
	#              STOP> for halts/resets
	#              $R? <msg> for errors
	
	ansre = re.compile(rb'(STOP>|\$R[:;\?])')
	inp=b''
	ntries=0
	while ntries < 20: # ie try for 2s, given timeout in the next line
		select.select([serport],[],[],0.1)
		ntries += 1
		if (serport.in_waiting == 0):
			continue
		else:
			newinp = serport.read(serport.in_waiting)
			inp = inp + newinp
			match = ansre.search(inp)
			if match:
				if (match[1] == b'$R?'):
					Debug('Error in command')
					# FIXME this should be fatal
				return True # otherwise, OK
	return False

#----------------------------------------------------------------------------
def ConfigureReceiver(rxcfg):
	Debug('Configuring receiver')
	if (args.reset):
		Debug('Resetting: Hard,PVTData+SatData')
		SendCommand('exeResetReceiver,Hard,PVTData+SatData')
		# A hard reset closes the serial port 
		# For the moment, just reset and then exit
		# NB mosaicT takes about 12 s to reboot
		Cleanup()
		sys.exit(0)
	
	# FIXME This assumes that there is only Stream1 is enabled
	SendCommand('SetSBFoutput,Stream1,' + commInterface + ',none') # turn off output

	# Set up the Antenna
	# This fixes up a bug in RINEX output in version IDK of sbf2rin
	deltaH,deltaE,deltaN = '0.0','0.0','0.0'
	antType, antSerialNum = 'Unknown','Unknown'
	
	if 'antenna:delta h' in cfg:
		deltaH = cfg['antenna:delta h']
	if 'antenna:delta e' in cfg:
		deltaE = cfg['antenna:delta e']
	if 'antenna:delta n' in cfg:
		deltaN = cfg['antenna:delta n']
	if 'antenna:type' in cfg:
		antType= cfg['antenna:type']
	if 'antenna:serial number' in cfg:
		antSerialNum = cfg['antenna:serial number']
	# TBC when the antenna type is set, the receiver compensates for PCV. This may add noise to TT.
	cmd = 'SetAntennaOffset,Main,' + deltaE + ',' + deltaN + ',' + deltaH + ','  + antType + ',' + antSerialNum + ',0'
	SendCommand(cmd)
         
	comment_re = re.compile(r'^\s*#')
	fin = open(rxcfg,'r') # already checked its OK
	for l in fin:
		l=l.strip()
		if not l: # skip empty lines
			continue
		if (comment_re.search(l)): # skip comments
			continue
		SendCommand(l)
 
	SendCommand('SetSBFoutput,Stream1,' + commInterface + ',Rinex+SatVisibility,sec1') # default setup
 
	fin.close()


#---------------------------------------------------------------------------
def SVIDtoGNSSParams(svid):
	
	reqsig=-1
	constellation=-1
	prn=-1
	# This translates Septentrio SVIDs into GNSS and PRNs, as well as defining the signal
	# that we want to find CN0 for
	if (svid <= 37): # GPS
		reqsig=0   # L1CA
		constellation=GPS
		prn=svid
	elif (svid >=38 and svid <=62): # GLONASS
		reqsig=8 # L1CA
		constellation=GLONASS
		prn=svid-37
	elif (svid >=71 and svid <=106): # Galileo
		reqsig=17 # E1
		constellation=GALILEO
		prn=svid-70
	elif (svid >=120 and svid <=140): # SBAS
		reqsig=24 # L1CA
		constellation=SBAS
		prn=svid-100
	elif (svid >= 141 and svid <= 180): # Beidou
		reqsig=28 # B1I
		constellation=BEIDOU
		prn=svid-140
	elif (svid >= 181 and svid <=187): # QZSS
		reqsig=6 #L1CA
		constellation=QZSS
		prn=svid-180
	elif (svid >= 198 and svid <=215): # SBAS
		reqsig=24
		onstellation=SBAS
		prn=svid-57
	elif (svid >= 223 and svid <= 245): # Beidou
		reqsig=28 #B1I
		constellation=BEIDOU
		prn=svid-182
	return [constellation,prn,reqsig]

# When decoding SBF blocks, we will use the same variable names as in the documentation

#----------------------------------------------------------------------------
# SatVisibility Block 4012
# This is used for status reporting and broadcasting
def ParseSatVisibility(d,dlen):
	#nGPS,nGLONASS,nBeidou,nGalileo,nSBAS = 0,0,0,0,0
	TOW,WNc,N,SBLength = struct.unpack_from('IHBB',d) # 8 bytes - this is the offset for unpacking Sat Info blocks
	for m in range(0,N):
		blkStart = 8 + m *SBLength
		SVID,FreqNr,Azimuth,Elevation = struct.unpack_from('BBHh',d[blkStart:blkStart + SBLength])
		#Debug(str(SVID) + '  ' + str(Azimuth/100) + ' ' + str(Elevation/100))
		if (Azimuth == 65535 or Elevation == -32768): # do not use!
			continue
		Azimuth = Azimuth/100.0
		Elevation = Elevation/100.0
		
		constellation,prn,reqsig = SVIDtoGNSSParams(SVID)
		if (constellation==-1):
			continue
			continue
		if (SVID > MAXSVID): 
			continue
		
		if (GNSS[SVID][0]>=0):
			cn = GNSS[SVID][4] # save this since we won't assume the message order
			GNSS[SVID]=[constellation,prn,Azimuth,Elevation,cn]
		else:
			GNSS[SVID]=[constellation,prn,Azimuth,Elevation,-1]

#----------------------------------------------------------------------------
# MeasEpoch Block 4027
# This block defines what satellites we are actually tracking
#

def ParseMeasEpoch(d):
	
	TOW, WNc, N1, SB1Length, SB2Length = struct.unpack_from('IHBBB',d)
	n2Cnt=0
	Debug('TOW='+str(TOW))
	for n in range(0,N1):
		blk1Start = 12 + n*SB1Length + n2Cnt*SB2Length
		blk1=struct.unpack_from('4BIiHbBHBB',d[blk1Start:blk1Start+SB1Length])
		N2 = blk1[11]		
		sigType = blk1[1] & 31 # Bits 0-5
		if (sigType==31): # don't actually care about these anyway
			sigType = (blk1[10]  >> 3) & 0xff  + 32 
			
		SVID = blk1[2]
		CN = blk1[8]
		blk2Start = blk1Start + SB1Length
		constellation,prn,reqsig = SVIDtoGNSSParams(SVID)
		
		if (constellation == SBAS):
			reqsig = sigType  # KLUDGE ? for earlier firmware ?
			
		if (constellation==-1):
			n2Cnt += N2
			continue
		
		if (sigType != reqsig): # Search the sub blocks for the desired signal 
			CN = 255
			for m in range(0,N2):
				subBlkStart = blk2Start + m * SB2Length
				blk2=struct.unpack_from('4BbB',d[subBlkStart:subBlkStart + SB2Length])
				#print(blk2)
				sig2Type = blk2[0] & 31
				if (sig2Type==31): # don't actually care about these anyway
					sigType = (blk1[5]  >> 3) & 0xff  + 32 
				
				if (sig2Type == reqsig):
					CN = blk2[2]
					break
		else:
			pass
		
		if (CN >=0 and CN < 255): # CN is used to flag that we're tracking at some level
			GNSS[SVID][0] = constellation
			GNSS[SVID][1] = prn
			GNSS[SVID][4] = int(CN/4) + 10 # NB different scaling for sigType = 1,2 (L1P,L2P) - but we're not tracking
			
		n2Cnt += N2

		
# ---------------------------------------------------------------------------
def UpdateStatusFile(rxStatus):
	try:
		fstat = open(rxStatus,'w')
	except:
		return

	bds = ''
	gal = ''
	glo = ''
	gps = ''
	qzss = ''
	sbas=''
	
	nSats = 0
	for sv in GNSS:
		if (sv[0] >= 0 and sv[4] > 0):
			nSats += 1
			constellation = sv[0]
			prn = str(sv[1])
			if (constellation == BEIDOU):
				bds += prn + ','
			elif (constellation == GALILEO):
				gal += prn + ','
			elif (constellation == GLONASS):
				glo += prn + ','
			elif (constellation == GPS):
				gps += prn + ','
			elif (constellation == SBAS):
				sbas += prn + ','
			elif (constellation == QZSS):
				qzss += prn + ','
			
	# Strip trailing comma
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
	if (len(sbas) > 0):
		sbas = sbas[:-1]
		
	Debug('nsats = ' + str(nSats))
	Debug('BDS = ' + bds)
	Debug('GAL = ' + gal )
	Debug('GLO = ' + glo )
	Debug('GPS = ' + gps )
	Debug('QZSS = ' + qzss )
	Debug('SBAS = ' + sbas )
	
	fstat.write('nsats = ' + str(nSats) + '\n')
	fstat.write('BDS = ' + bds + '\n')
	fstat.write('GAL = ' + gal + '\n')
	fstat.write('GLO = ' + glo + '\n')
	fstat.write('GPS = ' + gps + '\n')
	fstat.write('QZSS = ' + qzss + '\n')
	fstat.write('SBAS = ' + sbas + '\n')
	fstat.close()

# ---------------------------------------------------------------------------
# Multicast satellite information for gnssview
#
def BroadcastStatus():
	msg = b''
	for sv in GNSS:
		if (sv[0] >= 0 and sv[4] > 0): # constellation set and C/N > 0
			svstat = '{:d},{:d},{:d},{:g},{:g},{:g}\n'.format(int(time.time()),sv[0],sv[1],sv[2],sv[3],sv[4])
			msg = msg + svstat.encode('utf-8')			
	sock.sendto(msg, (mcastGroup, mcastPort))

	
# ----------------------------------------------------------------------------
def ClearGNSS():
	for sv in GNSS:
		sv[0],sv[1],sv[2],sv[3],sv[4] = -1,-1,-1,-1,-1
		
#-----------------------------------------------------------------------------
# Main 
#-----------------------------------------------------------------------------

home =os.environ['HOME'] + os.sep
configFile = os.path.join(home,'etc','gpscv.conf')

parser = argparse.ArgumentParser(description='Log a Septentrio receiver',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--reset','-r',help='reset receiver and exit',action='store_true')
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

rxTimeout = 600
if ('receiver:timeout' in cfg):
	rxTimeout = int(cfg['receiver:timeout'])

port = cfg['receiver:port']

dataPath = ottplib.MakeAbsolutePath(cfg['paths:receiver data'], home)

logStatus = False
if ('receiver:status file' in cfg):
	if (cfg['receiver:status file'].lower() == 'none'):
		logStatus=False
	else:
		rxStatus = ottplib.MakeAbsoluteFilePath(cfg['receiver:status file'], home,home + '/log')
		logStatus=True

dataExt = cfg['receiver:file extension']
if (None == re.search(r'\.$',dataExt)): # add a '.' separator if needed
	dataExt = '.' + dataExt 
	
# Check that the receiver configuration file exists
rxCfg = 	ottplib.MakeAbsoluteFilePath(cfg['receiver:configuration'], home, home + '/etc/')
if (not os.path.isfile(rxCfg)):
		ErrorExit(rxCfg + ' not found')
		
# Check the receiver model is supported
rxModel = cfg['receiver:model'].lower()

if ('mosaict' == rxModel):
	pass
else:
	ErrorExit('Receiver model ' + rxModel + ' is not supported')

commInterface = 'USB1'
if ('receiver:communication interface' in cfg):
	commInterface = cfg['receiver:communication interface'].upper()
	
# Create the process lock		
lockFile = ottplib.MakeAbsoluteFilePath(cfg['receiver:lock file'],home,home + '/etc')
Debug('Creating lock ' + lockFile)
if (not ottplib.CreateProcessLock(lockFile)):
	ErrorExit("Couldn't create a lock")

signal.signal(signal.SIGINT,SignalHandler) 
signal.signal(signal.SIGTERM,SignalHandler) 
signal.signal(signal.SIGHUP,SignalHandler) # not usually run with a controlling TTY, but handle it anyway

# If broadcasting status
broadcast = False
if ('receiver:broadcast status' in cfg):
	broadcast = (cfg['receiver:broadcast status'].lower() == 'true' or cfg['receiver:broadcast status'].lower() == 'yes')
		
if broadcast:
	mcastGroup = MCAST_GRP
	if ('gnssview:address' in cfg):
		mcastGroup = cfg['gnssview:address']
	mcastPort = MCAST_PORT
	if ('gnssview:port' in cfg):
		mcastPort = cfg['gnssview:port']
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
	sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, MULTICAST_TTL)
	
# Create UUCP lock for the serial port
uucpLockPath = '/var/lock';
if ('paths:uucp lock' in cfg):
	uucpLockPath = cfg['paths:uucp lock']
Debug('Creating uucp lock in ' + uucpLockPath)
ret = subprocess.check_output(['/usr/local/bin/lockport','-d',uucpLockPath,'-p',str(os.getpid()),port,sys.argv[0]])

if (re.match(rb'1',ret)==None):
	ottplib.RemoveProcessLock(lockFile)
	ErrorExit('Could not obtain a lock on ' + port + '.Exiting.')

Debug('Opening ' + port)

serport=None # so that this can flag failure to open the port
try:
	serport = serial.Serial(port,115200,timeout=0.2)
except:
	Cleanup()
	ErrorExit('Failed to open ' + port)
	
ConfigureReceiver(rxCfg)

tt = time.time()
mjd = ottplib.MJD(tt)
fdata = OpenDataFile(mjd)
tNext=(mjd-40587+1)*86400
tThen = 0
tLastStatusUpdate=0

tLastMsg=time.time()
inp = b''

# Since we like using regexes, pre-compile the main ones

# The header structure for SBF packets is 
# Sync (2b) | CRC (2b) | ID (2b) | length (2b) 
# Sync == $@

sbfre = re.compile(rb'\x24\x40(..)(..)(..)([\s\S]*)') 
killed = False


# The list GNSS contains current SVs 
GNSS=[]
for s in range(0,MAXSVID+1):
	GNSS.append([-1,-1,-1,-1,-1])

gotVis = 0
gotCN0 = 0
nBadCRC=0

while (not killed):
	
	# Check for timeout
	if (time.time() - tLastMsg > rxTimeout):
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
	fdata.write(newinp) # log what we just got
	
	tNow = time.time()	# got something- tag the time
	tLastMsg = tNow     # resets the timeout
			
	if (tNow >= tNext):
		# (this way is safer than just incrementing mjd)
		fdata.close()
		mjd=int(tNow/86400) + 40587	
		fdata = OpenDataFile(mjd)
		tNext=(mjd-40587+1)*86400	# seconds at next MJD
		
	if (logStatus or broadcast):
		inp = inp + newinp
		
		# A reasonable limit on the size of the buffered data needs to be set
		# to guard against odd problems (seen with ublox receivers but not Septentrio) 
		if len(inp) > MAXBUFLEN:
			print ('Buffer too big',len(inp),'bytes') # FIXME remove this one day
			inp=b'' # empty the buffer AFTER reporting its size
			continue
			
		# Try to find an SBF message
		match = sbfre.search(inp)
		
		# Note that match only contains one match.
		# Any remaining matches are determined within the loop
		while match:
			
			inp = inp[match.start():] # discard the prematch string
			
			pktCRC, = struct.unpack_from('<H',match.group(1)); # ushort, little endian
			pktID, =  struct.unpack_from('<H',match.group(2)); # ushort, little endian
			pktLen, = struct.unpack_from('<H',match.group(3)); # ushort, little endian
			
			inpLen=len(inp)

			if (pktLen > inpLen):
				Debug('SHORT!' + str(pktLen) + ',' + str(inpLen))
				break # need a bit more 
			
			# Check the length - should be a multiple of 4. If it isn't, then the packet is incomplete
			if (pktLen %4 > 0):
				Debug('Bad length')
				inp = inp[2:] # something's wrong, like lost data, so chop off the bogus sync
				match = sbfre.search(inp) # have another go
				continue
			
			# Check the CRC
			crcDataStart = match.start()+4
			crc= CRC_CCITT(inp[crcDataStart:crcDataStart+pktLen-4])
			if not(pktCRC == crc):
				nBadCRC +=1
				Debug('Bad CRC: ' + str(pktID) + ' ' + str(pktLen) + ' ' + str(inpLen) )
				if (pktLen == inpLen):
					inp=b'' # eat the lot 
					break #and there are no more match
				else:
					inp = inp[pktLen:] # still some chewy bits 
					match = sbfre.search(inp) # and we need to have another go
					continue
			
			# Length OK CRC OK so parse away
			data = match.group(4)
			if ((pktID & 8191) == 4027):
				Debug('pkt 4027 ' + str(pktLen))
				ParseMeasEpoch(data);
				gotCN0=1
			elif (broadcast and ((pktID & 8191) == 4012)):
				Debug('pkt 4012 ' + str(pktLen))
				ParseSatVisibility(data,pktLen-8)
				gotVis=1
			else:
				Debug('pkt ' + str(pktID & 8191) + ' ' + str(pktLen))
				
			# Tidy up the input buffer - remove what we just parsed
			if (pktLen == inpLen):
				inp=b'' # we ate the lot 
				break # 
			else:
				inp = inp[pktLen:] # still some chewy bits 
				match = sbfre.search(inp) # and we need to have another go
			
		if gotCN0:
			if logStatus:
				Debug('N BAD CRC = ' + str(nBadCRC))
				tt = time.time()
				if (tt - tLastStatusUpdate > STATUS_LOGGING_INTERVAL):
					UpdateStatusFile(rxStatus)
					tLastStatusUpdate = tt
			if broadcast and gotVis:
				BroadcastStatus()
			ClearGNSS()
			gotVis, gotCN0 = 0,0
					
	
Cleanup()
