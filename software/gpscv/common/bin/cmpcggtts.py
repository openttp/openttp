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
# Compares pairs of CGGTTS files
# 
# Example
# cmpcggtts.py ref cal 58418 58419
# 
# cmpcggtts.py --debug --refext gps.C1.30s.dat --calext gps.C1.30s.dat  ./ ../ottp1 58418 58419
#
# 

import argparse
from datetime import datetime
import matplotlib.pyplot as plt
import numpy as np
import os
import re
import sys

VERSION = "0.2.3"
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
DSG_MAX=20.0
ELV_MASK=0.0

NO_WEIGHT = 1
ELV_WEIGHT = 2

USE_GPSCV = 1
USE_AIV = 2

MODELED_IONOSPHERE=1
MEASURED_IONOSPHERE=2

# operating mode
MODE_TT=1 # default
MODE_DELAY_CAL=2


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
def ReadCGGTTSHeader(fin,fname):
	
	intdly=''
	cabdly=''
	antdly=''
	checksumStart=0 # workaround for R2CGGTTS output
	
	# Read the header
	header={}
	hdr=''; # accumulate header for checksumming
	lineCount=0
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.search('DATA FORMAT VERSION\s+=\s+(01|02|2E)',l)
	if (match):
		header['version'] = match.group(1)
	else:
		if (re.search('RAW CLOCK RESULTS',l)):
			header['version'] = 'RAW'
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
	
	if not(header['version'] == 'RAW'):
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('REV DATE') >= 0):
			(tag,val) = l.split('=')
			header['rev date'] = val.strip()
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
		
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('RCVR') >= 0):
			header['rcvr'] = l
			match = re.search('R2CGGTTS\s+v(\d+)\.(\d+)',l)
			if (match):
				majorVer=int(match.group(1))
				minorVer=int(match.group(2))
				if ( (majorVer == 8 and minorVer <=1) ):
					checksumStart = 1
					Debug('Fixing checksum for R2CGGTTS v'+str(majorVer)+'.'+str(minorVer))
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('CH') >= 0):
			header['ch'] = l
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
			
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('IMS') >= 0):
			header['ims'] = l
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
		
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	if (l.find('LAB') == 0):
		header['lab'] = l
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}	
	
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.match('X\s+=\s+(.+)\s+m',l)
	if (match):
		header['x'] = match.group(1)
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
	
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.match('Y\s+=\s+(.+)\s+m',l)
	if (match):
		header['y'] = match.group(1)
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
		
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.match('Z\s+=\s+(.+)\s+m',l)
	if (match):
		header['z'] = match.group(1)
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
		
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	if (l.find('FRAME') == 0):
		header['frame'] = l
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
	
	comments = ''
	while True:
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if not l:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
		if (l.find('COMMENTS') == 0):
			comments = comments + l +'\n'
		else:
			break
	
	header['comments'] = comments
	
	if (header['version'] == '01' or header['version'] == '02'):
		#l = fin.readline().rstrip()
		#lineCount = lineCount +1
		match = re.match('INT\s+DLY\s+=\s+(.+)\s+ns',l)
		if (match):
			header['int dly'] = match.group(1)
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			print l
			return {}
		
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		match = re.match('CAB\s+DLY\s+=\s+(.+)\s+ns',l)
		if (match):
			header['cab dly'] = match.group(1)
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			print l
			return {}
		
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		match = re.match('REF\s+DLY\s+=\s+(.+)\s+ns',l)
		if (match):
			header['ref dly'] = match.group(1)
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
			
	elif (header['version'] == '2E' or header['version'] == 'RAW'):
		
		#l = fin.readline().rstrip()
		#lineCount = lineCount +1
		
		match = re.match('(TOT DLY|SYS DLY|INT DLY)',l)
		if (match.group(1) == 'TOT DLY'): # if TOT DLY is provided, then finito
			(dlyname,dly) = l.split('=',1)
			header['tot dly'] = dly.strip()
		
		elif (match.group(1) == 'SYS DLY'): # if SYS DLY is provided, then read REF DLY
		
			(dlyname,dly) = l.split('=',1)
			header['sys dly'] = dly.strip()
			
			l = fin.readline().rstrip()
			hdr += l
			lineCount = lineCount +1
			match = re.match('REF\s+DLY\s+=\s+(.+)\s+ns',l)
			if (match):
				header['ref dly'] = match.group(1)
			else:
				Warn('Invalid format in {} line {}'.format(fname,lineCount))
				return {}
				
		elif (match.group(1) == 'INT DLY'): # if INT DLY is provided, then read CAB DLY and REF DLY
		
			(dlyname,dly) = l.split('=',1)
			# extra spaces in constellation and code for r2cggtts
			match = re.search('(\d+\.?\d?)\sns\s\(\w+\s(\w+)\s*\)(,\s*(\d+\.?\d?)\sns\s\(\w+\s(\w+)\s*\))?',dly)
			if (match):
				header['int dly'] = match.group(1)
				header['int dly code'] = match.group(2) # non-standard but convenient
				if (not(match.group(5) == None) and not (match.group(5) == None)):
					header['int dly 2'] = match.group(4) 
					header['int dly code 2'] = match.group(5) 
			else:
				Warn('Invalid format in {} line {}'.format(fname,lineCount))
				return {}
				
			l = fin.readline().rstrip()
			hdr += l
			lineCount = lineCount +1
			match = re.match('CAB\s+DLY\s+=\s+(.+)\s+ns',l)
			if (match):
				header['cab dly'] = match.group(1)
			else:
				Warn('Invalid format in {} line {}'.format(fname,lineCount))
				return {}
			
			l = fin.readline().rstrip()
			hdr += l
			lineCount = lineCount +1
			match = re.match('REF\s+DLY\s+=\s+(.+)\s+ns',l)
			if (match):
				header['ref dly'] = match.group(1)
			else:
				Warn('Invalid format in {} line {}'.format(fname,lineCount))
				return {}
	if not (header['version'] == 'RAW'):
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('REF') == 0):
			(tag,val) = l.split('=')
			header['ref'] = val.strip()
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
		
		l = fin.readline().rstrip()
		lineCount = lineCount +1
		if (l.find('CKSUM') == 0):
			hdr += 'CKSUM = '
			(tag,val) = l.split('=')
			cksum = int(val,16)
			if (not(CheckSum(hdr[checksumStart:]) == cksum)):
				if (enforceChecksum):
					sys.stderr.write('Bad checksum in header of ' + fname + '\n')
					exit()
				else:
					Warn('Bad checksum in header of ' + fname)
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
	
	return header;

