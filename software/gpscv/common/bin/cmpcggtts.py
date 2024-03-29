#!/usr/bin/python3
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
# Compares pairs of CGGTTS files
# 
# Example
# cmpcggtts.py ref cal 58418 58419
# 
# cmpcggtts.py --debug --refext gps.C1.30s.dat --calext gps.C1.30s.dat  ./ ../ottp1 58418 58419
#
# 

import argparse
from   datetime import datetime
import matplotlib.pyplot as plt
import numpy as np
import os
import re
import sys

# This is where cggttslib is installed
sys.path.append("/usr/local/lib/python3.6/site-packages") # Ubuntu 18.04
sys.path.append("/usr/local/lib/python3.8/site-packages") # Ubuntu 20.04

import cggttslib

VERSION = "0.4.2"
AUTHORS = "Michael Wouters"

# cggtts versions
CGGTTS_RAW = 0
CGGTTS_V1  = 1
CGGTTS_V2  = 2
CGGTTS_V2E = 3
CGGTTS_UNKNOWN = 999

# cggtts data fields
PRN=0
CL=1 
MJD=2
STTIME=3
TRKL=4
ELV=5
AZTH=6
REFSV=7
SRSV=8
REFGPS=9
REFSYS=9
SRGPS=10
SRSYS=10
DSG=11
IOE=12
MDTR=13
SMDT=14
MDIO=15
SMDI=16
# CK=17 # for V1, single frequency
MSIO=17
SMSI=18 
ISG=19 # only for dual frequency
# CK=20  # V1, dual freqeuncy
# CK=20  # V2E, single frequency
FRC=21 
# CK=23  #V2E, ionospheric measurements available

MIN_TRACK_LENGTH=750
DSG_MAX = 20.0 # in ns
SRSYS_MAX = 9999.9 # in ns
ELV_MASK = 0.0 # in degrees

NO_WEIGHT = 1
ELV_WEIGHT = 2

USE_GPSCV = 1
USE_AIV = 2

MODELED_IONOSPHERE=1
MEASURED_IONOSPHERE=2

# operating mode
MODE_TT=1 # default
MODE_DELAY_CAL=2

# measurement codes
C1  = 'C1'
L3P = 'L3P'

# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ------------------------------------------
def Warn(msg):
	if (not args.nowarn):
		sys.stderr.write('WARNING! '+ msg+'\n')
	return

# ------------------------------------------
def Info(msg):
	if (not args.quiet):
		sys.stdout.write(msg+'\n')
	return

# ------------------------------------------
def SetDataColumns(ver,isdf):
	global PRN,MJD,STTIME,ELV,AZTH,REFSV,REFSYS,IOE,MDTR,MDIO,MSIO,FRC,CK
	if (CGGTTS_RAW == ver): # CL field is empty
		PRN=0
		MJD=1
		STTIME=2
		ELV=3
		AZTH=4
		REFSV=5
		REFSYS=6
		IOE=7
		MDTR=8
		MDIO=9
		MSIO=10
		FRC=11
	elif (CGGTTS_V1 == ver):
		if (isdf):
			CK = 20
		else:
			CK = 17
	elif (CGGTTS_V2 == ver):
		if (isdf):
			FRC=22
			CK =23
		else:
			FRC=19
			CK =20
	elif (CGGTTS_V2E == ver):
		if (isdf):
			FRC = 22
			CK  = 23
		else:
			FRC= 19
			CK = 20

# ------------------------------------------

