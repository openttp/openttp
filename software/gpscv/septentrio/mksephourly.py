#!/usr/bin/python3


#
# The MIT License (MIT)
#
# Copyright (c) 2021 Michael J. Wouters, Louis Marais
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

import argparse
import glob
import os
import re
import subprocess
import sys
import time

# This is where ottplib is installed
sys.path.append("/usr/local/lib/python3.8/site-packages") # Ubuntu 20
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22
import ottplib

VERSION = "1.0.1"
AUTHORS = "Michael Wouters"

debug = False

# CGGTTS V2E columns

# cggtts data fields
PRN=0
CL=1 
MJD=2
STTIME=3
TRKL=4
ELV=5
AZTH=6
REFSV=7
SRSV=8
REFGPS=9
REFSYS=9
SRGPS=10
SRSYS=10
DSG=11
IOE=12
MDTR=13
SMDT=14
MDIO=15
SMDI=16
# CK=17 # for V1, single frequency
MSIO=17
SMSI=18 
ISG=19 # only for dual frequency
# CK=20  # V1, dual freqeuncy
# CK=20  # V2E, single frequency
FRC=21 
# CK=23  #V2E, ionospheric measurements available

MIN_TRACK_LENGTH=750
DSG_MAX = 20.0 # in ns
SRSYS_MAX = 9999.9 # in ns
ELV_MASK = 0.0 # in degrees

RUN_SBF2RNX = '/usr/local/bin/runsbf2rnx.py'
RUN_SBF2RNX_CONF = 'hourly.runsbf2rnx.conf'

MKCGGTTS    = '/usr/local/bin/mkcggtts.py'
MKCGGTTS_CONF = 'hourly.mkcggtts.conf'

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
	reqd = ['main:cggtts source','main:summary path']
	
	for k in reqd:
		if (not k in cfg):
			ErrorExit('The required configuration entry "' + k + '" is undefined')
		
	return cfg


# ------------------------------------------
def SetDataColumns(isdf):
	global FRC,CK
	if (isdf):
		FRC = 22
		CK  = 23
	else:
		FRC= 19
		CK = 20
			
