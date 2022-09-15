#!/usr/bin/python
#

#
# The MIT License (MIT)
#
# Copyright (c) 2020 Michael J. Wouters
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
# Quality check ublox9 rx  files
#


# 2022-09-16 ELM Updated for Python3 (RIP Python 2.7). Bumped version to 0.0.2

import argparse
import os
import re
import sys

VERSION = "0.0.2"
AUTHORS = "Michael Wouters"

parser = argparse.ArgumentParser(description='Quality check ublox rx files')
parser.add_argument('infile',help='input file',type=str)
parser.add_argument('--verbose',help='verbose output',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

try:
	fin = open(args.infile,'r')
except:
	sys.stderr.write('Can\'t open '+args.infile)
	sys.exit()

nbad = 0
tfirst = -1
tlast  = -1
tprev  = -1

nraw = 0
ngaps = 0
gotRaw = False
nrawmissed = 0

for l in fin:
	l=l.rstrip()
	if (l.find('#') >= 0 or l.find('@') >= 0):
		continue
	match = re.match('^(\w{2})(\w{2}) (\d{2}):(\d{2}):(\d{2})',l)
	if (match):
		tnow = int(match.group(3))*3600 + int(match.group(4))*60 + int(match.group(5))
		if (tfirst == -1):
			tfirst = tnow
			tprev  = tnow
		if (tnow > tlast):
			tlast = tnow
		if (tnow > tprev):
			if (not gotRaw):
				print ('Missed raw before ',match.group(3),match.group(4),match.group(5))
				nrawmissed += 1
			gotRaw = False
			
		if (tnow - tprev) >= 2:
			if (args.verbose):
				print ('Gap ending ',match.group(3),match.group(4),match.group(5), ' length', tnow - tprev - 1)
			ngaps += 1
			
		tprev = tnow
		
		classid = match.group(1)
		msgid   = match.group(2)
		if (classid == '02' and msgid == '15'):
			nraw = nraw + 1
			gotRaw = True
	else:
		sys.stderr.write('Bad input:'+l+'\n')
		nbad += 1

print ('First of day ',tfirst)
print ('Last of day  ',tlast)
print ('Last time ',tnow)
print ('Expected      ',tlast - tfirst + 1) # yes i know there's a bug
print ('Gaps ',ngaps)
print ('Number of raw measurements ', nraw)
print ('Missed raw ', nrawmissed)
print ('Bad input lines = ',nbad)

fin.close()
