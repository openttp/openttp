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

import os
import re
import sys

LIB_MAJOR_VERSION  = 1
LIB_MINOR_VERSION  = 0
LIB_PATCH_VERSION  = 0

debug=False


# ------------------------------------------------------------------------------------
# 'STANDARD' LIBRARY  FUNCTIONS 
# ------------------------------------------------------------------------------------

# ------------------------------------------------------------------------------------
# Return the library version
# ------------------------------------------------------------------------------------

def LibVersion():
	return '{:d}.{:d}.{:d}'.format(LIB_MAJOR_VERSION,LIB_MINOR_VERSION,LIB_PATCH_VERSION)

def LibMajorVersion():
	return LIB_MAJOR_VERSION

def LibMinorVersion():
	return LIB_MINOR_VERSION

def LibPatchVersion():
	return LIB_PATCH_VERSION

# ------------------------------------------------------------------------------------
# Miscellaneous 
# ------------------------------------------------------------------------------------

def SetDebugging(debugOn):
	global _debug
	_debug = debugOn


def SetWarnings(warningsOn):
	global _warn
	_warn = warningsOn
	
# ------------------------------------------------------------------------------------
# 
# ------------------------------------------------------------------------------------



# ------------------------------------------------------------------------------------
# CGGTTS data class 
# ------------------------------------------------------------------------------------

