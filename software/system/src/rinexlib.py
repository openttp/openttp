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
import subprocess
import sys
import time

LIB_MAJOR_VERSION  = 0
LIB_MINOR_VERSION  = 5
LIB_PATCH_VERSION  = 1

compressionExtensions = ['.gz','.GZ','.z','.Z']

C_HATANAKA = 0x01
C_GZIP     = 0x02
C_BZIP2    = 0x04
C_COMPRESS = 0x08

# ------------------------------------------
# Return the library version
#
def LibVersion():
	return '{:d}.{:d}.{:d}'.format(LIB_MAJOR_VERSION,LIB_MINOR_VERSION,LIB_PATCH_VERSION)

def LibMajorVersion():
	return LIB_MAJOR_VERSION

def LibMinorVersion():
	return LIB_MINOR_VERSION

def LibPatchVersion():
	return LIB_PATCH_VERSION


# ------------------------------------------
def SetDebugging(debugOn):
	global __debug
	__debug = debugOn

# ------------------------------------------
def FindObservationFile(dirname,staname,yyyy,doy,rnxver,reqd):
	if (rnxver == 2):
		yy = yyyy - int(yyyy/100)*100
		
		bname1 = os.path.join(dirname,'{}{:03d}0.{:02d}o'.format(staname,doy,yy))
		(found,ext) = __FindFile(bname1,compressionExtensions)
		if (found):
			return (bname1,ext)
		
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}O'.format(staname,doy,yy))
		(found,ext) = __FindFile(bname2,compressionExtensions)
		if (found):
			return (bname2,ext)
		
		if (reqd):	
			__ErrorExit("Can't find obs file:\n\t" + bname1 + '\n\t' + bname2 )
			
	elif (rnxver==3):
		# Try a V3 name first
		bname1 = os.path.join(dirname,'{}_R_{:04d}{:03d}0000_01D_30S_MO.rnx'.format(staname,yyyy,doy))
		(found,ext) = __FindFile(bname1,compressionExtensions)
		if (found):
			return (bname1,ext)
		
		# Try version 2
		yy = yyyy - int(yyyy/100)*100
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}o'.format(staname,doy,yy))
		(found,ext) = __FindFile(bname2,compressionExtensions)
		if (found):
			return (bname2,ext)
		
		# Another try at version 2
		bname3 = os.path.join(dirname,'{}{:03d}0.{:02d}O'.format(staname,doy,yy))
		(found,ext) = __FindFile(bname3,compressionExtensions)
		if (found):
			return (bname3,ext)
		
		if (reqd):	
			__ErrorExit("Can't find obs file:\n\t" + bname1 + '\n\t' + bname2 + '\n\t' + bname3)
			
	return ('','')

# ------------------------------------------		
def FindNavigationFile(dirname,staname,yyyy,doy,rnxver,reqd):
	if (rnxver == 2):
		yy = yyyy - int(yyyy/100)*100
		
		bname1 = os.path.join(dirname,'{}{:03d}0.{:02d}n'.format(staname,doy,yy))
		(found,ext) = __FindFile(bname1,compressionExtensions)
		if (found):
			return (bname1,ext)
		
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}N'.format(staname,doy,yy))
		(found,ext) = __FindFile(bname2,compressionExtensions)
		if (found):
			return (bname2,ext)
		
		if (reqd):	
			__ErrorExit("Can't find nav file:\n\t" + bname1 + '\n\t' + bname2)
			
	elif (rnxver == 3):
		# Mixed navigation files only
		bname1 = os.path.join(dirname,'{}_R_{:04d}{:03d}0000_01D_MN.rnx'.format(staname,yyyy,doy))
		(found,ext) = __FindFile(bname1,compressionExtensions)
		if (found):
			return (bname1,ext)
		
		bname2 = os.path.join(dirname,'{}_S_{:04d}{:03d}0000_01D_MN.rnx'.format(staname,yyyy,doy))
		(found,ext) = __FindFile(bname2,compressionExtensions)
		if (found):
			return (bname2,ext)
			
		# Try version 2 name (typically produced by sbf2rin)
		yy = yyyy - int(yyyy/100)*100
		bname3 = os.path.join(dirname,'{}{:03d}0.{:02d}P'.format(staname,doy,yy))
		(found,ext) = __FindFile(bname3,compressionExtensions)
		if (found):
			return (bname3,ext)
		
		# Try yet another version 2 name (typically produced by sbf2rin)
		bname4 = os.path.join(dirname,'{}{:03d}0.{:02d}N'.format(staname,doy,yy))
		(found,ext) = __FindFile(bname4,compressionExtensions)
		if (found):
			return (bname4,ext)
		
		if (reqd):	
			__ErrorExit("Can't find nav file:\n\t" + bname1 + "\n\t" + bname2 + "\n\t" + bname3 + + "\n\t" + bname4)
			
	return ('','') 

