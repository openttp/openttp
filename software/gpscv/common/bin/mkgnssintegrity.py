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
# Contructs an SV integrity file for a GNSS
# 

import argparse
from   datetime import datetime,timezone
import math as m
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

VERSION = "1.0.1"
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
refclk = 'REF'
outputDirectory = ''

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
cggttsl.SetDebugging(debug)
cggttsl.SetWarnings(debug)

if (args.version):
	ShowVersion()
	exit()

if (args.config):
	configFile = args.config
	if (not os.path.isfile(configFile)):
		ErrorExit(configFile + ' not found')
		
ottp.Debug('Using ' + configFile)

cfg=ottp.Initialise(configFile,['main:processing tool','main:processing config','main:reported outputs','main:output path'])

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

if 'main:reference' in cfg:
	refclk = cfg['main:reference']
	
# A bit more checking ...
repOutputs = cfg['main:reported outputs'].split(',')
repOutputs = [r.strip() for r in repOutputs]
repOutputs = [r.lower() for r in repOutputs]

if cfgFormat == MKCGGTTS_FORMAT:
	rnxVer = int(tcfg['rinex:version'])
	navSta = tcfg['rinex:nav sta']
	navDir = tcfg['rinex:nav directory']
	
	cggttsNaming = tcfg['cggtts:naming convention']
	#cggttsOutputs = tcfg['cggtts:outputs'].split(',')
	#cggttsOutputs = [c.strip() for c in cggttsOutputs]
	#cggttsOutputs = [c.lower() for c in cggttsOutputs]
	
else:
	ottp.ErrorExit('GPSCV format not supported yet')
	
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