def ReadCGGTTS(path,prefix,ext,mjd,startTime,stopTime,measCode):
	d=[]
	
	fname = path + '/' + str(mjd) + '.' + ext # default is MJD.cctf
	if (not os.path.isfile(fname)): # otherwise, BIPM style
		mjdYY = int(mjd/1000)
		mjdXXX = mjd % 1000
		fBIPMname = path + '/' + prefix + '{:02d}.{:03d}'.format(mjdYY,mjdXXX)
		if (not os.path.isfile(fBIPMname)): 
			Warn('Unable to open ' + fBIPMname + ' or ' + fname)
			return ([],[],{})
		fname = fBIPMname
		
	ver = CGGTTS_UNKNOWN
	(header,warnings,checksumOK) = cggttslib.ReadHeader(fname)
	if (not header):
		Warn(warnings)
		return ([],[],{})
	if (not(warnings == '')): # header OK, but there was a warning
		Warn(warnings)
	if (not checksumOK and enforceChecksum):
		sys.exit()
		
	# OK to open the file
	fin = open(fname,'r')
	
	if (header['version'] == 'RAW'):
		Debug('Raw clock results')
		ver=CGGTTS_RAW
		header['version']='raw'
	elif (header['version'] == '01'):	
		Debug('V01')
		ver=CGGTTS_V1
		header['version']='V1'
	elif (header['version'] == '02'):	
		Debug('V02')
		Warn('The data are treated as GPS')
		ver=CGGTTS_V2
		header['version']='V02'
	elif (header['version'] == '2E'):
		Debug('V2E')
		ver=CGGTTS_V2E
		header['version']='V2E'
	else:
		Warn('Unknown format - the header is incorrectly formatted')
		return ([],[],{})
	
	hasMSIO = False
	# Eat the header
	while True:
		l = fin.readline()
		if not l:
			Warn('Bad format')
			return ([],[],{})
		if (re.search('STTIME TRKL ELV AZTH',l)):
			if (re.search('MSIO',l)):
				hasMSIO=True
				Debug('MSIO present')
			continue
		match = re.match('\s+hhmmss',l)
		if match:
			break
		
	SetDataColumns(ver,hasMSIO)
	if (hasMSIO):
		header['MSIO present'] = 'yes' # A convenient bodge. Sorry Mum.
	else:
		header['MSIO present'] = 'no'
	
	if (ver == CGGTTS_V1):
		if (hasMSIO):
			cksumend=115
		else:
			cksumend=101
	elif (ver == CGGTTS_V2):
		if (hasMSIO):
			cksumend=125
		else:
			cksumend=111
	elif (ver ==CGGTTS_V2E):
		if (hasMSIO):
			cksumend=125
		else:
			cksumend=111
			
	nextday =0
	stats=[]
	
	nLowElevation=0
	nHighDSG=0
	nHighSRSYS=0
	nShortTracks=0
	nBadSRSV=0
	nBadMSIO =0
	 
	while True:
		l = fin.readline().rstrip()
		if l:
			fields = l.split()
			if (ver != CGGTTS_RAW):
				cksum = int(fields[CK],16)
				#print CK,fields[CK],cksum,CheckSum(l[:cksumend])
				if (not(cksum == (cggttslib.CheckSum(l[:cksumend])))):
					if (enforceChecksum):
						sys.stderr.write('Bad checksum in data of ' + fname + '\n')
						exit()
					else:
						Warn('Bad checksum in data of ' + fname )
			# V1 is GPS-only and doesn't have the constellation identifier prepending 
			# the PRN, so we'll add it for compatibility with later versions
			if (ver == CGGTTS_V1):
				thePRN = int(fields[PRN])
				fields[PRN]= "G{:02d}".format(thePRN)
			
			if (ver == CGGTTS_V2): 
				thePRN = int(fields[PRN])
				fields[PRN]= "G{:02d}".format(thePRN) # FIXME works for GPS only
				
			theMJD = int(fields[MJD])
			hh = int(fields[STTIME][0:2])
			mm = int(fields[STTIME][2:4])
			ss = int(fields[STTIME][4:6])
			tt = hh*3600+mm*60+ss
			dsg=0.0
			srsys=0.0
			trklen=999
			reject=False
			if (not(CGGTTS_RAW == ver)): # DSG not defined for RAW
				if (fields[DSG] == '9999' or fields[DSG] == '****'): # field is 4 wide so 4 digits (no sign needed)
					reject=True
					dsg = 999.9 # so that we count '****' as high DSG
				else:
					dsg = int(fields[DSG])/10.0
				if (fields[SRSYS] == '99999' or fields[SRSYS] == '******'): # field is 6 wide so sign + 5 digits -> max value is 99999
					reject=True;
					srsys = 9999.9 # so that we count '******' as high SRSYS
				else:
					srsys = int(fields[SRSYS])/10.0
				trklen = int(fields[TRKL])
			elv = int(fields[ELV])/10.0
			if (elv < elevationMask):
				nLowElevation +=1
				reject=True
			if (dsg > maxDSG):
				nHighDSG += 1
				reject = True
			if (abs(srsys) > maxSRSYS):
				nHighSRSYS += 1
				reject = True
			if (trklen < minTrackLength):
				nShortTracks +=1
				reject = True
			if (not(CGGTTS_RAW == ver)):
				if (fields[SRSV] == '99999' or fields[SRSV]=='*****'):
					nBadSRSV +=1
					reject=True
				if (hasMSIO):
					if (fields[MSIO] == '9999' or fields[MSIO] == '****' or fields[SMSI]=='***' ):
						nBadMSIO +=1
						reject=True
			if (CGGTTS_RAW == ver):
				if (hasMSIO):
					if (fields[MSIO] == '9999' or fields[MSIO] == '****'):
						nBadMSIO +=1
						reject=True	
			if (reject):
				continue
			frc = ''
			if (ver == CGGTTS_V2 or ver == CGGTTS_V2E):
				frc=fields[FRC]
			if (measCode == ''): # Not set so we ignore it
				frc = ''
				
			msio = 0
			if (hasMSIO):
				msio = float(fields[MSIO])/10.0
			if ((tt >= startTime and tt < stopTime and theMJD == mjd and frc==measCode) or args.keepall):
				d.append([fields[PRN],int(fields[MJD]),tt,trklen,elv,float(fields[AZTH])/10.0,float(fields[REFSV])/10.0,float(fields[REFSYS])/10.0,
					dsg,int(fields[IOE]),float(fields[MDIO])/10.0,msio,frc])
			else:
				nextday += 1
		else:
			break
		
	fin.close()
	stats.append([nLowElevation,nHighDSG,nShortTracks,nBadSRSV,nBadMSIO])

	Debug(str(nextday) + ' tracks after end of the day removed')
	Debug('low elevation = ' + str(nLowElevation))
	Debug('high DSG = ' + str(nHighDSG))
	Debug('high SRSYS = ' + str(nHighSRSYS))
	Debug('short tracks = ' + str(nShortTracks))
	Debug('bad SRSV = ' + str(nBadSRSV))
	Debug('bad MSIO = ' + str(nBadMSIO))
	return (d,stats,header)
	
