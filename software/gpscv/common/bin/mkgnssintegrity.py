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
from   datetime import datetime,timezone
import os
import re
import subprocess
import sys
import time


# This is where cggttslib is installed
sys.path.append("/usr/local/lib/python3.6/site-packages") # Ubuntu 18.04
sys.path.append("/usr/local/lib/python3.8/site-packages") # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22.04

import ottplib as ottp
import cggttslib as cggttsl
from cggttslib import CGGTTS

import rinexlib

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
gnss = [] # top level of data structure will be a list, with index corresponding to SVN
nav = []

# ------------------------------------------
def ShowVersion():
	print (os.path.basename(sys.argv[0])+" "+VERSION)
	print ('Written by ' + AUTHORS)
	return

# ------------------------------------------
def ParseObs(fname,rnxVer,leapSecs):
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
			ottp.Debug('Finished')
			break
		if l[0] == '>': # beginning of record - extract the timestamp NB this is usually GPS
			hdr = l.split()
			dt = datetime(int(hdr[1]),int(hdr[2]),int(hdr[3]),tzinfo=timezone.utc) 
			currMJD = ottp.MJD(dt.timestamp()) 
			currTOD = 3600*int(hdr[4]) + 60*int(hdr[5]) + int(float(hdr[6])) + leapSecs
			if currTOD >= 86400: # assuming roughly 1 day of data !
				currTOD = 0
				currMJD += 1
		if l[0] == 'G': # GPS only!
			svn = int(l[1:3])
			if gnss[svn] == None: 
				gnss[svn]=[[[currMJD,currTOD],[currMJD,currTOD],0,[],[]]] # create new list
			else:
				currTrk = gnss[svn][-1]
				if ((currMJD - currTrk[1][0])*86400 + (currTOD - currTrk[1][1])) > MAX_OBS_BREAK: # start a new track
					gnss[svn].append([[currMJD,currTOD],[currMJD,currTOD],0,[],[]])
				else:
					gnss[svn][-1][1][0] = currMJD
					gnss[svn][-1][1][1] = currTOD
			
	fin.close()

# ------------------------------------------
def ParseNav(fname,rnxVer):
	fin = open(fname,'r')
	
	while 1:
		l = fin.readline()
		if (len(l) == 0): # EOF 
			reading = False
			break
		if 'END OF HEADER' in l:
			break
	currMJD=-1
	currTOD=-1
	while 1:
		l = fin.readline()
		if (len(l) == 0): # EOF
			ottp.Debug('Finished')
			break
		if l[0] == 'G':
			svn = int(l[1:3])
			# get the time
			hdr = l[0:23].split() # feelin' the love for FORTRAN
			dt = datetime(int(hdr[1]),int(hdr[2]),int(hdr[3]),tzinfo=timezone.utc) 
			currMJD = ottp.MJD(dt.timestamp()) 
			currTOD = 3600*int(hdr[4]) + 60*int(hdr[5]) + int(float(hdr[6])) 
			if currTOD >= 86400: # assuming roughly 1 day of data !
				currTOD = 0
				currMJD += 1
			for i in range(1,6):
				fin.readline()
			l = fin.readline()
			# health is in columns 23-41
			health = int(float(l[23:42]))
			
			if nav[svn] == None: 
				nav[svn]=[[[currMJD,currTOD],health]] # create new list
			else:
				nav[svn].append([[currMJD,currTOD],health])
			


# ------------------------------------------

home = os.environ['HOME'] 
root = home 
configFile = os.path.join(root,'etc','mkgnssintegrity.conf') # try this first
cfgFormat = MKCGGTTS_FORMAT
appName = os.path.basename(sys.argv[0])
examples=''
create = True

cggttsl.SetDebugging(True)
cggttsl.SetWarnings(True)

if ottp.LibMajorVersion() >= 0 and ottp.LibMinorVersion() < 2: # a bit redundant since this will fail anyway on older versions of ottplib ...
	print('ottplib minor version < 2')
	sys.exit(1)
	
parser = argparse.ArgumentParser(description='Generate a GNSS integrity file from RINEX and CGGTTS',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)
parser.add_argument('mjd',nargs = '*',help='first MJD [last MJD] (if not given, the MJD of the previous day is used)')
parser.add_argument('--config','-c',help='use an alternate configuration file')
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

debug = args.debug
ottp.SetDebugging(debug)
rinexlib.SetDebugging(debug)

if (args.version):
	ShowVersion()
	exit()

if (args.config):
	configFile = args.config
	if (not os.path.isfile(configFile)):
		ErrorExit(configFile + ' not found')
		
ottp.Debug('Using ' + configFile)

cfg=ottp.Initialise(configFile,['main:processing tool','main:processing config','main:reported outputs'])

startMJD = ottp.MJD(time.time()) - 1 # previous day
stopMJD  = startMJD
	
