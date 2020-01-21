#!/usr/bin/python
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
# Quality check TIC files
#

import argparse
import os
import re
import sys

VERSION = "0.1.0"
AUTHORS = "Michael Wouters"

parser = argparse.ArgumentParser(description='Quality check TIC files')
parser.add_argument('infile',help='input file',type=str)
parser.add_argument('--verbose',help='verbose output',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

try:
	fin = open(args.infile,'r')
except:
	sys.stderr.write('Can\'t open '+args.infile)
	sys.exit()

tlast=-1
nDuplicates=0
dupCount = 0
nGaps=0
nMeas=0
minRdg=maxRdg=0
for l in fin:
	l=l.rstrip()
	if (l.find('#') >= 0):
		continue
	(tstamp,rdg)=l.split()
	match = re.match('^(\d{2}):(\d{2}):(\d{2})',tstamp)
	if (match):
		fRdg = float(rdg)
		if (nMeas == 0):
			minRdg = maxRdg = fRdg
		else:
			if (fRdg > maxRdg):
				maxRdg = fRdg
			if (fRdg < minRdg):
				minRdg = fRdg
		nMeas += 1
		t = int(match.group(1))*3600 + int(match.group(2))*60 + int(match.group(3))
		if (tlast==-1):
			tlast=t
		else:
			if (t==tlast):
				dupCount += 1
				if (args.verbose):
					sys.stdout.write('Duplicate at ' + tstamp +'\n')
				if (dupCount == 1):
					nDuplicates += 1
			else:
				dupCount=0
			if (t-tlast >= 2):
				nGaps += 1
				if (args.verbose):
					sys.stdout.write('Gap at ' + tstamp +'\n')
		tlast=t
		hh = int(t/3600)
		mm = int((t-hh*3600)/60)
		ss = t-hh*3600-mm*60
	else:
		sys.stderr.write('Bad input:'+l)
		
fin.close()
sys.stdout.write('Measurements=' + str(nMeas)+'\n')
sys.stdout.write('Duplicates=' + str(nDuplicates)+'\n')
sys.stdout.write('Gaps=' + str(nGaps)+'\n')
sys.stdout.write('Range=[' + str(minRdg)+ ',' + str(maxRdg) + ']\n')
