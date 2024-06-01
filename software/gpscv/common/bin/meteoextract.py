#!/usr/bin/python3
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

import argparse
import datetime
import math
import numpy as np
import os
import re
import sys
import time

# This is where ottplib is installed
sys.path.append("/usr/local/lib/python3.6/site-packages")  # Ubuntu 18.04
sys.path.append("/usr/local/lib/python3.8/site-packages")  # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22.04

try: 
	import ottplib as ottp
except ImportError:
	sys.exit('ERROR: Must install ottplib\n eg openttp/software/system/installsys.py -i ottplib')

VERSION = "0.0.1"
AUTHORS = "Michael Wouters"

OBS_PR = 0   # pressure
OBS_TE = 1   # external dry temperature
OBS_TI = 2   # internal dry temperature
OBS_HE = 3   # external relative humidity
OBS_HI = 4   # internal relative huimidity
OBS_TI_T = 5 # internal dry temperature for TW sensor
OBS_TI_G = 6 # internal dry temperature for GNSS sensor
HI_T = 7 		 # internal relative humidity for TW sensor
HI_G = 8     # internal relative humidity for GNSS sensor

MODE_EXTRACT_ALL = 0
MODE_DAILY_STATS = 1

lab = 'au'   # there's no place like home
inpath = './'

mode = MODE_DAILY_STATS

examples = 'You wish'

parser = argparse.ArgumentParser(description='Extract meteo data, average and write to a single file',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

parser.add_argument('mjd',nargs = '*',help='first MJD [last MJD]')

parser.add_argument('--inpath',help='path to input files')

parser.add_argument('--lab',help='lab identifier')

group = parser.add_mutually_exclusive_group()
group.add_argument('--dailystats',help='daily average etc (default)',action='store_true')
group.add_argument('--extractall',help='extract all measurements',action='store_true')


parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug
ottp.SetDebugging(debug)

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

if args.inpath:
	inpath = args.inpath

if args.lab:
	lab = args.lab.lower()

if args.dailystats:
	mode = MODE_DAILY_STATS
elif args.extractall:
	mode= MODE_EXTRACT_ALL

obsIndex      = [-1,-1,-1,-1,-1,-1,-1,-1,-1]
dailyStats= [[0.0,0.0,0.0,0],[0.0,0.0,0.0,0],[0.0,0.0,0.0,0],[0.0,0.0,0.0,0],[0.0,0.0,0.0,0],[0.0,0.0,0.0,0],[0.0,0.0,0.0,0],[0.0,0.0,0.0,0],[0.0,0.0,0.0,0]]

for mjd in range(startMJD,stopMJD+1):
	mm = int(mjd/1000)
	mmm = mjd - mm * 1000
	fname = 'met{}{:02d}.{:03d}'.format(lab,mm,mmm)
	fname = os.path.join(inpath,fname)
	if (not os.path.isfile(fname)):
		ottp.Debug(fname + ' is missing')
		continue
	ottp.Debug('Opening ' + fname)
	fin = open(fname,'r')

	readingHeader = True
	for a in range(0,9):
		dailyStats[a] =  [0.0,0.0,0.0,0]

	while True:
		l = fin.readline()
		if l:
			if readingHeader:
				if re.search('TYPES OF OBSERV',l):
					try:
						nobs = int(l[0:6])
						for iobs in range(0,nobs):
							obsType = l[6 + iobs*6 + 2:6 + (iobs+1)*6].strip()
							if obsType == 'TE':
								obsIndex[OBS_TE] = iobs
							elif obsType == 'TI':
								obsIndex[OBS_TI] = iobs
					except:
						ottp.ErrorExit('Parsing error in '  + l)
						sys.exit(1)
						
				if re.search('END OF HEADER',l):
					# Check that the required fields are present
					#if obsIndex[OBS_TE] == -1 or obsIndex[OBS_TI] == -1:
						#ottp.ErrorExit('A required observation is missing')
					readingHeader = False
				continue
			else:
				# Read a record
				yyyy= int(l[1:3])
				if (yyyy >= 80):
					yyyy += 1900
				else:
					yyyy += 2000
				mon = int(l[3:6])
				dd = int(l[6:9])
				hh = int(l[9:12])
				mm = int(l[12:15])
				ss = int(l[15:18])
				dmjd = ottp.MJD(datetime.datetime(yyyy,mon,dd,tzinfo=datetime.timezone.utc).timestamp()) # don't assume that data is confinde to MJD of file name 
				tod = hh*3600 + mm*60 + ss
				# FIXME
				# Extract the required observations
				currObs = [9999.9,9999.9,9999.9,9999.9,9999.9,9999.9,9999.9,9999.9,9999.9]
				for obs in range(0,9):
					iObs = obsIndex[obs]
					if (iObs > -1):
						currObs[obs]= float(l[18 + iObs*7:18 + (iObs + 1)*7])
						
				if (mode == MODE_EXTRACT_ALL):
					print('{:d} {:d} {:g} {:g}'.format(dmjd,tod,currObs[OBS_TE],currObs[OBS_TI]))
				
				if (mode == MODE_DAILY_STATS):
					for iObs in range(0,9):
						if currObs[iObs] < 9999:
							dailyStats[iObs][0] += currObs[iObs] 
							dailyStats[iObs][3] += 1
						
		else:
			break
	
	if (mode == MODE_DAILY_STATS):
		outStr = '{:d} '.format(mjd)
		if dailyStats[OBS_TE][3] > 0:
			outStr += '{:g} '.format(dailyStats[OBS_TE][0]/dailyStats[OBS_TE][3])
		else:
			outStr += '9999.9 '
		if dailyStats[OBS_TI][3] > 0:
			outStr += '{:g} '.format(dailyStats[OBS_TI][0]/dailyStats[OBS_TI][3])
		else:
			outStr += '9999.9 '
		print(outStr)
		
		
