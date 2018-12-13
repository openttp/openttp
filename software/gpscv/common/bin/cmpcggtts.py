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

import argparse
import numpy as np
import os
import re
import sys

VERSION = "0.2"
AUTHORS = "Michael Wouters"

# cggtts versions

CGGTTS_RAW = 0
CGGTTS_V1  = 1
CGGTTS_V2  = 2
CGGTTS_V2E = 3
CGGTTS_UNKNOWN = 999


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
#CK=17 # for V1, single frequency
MSIO=17
SMSI=18 
ISG=19 # only for dual frequency
FRC=21 

MIN_TRACK_LENGTH=750
DSG_MAX=200
IONO_CORR=1 # this means the ionospheric correction is removed!
ELV_MASK=0

USE_GPSCV = 1
USE_AIV = 2
# ------------------------------------------
def ShowVersion():
	print  os.path.basename(sys.argv[0]),' ',VERSION
	print 'Written by',AUTHORS
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ------------------------------------------
def Warn(msg):
	if (not args.nowarn):
		sys.stderr.write(msg+'\n')
	return

# ------------------------------------------
def SetDataColumns(ver,isdf):
	global PRN,MJD,STTIME,ELV,AZTH,REFSV,REFSYS,IOE,MDTR,MDIO,MSIO,FRC
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
		pass
	elif (CGGTTS_V2E == ver):
		if (isdf):
			FRC=21
		else:
			FRC=19
			
# ------------------------------------------
def ReadCGGTTS(fname,mjd):
	d=[]
	Debug('--> Reading ' + fname)
	try:
		fin = open(fname,'r')
	except:
		Warn('Unable to open ' + fname)
		# not fatal
		return ([],[],[])
	
	ver = CGGTTS_UNKNOWN
	l = fin.readline().rstrip()
	if (re.match('\s+RAW CLOCK RESULTS',l)):
		Debug('Raw clock results')
		ver=CGGTTS_RAW
	elif (re.search('GGTTS GPS DATA FORMAT VERSION = 01',l)):	
		Debug('V1')
		ver=CGGTTS_V1
	elif (re.search('GENERIC DATA FORMAT VERSION = 2E',l)):
		Debug('V2E')
		ver=CGGTTS_V2E
	else:
		Warn('Unknown format - the header is incorrectly formatted')
		return ([],[],[])
	
	dualFrequency = False
	# Eat the header
	while True:
		l = fin.readline()
		if not l:
			Warn('Bad format')
			return ([],[],[])
		if (re.search('^INT DLY',l)):
			pass
		if (re.search('STTIME TRKL ELV AZTH',l)):
			if (re.search('MSIO',l)):
				dualFrequency=True
				Debug('Dual frequency data')
			continue
		match = re.match('\s+hhmmss',l)
		if match:
			break
		
	SetDataColumns(ver,dualFrequency)
	
	nextday =0
	stats=[]
	delays=[]
	nLowElevation=0
	nHighDSG=0
	nShortTracks=0
	while True:
		l = fin.readline().rstrip()
		if l:
			fields = l.split()
			theMJD = int(fields[MJD])
			hh = int(fields[STTIME][0:2])
			mm = int(fields[STTIME][2:4])
			ss = int(fields[STTIME][4:6])
			tt = hh*3600+mm*60+ss
			dsg=0
			trklen=999
			reject=False
			if (not(CGGTTS_RAW == ver)): # DSG not defined for RAW
				dsg = int(fields[DSG])/10
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
			if (reject):
				continue
			frc = 0
			if (ver == CGGTTS_V2 or ver == CGGTTS_V2E):
				frc=fields[FRC]
			msio = 0
			if (dualFrequency):
				msio = float(fields[MSIO])/10.0
			if ((tt < 86399 and theMJD == mjd) or args.keepall):
				d.append([fields[PRN],int(fields[MJD]),tt,trklen,elv,float(fields[AZTH])/10.0,float(fields[REFSV])/10.0,float(fields[REFSYS])/10.0,
					dsg,int(fields[IOE]),float(fields[MDIO])/10.0,msio,frc])
			else:
				nextday += 1
		else:
			break
		
	fin.close()
	stats.append([nLowElevation,nHighDSG,nShortTracks])
	delays.append([])
	
	Debug(str(nextday) + ' tracks after end of the day removed')
	Debug('low elevation = ' + str(nLowElevation))
	Debug('high DSG = ' + str(nHighDSG))
	Debug('short tracks = ' + str(nShortTracks))
	return (d,stats,delays)
	
# ------------------------------------------
def CheckSum(l):
	cksum = 0
	for c in l:
		cksum = cksum + ord(c)
	return cksum % 256

parser = argparse.ArgumentParser(description='Match and difference CGGTTS files')

