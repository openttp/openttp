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
# Doesn't do much at present
#
# 
# Weird example: replace TOTDLY in v2E file with the usual trio of delays
#
# editcggtts.py --intdly 0 --cabdly 0 --refdly 0 --system GPS --code C1 55555.cggtts
#

import argparse
import os
import re
import sys

VERSION = "0.1.1"
AUTHORS = "Michael Wouters"

# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ------------------------------------------
def CheckSum(l):
	cksum = 0
	for c in l:
		cksum = cksum + ord(c)
	return cksum % 256

# ------------------------------------------
def ReadHeader(fname):
	Debug('Reading ' + fname)
	try:
		fin = open(fname,'r')
	except:
		Warn('Unable to open ' + fname)
		# not fatal
		return {}
	
	intdly=''
	cabdly=''
	antdly=''
	
	# Read the header
	header={}
	lineCount=0
	
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	match = re.match('(GGTTS GPS DATA FORMAT VERSION|CGGTTS     GENERIC DATA FORMAT VERSION)\s+=\s+(01|2E)',l)
	if (match):
		header['version'] = match.group(2)
		version = 1
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
	
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	if (l.find('REV DATE') >= 0):
		(tag,val) = l.split('=')
		header['rev date'] = val.strip()
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
	
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	if (l.find('RCVR') >= 0):
		header['rcvr'] = l
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
		
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	if (l.find('CH') >= 0):
		header['ch'] = l
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
		
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	if (l.find('IMS') >= 0):
		header['ims'] = l
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
		
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	if (l.find('LAB') == 0):
		header['lab'] = l
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}	
	
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	match = re.match('^X\s+=\s+(.+)\s+m$',l)
	if (match):
		header['x'] = match.group(1)
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
	
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	match = re.match('^Y\s+=\s+(.+)\s+m$',l)
	if (match):
		header['y'] = match.group(1)
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
		
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	match = re.match('^Z\s+=\s+(.+)\s+m$',l)
	if (match):
		header['z'] = match.group(1)
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
		
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	if (l.find('FRAME') == 0):
		header['frame'] = l
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
	
	comments = ''
	
	
	while True:
		l = fin.readline().rstrip()
		lineCount = lineCount +1
		if not l:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
		if (l.find('COMMENTS') == 0):
			comments = comments + l +'\n'
		else:
			break
	
	header['comments'] = comments
	
	if (header['version'] == '01'):
		#l = fin.readline().rstrip()
		#lineCount = lineCount +1
		match = re.match('^INT\s+DLY\s+=\s+(.+)\s+ns$',l)
		if (match):
			header['int dly'] = match.group(1)
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
		
		l = fin.readline().rstrip()
		lineCount = lineCount +1
		match = re.match('^CAB\s+DLY\s+=\s+(.+)\s+ns$',l)
		if (match):
			header['cab dly'] = match.group(1)
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
		
		
		l = fin.readline().rstrip()
		lineCount = lineCount +1
		match = re.match('^REF\s+DLY\s+=\s+(.+)\s+ns$',l)
		if (match):
			header['ref dly'] = match.group(1)
		else:
			Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return {}
			
	elif (header['version'] == '2E'):
		
		#l = fin.readline().rstrip()
		#lineCount = lineCount +1
		
		match = re.match('^(TOT DLY|SYS DLY|INT DLY)',l)
		if (match.group(1) == 'TOT DLY'): # if TOT DLY is provided, then finito
			(dlyname,dly) = l.split('=',1)
			header['tot dly'] = dly.strip()
		
		elif (match.group(1) == 'SYS DLY'): # if SYS DLY is provided, then read REF DLY
		
			(dlyname,dly) = l.split('=',1)
			header['sys dly'] = dly.strip()
			
			l = fin.readline().rstrip()
			lineCount = lineCount +1
			match = re.match('^REF\s+DLY\s+=\s+(.+)\s+ns$',l)
			if (match):
				header['ref dly'] = match.group(1)
			else:
				Warn('Invalid format in {} line {}'.format(fname,lineCount))
				return {}
				
		elif (match.group(1) == 'INT DLY'): # if INT DLY is provided, then read CAB DLY and REF DLY
		
			(dlyname,dly) = l.split('=',1)
			header['int dly'] = dly.strip()
			
			l = fin.readline().rstrip()
			lineCount = lineCount +1
			match = re.match('^CAB\s+DLY\s+=\s+(.+)\s+ns$',l)
			if (match):
				header['cab dly'] = match.group(1)
			else:
				Warn('Invalid format in {} line {}'.format(fname,lineCount))
				return {}
			
			l = fin.readline().rstrip()
			lineCount = lineCount +1
			match = re.match('^REF\s+DLY\s+=\s+(.+)\s+ns$',l)
			if (match):
				header['ref dly'] = match.group(1)
			else:
				Warn('Invalid format in {} line {}'.format(fname,lineCount))
				return {}
	
	l = fin.readline().rstrip()
	lineCount = lineCount +1
	if (l.find('REF') == 0):
		(tag,val) = l.split('=')
		header['ref'] = val.strip()
	else:
		Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return {}
	
	fin.close()
	return header;
# ------------------------------------------
def Warn(msg):
	if (not args.nowarn):
		sys.stderr.write(msg+'\n')
	return

parser = argparse.ArgumentParser(description='Edit CGGTTS files',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('infile',help='input file',type=str)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--cabdly',help='set cable delay')
parser.add_argument('--intdly',help='set internal delay')
parser.add_argument('--refdly',help='set reference delay')
parser.add_argument('--system',help='GNSS system')
parser.add_argument('--code',help='observation code')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug
	
hdr = ReadHeader(args.infile)

# Print the new header
hdrstring  = ''

hdrout = 'CGGTTS     GENERIC DATA FORMAT VERSION = ' + hdr['version']
print hdrout
hdrstring += hdrout

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


print hdr['comments'].rstrip()
hdrout = hdr['comments']
hdrout=hdrout.replace('\n','').replace('\r','')
hdrstring += hdrout

if (hdr['version'] == '2E'):
	if (hdr['tot dly']):
		# If CAB, ANT, INT specified, replace TOT DLY
		if (args.intdly and args.cabdly and args.intdly and args.system and args.code):
			print 'INT DLY = ' + args.intdly + ' ns (' + args.system + ' ' + args.code + ')     CAL_ID = none'
			print 'CAB DLY = ' + args.cabdly + ' ns'
			print 'REF DLY = ' + args.refdly + ' ns'
			
hdrout = 'REF = ' + hdr['ref']
print hdrout
hdrstring += hdrout 

hdrstring += 'CKSUM = '
cksum = CheckSum(hdrstring)
print 'CKSUM = {:02X}'.format(cksum)

fin = open(args.infile,'r')
for l in fin:
	if (l.find('STTIME TRKL ELV AZTH') > 0): # lazy
		print
		print l.rstrip()
		break

for l in fin:
	print l.rstrip()
