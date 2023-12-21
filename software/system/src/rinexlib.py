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

LIB_MAJOR_VERSION  = 0
LIB_MINOR_VERSION  = 3
LIB_PATCH_VERSION  = 0

compressionExtensions = ['.gz','.GZ','.z','.Z']

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
		
		# Try version 2 name (typically produced by sbf2rin)
		yy = yyyy - int(yyyy/100)*100
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}P'.format(staname,doy,yy))
		(found,ext) = __FindFile(bname2,compressionExtensions)
		if (found):
			return (bname2,ext)
		
		# Try yet another version 2 name (typically produced by sbf2rin)
		bname3 = os.path.join(dirname,'{}{:03d}0.{:02d}N'.format(staname,doy,yy))
		(found,ext) = __FindFile(bname3,compressionExtensions)
		if (found):
			return (bname3,ext)
		
		if (reqd):	
			__ErrorExit("Can't find nav file:\n\t" + bname1 + "\n\t" + bname2 + "\n\t" + bname3)
			
	return ('','') 

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
# INTERNALS
# ------------------------------------------

__debug = False

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
