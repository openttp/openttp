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

def EditLine(key,fin,newValue,newHeader,fout):
	l = fin.readline().rstrip()
	if key in newHeader: # overrides newValue 
		hdrout = newHeader[key]
	else:
		if newValue:
			fields = l.split('=',1)
			hdrout = fields[0].strip() + ' = ' + newValue
		else:
			hdrout = l
	fout.write(hdrout + '\n')
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
parser.add_argument('--header',help='replace header lines from file')
parser.add_argument('--comments',help='set comment')
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
	newHeader = {}
	try:
		fin = open(args.header,'r')
		for l in fin:
			l = l.rstrip()
			fields = l.split('=',1)
			newHeader[fields[0].strip()] = l
		fin.close()
	except:
		sys.exit('Blah')


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
	
	fin = open(finName,'r')
	
	# Version information in the first line
	hdrstring = ''
	l = fin.readline().rstrip()
	hdrstring += l
	fout.write(l +'\n')
	
	# Update the REV DATE, unless this is disabled
	if args.keeprev:
		revDate = None
	else:
		revDate = datetime.now().strftime('%Y-%m-%d') # FIXME UTC?
	hdrstring += EditLine('REV DATE',fin,revDate,newHeader,fout)
	
	newValue = None
	hdrstring += EditLine('RCVR',fin,newValue,newHeader,fout)
	hdrstring += EditLine('CH',fin,newValue,newHeader,fout)
	hdrstring += EditLine('IMS',fin,newValue,newHeader,fout)
	hdrstring += EditLine('LAB',fin,newValue,newHeader,fout)
	hdrstring += EditLine('X',fin,newValue,newHeader,fout)
	hdrstring += EditLine('Y',fin,newValue,newHeader,fout)
	hdrstring += EditLine('Z',fin,newValue,newHeader,fout)
	hdrstring += EditLine('FRAME',fin,newValue,newHeader,fout)
	# TODO some times there are multiple comments
	hdrstring += EditLine('COMMENTS',fin,args.comments,newHeader,fout)

	lastPos = fin.tell()
	l = fin.readline()
	fin.seek(lastPos)
	
	# Why would you edit the delays ?
	# But, for completeness
	if re.match('TOT DLY',l):
		hdrstring += EditLine('TOT DLY',fin,newValue,newHeader,fout)
	elif re.match('SYS DLY',l):
		hdrstring += EditLine('SYS DLY',fin,newValue,newHeader,fout)
		hdrstring += EditLine('REF DLY',fin,newValue,newHeader,fout)
	elif re.match('INT DLY',l):
		hdrstring += EditLine('INT DLY',fin,newValue,newHeader,fout)
		hdrstring += EditLine('CAB DLY',fin,newValue,newHeader,fout)
		hdrstring += EditLine('REF DLY',fin,newValue,newHeader,fout)
	
	hdrstring += EditLine('REF',fin,newValue,newHeader,fout)
	
	fin.readline() # eat the CKSUM line
	hdrstring += 'CKSUM = '
	cksum = cggttslib.CheckSum(hdrstring) # compute the new checksum
	fout.write('CKSUM = {:02X}\n'.format(cksum))
	
	# Now the rest of the file
	for l in fin:
		fout.write(l)
	
	fin.close()
	fout.close()
	
	if (args.replace):
		os.remove(finName)
		os.rename(foutName,finName)
		