if (args.mjd):
	if 1 == len(args.mjd):
		startMJD = int(args.mjd[0])
		stopMJD  = startMJD
	elif ( 2 == len(args.mjd)):
		startMJD = int(args.mjd[0])
		stopMJD  = int(args.mjd[1])
		if (stopMJD < startMJD):
			ottp.ErrorExit('Stop MJD is before start MJD')
	else:
		ottp.ErrorExit('Too many MJDs')

# Boilerplate done

tool = cfg['main:processing tool'].lower()
if tool == 'mkcggtts':
	cfgFormat = MKCGGTTS_FORMAT
elif tool == 'gpscv':
	cfgFormat = GPSCV_FORMAT
else:
	ottp.ErrorExit('Unknown option ' + tool)

toolConfigFile = ottp.MakeAbsoluteFilePath(cfg['main:processing config'],root,os.path.join(home,'etc'))
tcfg = ottp.Initialise(toolConfigFile,[])

# A bit more checking ...
repOutputs = cfg['main:reported outputs'].split(',')
repOutputs = [r.strip() for r in repOutputs]
repOutputs = [r.lower() for r in repOutputs]

if cfgFormat == MKCGGTTS_FORMAT:
	rnxVer = int(tcfg['rinex:version'])
	obsSta = tcfg['rinex:obs sta']
	obsDir = tcfg['rinex:obs directory']
	navSta = tcfg['rinex:nav sta']
	navDir = tcfg['rinex:nav directory']
	
	cggttsNaming = tcfg['cggtts:naming convention']
	#cggttsOutputs = tcfg['cggtts:outputs'].split(',')
	#cggttsOutputs = [c.strip() for c in cggttsOutputs]
	#cggttsOutputs = [c.lower() for c in cggttsOutputs]
	
else:
	ottp.ErrorExit('GPSCV format not supported yet')
	
obsDir = ottp.MakeAbsolutePath(obsDir,root)
navDir = ottp.MakeAbsolutePath(navDir,root)

ottp.Debug('RINEX version is {:d}'.format(rnxVer))

# Data structure for GNSS observations

# List (index corresponds to SV) - fixed size
# 	List of SV tracks - variable size
#     Track start (from OBS)
#     Track finish (from OBS)
#     Health (from NAV) - initially unset
#     List of REFSV   corresponding to the track interval (from CGGTTS)
#     List of REFGPS  corresponding to the track interval (from CGGTTS)
lat = cfg['main:latitude']
lon = cfg['main:longitude']
ht  = cfg['main:height']