class CGGTTS:
	
	# CGGTTS versions
	CGGTTS_RAW = 0
	CGGTTS_V1  = 1
	CGGTTS_V2  = 2
	CGGTTS_V2E = 3
	CGGTTS_UNKNOWN = 999

	def __init__(self,fileName,mjd,maxDSG = 20.0, maxSRSYS = 9999.9,minTrackLength=750,elevationMask=0.0): # constructor
		self.fileName = fileName
		self.mjd = mjd
		self.maxDSG   = maxDSG
		self.maxSRSYS = maxSRSYS
		self.minTrackLength = minTrackLength
		self.elevationMask = elevationMask
		self.version = self.CGGTTS_UNKNOWN
		self.header  = {}
		self.tracks  = []
	
		# CGGTTS field identifiers
		# These get diddled with when the file is read so that they align with the file version
		self.PRN=0
		self.CL=1 
		self.MJD=2
		self.STTIME=3
		self.TRKL=4
		self.ELV=5
		self.AZTH=6
		self.REFSV=7
		self.SRSV=8
		self.REFGPS=9
		self.REFSYS=9
		self.SRGPS=10
		self.SRSYS=10
		self.DSG=11
		self.IOE=12
		self.MDTR=13
		self.SMDT=14
		self.MDIO=15
		self.SMDI=16
		# CK=17 # for V1, single frequency
		self.MSIO=17
		self.SMSI=18 
		self.ISG=19 # only for dual frequency
		# CK=20  # V1, dual freqeuncy
		# CK=20  # V2E, single frequency
		self.FRC=21 
		# CK=23  #V2E, ionospheric measurements available
	
	def Read(self,startTime=0,stopTime=86399,measCode='',delays=[],keepAll=False):
		
		
		enforceChecksum = False
		
		_Debug('Reading ' + self.fileName)
		
		
		(self.header,warnings,checksumOK) = ReadHeader(self.fileName,delays)
		if (not self.header):
			return ([],[],{})
		
		if (not checksumOK and enforceChecksum):
			sys.exit()
			
		# OK to read the file
		fin = open(self.fileName,'r')
		
		if (self.header['version'] == 'RAW'):
			self.version= self.CGGTTS_RAW
			self.header['version']='raw'
		elif (self.header['version'] == '01'):	
			self.version = self.CGGTTS_V1
			self.header['version']='V1'
		elif (self.header['version'] == '02'):	
			_Warn('The data may be treated as GPS')
			self.version = self.CGGTTS_V2
			self.header['version']='V02'
		elif (self.header['version'] == '2E'):
			self.version = self.CGGTTS_V2E
			self.header['version']='V2E'
		else:
			_Warn('Unknown format - the header is incorrectly formatted')
			return ([],[],{})
		
		_Debug('CGGTTS version ' + self.header['version'])
		
		hasMSIO = False
		# Eat the header
		while True:
			l = fin.readline()
			if not l:
				_Warn('Bad format')
				return ([],[],{})
			if (re.search('STTIME TRKL ELV AZTH',l)):
				if (re.search('MSIO',l)):
					hasMSIO=True
					_Debug('MSIO present')
				continue
			match = re.match('\s+hhmmss',l)
			if match:
				break
			
		self.SetDataColumns(self.version,hasMSIO)
		if (hasMSIO):
			self.header['MSIO present'] = 'yes' # A convenient bodge. Sorry Mum.
		else:
			self.header['MSIO present'] = 'no'
		
		# nb cksumend is where we stop the checksum ie before CK
		if (self.version == self.CGGTTS_V1):
			if (hasMSIO):
				cksumend=115
			else:
				cksumend=101
		elif (self.version == self.CGGTTS_V2):
			if (hasMSIO):
				cksumend=125
			else:
				cksumend=111
		elif (self.version == self.CGGTTS_V2E):
			if (hasMSIO):
				cksumend=125
			else:
				cksumend=111
				
		nextday =0
		stats=[]
		
		nLowElevation=0
		nHighDSG=0
		nHighSRSYS=0
		nShortTracks=0
		nBadSRSV=0
		nBadMSIO =0
		
		while True:
			l = fin.readline().rstrip()
			if l:
				
				trk = [None]*24 # FIXME this may need to increase one day
				
				# First, fiddle with the SAT identifier to simplify matching
				# SAT is first three columns
				satid = l[0:3]
				
				# V1 is GPS-only and doesn't have the constellation identifier prepending 
				# the PRN, so we'll add it for compatibility with later versions
				if (self.version == self.CGGTTS_V1):
					trk[self.PRN] = 'G{:02d}'.format(int(satid))
					
				if (self.version == self.CGGTTS_V2): 
					trk[self.PRN] = 'G{:02d}'.format(int(satid)) # FIXME works for GPS only
					
				if (self.version == self.CGGTTS_V2E): 
					# CGGTTS V2E files may not necessarily have zero padding in SAT
					if (' ' == satid[1]):
						trk[self.PRN] = '{}0{}'.format(satid[0],satid[2]) # TESTED
					else:
						trk[self.PRN] = satid
					
				if (self.version != self.CGGTTS_RAW): # should have a checksum 
					
					cksum = int(l[cksumend:cksumend+2],16)
					if (not(cksum == (CheckSum(l[:cksumend])))):
						if (enforceChecksum):
							sys.stderr.write('Bad checksum in data of ' + fname + '\n')
							sys.stderr.write(l + '\n')
							exit()
						else:
							_Warn('Bad checksum in data of ' + fname)
					
				trk[self.MJD] = int(l[7:12])
				theMJD = trk[self.MJD]
				sttime = l[13:19]
				hh = int(sttime[0:2])
				mm = int(sttime[2:4])
				ss = int(sttime[4:6])
				tt = hh*3600+mm*60+ss
				trk[self.STTIME] = tt
				
				reject=False
				trk[self.TRKL]  = l[20:24] 
				trk[self.ELV]   = float(l[25:28])/10.0
				trk[self.AZTH]  = float(l[29:33])/10.0
				trk[self.REFSV] = float(l[34:45])/10.0
				trk[self.SRSV]  = float(l[46:52])/10.0
				trk[self.REFSYS]= float(l[53:64])/10.0
				srsys = l[65:71]
				dsg   = l[72:76]
				trk[self.IOE]   = int(l[77:80])
				trk[self.MDIO]  = float(l[91:95])/10.0
				if (hasMSIO):
					trk[self.MSIO] = l[101:105]
					
				if (not(self.CGGTTS_RAW == self.version)): # DSG not defined for RAW
					if (dsg == '9999' or dsg == '****'): # field is 4 wide so 4 digits (no sign needed)
						reject=True
						trk[self.DSG] = 999.9 # so that we count '****' as high DSG
					else:
						trk[self.DSG] = float(dsg)/10.0
					if (srsys == '99999' or srsys == '******'): # field is 6 wide so sign + 5 digits -> max value is 99999
						reject=True;
						trk[self.SRSYS] = 9999.9 # so that we count '******' as high SRSYS
					else:
						trk[self.SRSYS] = float(srsys)/10.0
					trk[self.TRKL] = int(trk[self.TRKL]) 
				else:
					trk[self.DSG] = 0.0
					trk[self.SRSYS] = 0.0
					trk[self.TRKL] = 999
					
				if (trk[self.ELV] < self.elevationMask):
					nLowElevation +=1
					reject=True
				if (trk[self.DSG] > self.maxDSG):
					nHighDSG += 1
					reject = True
				if (abs(trk[self.SRSYS]) > self.maxSRSYS):
					nHighSRSYS += 1
					reject = True
				if (trk[self.TRKL]  < self.minTrackLength):
					nShortTracks +=1
					reject = True
				if (not(self.CGGTTS_RAW == self.version)):
					if (trk[self.SRSV] == '99999' or trk[self.SRSV]=='*****'):
						nBadSRSV +=1
						reject=True
					if (hasMSIO):
						if (trk[self.MSIO] == '9999' or trk[self.MSIO] == '****' or trk[self.SMSI]=='***' ):
							nBadMSIO +=1
							reject=True
				if (self.CGGTTS_RAW == self.version):
					if (hasMSIO):
						if (trk[self.MSIO] == '9999' or trk[self.MSIO] == '****'):
							nBadMSIO +=1
							reject=True	
				if (reject):
					continue
				frc = ''
				if (self.version == self.CGGTTS_V2 or self.version == self.CGGTTS_V2E):
					if hasMSIO:
						trk[self.FRC] = l[121:124] # set this for debugging 
					else:
						trk[self.FRC] = l[107:110]
					frc = trk[self.FRC]
				if (measCode == ''): # Not set so we ignore it
					frc = ''
					
				if (hasMSIO):
					trk[self.MSIO] = float(trk[self.MSIO])/10.0
				else:
					trk[self.MSIO] = 0.0
					
				# print(trk)
				
				if ((tt >= startTime and tt < stopTime and theMJD == self.mjd and frc==measCode) or keepAll):
					self.tracks.append(trk)
				elif (frc==measCode): # don't count observation code mismatches
					nextday += 1
			else:
				break
			
		fin.close()
		stats.append([nLowElevation,nHighDSG,nShortTracks,nBadSRSV,nBadMSIO])

		_Debug(str(nextday) + ' tracks after end of the day removed')
		_Debug('low elevation = ' + str(nLowElevation))
		_Debug('high DSG = ' + str(nHighDSG))
		_Debug('high SRSYS = ' + str(nHighSRSYS))
		_Debug('short tracks = ' + str(nShortTracks))
		_Debug('bad SRSV = ' + str(nBadSRSV))
		_Debug('bad MSIO = ' + str(nBadMSIO))
		return (stats)

	# ------------------------------------------
	def SetDataColumns(self,ver,isdf):
		
		if (self.CGGTTS_RAW == ver): # CL field is empty
			self.PRN=0
			self.MJD=1
			self.STTIME=2
			self.ELV=3
			self.AZTH=4
			self.REFSV=5
			self.REFSYS=6
			self.IOE=7
			self.MDTR=8
			self.MDIO=9
			self.MSIO=10
			self.FRC=11
		elif (self.CGGTTS_V1 == ver):
			if (isdf):
				self.CK = 20
			else:
				self.CK = 17
		elif (self.CGGTTS_V2 == ver):
			if (isdf):
				self.FRC=22
				self.CK =23
			else:
				self.FRC=19
				self.CK =20
		elif (self.CGGTTS_V2E == ver):
			if (isdf):
				self.FRC = 22
				self.CK  = 23
			else:
				self.FRC= 19
				self.CK = 20

