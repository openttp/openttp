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
# Quality check on CGGTTS files
#

import argparse
import os
import re
import sys
# This is where cggttslib is installed
sys.path.append("/usr/local/lib/python2.7/site-packages")
import cggttslib

VERSION = "0.2.0"
AUTHORS = "Michael Wouters"

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
#
def CheckFile(fname):
	
	Debug('Checking ' + fname)
	
	header={}
	(header,warnings,checksumOK) = cggttslib.ReadHeader(fname)
	if (header == None):
		Warn(warnings)
		return ({},{})
	if (not(warnings == '')): # header OK, but there was a warning
		Warn(warnings)
		
	# OK to open the file
	fin = open(fname,'r')
	lineCount = 0
	for l in fin: # Re-read the header
		lineCount += 1
		l.rstrip()
		if (l.find('.1ns.1ps') >= 0):
			break
	
	# Read the main body
	
	stats={}
	stats['ntracks'] = 0
	stats['maxsats'] = -1
	stats['minsats'] = 999999
	stats['lowelv']  = 0
	stats['highdsg']  = 0
	stats['shorttracks'] = 0
	
	satcnt=0
	sttime=''
	first=True
	for l in fin:
		lineCount = lineCount +1
		l.strip()
		track = l.split()
		if (track[3] != sttime):
			if (first):
				first = False
			else:
				if (satcnt > stats['maxsats']):
					stats['maxsats']=satcnt
				if (satcnt < stats['minsats']): 
					stats['minsats']=satcnt
			satcnt=1
			sttime = track[3]
		else:
			satcnt=satcnt+1
		stats['ntracks']  = stats['ntracks']  + 1
		if (int(track[4]) < minTrackLength):
			stats['shorttracks'] = stats['shorttracks'] + 1
		if (int(track[5]) < minElevation):
			stats['lowelv'] = stats['lowelv'] + 1
		if (int(track[11]) > maxDSG):
			stats['highdsg'] = stats['highdsg'] + 1
			
	# Last block of tracks TESTED
	if (satcnt > stats['maxsats']):
		stats['maxsats']=satcnt
	if (satcnt < stats['minsats']): # not initialized yet
		stats['minsats']=satcnt
					
	fin.close()
	
	return (header,stats)

# ------------------------------------------
def CheckHeaderKey(prevFile,prevHeader,currFile,currHeader,key,msg):
	if ((key in prevHeader) and (key in currHeader)):
		if (prevHeader[key] != currHeader[key]):
			print '{}->{}: {}: ({}) -> ({})'.format(prevFile,currFile,msg,
				prevHeader[key], currHeader[key])
		
# ------------------------------------------
def CompareHeaders(prevFile,prevHeader,currFile,currHeader):
	
	CheckHeaderKey(prevFile,prevHeader,currFile,currHeader,'rev date','REV DATE changed')
	
	# Antenna co-ordinates
	if ( (prevHeader['x'] != currHeader['x']) or
		   (prevHeader['y'] != currHeader['y']) or
		   (prevHeader['z'] != currHeader['z'])):
		print '{}->{}:coords changed: ({},{},{}) -> ({},{},{})'.format(prevFile,currFile,
				prevHeader['x'],prevHeader['y'],prevHeader['z'],
				currHeader['x'],currHeader['y'],currHeader['z'])
	
	# Delays
	CheckHeaderKey(prevFile,prevHeader,currFile,currHeader,'int dly','INT DLY changed') # INT DLY
	CheckHeaderKey(prevFile,prevHeader,currFile,currHeader,'cab dly','CAB DLY changed') # CAB DLY
	CheckHeaderKey(prevFile,prevHeader,currFile,currHeader,'ref dly','REF DLY changed') # REF DLY
	CheckHeaderKey(prevFile,prevHeader,currFile,currHeader,'sys dly','SYS DLY changed') # SYS DLY
	CheckHeaderKey(prevFile,prevHeader,currFile,currHeader,'tot dly','TOT DLY changed') # TOT DLY

	CheckHeaderKey(prevFile,prevHeader,currFile,currHeader,'ref','REF changed')
	
# ------------------------------------------
def PrettyPrintStats(fname,stats):
	print '{:12} {:>6} {:>6} {:>6} {:>6} {:>6} {:>6}'.format(fname,stats['ntracks'],stats['shorttracks'],
		stats['minsats'],stats['maxsats'],stats['highdsg'],stats['lowelv'])
	
# ------------------------------------------
# Main

# Sequence styles
Plain = 0
BIPM  = 1