for mjd in range(startMJD,stopMJD + 1):
	
	gnss = []
	nav  = []
	
	for rep in repOutputs:
	
		if not (rep + ':constellation' in tcfg):
			Debug(rep + ':constellation is missing')
			continue
		gnssName = tcfg[rep + ':constellation'].upper()
		
		if not (rep + ':code' in tcfg):
			Debug(rep + ':code is missing')
			continue
		gnssCode = tcfg[rep + ':code']
		
		for s in range(1,MAXSV+1):
			gnss.append(None)
			nav.append(None)
			
		tmjd =  (mjd - 40587)*86400
		utc = datetime.utcfromtimestamp(tmjd)
		doy = int(utc.strftime('%j')) # zero padded
		yyyy  = int(utc.strftime('%Y'))
		
		# Read the RINEX navigation file
		# This gives us SV health
		[baseName,zext] = rinexlib.FindNavigationFile(navDir,navSta,yyyy,doy,rnxVer,False)
		if not baseName:
			ottp.Debug(' .. skipped')
			continue
		ottp.DecompressFile(baseName,zext)
		nLeapSecs = rinexlib.GetLeapSeconds(baseName,rnxVer)
		ParseNav(baseName,rnxVer)
		ottp.RecompressFile(baseName,zext)

		# Read the RINEX observation file
		# This gives us visible satellites and track lengths
		[baseName,zext] = rinexlib.FindObservationFile(obsDir,obsSta,yyyy,doy,rnxVer,False)
		if not baseName:
			ottp.Debug(' .. skipped')
			continue
		ottp.DecompressFile(baseName,zext)
		ParseObs(baseName,rnxVer,nLeapSecs)
		ottp.RecompressFile(baseName,zext) # note that this will not compress if the file was initially uncompressed

		# Read the RAW CGGTTS file
		# This gives us REF-SV and REF-GPS(SV)
		
		# But first, construct a file name
		namingConvention = tcfg['cggtts:naming convention'].lower()
		if namingConvention == 'bipm':
			ext = ''
			if gnssName == 'GPS': # FIXME GPS only
				cCode = 'G'
			#elif gnssName == 'GALILEO':
			#	cCode = 'E'
			else:
				cCode = 'G'
			prefix = cCode + 'M' + tcfg['cggtts:lab id']+tcfg['cggtts:receiver id']
		elif namingConvention == 'plain':
			ext='cctf'
			prefix=''
		else: # try plain anyway
			ext='cctf'
			prefix=''
		cggttsPath = ottp.MakeAbsolutePath(tcfg[rep + ':ctts directory'],root)
		cggttsFile = cggttsl.FindFile(cggttsPath,prefix,ext,mjd)
		if not cggttsFile:
			Debug("Skipping: can't find a CGGTTS file for MJD {:d}".format(mjd))
			continue
		
		ottp.Debug('Using CGGTTS file ' + cggttsFile)
		cf = CGGTTS(cggttsFile,mjd)
		cf.Read()
		
		# Reorganize CGGTTS to index it by PRN
		# This will cut down on search time later 
		
		cbyprn = []
		for s in range(1,MAXSV+1):
			cbyprn.append([])
			
		for trk in cf.tracks:
			prn = int((trk[cf.PRN])[1:3])
			cbyprn[prn].append(trk)
				
		html = ''
		html += '<title>{} Space Vehicle Time Integrity for MJD {:d}, </title>\n'.format(gnssName,mjd)
		html += '<h2>{} Space Vehicle Time Integrity for MJD {:d}, </h2>\n'.format(gnssName,mjd)
		html += '<br>As viewed from ' + cfg['main:description'] +'</br>\n'
		html += '<br>Latitude {}, longitude {}, height {} m </br>\n'.format(lat,lon,ht)

		for prn in range(1,33):
			if ((not gnss[prn])):
				# This typically means that the SV was unhealthy, so doesn't appear in the RINEX observation file
				# Check the NAV data and add a record tagging unhealthy for the entire day
				ottp.Debug('{:d} -no OBS data - checking health'.format(prn))
				if nav[prn] == None:
					ottp.Debug('No NAV data')
					html += '{:d} {}\n'.format(prn,'no data')
				else:
					nUnhealthy = 0
					for n in nav[prn]:
						if n[1] > 0:
							nUnhealthy += 1
					ottp.Debug('{:d} unhealthy NAV records out of {:d}'.format(nUnhealthy,len(nav[prn])))
					if nUnhealthy >= 1:
						html += '{:d} {}\n'.format(prn,'UNHEALTHY')
				continue
			
			trkCount = 0
			for t in range(0,len(gnss[prn])): # for each track recorded for the SV
				trk = gnss[prn][t]
				if (trk[1][0] - trk[0][0])*86400 +  trk[1][1] - trk[0][1] > MIN_TRK_LEN: 
					trkCount += 1
					startMJD = trk[0][0]
					starts = trk[0][1]
					start = ''
					stop = ''
					if startMJD < mjd : # shouldn't happen
						start = '00:00'
						starts = 0
					else:
						hh = int(starts/3600)
						mm = int((starts - hh*3600)/60)
						start = '{:02d}:{:02d}'.format(hh,mm)
			
					stopMJD = trk[1][0]
					stops  = trk[1][1]
					if stopMJD > mjd: # will happen!
						stop = '23:59'
						stops = 86399
					else:
						hh = int(stops/3600)
						mm = int((stops - hh*3600)/60)
						stop = '{:02d}:{:02d}'.format(hh,mm)
					trklenhh = int((stops - starts)/3600)
					trklenmm = int((stops - starts - trklenhh*3600)/60)
					
					# Could be tricky here and try to remember where we are in the list of CGGTTS tracks but let's not do that shall we ?
					# Just start the beginning
					
					intMatches = [] # list of tracks in the reported interval
					for ct in cbyprn[prn]:
						cMJD = ct[cf.MJD]
						cTOD = ct[cf.STTIME]
						if (cMJD <= stopMJD and cMJD >= startMJD and cTOD <= stops and cTOD >=starts):
							intMatches.append(ct)
					
					nMatches     = len(intMatches)
					
					
					if nMatches < 2:
						Debug("Insufficient CGGTTS tracks")
						continue
					
					# Pass 1: get the mean
					avREFSV   = 0.0
					avREFGPS  = 0.0
					for m in intMatches:
						avREFSV  += m[cf.REFSV]
						avREFGPS += m[cf.REFGPS]
					avREFSV = avREFSV/nMatches
					avREFGPS = avREFGPS/nMatches
					
					# Pass 2: get the SD
					sdREFSV   = 0.0
					sdREFGPS  = 0.0
					for m in intMatches:
						sdREFSV  += (m[cf.REFSV] - avREFSV)**2
						sdREFGPS += (m[cf.REFGPS]- avREFGPS)**2
					sdREFSV = (sdREFSV/(nMatches - 1))**0.5	
					sdREFGPS = (sdREFGPS/(nMatches - 1))**0.5	
					
					# Pass 3: count outliers
					n500 = 0
					n4sd = 0
					for m in intMatches:
						if abs(m[cf.REFSV] - avREFSV) >500.0:
							n500 += 1
						if abs(m[cf.REFSV] - avREFSV) > 4.0*sdREFSV:
							n4sd += 1
						
					html += '<tr>{:d} {:d} {} {} {:d}h {:d}m {:d} {:g} {:g} {:g} {:g} {:g}</tr>\n'.format(prn,trkCount,start,stop,trklenhh,trklenmm,nMatches,avREFSV,avREFGPS,sdREFSV,n4sd,n500)
			
		print(html)
