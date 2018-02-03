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
# Manipulates RINEX navigation files
# In particular it can:
#   remove ephemerides with URA above a specified limit
#
# If an output file is not specified, then output is written to stdout
#

import argparse
import os
import sys

VERSION = "0.1"
AUTHORS = "Michael Wouters"

# ------------------------------------------
def ShowVersion():
	print  os.path.basename(sys.argv[0])," ",VERSION
	print "Written by",AUTHORS
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ------------------------------------------
# Main

parser = argparse.ArgumentParser(description='Edit a RINEX navigation file')
parser.add_argument('infile',help='input file',type=str)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
group = parser.add_mutually_exclusive_group()
group.add_argument('--output','-o',help='output to file/directory',default='')
group.add_argument('--replace','-r',help='replace edited file',action='store_true')
parser.add_argument('--ura','-u',help='remove entries with URA greater than this',default=0)
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

debug = args.debug

if (args.version):
	ShowVersion()
	exit()

finName = args.infile
foutName = finName + '.tmp'

if (args.output):
	if (os.path.isdir(args.output)):# if it's a directory, write output there
		foutName = os.path.join(args.output,os.path.basename(finName))
	else:# otherwise, write to the specified file
		foutName = args.output

uralim = float(args.ura)

try:
	fin = open(finName,'r')
except:
	print('Unable to open '+finName)
	exit()

if (args.replace or args.output):
	try:
		fout = open(foutName,'w')
	except:
		print('Unable to open ' + foutName)
		exit()
else:
	fout = sys.stdout

# Read the header
for l in fin:
	fout.write(l)
	if (l.find('PGM / RUN BY / DATE',60)>0):
		comment = '{:60}{:20}\n'.format('Edited with ' + os.path.basename(sys.argv[0]),'COMMENT')
		fout.write(comment)
		
	if (l.find('RINEX VERSION',60)>0):
		ver = l[:9]
		try:
			ver = float(l[:8])
			Debug('RINEX version is {}'.format(ver))
			# At the moment we are OK with any version
		except:
			sys.stderr.write('Unable to determine the RINEX version - continuing anyway\n')
			
	if (l.find('END OF HEADER',60)>0):
		Debug("Read header")
		break

# Read records
recLine=0
nrecs=0
output=True
rec=[]
ndropped=0
for l in fin:
	tmp = l
	tmp.strip()
	if (len(tmp)==0):
		continue
	recLine=recLine+1
	if (recLine < 9):
		rec.append(l)
	if (recLine == 7):
		if (uralim > 0):
			tmp = l
			tmp=tmp.replace("D","E")
			ura = float(tmp[:22])
			if (ura > uralim):
				output=False
				ndropped = ndropped + 1
	elif (recLine == 9):
		if (output):
			nrecs = nrecs+1
			for r in rec:
				fout.write(r)
		recLine=1
		output=True
		rec=[]
		rec.append(l)
		
# Output the last one
if (output):
	nrecs = nrecs + 1
	for r in rec:
		fout.write(r)

fin.close()
fout.close()

if (args.replace):
	os.remove(finName)
	os.rename(foutName,finName)
	
Debug("Number of output records = " + str(nrecs))
Debug("Dropped " + str(ndropped))