outputPath = ottp.MakeAbsolutePath(cfg['main:output path'],root)

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
		
		# Now run over the CGGTTS, identifying contiguous tracks, building up gnss[]
		for cg in cf.tracks:
			prn      = int(cg[cf.PRN][1:3])
			currMJD  = cg[cf.MJD]
			currTOD  = cg[cf.STTIME]
			if gnss[prn] == None: 
				gnss[prn]=[ [[currMJD,currTOD],[currMJD,currTOD],0,[cg[cf.REFSV]],[cg[cf.REFGPS]]] ] # create new list
			else:
				currTrk = gnss[prn][-1]
				if ((currMJD - currTrk[1][0])*86400 + (currTOD - currTrk[1][1])) > MAX_OBS_BREAK: # start a new track, but no filtering on MIN_TRK_LEN as yet
					gnss[prn].append([[currMJD,currTOD],[currMJD,currTOD],0,[cg[cf.REFSV]],[cg[cf.REFGPS]]])
				else:
					# update the track end time
					gnss[prn][-1][1][0] = currMJD
					gnss[prn][-1][1][1] = currTOD
					# and append new REFSV etc
					gnss[prn][-1][3].append(cg[cf.REFSV])
					gnss[prn][-1][4].append(cg[cf.REFGPS])
				
		html = '<!DOCTYPE html>'
		html += '<html>'
		html += '<head>'
		html += '<style>'
		html += 'body {font-family:"Verdana", Geneva, sans-serif;}'
		html += 'tr {text-align:right;}'
		html += 'th,td {padding-left:10px;padding-right:10px;}'
		html += 'div {padding-top:12px;padding-bottom:12px;}'
		html += '</style>'
		html += '<title>{} Space Vehicle Time Integrity for MJD {:d}</title>'.format(gnssName,mjd)
		html += '</head>'
		
		html += '<body>'
		
		html += '<h2>{} Space Vehicle Time Integrity for MJD {:d}</h2>'.format(gnssName,mjd)
		html += '<div>As viewed from ' + cfg['main:description'] +'<br>'
		html += 'Latitude {}, longitude {}, height {} m </div>'.format(lat,lon,ht)
		html += '<table style="border: 1px solid black;">'
		html += '<tr style="text-align:center;background-color:#e0e0e0"><th>A</th><th>B</th><th>C</th><th>D</th><th>E</th><th>F</th><th>G</th><th>H</th><th>I</th><th>J</th><th>K</th></tr>'
		html += '<tr style="white-space:pre;text-align:center;vertical-align:top;background-color:#e0e0e0;"><th>PRN</th><th>Block</th><th>Start</th><th>Stop</th><th>Interval</th><th>Measurements</th><th>'+refclk+'- <br>GPS (ns)</th><th>'+refclk+'- <br>SV (ns)</th><th>Std. dev. <br> (ns)</th><th>Outliers <br>&gt 4 std. dev. </th><th>Outliers <br>&gt; 500 ns</th></tr>'
		
		for prn in range(1,33):
			if ((not gnss[prn])):
				# This typically means that the SV was unhealthy, so doesn't appear in the RINEX observation file and thus the CGGTTS file
				# Check the NAV data and add a record tagging unhealthy for the entire day
				ottp.Debug('{:d} -no CGGTTS data - checking health'.format(prn))
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
						html += '<tr><td>{:d}</td><td colspan="10" style="text-align:center;">UNHEALTHY</td></tr>\n'.format(prn)
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
					todStr = '{:2d}h {:2d}m'.format(trklenhh,trklenmm) # FIXME html eats any extra whitespace
					nMeas = len(trk[3])
					
					if nMeas < 2:
						Debug("Insufficient CGGTTS tracks")
						continue
					
					# Pass 1: get the mean
					avREFSV   = 0.0
					avREFGPS  = 0.0
					for meas in trk[3]:
						avREFSV  += meas
					for meas in trk[4]:
						avREFGPS += meas
					avREFSV = avREFSV/nMeas
					avREFGPS = avREFGPS/nMeas
					
					# Stats are on REFSV because this shows noise better, unlike REFGPS
					# Pass 2: get the SD
					sdREFSV   = 0.0
					sdREFGPS  = 0.0
					for meas in trk[3]:
						sdREFSV  += (meas - avREFSV)**2
					for meas in trk[4]:
						sdREFGPS  += (meas - avREFGPS)**2
						
					sdREFSV = m.ceil((sdREFSV/(nMeas - 1))**0.5)	 # do all calculatiosn with rounded up sd, for consistency with output value
					
					# Pass 3: count outliers
					n500 = 0
					n4sd = 0
					for meas in trk[3]:
						if abs(meas - avREFSV) >500.0:
							n500 += 1
						if abs(meas - avREFSV) > 4.0*sdREFSV:
							n4sd += 1
					#if prn % 2 == 1:
						#rowStyle = ' style="background-color:#e0e0e0"';
					#else:
						#rowStyle = '';
					rowStyle=''
					if trkCount == 1:
						html += '<tr'+rowStyle+'><td>{:d}</td><td>{:d}</td><td>{}</td><td>{}</td><td>{}</td><td>{:d}</td><td>{:g}</td><td>{:g}</td><td>{:g}</td><td>{:g}</td><td>{:g}</td></tr>\n'.format(prn,trkCount,start,stop,todStr,nMeas,int(round(avREFGPS)),int(round(avREFSV)),int(sdREFSV),n4sd,n500)
					else:
						html += '<tr'+rowStyle+'><td></td><td>{:d}</td><td>{}</td><td>{}</td><td>{}</td><td>{:d}</td><td>{:g}</td><td>{:g}</td><td>{:g}</td><td>{:g}</td><td>{:g}</td></tr>\n'.format(trkCount,start,stop,todStr,nMeas,int(round(avREFGPS)),int(round(avREFSV)),int(sdREFSV),n4sd,n500)
		
		html += '</table>'
		
		# Make the footer
		html += '<ol type = "A">'
		html += '<li>Space vehicle identification number.</li>'
		html += '<li>The period when the satellite was in view. Each of these blocks is a sequence of measurements longer than 60 minutes, containing no breaks of longer than 60 seconds.</li>'
		html += '<li>Time at the start of the block (UTC).</li>'
		html += '<li>Time at the end of the block (UTC).</li>'
		html += '<li>Length of block.</li>'
		html += '<li>Number of pseudorange measurements in the block, sampled every 30 s.</li>'
		html += '<li>Average value of ' + refclk + ' minus GPS satellite time during the block, in nanoseconds.</li>'
		html += '<li>Average value of ' + refclk + ' minus space vehicle time during the block, in nanoseconds.</li>'
		html += '<li>Standard deviation of H during the block, in nanoseconds.</li>'
		html += '<li>Number of measurements more than 4 standard deviations from the average.</li>'
		html += '<li>Number of measurements more than 500 ns from the average.</li>'
		html += '</ol>'
		
		html += '<div>'
		html += 'Pseudoranges have been corrected using the broadcast ephemerides and ionosphere model.'
		html += '</div>'
		
		html += '<div>'
		html += 'Generated by mkgnssintegrity.py v' + VERSION
		html += '</div>'
		html += '</body>'
		html += '</html>'
		
		# 
		fname = os.path.join(outputPath,'{:5d}.html'.format(mjd))
		fout  = open(fname,'w')
		fout.write(html)
		fout.close()
		ottp.Debug('Wrote ' + fname)