# ------------------------------------------------------------------------------------
# Calculate the checksum for a string, as defined by the CGGTTS specification
# ------------------------------------------------------------------------------------

def CheckSum(l):
	cksum = 0
	for c in l:
		cksum = cksum + ord(c)
	return int(cksum % 256)

# ------------------------------------------------------------------------------------
# Try to find a CGGTTS file, given some hints
# Returns full path if successful
# ------------------------------------------------------------------------------------

def FindFile(path,prefix,ext,mjd):
	fname = path + '/' + str(mjd) + '.' + ext # default is MJD.cctf
	if (not os.path.isfile(fname)): # otherwise, try BIPM style name
		mjdYY = int(mjd/1000)
		mjdXXX = mjd % 1000
		fBIPMname = path + '/' + prefix + '{:02d}.{:03d}'.format(mjdYY,mjdXXX)
		if (not os.path.isfile(fBIPMname)): 
			return ('')
		fname = fBIPMname
	return fname

# ------------------------------------------------------------------------------------
# Read the CGGTTS header, storing the result in a dictionary
# Returns (header,warnings,checksumOK)
# ------------------------------------------------------------------------------------

def ReadHeader(fname,intdelays=[]):
	
	try:
		fin = open(fname,'r')
	except:
		_Warn('Unable to open ' + fname)
		return ({},'Unable to open ' + fname,False) # don't want to exit because a missing file may be acceptable
	
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
			_Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
	
	if not(header['version'] == 'RAW'):
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('REV DATE') >= 0):
			(tag,val) = l.split('=')
			header['rev date'] = val.strip()
		else:
			_Warn('Invalid format in {} line {}'.format(fname,lineCount))
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
			_Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
			
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('IMS') >= 0):
			header['ims'] = l
		else:
			_Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	if (l.find('LAB') == 0):
		header['lab'] = l
	else:
		_Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)	
	
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.match('X\s+=\s+(.+)\s+m',l)
	if (match):
		header['x'] = match.group(1)
	else:
		_Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
	
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.match('Y\s+=\s+(.+)\s+m',l)
	if (match):
		header['y'] = match.group(1)
	else:
		_Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	match = re.match('Z\s+=\s+(.+)\s+m',l)
	if (match):
		header['z'] = match.group(1)
	else:
		_Warn('Invalid format in {} line {}'.format(fname,lineCount))
		return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
	l = fin.readline().rstrip()
	hdr += l
	lineCount = lineCount +1
	if (l.find('FRAME') == 0):
		header['frame'] = l
	else:
		_Warn('Invalid format in {} line {}'.format(fname,lineCount))
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
		_Warn('Invalid format in {} line {}: too many comment lines'.format(fname,lineCount))
		warnings += 'Invalid format in {} line {}: too many comment lines;'.format(fname,lineCount)
	header['comments'] = comments
	
	if (header['version'] == '01' ):
		#l = fin.readline().rstrip()
		#lineCount = lineCount +1
		match = re.match('INT\s+DLY\s+=\s+(.+)\s+ns',l)
		if (match):
			header['int dly'] = match.group(1)
		else:
			_Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		match = re.match('CAB\s+DLY\s+=\s+(.+)\s+ns',l)
		if (match):
			header['cab dly'] = match.group(1)
		else:
			_Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
		
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		match = re.match('REF\s+DLY\s+=\s+(.+)\s+ns',l)
		if (match):
			header['ref dly'] = match.group(1)
		else:
			_Warn('Invalid format in {} line {}'.format(fname,lineCount))
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
			
	elif (header['version'] == '02' or header['version'] == '2E' or header['version'] == 'RAW'):
		
		# Some of the options here are not Version '02' but that's OK
		
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
				_Warn('Invalid format in {} line {}'.format(fname,lineCount))
				return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)
				
		elif (match.group(1) == 'INT DLY'): # if INT DLY is provided, then read CAB DLY and REF DLY
			if intdelays:
				nfound = 0
				for i in range(0,len(intdelays)):
					d = intdelays[i]
					if d in l:
						match = re.search('([+-]?\d+\.?\d?)\sns\s\(\s*' + d + '\s*\)',l)
						if match:
							nfound += 1
							if i == 0:
								header['int dly'] = match.group(1)
								ss = d.split()
								header['int dly code'] = ss[-1] 
							elif i == 1:
								header['int dly 2'] = match.group(1)
								ss = d.split()
								header['int dly code 2'] = ss[-1] 
				if not(nfound == len(intdelays)):
					_Warn('Could not find the specified delays in {} line {} INT DLY'.format(fname,lineCount))
					return ({},'Could not find the specified delays in {} line {} INT DLY'.format(fname,lineCount),False)
			else:
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
					_Warn('Invalid format in {} line {} INT DLY'.format(fname,lineCount))
					return ({},'Invalid format in {} line {} INT DLY'.format(fname,lineCount),False)
					
			l = fin.readline().rstrip()
			hdr += l
			lineCount = lineCount +1
			match = re.match('CAB\s+DLY\s+=\s+(.+)\s+ns',l)
			if (match):
				header['cab dly'] = match.group(1)
			else:
				_Warn('Invalid format in {} line {} CAB DLY'.format(fname,lineCount))
				return ({},'Invalid format in {} line {} CAB DLY'.format(fname,lineCount),False)
			
			l = fin.readline().rstrip()
			hdr += l
			lineCount = lineCount +1
			match = re.match('REF\s+DLY\s+=\s+(.+)\s+ns',l)
			if (match):
				header['ref dly'] = match.group(1)
			else:
				_Warn('Invalid format in {} line {} REF DLY'.format(fname,lineCount))
				return ({},'Invalid format in {} line {} REF DLY'.format(fname,lineCount),False)
			
	if not (header['version'] == 'RAW'):
		l = fin.readline().rstrip()
		hdr += l
		lineCount = lineCount +1
		if (l.find('REF') == 0):
			(tag,val) = l.split('=')
			header['ref'] = val.strip()
		else:
			_Warn('Invalid format in {} line {} REF'.format(fname,lineCount))
			return ({},'Invalid format in {} line {} REF'.format(fname,lineCount),False)
		
		l = fin.readline().rstrip()
		lineCount = lineCount +1
		if (l.find('CKSUM') == 0):
			hdr += 'CKSUM = '
			(tag,val) = l.split('=')
			cksum = int(val,16)
			checksumOK = CheckSum(hdr[checksumStart:]) == cksum
			if (not checksumOK):
				_Warn('Bad checksum in ' + fname)
				warnings += 'Bad checksum in ' + fname
		else:
			return ({},'Invalid format in {} line {}'.format(fname,lineCount),False)

	return (header,warnings,checksumOK)

