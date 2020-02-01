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

examples =  'Usage examples:\n'
examples += '1. Change the comment in the file GZAU0158.654 and replace the file\n'
examples += '   editcggtts.py --replace --comments=\'Antenna was moved\' ~/cggtts/GZAU0158.654\n'

parser = argparse.ArgumentParser(description='Edit CGGTTS files',
	formatter_class=argparse.RawDescriptionHelpFormatter, epilog = examples)

parser.add_argument('infile',nargs='+',help='input file(s)',type=str)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--comments',help='set comment')
#parser.add_argument('--cabdly',help='set cable delay')
#parser.add_argument('--intdly',help='set internal delay')
#parser.add_argument('--refdly',help='set reference delay')

group = parser.add_mutually_exclusive_group()
group.add_argument('--output','-o',help='output to file/directory',default='')
group.add_argument('--tmp',help='output is to file(s) with .tmp added to name',action='store_true')
group.add_argument('--replace','-r',help='replace edited file',action='store_true')

parser.add_argument('--nosequence',help='do not interpret (two) input file names as a sequence',action='store_true')
parser.add_argument('--nowarn',help='suppress warnings',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

infiles = []

if (2==len(args.infile)):
	if (args.nosequence ): 
		infiles = args.infile
	else:
		(infiles,warnings,badSequence) = cggttslib.MakeFileSequence(args.infile[0],args.infile[1])
		if (badSequence):
			Warn(warnings)
			sys.exit(0)
else:
	infiles = args.infile

# Process the files	
for finName in infiles:
	
	if (not os.path.isfile(finName)):
		Warn(finName + ' is missing')
		continue
	
	(hdr,warnings,checksumOK) = cggttslib.ReadHeader(finName)
	if (not hdr):
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
			
	foutName = finName + '.tmp'
	
	if (args.output):
		if (os.path.isdir(args.output)):# if it's a directory, write output there
			foutName = os.path.join(args.output,os.path.basename(finName))
		else:# otherwise, write to the specified file
			foutName = args.output
		
	if (args.replace or args.output or args.tmp):
		try:
			fout = open(foutName,'w')
		except:
			print('Unable to open ' + foutName)
			exit()
	else:
		fout = sys.stdout
	
	# Print the new header
	hdrstring  = ''

	hdrout = 'CGGTTS     GENERIC DATA FORMAT VERSION = ' + hdr['version']
	fout.write(hdrout + '\n')
	
	hdrstring += hdrout

	# If the header information changes, the REV DATE should be updated
	# if (args.cabdly or args.refdly or args.comments):
	if (args.comments):
		hdr['rev date'] = datetime.now().strftime('%Y-%m-%d')
		
	hdrout = 'REV DATE = ' + hdr['rev date']
	fout.write(hdrout + '\n')
	hdrstring += hdrout

	hdrout =  hdr['rcvr']
	fout.write(hdrout + '\n')
	hdrstring += hdrout

	hdrout = hdr['ch']
	fout.write(hdrout + '\n')
	hdrstring += hdrout

	hdrout =  hdr['ims']
	fout.write(hdrout + '\n')
	hdrstring += hdrout

	hdrout =  hdr['lab']
	fout.write(hdrout + '\n')
	hdrstring += hdrout

	hdrout =  'X = ' + hdr['x'] + ' m'
	fout.write(hdrout + '\n')
	hdrstring += hdrout

	hdrout =  'Y = ' + hdr['y'] + ' m'
	fout.write(hdrout + '\n')
	hdrstring += hdrout

	hdrout =  'Z = ' + hdr['z'] + ' m'
	fout.write(hdrout + '\n')
	hdrstring += hdrout

	hdrout =  hdr['frame']
	fout.write(hdrout + '\n')
	hdrstring += hdrout

	if (args.comments):
		hdrout = 'COMMENTS = ' + args.comments
	else:
		hdrout = 'COMMENTS = ' + hdr['comments'] # note that this replaces a multi-line comment with a single line
	fout.write(hdrout + '\n')
	hdrstring += hdrout

	if (hdr['version'] == '2E'):
		if ('tot dly' in hdr):
			hdrout = 'TOT DLY = ' + hdr['tot dly']
			fout.write(hdrout + '\n')
			hdrstring += hdrout
		elif ('sys dly' in hdr):
			hdrout = 'SYS DLY = ' + hdr['sys dly']
			fout.write(hdrout + '\n')
			hdrstring += hdrout
		elif ('int dly' in hdr):
			#if (args.intdly):
			#	hdr['int dly']= args.intdly
			hdrout = 'INT DLY = {:.1f} ns'.format(float(hdr['int dly']))
			fout.write(hdrout + '\n')
			hdrstring += hdrout

	if ('cab dly' in hdr):
		#if (args.cabdly):
		#	hdr['cab dly']= args.cabdly
		hdrout = 'CAB DLY = {:.1f} ns'.format(float(hdr['cab dly']))
		fout.write(hdrout + '\n')
		hdrstring += hdrout
		
	if ('ref dly' in hdr):
		#if (args.refdly):
		#	hdr['ref dly']= args.refdly
		hdrout = 'REF DLY = {:.1f} ns'.format(float(hdr['ref dly']))
		fout.write(hdrout + '\n')
		hdrstring += hdrout
			
	hdrout = 'REF = ' + hdr['ref']
	fout.write(hdrout + '\n')
	hdrstring += hdrout 

	hdrstring += 'CKSUM = '
	cksum = cggttslib.CheckSum(hdrstring) # compute the new checksum
	fout.write('CKSUM = {:02X}\n'.format(cksum))

	fin = open(finName,'r')
	for l in fin:
		if (l.find('STTIME TRKL ELV AZTH') > 0): # lazy
			fout.write('\n')
			fout.write(l)
			break

	for l in fin:
		fout.write(l)

	fin.close()
	fout.close()
	
	if (args.replace):
		os.remove(finName)
		os.rename(foutName,finName)
		