# ------------------------------------------
def ReadCGGTTS(fname):
	
	Debug('Reading ' + fname)
	d = []
	try:
		fin = open(fname,'r')
	except:
		Debug('Unable to open ' + fname)
		return d

	# This code lifted from cmpcggtts.py
	
	# Eat the header
	hasMSIO = False
	while True:
		l = fin.readline()
		if not l:
			Debug('Bad format')
			return d
		if (re.search('STTIME TRKL ELV AZTH',l)):
			if (re.search('MSIO',l)):
				hasMSIO=True
				Debug('MSIO present')
			continue
		if 'hhmmss' in l:
			break
		
	SetDataColumns(hasMSIO)
	
	# Note that only V2E is supported!
	
	while True:
		l = fin.readline().rstrip()
		if l:
			fields = [None]*24 # FIXME this may need to increase one day
			
			# First, fiddle with the SAT identifier to simplify matching
			# SAT is first three columns
			satid = l[0:3]
			
			# CGGTTS V2E files may not necessarily have zero padding in SAT
			if (' ' == satid[1]):
				fields[PRN] = '{}0{}'.format(satid[0],satid[2]) # TESTED
			else:
				fields[PRN] = satid
				
			fields[MJD] = l[7:12]
			theMJD = int(fields[MJD])
			fields[STTIME] = l[13:19]
			hh = int(fields[STTIME][0:2])
			mm = int(fields[STTIME][2:4])
			ss = int(fields[STTIME][4:6])
			tt = hh*3600+mm*60+ss
			dsg=0.0
			srsys=0.0
			trklen=999
			reject=False
			fields[TRKL]  = l[20:24]
			fields[ELV]   = l[25:28]
			fields[AZTH]  = l[29:33]
			fields[REFSV] = l[34:45]
			fields[SRSV]  = l[46:52]
			fields[REFSYS]= l[53:64]
			fields[SRSYS] = l[65:71]
			fields[DSG]   = l[72:76]
			fields[IOE]   = l[77:80]
			fields[MDIO]  = l[91:95]
			if (hasMSIO):
				fields[MSIO] = l[101:105]
				
			if (fields[DSG] == '9999' or fields[DSG] == '****'): # field is 4 wide so 4 digits (no sign needed)
				reject=True
				dsg = 999.9 # so that we count '****' as high DSG
			else:
				dsg = int(fields[DSG])/10.0
			if (fields[SRSYS] == '99999' or fields[SRSYS] == '******'): # field is 6 wide so sign + 5 digits -> max value is 99999
				reject=True;
				srsys = 9999.9 # so that we count '******' as high SRSYS
			else:
				srsys = int(fields[SRSYS])/10.0
			trklen = int(fields[TRKL])
			
			elv = int(fields[ELV])/10.0
			if (elv < elevationMask):
				reject=True
			if (dsg > maxDSG):
				reject = True
			if (abs(srsys) > maxSRSYS):
				reject = True
			if (trklen < minTrackLength):
				reject = True
				
			if (fields[SRSV] == '99999' or fields[SRSV]=='*****'):
				reject=True
			if (hasMSIO):
				if (fields[MSIO] == '9999' or fields[MSIO] == '****' or fields[SMSI]=='***' ):
					reject=True
		
			if (reject):
				continue
			frc = ''
			
			if hasMSIO:
				fields[FRC] = l[121:124] # set this for debugging 
			else:
				fields[FRC] = l[107:110]
			frc = fields[FRC]
				
			msio = 0
			if (hasMSIO):
				msio = float(fields[MSIO])/10.0

			d.append([int(fields[MJD]),tt,float(fields[REFSYS])/10.0])
			
		else:
			break
			
	fin.close()
	
	return d

# -----------------------------------------------

elevationMask  = ELV_MASK
minTrackLength = MIN_TRACK_LENGTH
maxDSG         = DSG_MAX
maxSRSYS       = SRSYS_MAX

home = os.environ['HOME'] 
root = home 
configFile = os.path.join(root,'etc','mksephourly.conf')

parser = argparse.ArgumentParser(description='')
examples =  'Usage examples\n'
parser = argparse.ArgumentParser(description='Hourly generation of average REFSYS estimates from CGGTTS',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)
parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

configFile = args.config;

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + ' not found')
	
cfg=Initialise(configFile)

# Script is run hourly, so to make sure we get the data from the end of the day
# use the current time - 1 hour to get the MJD

mjd = ottplib.MJD(time.time() - 3600) 
Debug('Generating for {:d}'.format(mjd))

# mjd = 59898 # FIXME

runsbf2rnxConf = RUN_SBF2RNX_CONF
if 'main:runsbf2rnx conf' in cfg:
	runsbf2rnxConf = cfg['main:runsbf2rnx conf']
runsbf2rnxConf = ottplib.MakeAbsoluteFilePath(runsbf2rnxConf,root,os.path.join(root,'etc'))
Debug('Using ' + runsbf2rnxConf)

mkcggttsConf = MKCGGTTS_CONF
if 'main:mkcggtts conf' in cfg:
	mkcggttsConf = cfg['main:mkcggtts conf']
mkcggttsConf = ottplib.MakeAbsoluteFilePath(mkcggttsConf,root,os.path.join(root,'etc'))
Debug('Using ' + mkcggttsConf)

mkcggttsCfg = ottplib.LoadConfig(mkcggttsConf,{'tolower':True})
if (mkcggttsCfg == None):
	ErrorExit("Error loading " + mkcggttsCfg)
	
# Determine the file to load from hourly.mkcggtts.conf
cggtts = cfg['main:cggtts source'].lower()
cggttsPath = mkcggttsCfg[ cggtts + ':directory' ]
cggttsPath = ottplib.MakeAbsolutePath(cggttsPath,root)