# ------------------------------------------
def GetDelay(headers,delayName):
	for h in headers:
		if (delayName in h):
			if (delayName == 'tot dly'):
				return True,0.0
			#print h[delayName]
			return True,float(h[delayName])
	return False,0.0

# ------------------------------------------
def GetVersion(headers):
	ver =''
	for h in headers:
		ver = h['version']
	return ver
	
# ------------------------------------------
def GetMeasCode(d,col):
	frc = d[0][col]
	Debug('GetMeasCode:' + frc)
	if (frc == 'L1C'):
		frc = 'L1C'
		Debug('-->' + frc)
	return frc
	
	
# ------------------------------------------
def HasMSIO(headers):
	for h in headers:
		if (h['MSIO present'] == 'no'): # one bad one spoils the bunch
			return False
	return True

# ------------------------------------------
def GetFloat(msg,defaultValue):
	ok = False
	while (not ok):
		val = input(msg)
		val=val.strip()
		if (len(val) == 0):
			return defaultValue
		try:
			fval = float(val)
			ok = True
		except:
			ok = False
	return fval

# ------------------------------------------
def AverageTracks(trks,useMSIO):
	avs=[]
	iref = 0
	mjd1 = trks[iref][MJD]
	st1  = trks[iref][STTIME]
	if (useMSIO):
		av = trks[iref][REFSYS] + trks[iref][REF_IONO] - trks[iref][REF_MSIO]
	else:
		av = trks[iref][REFSYS] + IONO_OFF*trks[iref][REF_IONO] 
	iref +=1
	svcnt=1
	trklen = len(trks)
	while iref < trklen:
		mjd2 = trks[iref][MJD]
		st2 = trks[iref][STTIME]
		if (mjd2 == mjd1 and st2 == st1):
			#av += trks[iref][REFSYS] + IONO_OFF*trks[iref][REF_IONO]
			if (useMSIO):
				av += trks[iref][REFSYS] + trks[iref][REF_IONO] - trks[iref][REF_MSIO]
			else:
				av += trks[iref][REFSYS] + IONO_OFF*trks[iref][REF_IONO] 
			#print trks[iref][REFSYS] + IONO_OFF*trksf[iref][REF_IONO] 
			svcnt += 1
		else:
			avs.append([mjd1,st1, av/svcnt + refCorrection])
			#print mjd1-firstMJD+st1/86400.0, av/svcnt + refCorrection,svcnt
			svcnt=1
			mjd1=mjd2
			st1 = st2
			if (useMSIO):
				av = trks[iref][REFSYS] + trks[iref][REF_IONO] - trks[iref][REF_MSIO]
			else:
				av = trks[iref][REFSYS] + IONO_OFF*trks[iref][REF_IONO] 
		iref += 1
	avs.append([mjd1,st1, av/svcnt]) # last one
	#print mjd1-firstMJD+st1/86400.0, av/svcnt,svcnt
	return avs
	
# ------------------------------------------

refExt = 'cctf'
calExt = 'cctf'
refPrefix=''
calPrefix=''

refMeasCode = ''
calMeasCode = ''

elevationMask=ELV_MASK
minTrackLength = MIN_TRACK_LENGTH
maxDSG=DSG_MAX
maxSRSYS = SRSYS_MAX
cmpMethod=USE_GPSCV
mode = MODE_TT
ionosphere=True
useRefMSIO=False
useCalMSIO=False
acceptDelays=False
matchEphemeris=False
refIonosphere=MODELED_IONOSPHERE
calIonosphere=MODELED_IONOSPHERE
weighting=NO_WEIGHT
enforceChecksum=False

startTime=0 # in seconds, from the start of the day
stopTime=86399

# Switch for the ionosphere correction
IONO_OFF = 0

# Data column indices

CAL_FRC=12
REF_FRC=12

examples =  'Usage examples\n'
examples += '1. Common-view time and frequency transfer\n'
examples += '    cmpcggtts.py ref cal 58418 58419\n'
examples += '2. Delay calibration with no prompts for delays\n'
examples += '    cmpcggtts.py --delaycal --acceptdelays ref cal 58418 58419\n'