# -----------------------------------------
def SetHatanakaTools(crx2rnx,rnx2crx):
	__CRX2RNX = crx2rnx
	__RNX2CRX = rnx2crx
	
# ------------------------------------------
def Decompress(fin):
	
	algo = 0x00
	
	fBase,ext = os.path.splitext(fin)
	
	if not ext:
		return [fin,'','',algo]
	
	fout = fBase
	cmd = []
	
	if ext.lower() == '.gz':
		algo = C_GZIP
		cmd = ['gunzip','-f',fin]
	elif ext.lower() == '.bz2':
		algo = C_BZIP2
		cmd = ['bunzip2','-f',fin] 
	elif ext.lower() == '.z':
		algo = C_COMPRESS     # can do this with gunzip
		cmd = ['gunzip','-f',fin]
	elif ext.lower() == '.crx' or re.match(r'\.\d{2}d',ext.lower()):
		algo = C_HATANAKA
		cmd  = [__CRX2RNX,fin,'-d'] # delete input file
		if ext == '.crx':
			fout  = fBase + '.rnx'
		elif ext == '.CRX':
			fout  = fBase + '.RNX'
		else:
			m = re.match(r'\.(\d{2})[dD]',ext)
			if m:
				fout = fBase + '.' + m.group(1)
				if ext[3] == 'd':
					fout += 'o'
				else:
					fout += 'O'
	else: # nothing to do, hopefully
		return [fin,algo]
	
	# and do it
	__Debug('Decompressing {} (ext {})'.format(fin,ext))
	try:
		x = subprocess.check_output(cmd) # eat the output
	except Exception as e:
		__ErrorExit('Failed to run ')
	
	# CRX2RNX renames .crx to .rnx (and .CRX to .RNX)
	# CRX2RNX renames .xxD to .xxO (and .xxd to .xxo)
	
	# and check for Hatanaka again
	ext2=''
	fBase2,ext2 = os.path.splitext(fBase)
	if ext2.lower() == '.crx' or re.match(r'\.\d{2}d',ext2.lower()): 
		
		algo |= C_HATANAKA
		cmd = [__CRX2RNX,fBase,'-d'] # delete input file
		__Debug('Decompressing {} format {}'.format(fBase,ext2))
		
		try:
			x = subprocess.check_output(cmd) # eat the output
		except Exception as e:
			ErrorExit('Failed to run ')
		
		if ext2 == '.crx':
			fout  = fBase2 + '.rnx'
		elif ext2 == '.CRX':
			fout  = fBase2 + '.RNX'
		else:
			m = re.match(r'\.(\d{2})[dD]',ext2)
			if m:
				fout = fBase2 + '.' + m.group(1)
				if ext2[3] == 'd':
					fout += 'o'
				else:
					fout += 'O'

	return [fout,algo] 

