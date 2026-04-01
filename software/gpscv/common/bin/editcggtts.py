#!/usr/bin/python3
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
sys.path.append("/usr/local/lib/python3.6/site-packages") # Ubuntu 18.04
sys.path.append("/usr/local/lib/python3.8/site-packages") # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.12/site-packages") # Ubuntu 20.04
import cggttslib

VERSION = "0.5.0"
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
def EditLine(key,fin,newValue,newhdr,fout):
	l = fin.readline().rstrip()
	if key in newhdr: # this takes precedence
		hdrout =	newhdr[key]
	else:
		if newValue:
			fields = l.split('=',1)
			hdrout = fields[0] + ' = ' + newValue
		else:
			hdrout = l
	fout.write(hdrout+'\n')
	return hdrout
	
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
parser.add_argument('--header',help='replace header fields with new header fields as given in a file') # FIXME should not be useable with other edits
parser.add_argument('--keeprev',help="don't update REV DATE",action='store_true')

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

if args.header:
	try:
		newhdr = {}
		with open(args.header,'r') as fin:
			for l in fin:
				newhdr[l.split('=')[0].strip().lower()] = l.rstrip()
	except:
		sys.exit(f'Unable to open {args.header}')

# Process the files	

for finName in infiles:
	
	if (not os.path.isfile(finName)):
		Warn(finName + ' is missing')
		continue
	
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

	try:
		fin = open(finName,'r')
	except:
		continue

	# First line is CGGTTS version
	l = fin.readline()
	fout.write(l)
	hdrstring += l.rstrip()
	
	# If the header information changes, the REV DATE should be updated
	if args.keeprev: # unless we say not to
		revDate = None
	else:
		revDate = datetime.now().strftime('%Y-%m-%d')
	hdrstring += EditLine('rev date',fin,revDate,newhdr,fout)
	
	newValue = None # placeholder for the moment
	# This section of the header is well-defined
	hdrstring += EditLine('rcvr',fin,newValue,newhdr,fout)
	hdrstring += EditLine('ch',fin,newValue,newhdr,fout)
	hdrstring += EditLine('ims',fin,newValue,newhdr,fout)
	hdrstring += EditLine('lab',fin,newValue,newhdr,fout)
	hdrstring += EditLine('x',fin,newValue,newhdr,fout)
	hdrstring += EditLine('y',fin,newValue,newhdr,fout)
	hdrstring += EditLine('z',fin,newValue,newhdr,fout)
	hdrstring += EditLine('frame',fin,newValue,newhdr,fout)

	l = fin.readline().rstrip() # comments
	fout.write(l + '\n')
	hdrstring += l
	 # TO DO some files incorrectly use multiple comment lines
	 
	# The section of the header that describes delays 
	# has a variant form depending on the way delays are specified
	# Normally, you wouldn't edit the delays but perhaps you want to anonymize the data
	
	lastPos = fin.tell()
	l = fin.readline()
	fin.seek(lastPos)
	if re.match('TOT DLY',l):    # If TOT DLY then no other delays
		hdrstring += EditLine('tot dly',fin,newValue,newhdr,fout)
	elif re.match('SYS DLY',l): # If SYS DLY then REF delay
		hdrstring += EditLine('sys dly',fin,newValue,newhdr,fout)
		hdrstring += EditLine('ref dly',fin,newValue,newhdr,fout)
	elif re.match('INT DLY',l): # If INT DLY then CAB DLY and REF DLY
		hdrstring += EditLine('int dly',fin,newValue,newhdr,fout)
		hdrstring += EditLine('cab dly',fin,newValue,newhdr,fout)
		hdrstring += EditLine('ref dly',fin,newValue,newhdr,fout)
	
	hdrstring += EditLine('ref',fin,newValue,newhdr,fout)

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
		
