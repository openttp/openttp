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
# Doesn't do much at present
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
def ReadCGGTTS(fname):
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
		
	while True:
		l = fin.readline().rstrip()
		if l:
			fields = l.split()
			hh = int(fields[2][0:2])
			mm = int(fields[2][2:4])
			ss = int(fields[2][4:6])
			d.append([fields[0],fields[1],hh*3600+mm*60+ss,fields[3],fields[4],fields[5],fields[6],fields[7],fields[8],fields[9],fields[10],fields[11]])
		else:
			break
	fin.close()
	
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
	d=ReadCGGTTS(fref)
	allref = allref + d

# Now match tracks


	