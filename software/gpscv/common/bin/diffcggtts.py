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
# Differences pairs of CGGTTS files
# 
# Example
# diffcggtts.py ref cal 58418 58419
# 
# diffcggtts.py --debug --refext gps.C1.30s.dat --calext gps.C1.30s.dat  ./ ../ottp1 58418 58419
#

import argparse
import os
import re
import sys

VERSION = "0.1"
AUTHORS = "Michael Wouters"

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
def ReadCGGTTS(fname,mjd):
	d=[]
	Debug('Reading ' + fname)
	try:
		fin = open(fname,'r')
	except:
		Warn('Unable to open ' + fname)
		# not fatal
		return []
	
	l = fin.readline().rstrip()
	match = re.match('^\s+RAW CLOCK RESULTS',l)
	if match:
		Debug('Raw clock results')
	else:
		Warn('Unknown format')
		return []
	
	# Eat the header
	while True:
		l = fin.readline()
		if not l:
			Warn('Bad format')
			return []
		match = re.match('^\s+hhmmss',l)
		if match:
			break
	nextday =0
	while True:
		l = fin.readline().rstrip()
		if l:
			fields = l.split()
			theMJD = int(fields[1])
			hh = int(fields[2][0:2])
			mm = int(fields[2][2:4])
			ss = int(fields[2][4:6])
			tt = hh*3600+mm*60+ss
			if ((tt < 86399 and theMJD == mjd) or args.keepall):
				d.append([fields[0],int(fields[1]),tt,int(fields[3]),int(fields[4]),int(fields[5]),int(fields[6]),int(fields[7]),int(fields[8]),int(fields[9]),int(fields[10]),fields[11]])
			else:
				nextday += 1
		else:
			break
		
	fin.close()
	
	Debug(str(nextday) + ' tracks after end of the day removed')
	
	return d
	
# ------------------------------------------
def CheckSum(l):
	cksum = 0
	for c in l:
		cksum = cksum + ord(c)
	return cksum % 256

parser = argparse.ArgumentParser(description='Match and difference CGGTTS files')
parser.add_argument('refDir',help='reference CGGTTS directory')
parser.add_argument('calDir',help='calibration CGGTTS directory')
parser.add_argument('firstMJD',help='first mjd')
parser.add_argument('lastMJD',help='last mjd');
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--nowarn',help='suppress warnings',action='store_true')
parser.add_argument('--refext',help='file extension for reference receiver (default = cctf)')
parser.add_argument('--calext',help='file extension for calibration receiver (default = cctf)')
parser.add_argument('--keepall',help='keep all tracks ,including those after the end of the GPS day',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

debug = args.debug

if (args.version):
	ShowVersion()
	exit()

refExt = 'cctf'
calExt = 'cctf'

if (args.refext):
	refExt = args.refext

if (args.calext):
	calExt = args.calext
	
firstMJD = int(args.firstMJD)
lastMJD  = int(args.lastMJD)

allref=[]
allcal=[]
for mjd in range(firstMJD,lastMJD+1):
	fref = args.refDir + '/' + str(mjd) + '.' + refExt
	d=ReadCGGTTS(fref,mjd)
	allref = allref + d

	fref = args.calDir + '/' + str(mjd) + '.' + calExt
	d=ReadCGGTTS(fref,mjd)
	allcal = allcal + d
	
# Now match tracks

matches = []
reflen = len(allref)
callen = len(allcal)
iref=0
jcal=0

PRN=0
MJD=1
STTIME=2
REFSYS=6

#print reflen,callen

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
			break # stop searching - need to move pointer2
		elif ((mjd1==mjd2)and (st1 == st2)):
			# times are matched so search for SV
			while ((mjd1 == mjd2) and (st1 == st2) and (jcal<callen)):
	
				mjd2=allcal[jcal][MJD]
				st2 =allcal[jcal][STTIME]
				prn2=allcal[jcal][PRN]
				if (prn1 == prn2):
					break
				jcal += 1
			if ((mjd1 == mjd2) and (st1 == st2) and (prn1 == prn2)):
				# match!
				print mjd1,st1,prn1,allref[iref][REFSYS],allcal[jcal][REFSYS]
				matches.append(allref[iref]+allcal[jcal])
				jcal += 1
				break
			else:
				break
		else:
			jcal += 1
			
	iref +=1
	
#print len(matches)
#print matches
avmatches=[]

lenmatch=len(matches)
mjd1=matches[1][MJD]
st1=matches[1][STTIME]
av = 0
nsv = 1
imatch=2
while imatch < lenmatch:
	mjd2 = matches[imatch][MJD]
	st2 = matches[imatch][STTIME]
	if (mjd1==mjd2 and st1==st2):
		nsv +=1
	else:
		print nsv
		mjd1 = mjd2
		st1  = st2
		nsv=1
	imatch += 1
	
	