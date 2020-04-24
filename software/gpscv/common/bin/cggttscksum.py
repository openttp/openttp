#!/usr/bin/python2
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
# Report checksum
# Useful for providing a CGGTTS header with a valid checksum

import argparse
import os
import sys

# This is where cggttslib is installed
sys.path.append("/usr/local/lib/python2.7/site-packages")
import cggttslib

VERSION = "0.0.2"
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
examples += '1. Report checksum for the header\n'
examples += '   cggttscksum.py  ~/cggtts/GZAU0158.654\n'

parser = argparse.ArgumentParser(description='Report checksum for a CGGTTS header',
	formatter_class=argparse.RawDescriptionHelpFormatter, epilog = examples)

parser.add_argument('infile',help='input file',type=str)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

infile = args.infile

try:
	fin = open(infile,'r')
except:
	print('Unable to open ' + infile)
	exit()

# There is no check that the header is correctly formatted	
hdr = ''
for l in fin:
	l = l.rstrip()
	if l.find('CKSUM = ') >= 0:
		hdr += 'CKSUM = ' # but this string has a checksum of zero anyway
		print('CKSUM = ' + str(hex(cggttslib.CheckSum(hdr))))
		sys.exit()
	hdr += l

print('Bad header - missing/incorrectly formatted CKSUM line')
	


			