# ------------------------------------------
def ReadCGGTTS(path,prefix,ext,mjd,startTime,stopTime):
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
		
	Debug('--> Reading ' + fname)
	try:
		fin = open(fname,'r')
	except:
		Warn('Unable to open ' + fname)
		# not fatal
		return ([],[],{})
	
	ver = CGGTTS_UNKNOWN
	header = ReadCGGTTSHeader(fin,fname)
	
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
		ver=CGGTTS_V2
		header['version']='V02'
	elif (header['version'] == '2E'):
		Debug('V2E')
		ver=CGGTTS_V2E
		header['version']='V2E'
	else:
		Warn('Unknown format - the header is incorrectly formatted')
		return ([],[],{})
	
	dualFrequency = False
	# Eat the header
	while True:
		l = fin.readline()
		if not l:
			Warn('Bad format')
			return ([],[],{})
		if (re.search('STTIME TRKL ELV AZTH',l)):
			if (re.search('MSIO',l)):
				dualFrequency=True
				Debug('Dual frequency data')
			continue
		match = re.match('\s+hhmmss',l)
		if match:
			break
		
	SetDataColumns(ver,dualFrequency)
	if (dualFrequency):
		header['dual frequency'] = 'yes' # A convenient bodge. Sorry Mum.
	else:
		header['dual frequency'] = 'no'
	
	if (ver == CGGTTS_V1):
		if (dualFrequency):
			cksumend=115
		else:
			cksumend=101
	elif (ver == CGGTTS_V2):
		if (dualFrequency):
			cksumend=125
		else:
			cksumend=111
	elif (ver ==CGGTTS_V2E):
		if (dualFrequency):
			cksumend=125
		else:
			cksumend=111
			
	nextday =0
	stats=[]
	
	nLowElevation=0
	nHighDSG=0
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
				if (not(cksum == (CheckSum(l[:cksumend])))):
					if (enforceChecksum):
						sys.stderr.write('Bad checksum in data of ' + fname + '\n')
						exit()
					else:
						Warn('Bad checksum in data of ' + fname )
			theMJD = int(fields[MJD])
			hh = int(fields[STTIME][0:2])
			mm = int(fields[STTIME][2:4])
			ss = int(fields[STTIME][4:6])
			tt = hh*3600+mm*60+ss
			dsg=0.0
			trklen=999
			reject=False
			if (not(CGGTTS_RAW == ver)): # DSG not defined for RAW
				if (fields[DSG] == '9999' or fields[DSG] == '****'):
					reject=True;
				else:
					dsg = int(fields[DSG])/10.0
				trklen = int(fields[TRKL])
			elv = int(fields[ELV])/10.0
			if (elv < elevationMask):
				nLowElevation +=1
				reject=True
			if (dsg > DSG_MAX):
				nHighDSG += 1
				reject = True
			if (trklen < minTrackLength):
				nShortTracks +=1
				reject = True
			if (not(CGGTTS_RAW == ver)):
				if (fields[SRSV] == '99999' or fields[SRSV]=='*****'):
					nBadSRSV +=1
					reject=True
				if (dualFrequency):
					if (fields[MSIO] == '9999' or fields[MSIO] == '****' or fields[SMSI]=='***' ):
						nBadMSIO +=1
						reject=True
			if (CGGTTS_RAW == ver):
				if (dualFrequency):
					if (fields[MSIO] == '9999' or fields[MSIO] == '****'):
						nBadMSIO +=1
						reject=True	
			if (reject):
				continue
			frc = 0
			if (ver == CGGTTS_V2 or ver == CGGTTS_V2E):
				frc=fields[FRC]
			msio = 0
			if (dualFrequency):
				msio = float(fields[MSIO])/10.0
			if ((tt >= startTime and tt < stopTime and theMJD == mjd) or args.keepall):
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
	Debug('short tracks = ' + str(nShortTracks))
	Debug('bad SRSV = ' + str(nBadSRSV))
	Debug('bad MSIO = ' + str(nBadMSIO))
	return (d,stats,header)
	