# ------------------------------------------
def Compress(fin,fOriginal,algo):
	
	# If fOriginal is empty, don't care about changes to case of the original file
	
	if not algo:
		return
			
	fBase,ext = os.path.splitext(fin)
	
	if algo & C_HATANAKA: # HATANAKA first
		cmd = [__RNX2CRX,fin,'-d'] # delete input file
		__Debug('Compressing (Hatanaka) {}'.format(fin))
		try:
			x = subprocess.check_output(cmd) # eat the output
		except Exception as e:
			__ErrorExit('Failed to run ')
		
		if ext== '.rnx':
			fin  = fBase + '.crx'
		elif ext == '.RNX':
			fin = fBase + '.CRX'
		else:
			m = re.match(r'\.(\d{2})[oO]',ext)
			if m:
				fin = fBase + '.' + m.group(1)
				if ext[3] == 'o':
					fin += 'd'
				else:
					fin += 'D'
		
	if (algo & C_GZIP):
		cmd = ['gzip','-f',fin] # force - overwrites existing
		ext = '.gz'
	elif (algo & C_BZIP2):
		cmd = ['bzip2','-f',fin] # force - overwrites existing
		ext = '.bz2'
	elif (algo & C_COMPRESS):
		pass
	
	__Debug('Compressing {}'.format(fin))
	try:
		x = subprocess.check_output(cmd) # eat the output
	except Exception as e:
		__ErrorExit('Failed to run ')
	
	fin += ext
	
	if (fOriginal):
		if not(fin == fOriginal): # Rename the file if necessary (to fix up case of file extensions)
			__Debug('{} <- {}'.format(fOriginal,fin))
			os.rename(fin,fOriginal)
	
	return 
	
# ------------------------------------------
def GetLeapSeconds(navFileName,rnxVers):
	# Expects decompressed file
	nLeap = 0
	fin = open(navFileName,'r')
	for l in fin:
		m = re.search('\s+(\d+)(\s+\d+\s+\d+\s+\d+)?\s+LEAP\s+SECONDS',l) # RINEX V3 has extra leap second information 
		if (m):
			nLeap = int(m.group(1))
			__Debug('Leap seconds = '+str(nLeap))
			break
	fin.close()
	return nLeap

# ------------------------------------------
def MJDtoRINEXObsName(mjd,template):
	
	fname = ''
	t    = (mjd-40587)*86400.0
	tod  = time.gmtime(t)
	yyyy = tod.tm_year
	yy   = tod.tm_year - int(tod.tm_year/100)*100
	doy  = tod.tm_yday
	
	# Version 2 names are of the format
	# xxxxDDDx.YY[oO]
	m = re.match(r'(\w{4})DDD(\d)\.YY([oO])',template)
	if m:
		fname = '{}{:03d}{}.{:02d}{}'.format(m.group(1),doy,m.group(2),yy,m.group(3))
		return fname
	
	# Next, try V3
	# See the format spec!
	m = re.match('(\w{9})_(\w)_YYYYDDD(\d{4})_(\w{3})_(\w{3})_(\w{2})\.(rnx|RNX)',template)
	if m:
		fname='{}_{}_{:04d}{:03d}{}_{}_{}_{}.{}'.format(m.group(1),m.group(2),yyyy,doy,m.group(3),m.group(4),m.group(5),m.group(6),m.group(7))
		return fname

	return fname

# ------------------------------------------
# INTERNALS
# ------------------------------------------

__debug = False

__CRX2RNX = 'CRX2RNX'
__RNX2CRX = 'RNX2CRX'

# ------------------------------------------
def __ErrorExit(msg):
	print (msg)
	sys.exit(0)

# ------------------------------------------
def __Debug(msg):
	if (__debug):
		sys.stderr.write('rinexlib:' + msg + '\n')

# ------------------------------------------
# Find a file with basename and a list of potential extensions
# (usually compression extensions - .gz, .Z etc
def __FindFile(basename,extensions):
	fname = basename
	__Debug('Trying ' + fname)
	if (os.path.exists(fname)):
		__Debug('Success')
		return (True,'')
	
	for ext in extensions:
		fname = basename + ext
		__Debug('Trying ' + fname)
		if (os.path.exists(fname)):
			__Debug('Success')
			return (True,ext)
	
	return (False,'') # flag failure