# ------------------------------------------
# Make a sequence of CGGTTS files from two file names
# ------------------------------------------
def MakeFileSequence(filename1,filename2):
	# Determine whether the file names determine a sequence
	# For simplicity, any filename in standard BIPM CGTTS format or NNNNN.***
	
	# Sequence styles
	Plain = 0	
	BIPM  = 1

	# First, need to strip the path
	fileSeq = []
	(path1,file1) = os.path.split(filename1)
	(path2,file2) = os.path.split(filename2)
	if (path1 != path2):
		return (fileSeq,'The paths have to be the same for a sequence',True)
	
	# Now try to guess the sequence
	# The extension has to be the same
	(file1root,file1ext) = os.path.splitext(file1)
	(file2root,file2ext) = os.path.splitext(file2)
	
	isSeq=False
	# First, test for 'plain' file names
	# Debug('Sequence {} -> {}'.format(file1,file2))
	match1 = re.match('^(\d+)$',file1root)
	match2 = re.match('^(\d+)$',file2root)
	if (match1 and match2):
		if (file1ext != file2ext):
			return(fileSeq,'The file extensions have to be the same for a sequence',True)
		
		isSeq = True
		sequenceStyle = Plain
		start = int(match1.group(1)) # NOTE that this won't work with names padded with leading zeroes
		stop  = int(match2.group(1))
		if (start > stop):
			tmp = stop
			stop = start
			start = tmp
		#Debug('Numbered file sequence: {}->{}'.format(start,stop))
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
				#Debug('BIPM file sequence: {} MJD {}->{}'.format(stub1,start,stop))
			else:
				isSeq = false
		else:
			isSeq = False
	# bail out if the sequence is not recognzied
	if (not isSeq):
		return (fileSeq,'The filenames do not form a recognised sequence',True)
		
	# Make the list of files
	for m in range(start,stop+1):
		# Construct the file name
		if (Plain == sequenceStyle):
			fname = str(m) + file1ext
			fileSeq.append(os.path.join(path1,fname))
		elif (BIPM == sequenceStyle):
			dd  = int(m/1000)
			ddd = int(m - dd*1000)
			fname = stub1 + '{:02d}.{:03d}'.format(dd,ddd)
			fileSeq.append(os.path.join(path1,fname))
	
	return (fileSeq,'',False)

# ------------------------------------------
# INTERNALS
# ------------------------------------------

_debug = False

_warn  = True


# ------------------------------------------
def _Debug(msg):
	if (_debug):
		sys.stderr.write('cggttslib: ' + msg + '\n')

# ------------------------------------------
def _Warn(msg):
	if (_warn):
		sys.stderr.write('WARNING! cggttslib: ' + msg + '\n')