# ------------------------------------------
def CheckSum(l):
	cksum = 0
	for c in l:
		cksum = cksum + ord(c)
	return int(cksum % 256)

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
def GetFloat(msg,defaultValue):
	ok = False
	while (not ok):
		val = raw_input(msg)
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
def AverageTracks(trks):
	avs=[]
	iref = 0
	mjd1 = trks[iref][MJD]
	st1  = trks[iref][STTIME]
	av = trks[iref][REFSYS] + IONO_OFF*trks[iref][REF_IONO] 
	iref +=1
	svcnt=1
	trklen = len(trks)
	while iref < trklen:
		mjd2 = trks[iref][MJD]
		st2 = trks[iref][STTIME]
		if (mjd2 == mjd1 and st2 == st1):
			av += trks[iref][REFSYS] + IONO_OFF*trks[iref][REF_IONO] 
			#print trks[iref][REFSYS] + IONO_OFF*trksf[iref][REF_IONO] 
			svcnt += 1
		else:
			avs.append([mjd1,st1, av/svcnt + refCorrection])
			#print mjd1-firstMJD+st1/86400.0, av/svcnt + refCorrection,svcnt
			svcnt=1
			mjd1=mjd2
			st1 = st2
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

elevationMask=ELV_MASK
minTrackLength = MIN_TRACK_LENGTH
maxDSG=DSG_MAX
cmpMethod=USE_GPSCV
mode = MODE_TT
ionosphere=True
acceptDelays=False
matchEphemeris=False
refIonosphere=MODELED_IONOSPHERE
calIonosphere=MODELED_IONOSPHERE
weighting=NO_WEIGHT
enforceChecksum=False

startTime=0 # in seconds
stopTime=86399

# Switches for the ionosphere correction
IONO_OFF = 0

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

# filtering
parser.add_argument('--elevationmask',help='elevation mask (in degrees, default '+str(elevationMask)+')')
parser.add_argument('--mintracklength',help='minimum track length (in s, default '+str(minTrackLength)+')')
parser.add_argument('--maxdsg',help='maximum DSG (in ns, default '+str(maxDSG)+')')
parser.add_argument('--matchephemeris',help='match on ephemeris [CV only] (default no)',action='store_true')

