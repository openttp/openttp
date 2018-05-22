#!/usr/bin/python
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
#
# gnssreport.py - Reports on a GNSS receiver's operation
#
# Modification history
# 2018-05-22 MJW First version
#
# TODO
# adev, delays, allow reporting  centred on 0 UTC

import argparse
import calendar
import os
import numpy as np
import re
import sys
import time

# This is where ottplib is installed
sys.path.append("/usr/local/lib/python2.7/site-packages")
import ottplib

VERSION = "0.1"
AUTHORS = "Michael Wouters"

# Globals
debug = False

# ------------------------------------------
def ShowVersion():
	print  os.path.basename(sys.argv[0]),'version',VERSION
	print "Written by",AUTHORS
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		print msg
	return

# ------------------------------------------
def Warn(msg):
	if (not args.nowarn):
		sys.stderr.write('Warning: '+msg+'\n')
	return

# ------------------------------------------
def ErrorExit(msg):
	print msg
	sys.exit(0)
	
# ------------------------------------------
def LoadConfig(configFile,reqd):
	Debug('Opening ' + configFile)
	cfg=ottplib.LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit("Error loading " + configFile)
		
	# Check for required arguments
	for k in reqd:
		if (not cfg.has_key(k)):
			ErrorExit("The required configuration entry \'" + k + "\' is undefined")
		
	return cfg

# ------------------------------------------
def LoadRxLogFile(fname):
	Debug('Opening ' + fname)
	
	restarts=[]
	
	try:
		fin = open(fname,'r')
	except:
		Warn('Unable to load ' + fname)
		return restarts
	
	for l in fin:
		match=re.match('^(\d{2}/\d{2}/\d{2})\s+(\d{2}):(\d{2}):(\d{2})\s+.+\s+restarted',l)
		if (match):
			tod=int(match.group(2))*3600 + int(match.group(3))*60 + int(match.group(4))
			mjd = ottplib.MJD(calendar.timegm(time.strptime(match.group(1), "%d/%m/%y")))
			restarts.append(mjd+tod/86400.0)
			
	fin.close()
	
	return restarts

# ------------------------------------------
def LoadCounterFile(fname,mjd,delay):
	Debug('Loading ' + fname)
	data=[]
	try:
		fin = open(fname,'r')
	except:
		Warn('Unable to load ' + fname)
		return data
	
	# Eat the header
	# Use the delay defined in the header if available
	lineCount=0
	
	for l in fin:
		lineCount += 1
		l.rstrip()
		match = re.match('^(\d{2}):(\d{2}):(\d{2})\s+(.+)',l)
		if (match):
			tod=int(match.group(1))*3600 + int(match.group(2))*60 + int(match.group(3))
			data.append([mjd+tod/86400.0,float(match.group(4))-delay])
		else:
			Warn('Bad data at line ' + str(lineCount) + ' in ' + fname)
			
	fin.close()
	return data

# ------------------------------------------
# Main 
# ------------------------------------------

home =os.environ['HOME'] + os.sep
configFile = os.path.join(home,'etc','gnssreport.conf')

parser = argparse.ArgumentParser(description='Reports on a GNSS receiver\'s operation')
parser.add_argument('MJD',nargs='+',help='MJD',type=int)
parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--nowarn',help='suppress warnings',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')
parser.add_argument('--summary',help='show a short summary of operation',action='store_true')
parser.add_argument('--sequence','-s',help='interpret input mjds as a sequence',action='store_true')
args = parser.parse_args()

debug = args.debug

if (args.version):
	ShowVersion()
	exit()

configFile = args.config;

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + " not found")
	
cfg=LoadConfig(configFile,[':receivers'])

receivers = cfg[':receivers'].split(',')

# Build a list of MJDs to report on
mjds=[];
if (args.sequence):
	if (2==len(args.MJD)):
		start = args.MJD[0]
		stop = args.MJD[1]
		if (start > stop):
			start = args.MJD[1]
			stop = args.MJD[0]
		for m in range(start,stop+1):
			mjds.append(m)
	else:
		sys.stderr.write('Two MJDs are needed for the --sequence option\n')
		exit()
else:
	for m in range(0,len(args.MJD)):
		mjds.append(args.MJD[m])
		
for rx in receivers:

	rx.strip();
	
	Debug('Checking ' + rx)
	
	configFile = ottplib.MakeAbsoluteFilePath(cfg[rx + ':gpscv configuration'],home,os.path.join(home,'etc'));
	
	gpscvCfg=LoadConfig(configFile,[])
	
	dataPath= ottplib.MakeAbsolutePath(gpscvCfg['paths:counter data'], home)

	ctrExt = gpscvCfg['counter:file extension']
	if (None == re.search(r'\.$',ctrExt)):
		ctrExt = '.' + ctrExt 
			
	# Load the log file for the receiver to identify  any restarts
	rxLogFile = ottplib.MakeAbsoluteFilePath(cfg[rx + ':log file'],home,os.path.join(home,'logs'));
	restarts = LoadRxLogFile(rxLogFile)
	
	# Trim restarts to the period of interest to reduce number of tests
	min = mjds[0]
	max = mjds[0]
	for m in mjds:
		if (m<min):
			min=m
		if (m > max):
			max=m
	
	holdoff = int(cfg[rx+':restart holdoff'])
	tmp = []
	for r in restarts:

		if ((r >= min - holdoff/86400.0) and (r <= max+1)): 
			tmp.append(r)
			
	restarts = tmp
	
	# And now load the counter data files(s) 
	ppsOffset = float(gpscvCfg['receiver:pps offset']) # in ns
	rxObs     = gpscvCfg['receiver:observations']
	Debug('pps offset = ' + str(ppsOffset))
	
	delay = ppsOffset
	nRestarts = 0
	
	data=[]
	for m in mjds:
		fname = os.path.join(dataPath,str(m)+'.'+gpscvCfg['counter:file extension'])
		data += LoadCounterFile(fname,m,delay)
	
	# Remove any suspect data after receiver restarts
	nDropped=0
	if (len(restarts) > 0):
		d=0
		while (d < len(data)):
			incr=1
			for r in restarts:
				if ((data[d][0] >= r) and (data[d][0] <= r + holdoff/86400.0)):
					del data[d]
					incr=0
					nDropped+=1
					break
			d+=incr # only increment if we didn't delete
	
	# Generate the report
	
	if (len(data) > 0):
		a=np.array(data)
		pfit = np.polyfit(a[:,0],a[:,1],1)
		ffe = pfit[0]/86400.0
		print '{} obs={} missing={} restarts={} dropped={} min={} max={} ffe={:.3e}'.format(
			rx,rxObs,len(mjds)*86400-len(data)-nDropped,len(restarts),nDropped,np.min(a[:,1]),np.max(a[:,1]),ffe)
	else:
		print '{} obs={} NO DATA'.format(rx,rxObs)
	
#
# end of main