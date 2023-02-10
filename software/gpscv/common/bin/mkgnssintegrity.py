#!/usr/bin/python3
#

#
# The MIT License (MIT)
#
# Copyright (c) 2023 Michael J. Wouters
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
# Contructs an integrity file for a GNSS
# 

import argparse
from   datetime import datetime
import os
import re
import subprocess
import sys
import time


# This is where cggttslib is installed
sys.path.append("/usr/local/lib/python3.6/site-packages") # Ubuntu 18.04
sys.path.append("/usr/local/lib/python3.8/site-packages") # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22.04
import ottplib

VERSION = "0.0.1"
AUTHORS = "Michael Wouters"

MKCGGTTS_FORMAT = 0
GPSCV_FORMAT = 1

BDS = 0
GAL = 1
GLO = 2
GPS = 3
MAXSV = 64 # per GNSS

MAX_OBS_BREAK = 60    # as per old files
MIN_TRK_LEN   = 3600  # as per old files

# Globals 
gps = [] # top level of data structure will be a list, with index corresponding to SVN

# ------------------------------------------
def ShowVersion():
	print (os.path.basename(sys.argv[0])+" "+VERSION)
	print ('Written by ' + AUTHORS)
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg + '\n')
	return

# ------------------------------------------
def ErrorExit(msg):
	print (msg)
	sys.exit(0)

# ------------------------------------------
def Initialise(configFile):
	cfg=ottplib.LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit("Error loading " + configFile)
		
	# Check for required arguments
	reqd = []
	for k in reqd:
		if (not k in cfg):
			ErrorExit('The required configuration entry "' + k + '" is undefined')
		
	return cfg

# ------------------------------------------
# Find a file with basename and a list of potential extensions
# (usually compression extensions - .gz, .Z etc
def FindFile(basename,extensions):
	fname = basename
	Debug('Trying ' + fname )
	if (os.path.exists(fname)):
		Debug('Success')
		return (True,'')
	
	for ext in extensions:
		fname = basename + ext
		Debug('Trying ' + fname )
		if (os.path.exists(fname)):
			Debug('Success')
			return (True,ext)
	
	return (False,'') # flag failure

# ------------------------------------------
def FindRINEXObservationFile(dirname,staname,yyyy,doy,rnxver,reqd):
	if (rnxver == 2):
		yy = yyyy - int(yyyy/100)*100
		
		bname1 = os.path.join(dirname,'{}{:03d}0.{:02d}o'.format(staname,doy,yy))
		(found,ext) = FindFile(bname1,['.gz'])
		if (found):
			return (bname1,ext)
		
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}O'.format(staname,doy,yy))
		(found,ext) = FindFile(bname2,['.gz'])
		if (found):
			return (bname2,ext)
		
		if (reqd):	
			ErrorExit("Can't find obs file:\n\t" + bname1 + '\n\t' + bname2 )
			
	elif (rnxver==3):
		# Try a V3 name first
		bname1 = os.path.join(dirname,'{}_R_{:04d}{:03d}0000_01D_30S_MO.rnx'.format(staname,yyyy,doy))
		(found,ext) = FindFile(bname1,['.gz'])
		if (found):
			return (bname1,ext)
		
		# Try version 2
		yy = yyyy - int(yyyy/100)*100
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}o'.format(staname,doy,yy))
		(found,ext) = FindFile(bname2,['.gz'])
		if (found):
			return (bname2,ext)
		
		# Another try at version 2
		bname3 = os.path.join(dirname,'{}{:03d}0.{:02d}O'.format(staname,doy,yy))
		(found,ext) = FindFile(bname3,['.gz'])
		if (found):
			return (bname3,ext)
		
		if (reqd):	
			ErrorExit("Can't find obs file:\n\t" + bname1 + '\n\t' + bname2 + '\n\t' + bname3)
			
	return ('','')

# ------------------------------------------		
#
def FindRINEXNavigationFile(dirname,staname,yyyy,doy,rnxver,reqd):
	if (rnxver == 2):
		yy = yyyy - int(yyyy/100)*100
		
		bname1 = os.path.join(dirname,'{}{:03d}0.{:02d}n'.format(staname,doy,yy))
		(found,ext) = FindFile(bname1,['.gz'])
		if (found):
			return (bname1,ext)
		
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}N'.format(staname,doy,yy))
		(found,ext) = FindFile(bname2,['.gz'])
		if (found):
			return (bname2,ext)
		
		if (reqd):	
			ErrorExit("Can't find nav file:\n\t" + bname1 + '\n\t' + bname2)
			
	elif (rnxver == 3):
		# Mixed navigation files only
		bname1 = os.path.join(dirname,'{}_R_{:04d}{:03d}0000_01D_MN.rnx'.format(staname,yyyy,doy))
		(found,ext) = FindFile(bname1,['.gz'])
		if (found):
			return (bname1,ext)
		
		# Try version 2 name (typically produced by sbf2rin)
		yy = yyyy - int(yyyy/100)*100
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}P'.format(staname,doy,yy))
		(found,ext) = FindFile(bname2,['.gz'])
		if (found):
			return (bname2,ext)
		
		if (reqd):	
			ErrorExit("Can't find nav file:\n\t" + bname1)
			
	return ('','')

# ------------------------------------------
def DecompressFile(basename,ext):
	if (ext == '.gz'):
		subprocess.check_output(['gunzip',basename + ext])
		Debug('Decompressed ' + basename)

# ------------------------------------------
def RecompressFile(basename,ext):
	if (ext == '.gz'):
		subprocess.check_output(['gzip',basename])
		Debug('Recompressed ' + basename)