# parser.add_argument('--weighting', type=str,help='weighting of tracks (default=none)')

# analysis mode
parser.add_argument('--cv',help='compare in common view (default)',action='store_true')
parser.add_argument('--aiv',help='compare in all-in-view',action='store_true')

parser.add_argument('--acceptdelays',help='accept the delays (no prompts in delay calibration mode)',action='store_true')
parser.add_argument('--delaycal',help='delay calibration mode',action='store_true')
parser.add_argument('--timetransfer',help='time-transfer mode (default)',action='store_true')
parser.add_argument('--ionosphere',help='use the ionosphere in delay calibration mode (default = not used)',action='store_true')

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

if (args.matchephemeris):
	matchEphemeris=True
	
if (args.mintracklength):
	minTrackLength = args.mintracklength

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
		
if (args.timetransfer):
	mode = MODE_TT

if (args.checksum):
	enforceChecksum = True
	
firstMJD = int(args.firstMJD)
lastMJD  = int(args.lastMJD)

# Print the settings
if (not args.quiet):
	print
	print 'Elevation mask = ',elevationMask,'deg'
	print 'Minimum track length =',minTrackLength,'s'
	print 'Maximum DSG =', maxDSG,'ns'
	if (weighting == NO_WEIGHT):
		print 'Weighting = none'
	elif (weighting == ELV_WEIGHT):
		print 'Weighting = elevation'
	print 
	
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
	(d,stats,header)=ReadCGGTTS(args.refDir,refPrefix,refExt,mjd,startT,stopT)
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
			
	(d,stats,header)=ReadCGGTTS(args.calDir,calPrefix,calExt,mjd,startT,stopT)
	allcal = allcal + d
	calHeaders.append(header)
	
reflen = len(allref)
callen = len(allcal)
if (reflen==0 or callen == 0):
	sys.stderr.write('Insufficient data\n')
	exit()

Debug('CAL total tracks= {}'.format(len(allcal)))
			
refCorrection = 0 # sign convention: add to REFSYS
calCorrection = 0 # ditto
	
if (mode == MODE_DELAY_CAL and not(acceptDelays)):
	# Possible delay combinations are
	# TOT DLY
	# SYS + REF
	# INT DLY + CAB DLY + REF DLY
	# Warning! Delays are assumed not to change
	
	print
	print 'REF/HOST receiver'
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
			
			ok,cabDelay = GetDelay(refHeaders,'cab dly') 
			newCabDelay = GetFloat('New CAB DLY [{} ns]: '.format(cabDelay),cabDelay)
			
			ok,refDelay = GetDelay(refHeaders,'ref dly') 
			newRefDelay = GetFloat('New REF DLY [{} ns]: '.format(refDelay),refDelay)
			
			refCorrection = (intDelay + cabDelay - refDelay) - (newIntDelay + newCabDelay - newRefDelay)
			
			Info('Reported INT DLY={} CAB DLY={} INT DLY={}'.format(newIntDelay,newCabDelay,newRefDelay))
			
	Info('Delay delta = {}'.format(refCorrection))	
	
	print
	print 'CAL/TRAV receiver'
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
				print 'WARNING! P3 delay changes will not be used!'
				
			ok,cabDelay = GetDelay(calHeaders,'cab dly') 
			newCabDelay = GetFloat('New CAB DLY [{} ns]: '.format(cabDelay),cabDelay)
			
			ok,refDelay = GetDelay(calHeaders,'ref dly') 
			newRefDelay = GetFloat('New REF DLY [{} ns]: '.format(refDelay),refDelay)
			
			calCorrection = (intDelay + cabDelay - refDelay) - (newIntDelay + newCabDelay - newRefDelay)
			
			Info('Reported INT DLY={} CAB DLY={} INT DLY={}'.format(newIntDelay,newCabDelay,newRefDelay))
			
	Info('Delay delta = {}'.format(refCorrection))	
	