# required arguments
parser.add_argument('refDir',help='reference CGGTTS directory')
parser.add_argument('calDir',help='calibration CGGTTS directory')
parser.add_argument('firstMJD',help='first mjd')
parser.add_argument('lastMJD',help='last mjd');

# filtering
parser.add_argument('--elevationmask',help='elevation mask (in degrees, default 0)')
parser.add_argument('--mintracklength',help='minimum track length (in s, default 750')
parser.add_argument('--maxdsg',help='maximum DSG (in ns, default ')

# analysis mode
parser.add_argument('--cv',help='compare in common view (default)',action='store_true')
parser.add_argument('--aiv',help='compare in all-in-view',action='store_true')

parser.add_argument('--refext',help='file extension for reference receiver (default = cctf)')
parser.add_argument('--calext',help='file extension for calibration receiver (default = cctf)')

parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--nowarn',help='suppress warnings',action='store_true')
parser.add_argument('--keepall',help='keep all tracks ,including those after the end of the GPS day',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

debug = args.debug

if (args.version):
	ShowVersion()
	exit()

refExt = 'cctf'
calExt = 'cctf'
elevationMask=ELV_MASK
minTrackLength = MIN_TRACK_LENGTH
maxDSG=DSG_MAX
cmpMethod=USE_GPSCV

if (args.refext):
	refExt = args.refext

if (args.calext):
	calExt = args.calext

if (args.elevationmask):
	elevationMask = float(args.elevationmask)

if (args.maxdsg):
	maxDSG = float(args.maxdsg)

if (args.mintracklength):
	minTrackLength = args.mintracklength

if (args.cv):
	cmpMethod = USE_GPSCV

if (args.aiv):
	cmpMethod = USE_AIVparser.add_argument('--aiv',help='compare in all-in-view',action='store_true')
	
firstMJD = int(args.firstMJD)
lastMJD  = int(args.lastMJD)

allref=[]
allcal=[]

for mjd in range(firstMJD,lastMJD+1):
	fref = args.refDir + '/' + str(mjd) + '.' + refExt
	(d,stats,delays)=ReadCGGTTS(fref,mjd)
	allref = allref + d

for mjd in range(firstMJD,lastMJD+1):
	fcal= args.calDir + '/' + str(mjd) + '.' + calExt
	(d,stats,delays)=ReadCGGTTS(fcal,mjd)
	allcal = allcal + d


reflen = len(allref)
callen = len(allcal)
if (reflen==0 or callen == 0):
	print('Insufficient data')
	exit()

# Can redefine the indices now
PRN=0
MJD=1
STTIME=2
REFSYS=7

avmatches=[]

foutName = 'ref.cal.av.matches.txt'
try:
	favmatches = open(foutName,'w')
except:
	print('Unable to open ' + foutName)
	exit()

foutName = 'ref.cal.matches.txt'
try:
	fmatches = open(foutName,'w')
except:
	print('Unable to open ' + foutName)
	exit()
	
if (cmpMethod == USE_GPSCV):
	matches = []
	iref=0
	jcal=0

	while iref < reflen:
		
		mjd1=allref[iref][MJD]
		prn1=allref[iref][PRN]
		st1 =allref[iref][STTIME]
		
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
					print mjd1,mjd2,st1,st2,prn1,prn2,jtmp
					if (prn1 == prn2):
						break
					jtmp += 1
				if ((mjd1 == mjd2) and (st1 == st2) and (prn1 == prn2)):
					# match!
					fmatches.write('{} {} {} {} {}\n'.format(mjd1,st1,prn1,allref[iref][REFSYS],allcal[jtmp][REFSYS]))
					matches.append(allref[iref]+allcal[jtmp])
					break
				else:
					break
			else:
				jcal += 1
				
		iref +=1
		
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
			avref  += matches[imatch][REFSYS] 
			avcal  += matches[imatch][ncols + REFSYS] 
		else:
			favmatches.write('{} {} {} {} {} {}\n'.format(mjd1,st1,avref/nsv,avcal/nsv,(avref-avcal)/nsv,nsv))
			avmatches.append([mjd1,st1,(avref-avcal)/nsv])
			mjd1 = mjd2
			st1  = st2
			nsv=1
			avref = matches[imatch][REFSYS]
			avcal = matches[imatch][ncols + REFSYS]
		imatch += 1
	# last one
	favmatches.write('{} {} {} {} {} {}\n'.format(mjd1,st1,avref/nsv,avcal/nsv,(avref-avcal)/nsv,nsv))
	avmatches.append([mjd1,st1,(avref-avcal)/nsv])

elif (cmpMethod == USE_AIV):
	# Opposite order here
	# First average at each time
	# and then match times
	pass

fmatches.close()
favmatches.close()

# Now for some statistics
# 