# ------------------------------------------
def ParseV3Obs(fname,rnxVer,leapSecs):
	fin = open(fname,'r')
	
	while 1:
		l = fin.readline()
		if (len(l) == 0): # EOF 
			reading = False
			break
		if 'TIME OF FIRST OBS' in l:
			if not 'GPS' in l:
				print('Warning! Not GPS time')
		if 'END OF HEADER' in l:
			break
	currMJD=-1
	currTOD=-1
	while 1:
		l = fin.readline()
		if (len(l) == 0): # EOF
			Debug('Finished')
			break
		if l[0] == '>': # beginning of record - extract the timestamp NB this is usually GPS
			hdr = l.split()
			dt = datetime(int(hdr[1]),int(hdr[2]),int(hdr[3])) # already 'UTC' sorta
			currMJD = ottplib.MJD(dt.timestamp()) 
			currTOD = 3600*int(hdr[4]) + 60*int(hdr[5]) + int(float(hdr[6])) + leapSecs
			if currTOD >= 86400: # assuming roughly 1 day of data !
				currTOD = 0
				currMJD += 1
		if l[0] == 'G':
			svn = int(l[1:3])
			if gps[svn] == None: 
				gps[svn]=[[[currMJD,currTOD],[currMJD,currTOD],0,0.0]]
			else:
				currTrk = gps[svn][-1]
				if ((currMJD - currTrk[1][0])*86400 + (currTOD - currTrk[1][1])) > MAX_OBS_BREAK: # start a new track
					gps[svn].append([[currMJD,currTOD],[currMJD,currTOD],0,0.0])
				else:
					gps[svn][-1][1][0] = currMJD
					gps[svn][-1][1][1] = currTOD
			
	fin.close()
# ------------------------------------------

home = os.environ['HOME'] 
root = home 
configFile = os.path.join(root,'etc','mkcggtts.conf') # try this first
cfgFormat = MKCGGTTS_FORMAT
appName = os.path.basename(sys.argv[0])
examples=''
create = True

if ottplib.LibMajorVersion() >= 0 and ottplib.LibMinorVersion() < 1: # a bit redundant since this will fail anyway on older versions of ottplib ...
	print('ottplib minor version < 1')
	sys.exit(1)

parser = argparse.ArgumentParser(description='Generate a GNSS integrity file from RINEX and CGGTTS',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)
parser.add_argument('mjd',nargs = '*',help='first MJD [last MJD] (if not given, the MJD of the previous day is used)')
parser.add_argument('--config','-c',help='use an alternate configuration file')
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

debug = args.debug

if (args.version):
	ShowVersion()
	exit()

if (args.config):
	configFile = args.config
	if (not os.path.isfile(configFile)):
		ErrorExit(configFile + ' not found')
else:
	# If mkcggtts is used, the information we need is in mkcggtts.conf
	if (not os.path.isfile(configFile)):
		configFile = os.path.join(root,'etc','gpscv.conf')
		if not os.path.isfile(configFile):
			ErrorExit('A mkcggtts.conf or gpscv.conf was not found')
		cfgFormat = GPSCV_FORMAT
		
Debug('Using ' + configFile)

cfg=Initialise(configFile)

startMJD = ottplib.MJD(time.time()) - 1 # previous day
stopMJD  = startMJD
	
if (args.mjd):
	if 1 == len(args.mjd):
		startMJD = int(args.mjd[0])
		stopMJD  = startMJD
	elif ( 2 == len(args.mjd)):
		startMJD = int(args.mjd[0])
		stopMJD  = int(args.mjd[1])
		if (stopMJD < startMJD):
			ErrorExit('Stop MJD is before start MJD')
	else:
		ErrorExit('Too many MJDs')

# Boilerplate done

if cfgFormat == MKCGGTTS_FORMAT:
	rnxVer = int(cfg['rinex:version'])
	obsSta = cfg['rinex:obs sta']
	obsDir = cfg['rinex:obs directory']
	navSta = cfg['rinex:nav sta']
	navDir = cfg['rinex:nav directory']

obsDir = ottplib.MakeAbsolutePath(obsDir,root)

Debug('RINEX version is {:d}'.format(rnxVer))

# Data structure for each GNSS
# List (index corresponds to SV) - fixed size
# 	List of SV tracks - variable size
#     Track start (from OBS)
#     Track finish (from OBS)
#     Health (from NAV)
#     REFSV  (from CGGTTS)

for mjd in range(startMJD,stopMJD + 1):
	for s in range(1,MAXSV+1):
		gps.append(None)
	tmjd =  (mjd - 40587)*86400
	utc = datetime.utcfromtimestamp(tmjd)
	doy = int(utc.strftime('%j')) # zero padded
	yyyy  = int(utc.strftime('%Y'))
	[baseName,ext] = FindRINEXObservationFile(obsDir,obsSta,yyyy,doy,rnxVer,False)
	if not baseName:
		Debug(' .. skipped')
		continue
	DecompressFile(baseName,ext)
	ParseV3Obs(baseName,rnxVer,18)
	RecompressFile(baseName,ext)
	# Read the navigtion file
	[baseName,ext] = FindRINEXNavigationFile(navDir,navSta,yyyy,doy,rnxVer,False)
	if not baseName:
		Debug(' .. skipped')
		continue
	DecompressFile(baseName,ext)
	ParseV3Obs(baseName,rnxVer,18)
	RecompressFile(baseName,ext)
	
prn = -1
for g in gps:
	prn += 1
	if not g:
		continue
	
	for trk in g: 
		if (trk[1][0] - trk[0][0])*86400 +  trk[1][1] - trk[0][1] > MIN_TRK_LEN:
			print(prn,trk)
		else:
			#print(svn,trk)
			pass
		