# Can redefine the indices now
PRN = 0
MJD = 1
STTIME = 2
REFSYS = 7
IOE = 9
CAL_IONO = 10
REF_IONO = 10

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
					fmatches.write('{} {} {} {} {}\n'.format(mjd1,st1,prn1,allref[iref][REFSYS],allcal[jtmp][REFSYS]))
					tMatch.append(mjd1-firstMJD+st1/86400.0)  #used in the linear fit
					delta = (allref[iref][REFSYS] + IONO_OFF*allref[iref][REF_IONO]  + refCorrection) - (
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
	if (matchEphemeris):
		Info(str(nEphemerisMisMatches) + ' mismatched ephemerides')
	
	# Calculate the average clock difference at each track time for plotting etc.
	# These differences are not used for the linear fit	
	ncols = 13
	lenmatch=len(matches)
	mjd1=matches[0][MJD]
	st1=matches[0][STTIME]
	avref = matches[0][REFSYS]
	avcal = matches[0][ncols + REFSYS]
	nsv = 1
	imatch=1
	while imatch < lenmatch:
		mjd2 = matches[imatch][MJD]
		st2  = matches[imatch][STTIME]
		if (mjd1==mjd2 and st1==st2):
			nsv += 1
			avref  += matches[imatch][REFSYS] + IONO_OFF*matches[imatch][REF_IONO]  + refCorrection
			avcal  += matches[imatch][ncols + REFSYS] + IONO_OFF*matches[imatch][ncols+CAL_IONO] + calCorrection
		else:
			favmatches.write('{} {} {} {} {} {}\n'.format(mjd1,st1,avref/nsv,avcal/nsv,(avref-avcal)/nsv,nsv))
			tAvMatches.append(mjd1-firstMJD+st1/86400.0)
			avMatches.append((avref-avcal)/nsv)
			mjd1 = mjd2
			st1  = st2
			nsv=1
			avref = matches[imatch][REFSYS] + IONO_OFF*matches[imatch][REF_IONO]  + refCorrection
			avcal = matches[imatch][ncols + REFSYS] + IONO_OFF*matches[imatch][ncols+CAL_IONO] + calCorrection
		imatch += 1
	# last one
	favmatches.write('{} {} {} {} {} {}\n'.format(mjd1,st1,avref/nsv,avcal/nsv,(avref-avcal)/nsv,nsv))
	tAvMatches.append(mjd1-firstMJD+st1/86400.0)
	avMatches.append((avref-avcal)/nsv)
elif (cmpMethod == USE_AIV):
	# Opposite order here
	# First average at each time
	# and then match times
	refavs = AverageTracks(allref)
	calavs = AverageTracks(allcal)
	
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
		print 'Offsets (REF - CAL)  [subtract from CAL \'INT DLY\' to correct]'
		print '  at midpoint {} ns'.format(meanOffset)
		print '  median  {} ns'.format(np.median(deltaMatch))
		print '  mean {} ns'.format(np.mean(deltaMatch))
		print '  std. dev {} ns'.format(np.std(deltaMatch))
		print 'slope {} ps/day +/- {} ps/day'.format(p[0]*1000,slopeErr*1000)
		print 'RMS of residuals {} ns'.format(rmsResidual)
		
	f,(ax1,ax2,ax3)= plt.subplots(3,sharex=True,figsize=(8,11))
	f.suptitle('Delay calibration ' + datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S'))
	ax1.plot(tMatch,deltaMatch,ls='None',marker='.')
	ax1.plot(tAvMatches,avMatches)
	ax1.set_title('REF-CAL (filtered)')
	ax1.set_ylabel('REF-CAL (ns)')
	ax1.set_xlabel('MJD - '+str(firstMJD))
	
	ax2.plot(tMatch,calMatch,ls='None',marker='.')
	ax2.set_title('CAL (filtered)')
	ax2.set_ylabel('REFSYS (ns)')
	ax2.set_xlabel('MJD - '+str(firstMJD))
	
	ax3.plot(tMatch,refMatch,ls='None',marker='.')
	ax3.set_title('REF (filtered)')
	ax3.set_ylabel('REFSYS (ns)')
	ax3.set_xlabel('MJD - '+str(firstMJD))
	
	plt.savefig('ref.cal.ps',papertype='a4')
	
	if (not args.quiet):
		plt.show()

elif (MODE_TT == mode):
	if (not args.quiet):
		print ' Linear fit to data'
		print 'Offset (REF - CAL) at midpoint {} ns '.format(meanOffset)
		print 'ffe = {:.3e} +/- {:.3e}'.format(p[0]*1.0E-9/86400.0,slopeErr*1.0E-9/86400.0)
		
