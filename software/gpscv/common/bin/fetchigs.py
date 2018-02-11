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
# Fetches IGS products
#

import argparse
import calendar
import os
import re
import sys
import time
import urllib2

# A fudge
sys.path.append("/usr/local/lib/python2.7/site-packages")
import ottplib

VERSION = "0.1"
AUTHORS = "Michael Wouters"

# ---------------------------------------------
def ShowVersion():
	print  os.path.basename(sys.argv[0]),' ',VERSION
	print 'Written by',AUTHORS
	return

# ------------------------------------------
def ErrorExit(msg):
	sys.stderr.write(msg+'\n')
	sys.exit(0)
	
# ------------------------------------------
def Initialise(configFile):
	cfg=ottplib.LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit("Error loading " + configFile)
		
	# Check for required arguments
	reqd = ['main:data centres']
	for k in reqd:
		if (not cfg.has_key(k)):
			ErrorExit("The required configuration entry " + k + " is undefined")
		
	return cfg

# ---------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ---------------------------------------------
def DateToMJD(d):
	
	if (re.match('^\d{5}$',d)): # just check the format
		return int(d)
	match = re.match('^\d{4}-\d{1,3}$',d)
	if (match): # YYYY-DOY
		utctod = time.strptime(d,'%Y-%j')
		return ottplib.MJD(calendar.timegm(utctod))
	match = re.match('^\d{4}-\d{1,2}-\d{1,2}$',d) # YYYY-MM-DD
	if (match): 
		utctod = time.strptime(d,'%Y-%m-%d')
		return ottplib.MJD(calendar.timegm(utctod))
	return -1

# ---------------------------------------------
def MJDtoYYYYDOY(mjd):
	tt = (mjd - 40587)*86400
	utctod = time.gmtime(tt)
	return (utctod.tm_year,utctod.tm_yday)

# ---------------------------------------------
def MJDtoGPSWeekDay(mjd):
	ttGPS = (mjd - 40587)*86400 - 315964800 # ignores leap seconds but this won't cause problems for a while :-)
	GPSWn = int(ttGPS/(7*86400))
	GPSday = (ttGPS - GPSWn*86400*7)/86400
	return (GPSWn, GPSday)

# ---------------------------------------------
def FetchFile(url,destination):
	Debug('Downloading '+ url)
	# return True
	try:
		u = urllib2.urlopen(url,None,60)
	except urllib2.URLError, e:
		sys.stderr.write('FetchFile() failed: ' + str(e.reason) + '\n')
		return False
	
	fout = open(destination,'wb')
	blockSize = 8192
	while True:
		buffer = u.read(blockSize)
		if not buffer:
			break
		fout.write(buffer)
	fout.close
	return True

# ---------------------------------------------
# Main

home =os.environ['HOME'] + '/';
configFile = os.path.join(home,'etc/fetchigs.conf');

parser = argparse.ArgumentParser(description='Fetches IGS data products')

parser.add_argument('start',nargs='?',help='start (MJD/yyyy-doy/yyyy-mm-dd, default = today)',type=str)
parser.add_argument('stop',nargs='?',help='stop (MJD/yyyy-doy/yyyy-mm-dd, default = start)',type=str)

parser.add_argument('--config','-c',help='use this configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--outputdir',help='set output directory',default='.')

parser.add_argument('--ephemeris',help='get broadcast ephemeris',action='store_true')
parser.add_argument('--clocks',help='get clock products (.clk)',action='store_true')
parser.add_argument('--orbits',help='get orbit products (.sp3)',action='store_true')

group = parser.add_mutually_exclusive_group()
group.add_argument('--rapid',help='fetch rapid products',action='store_true')
group.add_argument('--final',help='fetch final products',action='store_true')

parser.add_argument('--centre',help='set data centres',default='cddis')
parser.add_argument('--listcentres','-l',help='list available data centres',action='store_true')

#parser.add_argument('--proxy','-p',help='proxy server (address:port)',default='')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

if (args.version):
	ShowVersion()
	sys.exit(0)

debug = args.debug

configFile = args.config;

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + " not found")

cfg=Initialise(configFile)

centres = cfg['main:data centres'].split(',')

if (args.listcentres):	
	print 'Available data centres:'
	for c in centres:
		print c, cfg[c.lower() + ':base url']
	sys.exit(0)

dataCentre = args.centre.lower()
# Check that we've got this
found = False
for c in centres:
	if (c.lower() == dataCentre):
		found=True
		break
if (not found):
	ErrorExit('The data centre ' + dataCentre + ' is not defined in ' + configFile)
	
today = time.time() # save this in case the day rolls over while the script is running
mjdToday = ottplib.MJD(today)
start = mjdToday
stop  = mjdToday

if (args.start):
	start = DateToMJD(args.start)
	stop = start
	
if (args.stop):
	stop = DateToMJD(args.stop)

outputdir = args.outputdir

Debug('start = {},stop = {} '.format(start,stop))

# Daily data
baseURL = cfg[dataCentre + ':base url']
brdcPath = cfg[dataCentre + ':broadcast ephemeris']

# Products
prodPath = cfg[dataCentre + ':products']

for m in range(start,stop+1):
	Debug('fetching files for MJD ' + str(m))
	(yyyy,doy) = MJDtoYYYYDOY(m)
	if (args.ephemeris):
		yy = yyyy-100*int(yyyy/100)
		fname = 'brdc{:03d}0.{:02d}n.Z'.format(doy,yy)
		url = '{}/{}/{:04d}/{:03d}/{:02d}n/{}'.format(baseURL,brdcPath,yyyy,doy,yy,fname)
		FetchFile(url,'{}/{}'.format(outputdir,fname))
	if (args.clocks):
		(GPSWn,GPSday) = MJDtoGPSWeekDay(m)
		if (args.rapid):
			fname = 'igr{:04d}{:1d}.clk.Z'.format(GPSWn,GPSday) # file name is igrWWWWd.clk.Z
		elif (args.final):
			fname = 'igs{:04d}{:1d}.clk.Z'.format(GPSWn,GPSday)
		url = '{}/{}/{:04d}/{}'.format(baseURL,prodPath,GPSWn,fname)
		FetchFile(url,'{}/{}'.format(outputdir,fname))
	if (args.orbits):
		(GPSWn,GPSday) = MJDtoGPSWeekDay(m)
		if (args.rapid):
			fname = 'igr{:04d}{:1d}.sp3.Z'.format(GPSWn,GPSday) # file name is igrWWWWd.sp3.Z
		elif (args.final):
			fname = 'igs{:04d}{:1d}.sp3.Z'.format(GPSWn,GPSday)
		url = '{}/{}/{:04d}/{}'.format(baseURL,prodPath,GPSWn,fname)
		FetchFile(url,'{}/{}'.format(outputdir,fname))