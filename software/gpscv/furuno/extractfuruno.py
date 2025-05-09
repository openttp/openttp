#!/usr/bin/python3
#

#
# The MIT License (MIT)
#
# Copyright (c) 2025 Michael J. Wouters
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
# extractfuruno.py - a script to decode Furnuno GPSDO log files
#
#

import argparse
import datetime
import glob
import os
import re
import sys

# This is where ottplib is installed
sys.path.append('/usr/local/lib/python3.6/site-packages')  # Ubuntu 18
sys.path.append('/usr/local/lib/python3.8/site-packages')  # Ubuntu 20
sys.path.append('/usr/local/lib/python3.10/site-packages') # Ubuntu 22

import ottplib as ottp
import time

VERSION = '0.0.1'
AUTHORS = "Michael Wouters"

# Output time stamp formats
TS_UNIX = 0
TS_MJD = 1
TS_DATE = 2

def ParseMessage(msg,msgName,nDataFields):
	msgNameLen = len(msgName)
	
	if len(msg) > msgNameLen:
		if msg[0:msgNameLen] == msgName:
			msgData = msg.split(',')
			if len(msgData) - 1 == nDataFields: # don't count the message name
				return msgData[1:nDataFields+1]
	
	return []
# ------------------------------------------
# Main 
# ------------------------------------------

home =os.environ['HOME'] + '/'
#configFile = os.path.join(home,'etc/furuno.conf')
dataPath = './'
dataExt   = 'gpsdo'
tsFormat = TS_MJD

tt = time.time()
mjd = ottp.MJD(tt) - 1 # previous day
compress=False

parser = argparse.ArgumentParser(description='Extract messages from a Furuno GPSDO log file')
parser.add_argument('mjd',nargs = '*',help='first MJD [last MJD]')
# parser.add_argument('--config','-c',help='use this configuration file',default=configFile)
parser.add_argument('--timestamp',default='mjd')
parser.add_argument('--comment',help='add comment to header',action='store_true')
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')

parser.add_argument('--gns',help='GNSS fix data (GNS)',action='store_true')
parser.add_argument('--cry',help='positioning and traim (PERDCRY)',action='store_true')
parser.add_argument('--crz',help='timing (PERDCRZ) ',action='store_true')

parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug
ottp.SetDebugging(debug)

#configFile = args.config

#if (os.path.isfile(configFile)):    # use is optional
	#cfg = ottp.Initialise(configFile,['data:path','data:ext'])
	#dataPath = cfg['data:path']
	#dataExt  = cfg['data:ext']

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

tmp = args.timestamp.lower()
if tmp == 'unix':
	tsFormat = TS_UNIX
	tsSpacer = ' ' * (10-2)
elif tmp == 'mjd':
	tsFormat = TS_MJD
	tsSpacer = ' '  * (11-2)
elif tmp == 'date':
	tsFormat = TS_DATE
	tsSpacer = ' ' * (19-2)
	
tsLast = -1
nDropped = 0
gotMessage = False # a requested message may be missing so need to check ...
dataStr = ''

if args.comment:
	if args.gns:
		print(f'# {tsSpacer }')
	if args.cry:
		print(f'# {tsSpacer } TRAIM_sol TRAIM_status')
	if args.crz:
		print(f'# {tsSpacer } freqMode  pps_error(ns) freq_error(ppb)')

for m in range(startMJD,stopMJD+1):
	baseName = f'{m}.{dataExt}'
	basePath = os.path.join(dataPath,baseName)
	recompress = False
	ottp.Debug(f'Processing {basePath}')
	
	if not os.path.isfile(basePath):
		zPath = basePath + '.gz'
		if not os.path.isfile(zPath):
			ottp.Debug(f'{basePath} is missing')
			continue
		else:
			ottp.DecompressFile(basePath,'.gz')
			recompress = True
	fin = open(basePath,'r')
	for l in fin:
		l = l.strip()
		# GNRMC messages define the start of each message block
		msgData = ParseMessage(l,'$GNRMC',13)
		if msgData:
			hh = int(msgData[0][0:2])
			mm = int(msgData[0][2:4])
			ss = int(msgData[0][4:6])
			TOD= hh*3600+mm*60 + ss
			
			dd  = int(msgData[8][0:2])
			mon = int(msgData[8][2:4])
			yyyy  = int(msgData[8][4:6]) + 2000
			
			mjd = ottp.MJD(datetime.datetime(yyyy,mon,dd,tzinfo=datetime.timezone.utc).timestamp())
						
			ts = datetime.datetime(yyyy,mon,dd,hh,mm,ss,tzinfo=datetime.timezone.utc).timestamp()
			# If there is more than one second between ZDA messages, dump any accumulated data
			if ts == tsLast + 1:
				if timeFix and gotMessage:
					print(dataStr)
					gotMessage = False
				if tsFormat == TS_UNIX:
					dataStr = f'{ts} '
				elif tsFormat == TS_MJD:
					dataStr = '{:12.6f} '.format(mjd + TOD/86400.0)
				elif tsFormat == TS_DATE:
					dataStr = f'{yyyy:04d}-{mon:02d}-{dd:02d} {hh:02d}:{mm:02d}:{ss:02d} '
			elif tsLast == -1: # start the ball rolling, possibly again
				if tsFormat == TS_UNIX:
					dataStr = f'{ts} '
				elif tsFormat == TS_MJD:
					dataStr = '{:12.6f} '.format(mjd + TOD/86400.0)
				elif tsFormat == TS_DATE:
					dataStr = f'{yyyy:04d}-{mon:02d}-{dd:02d} {hh:02d}:{mm:02d}:{ss:02d} '
			else:
				nDropped += 1
				tsLast = -1
				gotMessage = False
				ottp.Debug(f'Gap/bad time {ts} {tsLast}')
				continue
			tsLast = ts
			continue
		else:
			
			# Check whether we have good time ...
			msgData = ParseMessage(l,'$PERDCRW',9)
			if msgData:
				if msgData[2] == '2':
					timeFix = True
				else:
					timeFix = False
			
			if args.cry:
				msgData = ParseMessage(l,'$PERDCRY',11)
				if msgData:
					# fields (counting from 1) 7= TRAIM solution 8 = TRAIM status
					dataStr += f'{msgData[6]} {msgData[7]}' 
					gotMessage = True
					continue
					
			if args.crz:
				msgData = ParseMessage(l,'$PERDCRZ',11)
				if msgData:
					# fields (counting from 1) 2=freq mode,6=pps timing error (ns),7=freq error (ppb)
					dataStr += f'{msgData[1]} {msgData[5]} {msgData[6]}' 
					gotMessage = True
					continue
					
			if args.gns:
				msgData = ParseMessage(l,'$GNGNS',13)
				if msgData:
					# fields (counting from 1) 7=nsats in use
					dataStr += f'{msgData[6]}' 
					gotMessage = True
					continue
					
	fin.close()
	if recompress:
		ottp.RecompressFile(basePath,'.gz')
		
ottp.Debug(f'Number of messages dropped = {nDropped}')
