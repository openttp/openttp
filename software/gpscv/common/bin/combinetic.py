#!/usr/bin/python

#
# The MIT License (MIT)
#
# Copyright (c) 2019s Michael J. Wouters
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
# Matches timestamps in two TIC files and combines measurements
# Output is to stdout

import argparse
import sys
import os

VERSION = "0.1.0"
AUTHORS = "Michael Wouters"

parser = argparse.ArgumentParser(description='Match and combine TIC measurements')

# required arguments
parser.add_argument('file1',help='first TIC file')
parser.add_argument('file2',help='second TIC file')
parser.add_argument('--add',help='add TIC measurements',action='store_true')
parser.add_argument('--subtract',help='subtract (1-2) TIC measurements',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

try:
	fin1 = open(args.file1,'r')
except:
	sys.stderr.write('Unable to open ' + f1)
	exit()

try:
	fin2 = open(args.file2,'r')
except:
	sys.stderr.write('Unable to open ' + f2)
	exit()
	
d1=[]

for l in fin1:
	l = l.rstrip()
	f = l.split()
	hh = int(f[0][0:2])
	mm = int(f[0][3:5])
	ss = int(f[0][6:8])
	tod = hh*3600 + mm*60 + ss
	d1.append([tod,f[0],f[1]])

fin1.close()

if (len(d1)==0):
	sys.stderr.write(args.file1 + 'is empty\n')
	exit();
	
d2=[]

for l in fin2:
	f = l.split()
	hh = int(f[0][0:2])
	mm = int(f[0][3:5])
	ss = int(f[0][6:8])
	tod = hh*3600 + mm*60 + ss
	d2.append([tod,f[0],f[1]])

fin2.close()

if (len(d2)==0):
	sys.stderr.write(args.file2 + 'is empty\n')
	exit();

i1 = 0	
i2 = 0

while (i1 < len(d1) and i2 < len(d2)):
	if (d2[i2][0] == d1[i1][0]): # it's a match
		if (args.add):
			print d1[i1][1], float(d1[i1][2]) + float(d2[i2][2])
		else:
			print d1[i1][1], float(d1[i1][2]) - float(d2[i2][2])
		i1 = i1 + 1
		i2 = i2 + 1
	elif (d1[i1][0] > d2[i2][0] ): # d1 after d2, advance d2
		i2 = i2 + 1
	elif (d2[i2][0] > d1[i1][0]):  # d1 before d2, advance d1
		i1 = i1 + 1
		





	