constellation = mkcggttsCfg[ cggtts + ':constellation' ].upper()
code = mkcggttsCfg[ cggtts + ':code' ]
rinexObsPath =  mkcggttsCfg['rinex:obs directory' ] # could also come from runsbf2rnxConf 
rinexObsPath = ottplib.MakeAbsolutePath(rinexObsPath,root)
rinexNavPath =  mkcggttsCfg['rinex:nav directory' ]
rinexNavPath = ottplib.MakeAbsolutePath(rinexNavPath,root)

if (mkcggttsCfg['cggtts:naming convention'].lower() == 'plain'):
	cggttsFile = os.path.join(cggttsPath,str(mjd) + '.cctf')
elif (mkcggttsCfg['cggtts:naming convention'].upper() == 'BIPM'):
	X = 'G'
	F = 'Z' # dual frequency is default
	if ('GPS'==constellation):
		X='G'
		if ('C1' == code or 'P1' == code):
			F='M'
	elif ('GLO'==constellation):
		X='R'
		if ('C1' == code or 'P1' == code):
			F='M'
	elif ('BDS'==constellation):
		X='C'
		if ('B1' == code):
			F='M'
	elif ('GAL'==constellation):
		X='E'
		if ('E1' == code ):
			F='M'
	mjdDD = int(mjd/1000)
	mjdDDD = mjd - 1000*mjdDD
	cggttsFile = os.path.join(cggttsPath,'{}{}{}{}{:02d}.{:03d}'.format(X,F,mkcggttsCfg['cggtts:lab id'].upper(),
		mkcggttsCfg['cggtts:receiver id'].upper(),mjdDD,mjdDDD))


# Step 1: generate RINEX observation and navigation files for r2cggtts to digest
Debug('Running runsbf2rnx.py')
try:
	x = subprocess.check_output([RUN_SBF2RNX,'-c',runsbf2rnxConf,str(mjd)]) # eat the output
except:
	ErrorExit('Failed to run runsbf2rnx.py')
Debug(x.decode('utf-8'))
	
# Step 2: generate CGGTTS
Debug('Running mkcggtts.py')
try:
	x = subprocess.check_output([MKCGGTTS,'-c',mkcggttsConf,str(mjd)]) # eat the output
except:
	ErrorExit('Failed to run mkcggtts.py')
Debug(x.decode('utf-8'))

# Step 3: rewrite the summary file

summaryPath = ottplib.MakeAbsolutePath(cfg['main:summary path'],root)
summaryFilename = os.path.join(summaryPath,'{:d}.dat'.format(mjd))
Debug('Updating ' + summaryFilename)

d = ReadCGGTTS(cggttsFile)

if d: # tested
	try:
		fout = open(summaryFilename,'w')
	except:
		Debug('Unable to write ' + fout)

	lastMJD = d[0][0]
	lastTT  = d[0][1]
	nTracks = 0
	refSys  = 0
	for dd in d:
		currMJD = dd[0]
		currTT  = dd[1]
		#print(currMJD,lastMJD,currTT,lastTT)
		if (currMJD == lastMJD and currTT == lastTT):
			refSys  += dd[2]
			nTracks += 1
		else:
			fout.write('{:d} {:d} {:f} {:d}\n'.format(lastMJD,lastTT,refSys/nTracks,nTracks))
			refSys = dd[2]
			nTracks = 1
		
		lastMJD = currMJD 
		lastTT  = currTT
	# Last one
	fout.write('{:d} {:d} {:f} {:d}\n'.format(currMJD,currTT,refSys/nTracks,nTracks))		

	fout.close()

# Clean up, unless debugging is on
if not debug:
	
	# All RINEX
	files = glob.glob(os.path.join(rinexObsPath,'*'))
	for f in files:
		os.unlink(f)
	files = glob.glob(os.path.join(rinexNavPath,'*'))
	for f in files:
		os.unlink(f)
		
	# The cggtts file we just created
	os.unlink(cggttsFile)