parser = argparse.ArgumentParser(description='Match and difference CGGTTS files',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

# required arguments
parser.add_argument('refDir',help='reference CGGTTS directory')
parser.add_argument('calDir',help='calibration CGGTTS directory')
parser.add_argument('firstMJD',help='first mjd')
parser.add_argument('lastMJD',help='last mjd');

#
parser.add_argument('--starttime',help='start time of day HH:MM:SS for first MJD (default 0)')
parser.add_argument('--stoptime',help='stop time of day HH:MM:SS for last MJD (default 23:59:00)')

parser.add_argument('--calfrc',help='set the calibration FRC code (L1C,L3P,...)')
parser.add_argument('--reffrc',help='set the reference FRC code (L1C,L3P,...)')

# filtering
parser.add_argument('--elevationmask',help='elevation mask (in degrees, default '+str(elevationMask)+')')
parser.add_argument('--mintracklength',help='minimum track length (in s, default '+str(minTrackLength)+')')
parser.add_argument('--maxdsg',help='maximum DSG (in ns, default '+str(maxDSG)+')')
parser.add_argument('--maxsrsys',help='maximum SRSYS (in ns, default '+str(maxSRSYS)+')')
parser.add_argument('--matchephemeris',help='match on ephemeris [CV only] (default no)',action='store_true')

# parser.add_argument('--weighting', type=str,help='weighting of tracks (default=none)')

# analysis mode
parser.add_argument('--cv',help='compare in common view (default)',action='store_true')
parser.add_argument('--aiv',help='compare in all-in-view',action='store_true')

parser.add_argument('--acceptdelays',help='accept the delays (no prompts in delay calibration mode)',action='store_true')
parser.add_argument('--delaycal',help='delay calibration mode',action='store_true')
parser.add_argument('--timetransfer',help='time-transfer mode (default)',action='store_true')
parser.add_argument('--ionosphere',help='use the ionosphere in delay calibration mode (default = not used)',action='store_true')
parser.add_argument('--useRefMSIO',help='use the measured ionosphere (mdio is removed from refsys and msio is subtracted, useful for V1 CGGTTS) ',action='store_true')
parser.add_argument('--useCalMSIO',help='use the measured ionosphere (mdio is removed from refsys and msio is subtracted, useful for V1 CGGTTS) ',action='store_true')
parser.add_argument('--checksum',help='exit on bad checksum (default is to warn only)',action='store_true')

parser.add_argument('--refprefix',help='file prefix for reference receiver (default = MJD)')
parser.add_argument('--calprefix',help='file prefix for calibration receiver (default = MJD)')
parser.add_argument('--refext',help='file extension for reference receiver (default = cctf)')
parser.add_argument('--calext',help='file extension for calibration receiver (default = cctf)')

parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--nowarn',help='suppress warnings',action='store_true')
parser.add_argument('--quiet',help='suppress all output to the terminal',action='store_true')
parser.add_argument('--keepall',help='keep tracks after the end of the day',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

if (args.starttime):
	match = re.search('(\d+):(\d+):(\d+)',args.starttime)
	if match:
		hh = int(match.group(1))
		mm = int(match.group(2))
		ss = int(match.group(3))
		startTime=hh*3600+mm*60+ss
	
if (args.stoptime):
	match = re.search('(\d+):(\d+):(\d+)',args.stoptime)
	if match:
		hh = int(match.group(1))
		mm = int(match.group(2))
		ss = int(match.group(3))
		stopTime=hh*3600+mm*60+ss
		
if (args.refext):
	refExt = args.refext

if (args.calext):
	calExt = args.calext

if (args.refprefix):
	refPrefix = args.refprefix
	
if (args.calprefix):
	calPrefix = args.calprefix
	
if (args.elevationmask):
	elevationMask = float(args.elevationmask)

if (args.maxdsg):
	maxDSG = float(args.maxdsg)

if (args.maxsrsys):
	maxSRSYS = float(args.maxsrsys)
	
if (args.matchephemeris):
	matchEphemeris=True
	
if (args.mintracklength):
	minTrackLength = int(args.mintracklength)

#if (args.weighting):
	#args.weighting = args.weighting.lower()
	#if (args.weighting == 'none'):
		#weighting = NO_WEIGHT
	#elif (args.weighting == 'elevation'):
	#	weighting = ELV_WEIGHT
		
if (args.cv):
	cmpMethod = USE_GPSCV

if (args.aiv):
	cmpMethod = USE_AIV

if (args.acceptdelays):
	acceptDelays=True
	
if (args.delaycal):
	mode = MODE_DELAY_CAL
	ionosphere = False
	IONO_OFF=1
	if (args.ionosphere): # only use this option in delay calibration
		ionosphere=True
		IONO_OFF=0

if (args.useRefMSIO):
	useRefMSIO=True

if (args.useCalMSIO):
	useCalMSIO=True
	
if (args.timetransfer):
	mode = MODE_TT

if (args.checksum):
	enforceChecksum = True

if (args.reffrc): 
	refMeasCode = args.reffrc

if (args.calfrc):
	calMeasCode = args.calfrc
	
firstMJD = int(args.firstMJD)
lastMJD  = int(args.lastMJD)

# Print the settings
if (not args.quiet):
	print('\n')
	print('Elevation mask = ' + str(elevationMask) + ' deg')
	print('Minimum track length = ' + str(minTrackLength) + 's')
	print('Maximum DSG = ' + str(maxDSG) + ' ns')
	if (weighting == NO_WEIGHT):
		print('Weighting = none')
	elif (weighting == ELV_WEIGHT):
		print('Weighting = elevation')
	print('\n') 
	
allref=[] 
allcal=[]

refHeaders=[] # list of dictionaries
for mjd in range(firstMJD,lastMJD+1):
	if (mjd == firstMJD):
			 startT = startTime
	else:
			startT=0
	if (mjd == lastMJD):
			 stopT = stopTime
	else:
			stopT=86399
	(d,stats,header)=ReadCGGTTS(args.refDir,refPrefix,refExt,mjd,startT,stopT,refMeasCode)
	if (header):
		allref = allref + d
		refHeaders.append(header)

Debug('REF total tracks= {}'.format(len(allref)))
			
calHeaders=[]
for mjd in range(firstMJD,lastMJD+1):
	if (mjd == firstMJD):
			 startT = startTime
	else:
			startT=0
	if (mjd == lastMJD):
			 stopT = stopTime
	else:
			stopT=86399
			
	(d,stats,header)=ReadCGGTTS(args.calDir,calPrefix,calExt,mjd,startT,stopT,calMeasCode)
	if header:
		allcal = allcal + d
		calHeaders.append(header)
	
reflen = len(allref)
callen = len(allcal)
if (reflen==0 or callen == 0):
	sys.stderr.write('Insufficient data\n')
	exit()

Debug('CAL total tracks= {}'.format(len(allcal)))

refVer = GetVersion(refHeaders)
calVer = GetVersion(calHeaders)

calHasMSIO = HasMSIO(calHeaders)
refHasMSIO = HasMSIO(refHeaders)

if (useRefMSIO and not(refHasMSIO)):
	sys.stderr.write('REF data does not have MSIO');
	exit()
		
if (useCalMSIO and not(calHasMSIO)):
	sys.stderr.write('CAL data does not have MSIO');
	exit()

# If the ionosphere is off eg for delay cal, this is incompatible with L3P data
if (IONO_OFF==1):
	if (calMeasCode == 'L3P' or refMeasCode == 'L3P'):
		sys.stderr.write('Ionosphere is off but data is L3P\n');
		sys.stderr.write('Use --ionosphere option with L3P data\n');
		exit();
		
refCorrection = 0 # sign convention: add to REFSYS
calCorrection = 0 # ditto
	
if (mode == MODE_DELAY_CAL and not(acceptDelays)):
	# Possible delay combinations are
	# TOT DLY
	# SYS + REF
	# INT DLY + CAB DLY + REF DLY
	# Warning! Delays are assumed not to change
	
	print('\n')
	print('REF/HOST receiver')
	ok,totDelay = GetDelay(refHeaders,'tot dly')
	if ok:
		newTotDelay = GetFloat('New TOT DLY [{} ns]: '.format(totDelay),totDelay)
		refCorrection = totDelay - newTotDelay
		Info('Reported TOT DLY = {}'.format(newTotDelay))
	else:
		ok,sysDelay = GetDelay(refHeaders,'sys dly')
		if ok:
			newSysDelay = GetFloat('New SYS DLY [{} ns]: '.format(sysDelay),sysDelay)

			ok,refDelay = GetDelay(refHeaders,'ref dly')
			newRefDelay = GetFloat('New REF DLY [{} ns]: '.format(refDelay),refDelay)
			refCorrection = (sysDelay-refDelay) - (newSysDelay - newRefDelay)
			Info('Reported SYS DLY={} REF DLY={}'.format(newSysDelay,newRefDelay))
		else:
			# header tested so need to check further
			
			dlyCode = ''
			if ('int dly code' in refHeaders[0]):
				dlyCode=refHeaders[0]['int dly code']
			ok,intDelay = GetDelay(refHeaders,'int dly')
			newIntDelay = GetFloat('New INT DLY {}[{} ns]: '.format(dlyCode,intDelay),intDelay)
			
			# Dual frequency
			if ('int dly 2' in refHeaders[0]):
				dlyCode2 = ''
				if ('int dly code 2' in refHeaders[0]):
					dlyCode2=refHeaders[0]['int dly code 2']
				ok,intDelay2 = GetDelay(refHeaders,'int dly 2') 
				newIntDelay2 = GetFloat('New INT DLY {}[{} ns]: '.format(dlyCode2,intDelay2),intDelay2)
				print('WARNING! P3 delay changes will not be used!')
				
			ok,cabDelay = GetDelay(refHeaders,'cab dly') 
			newCabDelay = GetFloat('New CAB DLY [{} ns]: '.format(cabDelay),cabDelay)
			
			ok,refDelay = GetDelay(refHeaders,'ref dly') 
			newRefDelay = GetFloat('New REF DLY [{} ns]: '.format(refDelay),refDelay)
			
			refCorrection = (intDelay + cabDelay - refDelay) - (newIntDelay + newCabDelay - newRefDelay)
			
			if ('int dly 2' in calHeaders[0]):
				Info('Reported INT DLY ({})={} INT DLY ({})={} CAB DLY={} REF DLY={}'.format(dlyCode,newIntDelay,dlyCode2,newIntDelay2,newCabDelay,newRefDelay)) 
			else:
				Info('Reported INT DLY ({})={} CAB DLY={} REF DLY={}'.format(newIntDelay,dlyCode,newCabDelay,newRefDelay))
			
	Info('Delay delta = {}'.format(refCorrection))	
	
	print('\n')
	print('CAL/TRAV receiver')
	ok,totDelay = GetDelay(calHeaders,'tot dly')
	if ok:
		newTotDelay = GetFloat('New TOT DLY [{} ns]: '.format(totDelay),totDelay)
		calCorrection = totDelay - newTotDelay
		Info('Reported TOT DLY = {}'.format(newTotDelay))
	else:
		ok,sysDelay = GetDelay(calHeaders,'sys dly')
		if ok:
			newSysDelay = GetFloat('New SYS DLY [{} ns]: '.format(sysDelay),sysDelay)

			ok,refDelay = GetDelay(calHeaders,'ref dly')
			newRefDelay = GetFloat('New REF DLY [{} ns]: '.format(refDelay),refDelay)
			calCorrection = (sysDelay-refDelay) - (newSysDelay - newRefDelay)
			Info('Reported SYS DLY={} REF DLY={}'.format(newSysDelay,newRefDelay))
		else:
			# header tested so need to check further
			# Single frequency
			dlyCode = ''
			if ('int dly code' in calHeaders[0]):
				dlyCode=calHeaders[0]['int dly code']
				
			ok,intDelay = GetDelay(calHeaders,'int dly') 
			newIntDelay = GetFloat('New INT DLY {}[{} ns]: '.format(dlyCode,intDelay),intDelay)
			
			# Dual frequency
			if ('int dly 2' in calHeaders[0]):
				dlyCode2 = ''
				if ('int dly code 2' in calHeaders[0]):
					dlyCode2=calHeaders[0]['int dly code 2']
				ok,intDelay2 = GetDelay(calHeaders,'int dly 2') 
				newIntDelay2 = GetFloat('New INT DLY {}[{} ns]: '.format(dlyCode2,intDelay2),intDelay2)
				print('WARNING! P3 delay changes will not be used!')
				
			ok,cabDelay = GetDelay(calHeaders,'cab dly') 
			newCabDelay = GetFloat('New CAB DLY [{} ns]: '.format(cabDelay),cabDelay)
			
			ok,refDelay = GetDelay(calHeaders,'ref dly') 
			newRefDelay = GetFloat('New REF DLY [{} ns]: '.format(refDelay),refDelay)
			
			calCorrection = (intDelay + cabDelay - refDelay) - (newIntDelay + newCabDelay - newRefDelay)
			if ('int dly 2' in calHeaders[0]):
				Info('Reported INT DLY ({})={} INT DLY ({})={} CAB DLY={} REF DLY={}'.format(dlyCode,newIntDelay,dlyCode2,newIntDelay2,newCabDelay,newRefDelay)) 
			else:
				Info('Reported INT DLY ({})={} CAB DLY={} REF DLY={}'.format(newIntDelay,dlyCode,newCabDelay,newRefDelay))
			
	Info('Delay delta = {}'.format(calCorrection))	
	
# Can redefine the indices now
PRN = 0
MJD = 1
STTIME = 2
REFSYS = 7
IOE = 9
CAL_IONO = 10
REF_IONO = 10
CAL_MSIO=11
REF_MSIO=11
FRC = 12

# Averaged deltas, in numpy friendly format,for analysis
tMatch=[]
deltaMatch=[]
refMatch=[]
calMatch=[]
tAvMatches=[]
avMatches=[]

foutName = 'ref.cal.av.matches.txt'
try:
	favmatches = open(foutName,'w')
except:
	sys.stderr.write('Unable to open ' + foutName +'\n')
	exit()

foutName = 'ref.cal.matches.txt'
try:
	fmatches = open(foutName,'w')
except:
	sys.stderr.write('Unable to open ' + foutName +'\n')
	exit()

if (useRefMSIO):
	Debug('Using REF MSIO')
if (useCalMSIO):
	Debug('Using CAL MSIO')

useMSIO = useRefMSIO or useCalMSIO
REF_MSIO_ON = 0
if (useRefMSIO):
	REF_MSIO_ON = 1
	
CAL_MSIO_ON = 0
if (useCalMSIO):
	CAL_MSIO_ON = 1
	
if (useMSIO): # all ionosphere on
	ionosphere=True
	IONO_OFF=0

Debug('Matching tracks ...')
Debug('Ionosphere '+('removed' if (IONO_OFF==1) else 'included'))

if (cmpMethod == USE_GPSCV):
	matches = []
	iref=0
	jcal=0
	nEphemerisMisMatches=0
	# Rather than fitting to the average, fit to all matched tracks
	# so that outliers are damped by the least squares fit
	while iref < reflen:
		
		mjd1=allref[iref][MJD]
		prn1=allref[iref][PRN]
		st1 =allref[iref][STTIME]
		ioe1 = allref[iref][IOE]
		
		while (jcal < callen):
			mjd2=allcal[jcal][MJD]
			st2 =allcal[jcal][STTIME]
			
			if (mjd2 > mjd1):
				break # stop searching - move pointer 1
			elif (mjd2 < mjd1):
				jcal += 1 # need to move pointer 2 forward
			elif (st2 > st1):
				break # stop searching - need to move pointer 1
			elif ((mjd1==mjd2)and (st1 == st2)):
				# times are matched so search for SV
				jtmp = jcal
				while ((mjd1 == mjd2) and (st1 == st2) and (jtmp<callen)):
		
					mjd2=allcal[jtmp][MJD]
					st2 =allcal[jtmp][STTIME]
					prn2=allcal[jtmp][PRN]
					ioe2=allcal[jtmp][IOE]
					
					if (prn1 == prn2):
						break
					jtmp += 1
				if ((mjd1 == mjd2) and (st1 == st2) and (prn1 == prn2)):
					if (matchEphemeris and not(ioe1 == ioe2)):
						nEphemerisMisMatches += 1
						break
					refMSIO =  allref[iref][REF_MSIO] if refHasMSIO else 0 # yes, yes I know but YOU should know that there is no valid MSIO 
					calMSIO =  allcal[jtmp][CAL_MSIO] if calHasMSIO else 0
					fmatches.write('{} {} {} {} {} {} {} {} {}\n'.format(mjd1,st1,prn1,allref[iref][REFSYS],allcal[jtmp][REFSYS],refMSIO,calMSIO,allref[iref][REF_IONO],allcal[jtmp][CAL_IONO]))
					
					tMatch.append(mjd1-firstMJD+st1/86400.0)  #used in the linear fit
					
					if useMSIO:
						# This makes sense for eg L1C measurements which have been corrected using MDIO,  and MSIO is available
						delta = (allref[iref][REFSYS] + REF_MSIO_ON *( allref[iref][REF_IONO] - allref[iref][REF_MSIO] )  + refCorrection) - (
							allcal[jtmp][REFSYS] + CAL_MSIO_ON * (allcal[jtmp][CAL_IONO] - allcal[jtmp][REF_MSIO] ) + calCorrection)
					else:
						delta = (allref[iref][REFSYS] + IONO_OFF*allref[iref][REF_IONO] + refCorrection) - (
							allcal[jtmp][REFSYS] + IONO_OFF*allcal[jtmp][CAL_IONO] + calCorrection)
					refMatch.append(allref[iref][REFSYS])
					calMatch.append(allcal[jtmp][REFSYS])
					deltaMatch.append(delta) # used in the linear fit
					matches.append(allref[iref]+allcal[jtmp]) # 'matches' contains the whole record
					break
				else:
					break
			else:
				jcal += 1
				
		iref +=1
		
	Info(str(len(matches))+' matched tracks')
	
	if (len(matches) ==0):
		sys.stderr.write('No matched tracks\n')
		exit()
		
	if (matchEphemeris):
		Info(str(nEphemerisMisMatches) + ' mismatched ephemerides')
	
	# Calculate the average clock difference at each track time for plotting etc.
	# These differences are not used for the linear fit	
	ncols = 13
	lenmatch=len(matches)
	mjd1=matches[0][MJD]
	st1=matches[0][STTIME]
	if (useRefMSIO):
		avref = matches[0][REFSYS]  +  matches[0][REF_IONO] - matches[0][REF_MSIO] + refCorrection
	else:
		avref = matches[0][REFSYS]  +  IONO_OFF*matches[0][REF_IONO]  + refCorrection
	if (useCalMSIO):
		avcal = matches[0][ncols + REFSYS] + matches[0][ncols+CAL_IONO] - matches[0][ncols+CAL_MSIO] + calCorrection
	else:
		avcal = matches[0][ncols + REFSYS] + IONO_OFF*matches[0][ncols+CAL_IONO] + calCorrection
	nsv = 1
	imatch=1
	while imatch < lenmatch:
		mjd2 = matches[imatch][MJD]
		st2  = matches[imatch][STTIME]
		if (mjd1==mjd2 and st1==st2):
			nsv += 1
			if (useRefMSIO):
				avref  += matches[imatch][REFSYS] + matches[imatch][REF_IONO] - matches[imatch][REF_MSIO] + refCorrection
			else:
				avref  += matches[imatch][REFSYS] + IONO_OFF*matches[imatch][REF_IONO]  + refCorrection
			if (useCalMSIO):
				avcal  += matches[imatch][ncols + REFSYS] + matches[imatch][ncols+CAL_IONO] - matches[imatch][ncols+CAL_MSIO] + calCorrection
			else:
				avcal  += matches[imatch][ncols + REFSYS] + IONO_OFF*matches[imatch][ncols+CAL_IONO] + calCorrection
		else:
			favmatches.write('{} {} {} {} {} {}\n'.format(mjd1,st1,avref/nsv,avcal/nsv,(avref-avcal)/nsv,nsv))
			tAvMatches.append(mjd1-firstMJD+st1/86400.0)
			avMatches.append((avref-avcal)/nsv)
			mjd1 = mjd2
			st1  = st2
			nsv=1
			
			if (useRefMSIO):
				avref  = matches[imatch][REFSYS] + matches[imatch][REF_IONO] - matches[imatch][REF_MSIO] + refCorrection
			else:
				avref  = matches[imatch][REFSYS] + IONO_OFF*matches[imatch][REF_IONO]  + refCorrection
			if (useCalMSIO):
				avcal  = matches[imatch][ncols + REFSYS] + matches[imatch][ncols+CAL_IONO] - matches[imatch][ncols+CAL_MSIO] + calCorrection
			else:
				avcal  = matches[imatch][ncols + REFSYS] + IONO_OFF*matches[imatch][ncols+CAL_IONO] + calCorrection
				
		imatch += 1
	# last one
	favmatches.write('{} {} {} {} {} {}\n'.format(mjd1,st1,avref/nsv,avcal/nsv,(avref-avcal)/nsv,nsv))
	tAvMatches.append(mjd1-firstMJD+st1/86400.0)
	avMatches.append((avref-avcal)/nsv)
elif (cmpMethod == USE_AIV):
	# Opposite order here
	# First average at each time
	# and then match times
	refavs = AverageTracks(allref,useRefMSIO)
	calavs = AverageTracks(allcal,useCalMSIO)
	
	matches = []
	iref=0
	jcal=0
	refavlen = len(refavs)
	calavlen = len(calavs)
	
	while iref < refavlen:
		mjd1 = refavs[iref][0]
		st1  = refavs[iref][1]
		
		while (jcal < calavlen):
			mjd2=calavs[jcal][0]
			st2 =calavs[jcal][1]
			
			if (mjd2 > mjd1):
				break # stop searching - move pointer 1
			elif (mjd2 < mjd1):
				jcal += 1 # need to move pointer 2 forward
			elif (st2 > st1):
				break # stop searching - need to move pointer 1
			elif ((mjd1==mjd2) and (st1 == st2)):
				fmatches.write('{} {} {} {} {}\n'.format(mjd1,st1,999,refavs[iref][2],calavs[jcal][2]))
				tMatch.append(mjd1-firstMJD+st1/86400.0)
				refMatch.append(refavs[iref][2])
				calMatch.append(calavs[jcal][2])
				deltaMatch.append(refavs[iref][2] + refCorrection - (calavs[jcal][2] + calCorrection))
				#matches.append(allref[iref]+allcal[jtmp])
				# print mjd1,st1,refavs[iref][2],calavs[jcal][2]
				break
			else:
				jcal += 1
		iref += 1	
				
	sys.stderr.write('WARNING Not fully tested yet!\n')
	#exit()

fmatches.close()
favmatches.close()

# Analysis 

if (len(tMatch) < 2):
	sys.stderr.write('Insufficient data points left for analysis\n')
	exit()
	
p,V = np.polyfit(tMatch,deltaMatch, 1, cov=True)
slopeErr = np.sqrt(V[0][0])
meanOffset = p[1] + p[0]*(tMatch[0]+tMatch[-1])/2.0

if (MODE_DELAY_CAL==mode ):
	rmsResidual=0.0
	for t in range(0,len(tMatch)):
		delta = p[1] + p[0]*tMatch[t] - deltaMatch[t]
		rmsResidual = rmsResidual + delta*delta
	rmsResidual = np.sqrt(rmsResidual/(len(tMatch)-1)) # FIXME is this correct ?
	if (not args.quiet):
		
		print
		print('Offsets (REF - CAL)  [subtract from CAL \'INT DLY\' to correct]')
		print('  at midpoint {} ns'.format(meanOffset))
		print('  median  {} ns'.format(np.median(deltaMatch)))
		print('  mean {} ns'.format(np.mean(deltaMatch)))
		print('  std. dev {} ns'.format(np.std(deltaMatch)))
		print('slope {} ps/day +/- {} ps/day'.format(p[0]*1000,slopeErr*1000))
		print('RMS of residuals {} ns'.format(rmsResidual))
		
	f,(ax1,ax2,ax3)= plt.subplots(3,sharex=True,figsize=(8,11))
	f.suptitle('Delay calibration ' + datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S'))
	ax1.plot(tMatch,deltaMatch,ls='None',marker='.')
	ax1.plot(tAvMatches,avMatches)
	ax1.set_title('REF-CAL (filtered)')
	ax1.set_ylabel('REF-CAL (ns)')
	ax1.set_xlabel('MJD - '+str(firstMJD))
	
	ax2.plot(tMatch,calMatch,ls='None',marker='.')
	ax2.set_title('CAL (filtered) [ ' + args.calDir+ ' ]')
	ax2.set_ylabel('REFSYS (ns)')
	ax2.set_xlabel('MJD - '+str(firstMJD))
	
	ax3.plot(tMatch,refMatch,ls='None',marker='.')
	ax3.set_title('REF (filtered) [ ' + args.refDir+ ' ]')
	ax3.set_ylabel('REFSYS (ns)')
	ax3.set_xlabel('MJD - '+str(firstMJD))
	
	plt.savefig('ref.cal.ps',papertype='a4')
	
	if (not args.quiet):
		plt.show()

elif (MODE_TT == mode):
	if (not args.quiet):
		print(' Linear fit to data')
		print('Offset (REF - CAL) at midpoint {} ns '.format(meanOffset))
		print('ffe = {:.3e} +/- {:.3e}'.format(p[0]*1.0E-9/86400.0,slopeErr*1.0E-9/86400.0))
		
