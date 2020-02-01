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

import re

# ------------------------------------------
# Functions for reading CGGTTS files
#

# ------------------------------------------
# Calculate the checksum for a string, as defined by the CGTTS specification
# ------------------------------------------
def CheckSum(l):
	cksum = 0
	for c in l:
		cksum = cksum + ord(c)
	return int(cksum % 256)

# ------------------------------------------
# Read the CGGTTS header, storing the result in a dictionary
# Returns (header,warnings,checksumOK)
# ------------------------------------------
def ReadHeader(fname):
	
	try:
		fin = open(fname,'r')
	except:
		return ({},'Unable to open ' + fname,False)
	
	checksumStart=0 # workaround for R2CGGTTS output
	
	# Read the header
	header={}
	warnings=''
	hdr=''; # accumulate header for checksumming
	lineCount=0
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.search('DATA FORMAT VERSION\s+=\s+(01|02|2E)',l)
	if (match):
		header['version'] = match.group(1)
	else:
		if (re.search('RAW CLOCK RESULTS',l)):
			header['version'] = 'RAW'
		else:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
	
	if not(header['version'] == 'RAW'):
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('REV DATE') >= 0):
			(tag,val) = l.split('=')
			header['rev date'] = val.strip()
		else:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('RCVR') >= 0):
			header['rcvr'] = l
			match = re.search('R2CGGTTS\s+v(\d+)\.(\d+)',l)
			if (match):
				majorVer=int(match.group(1))
				minorVer=int(match.group(2))
				if ( (majorVer == 8 and minorVer <=1) ):
					checksumStart = 1
					# Debug('Fixing checksum for R2CGGTTS v'+str(majorVer)+'.'+str(minorVer))
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('CH') >= 0):
			header['ch'] = l
		else:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
			
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('IMS') >= 0):
			header['ims'] = l
		else:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	if (l.find('LAB') == 0):
		header['lab'] = l
	else:
		return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)	
	
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.match('X\s+=\s+(.+)\s+m',l)
	if (match):
		header['x'] = match.group(1)
	else:
		return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
	
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.match('Y\s+=\s+(.+)\s+m',l)
	if (match):
		header['y'] = match.group(1)
	else:
		return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.match('Z\s+=\s+(.+)\s+m',l)
	if (match):
		header['z'] = match.group(1)
	else:
		return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	if (l.find('FRAME') == 0):
		header['frame'] = l
	else:
		return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
	
	# Some incorrectly! formatted files have multiple COMMENTS lines.
	commentCount = 0
	comments = ''
	while True:
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if not l:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		if (l.find('COMMENTS') == 0):
			comments = comments + l +'\n'
			commentCount += 1
		else:
			break
	if (commentCount > 1):
		warnings += 'Invalid format in {} line {}: too many comment lines;'.format(fname,lineCount)
	header['comments'] = comments
	
	if (header['version'] == '01' or header['version'] == '02'):
		#l = fin.readline().rstrip()
		#lineCount = lineCount +1
		match = re.match('INT\s+DLY\s+=\s+(.+)\s+ns',l)
		if (match):
			header['int dly'] = match.group(1)
		else:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		match = re.match('CAB\s+DLY\s+=\s+(.+)\s+ns',l)
		if (match):
			header['cab dly'] = match.group(1)
		else:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		match = re.match('REF\s+DLY\s+=\s+(.+)\s+ns',l)
		if (match):
			header['ref dly'] = match.group(1)
		else:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
			
	elif (header['version'] == '2E' or header['version'] == 'RAW'):
		
		#l = fin.readline().rstrip()
		#lineCount = lineCount +1
		
		match = re.match('(TOT DLY|SYS DLY|INT DLY)',l)
		if (match.group(1) == 'TOT DLY'): # if TOT DLY is provided, then finito
			(dlyname,dly) = l.split('=',1)
			header['tot dly'] = dly.strip()
		
		elif (match.group(1) == 'SYS DLY'): # if SYS DLY is provided, then read REF DLY
		
			(dlyname,dly) = l.split('=',1)
			header['sys dly'] = dly.strip()
			
			l = fin.readline().rstrip()
			hdr += l
			lineCount = lineCount +1
			match = re.match('REF\s+DLY\s+=\s+(.+)\s+ns',l)
			if (match):
				header['ref dly'] = match.group(1)
			else:
				return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
				
		elif (match.group(1) == 'INT DLY'): # if INT DLY is provided, then read CAB DLY and REF DLY
		
			(dlyname,dly) = l.split('=',1)
			# extra spaces in constellation and code for r2cggtts
			match = re.search('([+-]?\d+\.?\d?)\sns\s\(\w+\s(\w+)\s*\)(,\s*([+-]?\d+\.?\d?)\sns\s\(\w+\s(\w+)\s*\))?',dly)
			if (match):
				header['int dly'] = match.group(1)
				header['int dly code'] = match.group(2) # non-standard but convenient
				if (not(match.group(5) == None) and not (match.group(5) == None)):
					header['int dly 2'] = match.group(4) 
					header['int dly code 2'] = match.group(5) 
			else:
				return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
				
			l = fin.readline().rstrip()
			hdr += l
			lineCount = lineCount +1
			match = re.match('CAB\s+DLY\s+=\s+(.+)\s+ns',l)
			if (match):
				header['cab dly'] = match.group(1)
			else:
				return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
			
			l = fin.readline().rstrip()
			hdr += l
			lineCount = lineCount +1
			match = re.match('REF\s+DLY\s+=\s+(.+)\s+ns',l)
			if (match):
				header['ref dly'] = match.group(1)
			else:
				return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
	if not (header['version'] == 'RAW'):
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('REF') == 0):
			(tag,val) = l.split('=')
			header['ref'] = val.strip()
		else:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
		l = fin.readline().rstrip()
		lineCount = lineCount +1
		if (l.find('CKSUM') == 0):
			hdr += 'CKSUM = '
			(tag,val) = l.split('=')
			cksum = int(val,16)
			checksumOK = CheckSum(hdr[checksumStart:]) == cksum
			if (not checksumOK):
				warnings += 'Bad checksum in ' + fname
		else:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
	
	return (header,warnings,checksumOK)