examples =  'Usage examples\n'
examples += '1. Check all files between MJDs 58654 and 58660\n'
examples += '  cggttsqc.py ~/cggtts/GZAU0158.654   ~/cggtts/GZAU0158.660\n'

parser = argparse.ArgumentParser(description='Quality check CGGTTS files',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

parser.add_argument('infile',nargs='+',help='input file',type=str)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--nowarn',help='suppress warnings',action='store_true')
parser.add_argument('--dsg',help='upper limit for DSG, in ns',default=20)
parser.add_argument('--elevation',help='lowerlimit for elevation, in degrees',default=10)
parser.add_argument('--tracklength',help='lower limit for track length, in s',default=780)
parser.add_argument('--checkheader',help='check for header changes',action='store_true')
parser.add_argument('--nosequence',help='do not interpret (two) input files as a sequence',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS);

args = parser.parse_args()

debug = args.debug

minElevation = int(args.elevation) * 10 # in units of 0.1 ns
minTrackLength = int(args.tracklength)
maxDSG = int(args.dsg) * 10 # in units of 0.1 ns

infiles = []

if (2==len(args.infile)):
	if (args.nosequence ): 
		infiles = args.infile
	else:
		# Determine whether the file names determine a sequence
		# For simplicity, any filename in standard BIPM CGTTS format or NNNNN.***
		# First, need to strip the path
		(path1,file1) = os.path.split(args.infile[0])
		(path2,file2) = os.path.split(args.infile[1])
		if (path1 != path2):
			sys.stderr.write('The paths have to be the same for a sequence\n')
			exit()
		# Now try to guess the sequence
		# The extension has to be the same
		(file1root,file1ext) = os.path.splitext(file1)
		(file2root,file2ext) = os.path.splitext(file2)
		
		isSeq=False
		# First, test for 'plain' file names
		Debug('Sequence {} -> {}'.format(file1,file2))
		match1 = re.match('^(\d+)$',file1root)
		match2 = re.match('^(\d+)$',file2root)
		if (match1 and match2):
			if (file1ext != file2ext):
				sys.stderr.write('The file extensions have to be the same for a sequence\n')
				exit()
			isSeq = True
			sequenceStyle = Plain
			start = int(match1.group(1)) # NOTE that this won't work with names padded with leading zeroes
			stop  = int(match2.group(1))
			if (start > stop):
				tmp = stop
				stop = start
				start = tmp
			Debug('Numbered file sequence: {}->{}'.format(start,stop))
		if (not isSeq): # try for BIPM style
			match1 = re.match('^([G|R|E|C|J][S|M|Z][A-Za-z]{2}[0-9_]{2})(\d{2})\.(\d{3})$',file1)
			match2 = re.match('^([G|R|E|C|J][S|M|Z][A-Za-z]{2}[0-9_]{2})(\d{2})\.(\d{3})$',file2)
			if (match1 and match2):
				stub1 = match1.group(1)
				stub2 = match2.group(1)
				if (stub1 == stub2):
					start  = int(match1.group(2) + match1.group(3))
					stop   = int(match2.group(2) + match2.group(3))
					if (start > stop):
						tmp = stop
						stop = start
						start = tmp
					isSeq = True
					sequenceStyle = BIPM
					Debug('BIPM file sequence: {} MJD {}->{}'.format(stub1,start,stop))
				else:
					isSeq = false
			else:
				isSeq = False
		# bail out if the sequence is not recognzied
		if (not isSeq):
			sys.stderr.write('The filenames do not form a recognised sequence\n')
			sys.exit()
		# Make a list of files
		for m in range(start,stop+1):
			# Construct the file name
			if (Plain == sequenceStyle):
				fname = str(m) + file1ext
				infiles.append(os.path.join(path1,fname))
			elif (BIPM == sequenceStyle):
				dd  = int(m/1000)
				ddd = int(m - dd*1000)
				fname = stub1 + '{:02d}.{:03d}'.format(dd,ddd)
				infiles.append(os.path.join(path1,fname))
else:
	infiles = args.infile

currHeader = {}
stats = {}
prevHeader = {}
prevFile = ''

if (not args.checkheader):
	print '{:12} {:6} {:>6} {:>6} {:>6} {:>6} {:>6}'.format('File','Tracks','Short','min SV','max SV','DSG','elv')

for f in infiles:
	(currHeader,stats) = CheckFile(f)
	if (currHeader and stats): # may not be readable
		currFile = os.path.basename(f)
		if (not args.checkheader):
			PrettyPrintStats(currFile,stats)
		if (prevHeader and args.checkheader):
			CompareHeaders(prevFile,prevHeader,currFile,currHeader)
		prevHeader = currHeader
		prevFile = currFile
