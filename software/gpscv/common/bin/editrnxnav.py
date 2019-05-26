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
#   replace fields in the header (this is FRAGILE! V3 only)
# If an output file is not specified, then output is written to stdout
#

import argparse
import os
import sys

VERSION = "0.1.3"
AUTHORS = "Michael Wouters"


# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ------------------------------------------
def ErrorExit(msg):
	print msg
	sys.exit(0)
	
# ------------------------------------------
# Main

parser = argparse.ArgumentParser(description='')

parser = argparse.ArgumentParser(description='Edit a RINEX navigation file',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('infile',help='input file',type=str)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
group = parser.add_mutually_exclusive_group()
group.add_argument('--output','-o',help='output to file/directory',default='')
group.add_argument('--replace','-r',help='replace edited file',action='store_true')
parser.add_argument('--ura','-u',help='remove entries with URA greater than this',default=0)
parser.add_argument('--leap','-l',help='add the leap second information from another navigation file',default='')
parser.add_argument('--ionosphere','-i',help='add the ionosphere information from another navigation file',default='')
parser.add_argument('--timesys','-t',help='add the time system information from another navigation file',default='')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

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
	ErrorExit('Unable to open '+ finName)
		
if (args.replace or args.output):
	try:
		fout = open(foutName,'w')
	except:
		ErrorExit('Unable to open ' + foutName)
else:
	fout = sys.stdout

leapinfo=''

if (args.leap):
	try:
		fref = open(args.leap,'r')
	except:
		ErrorExit('Unable to open ' + args.leap)
	for l in fref:
		if (l.find('LEAP SECONDS')>0):
			leapinfo=l
			leapinfo.rstrip('\r\n')
		if (l.find('END OF HEADER',60)>0):
			Debug("Read header for" + args.leap)
			break
	fref.close()

ionoinfo=''
if (args.ionosphere):
	try:
		fref = open(args.ionosphere,'r')
	except:
		print('Unable to open '+args.ionosphere)
		exit()
	for l in fref:
		if (l.find('IONOSPHERIC CORR')>0):
			ionoinfo=ionoinfo+l
		if (l.find('END OF HEADER',60)>0):
			Debug("Read header for" + args.ionosphere)
			break
	fref.close()

timesysinfo=''
if (args.timesys):
	try:
		fref = open(args.timesys,'r')
	except:
		print('Unable to open '+args.timesys)
		exit()
	for l in fref:
		if (l.find('TIME SYSTEM CORR')>0):
			timesysinfo=timesysinfo+l
		if (l.find('END OF HEADER',60)>0):
			Debug("Read header for" + args.timesys)
			break
	fref.close()
Debug(timesysinfo)

# Read the header

for l in fin:
	
	if (l.find('RINEX VERSION',60)>0):
		ver = l[:9]
		try:
			ver = float(l[:8])
			Debug('RINEX version is {}'.format(ver))
			# At the moment we are OK with any version
		except:
			sys.stderr.write('Unable to determine the RINEX version - continuing anyway\n')
		fout.write(l)
		
	if (l.find('PGM / RUN BY / DATE',60)>0):
		fout.write(l)
		comment = '{:60}{:20}\n'.format('Edited with ' + os.path.basename(sys.argv[0]),'COMMENT')
		fout.write(comment)
		
	if (l.find('IONOSPHERIC CORR',60)>0):
		if (args.ionosphere):
			pass # do nothing - eat the (multiline) record
		else:
			fout.write(l)
		
	if (l.find('TIME SYSTEM CORR',60)>0):
		if (args.ionosphere):
			fout.write(ionoinfo)
			args.ionosphere=''
		if (args.timesys):
			pass # do nothing - eat the (multiline) record
		else:
			fout.write(l)
		
	if (l.find('LEAP SECONDS',60)>0):
		if (args.ionosphere):
			fout.write(ionoinfo)
			args.ionosphere=''
		if (args.timesys):
			fout.write(timesysinfo)
			args.timesys=''
		if (args.leap):
			fout.write(leapinfo) # only one line, so write it now
			args.leap='' # done so flag it
		else:
			fout.write(l)
		
	if (l.find('END OF HEADER',60)>0):
		if (args.ionosphere):
			fout.write(ionoinfo)
		if (args.timesys):
			fout.write(timesysinfo)
		if (args.leap):
			fout.write(leapinfo)
		Debug("Read header")
		fout.write(l)
		break

# Read the body
if (args.ura):
	
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
				if ((ura > uralim) or (ura == 0)):
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
	Debug("Number of output records = " + str(nrecs))
	Debug("Dropped " + str(ndropped))
else:
	for l in fin:
		fout.write(l)

fin.close()
fout.close()

if (args.replace):
	os.remove(finName)
	os.rename(foutName,finName)
	

