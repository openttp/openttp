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
# Edit a CGGTTS file
#
#

import argparse
from datetime import datetime
import os
import re
import sys

# This is where cggttslib is installed
sys.path.append("/usr/local/lib/python2.7/site-packages")
import cggttslib

VERSION = "0.3.0"
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

# --------------------------------------------
# Main
# --------------------------------------------

# Sequence styles
Plain = 0
BIPM  = 1

parser = argparse.ArgumentParser(description='Edit CGGTTS files',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('infile',nargs='+',help='input file(s)',type=str)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--comments',help='set comment')
#parser.add_argument('--cabdly',help='set cable delay')
#parser.add_argument('--intdly',help='set internal delay')
#parser.add_argument('--refdly',help='set reference delay')
parser.add_argument('--nosequence',help='do not interpret (two) input file names as a sequence',action='store_true')
parser.add_argument('--nowarn',help='suppress warnings',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

infiles = []

# Construct the list of files to process
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

# Process the files	
for f in infiles:
	
	if (not os.path.isfile(f)):
		Warn(f + ' is missing')
		continue
	
	(hdr,warnings,checksumOK) = cggttslib.ReadHeader(f)
	if (hdr == None):
		Warn(warnings)
		continue
	if (not(warnings == '')): # header OK, but there was a warning
		Warn(warnings)
	
	# Do a few tests before proceeding
	#if (args.cabdly):
		#if (not 'cab dly' in hdr):
			#sys.stderr.write('CAB DLY is not defined in the header of ' + f + '\n')
			#sys.exit()

	#if (args.refdly):
		#if (not 'ref dly' in hdr):
			#sys.stderr.write('REF DLY is not defined in the header of ' + f + '\n')
			#sys.exit()
			
	# Print the new header
	hdrstring  = ''

	hdrout = 'CGGTTS     GENERIC DATA FORMAT VERSION = ' + hdr['version']
	print hdrout
	hdrstring += hdrout

	# If the header information changes, the REV DATE should be updated
	# if (args.cabdly or args.refdly or args.comments):
	if (args.comments):
		hdr['rev date'] = datetime.now().strftime('%Y-%m-%d')
		
	hdrout = 'REV DATE = ' + hdr['rev date']
	print hdrout
	hdrstring += hdrout

	hdrout =  hdr['rcvr']
	print hdrout
	hdrstring += hdrout

	hdrout = hdr['ch']
	print hdrout
	hdrstring += hdrout

	hdrout =  hdr['ims']
	print hdrout
	hdrstring += hdrout

	hdrout =  hdr['lab']
	print hdrout
	hdrstring += hdrout

	hdrout =  'X = ' + hdr['x'] + ' m'
	print hdrout
	hdrstring += hdrout

	hdrout =  'Y = ' + hdr['y'] + ' m'
	print hdrout
	hdrstring += hdrout

	hdrout =  'Z = ' + hdr['z'] + ' m'
	print hdrout
	hdrstring += hdrout

	hdrout =  hdr['frame']
	print hdrout
	hdrstring += hdrout

	if (args.comments):
		hdrout = 'COMMENTS = ' + args.comments
	else:
		hdrout = 'COMMENTS = ' + hdr['comments'] # note that this replaces a multi-line comment with a single line
	print hdrout
	hdrstring += hdrout

	if (hdr['version'] == '2E'):
		if ('tot dly' in hdr):
			hdrout = 'TOT DLY = ' + hdr['tot dly']
			print hdrout
			hdrstring += hdrout
		elif ('sys dly' in hdr):
			hdrout = 'SYS DLY = ' + hdr['sys dly']
			print hdrout
			hdrstring += hdrout
		elif ('int dly' in hdr):
			#if (args.intdly):
			#	hdr['int dly']= args.intdly
			hdrout = 'INT DLY = {:.1f} ns'.format(float(hdr['int dly']))
			print hdrout
			hdrstring += hdrout

	if ('cab dly' in hdr):
		#if (args.cabdly):
		#	hdr['cab dly']= args.cabdly
		hdrout = 'CAB DLY = {:.1f} ns'.format(float(hdr['cab dly']))
		print hdrout
		hdrstring += hdrout
		
	if ('ref dly' in hdr):
		#if (args.refdly):
		#	hdr['ref dly']= args.refdly
		hdrout = 'REF DLY = {:.1f} ns'.format(float(hdr['ref dly']))
		print hdrout
		hdrstring += hdrout
			
	hdrout = 'REF = ' + hdr['ref']
	print hdrout
	hdrstring += hdrout 

	hdrstring += 'CKSUM = '
	cksum = cggttslib.CheckSum(hdrstring)
	print 'CKSUM = {:02X}'.format(cksum)

	fin = open(f,'r')
	for l in fin:
		if (l.find('STTIME TRKL ELV AZTH') > 0): # lazy
			print
			print l.rstrip()
			break

	for l in fin:
		print l.rstrip()
