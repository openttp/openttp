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
#

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import time

VERSION = "0.1"
AUTHORS = "Michael Wouters"

# ------------------------------------------
def ShowVersion():
	print  os.path.basename(sys.argv[0])," ",VERSION
	print "Written by",AUTHORS
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ------------------------------------------
# Convert Unix time to (truncated) MJD
#
def MJD(unixtime):
        return int(unixtime/86400) + 40587

# ------------------------------------------
#
def MakeRINEXObservationName(dirname,staname,yy,doy,reqd):
	fname = '{}/{}{:03d}0.{:02d}o'.format(dirname,staname,doy,yy)
	if (os.path.exists(fname)):
		return fname
	else:
		fname = '{}/{}{:03d}0.{:02d}O'.format(dirname,staname,doy,yy)
		if (os.path.exists(fname)):
			return fname
	if (reqd):	
		print "Can't open",fname,"(or .o)"
		exit()
	return ''

# ------------------------------------------		
#
def MakeRINEXNavigationName(dirname,staname,yy,doy,reqd):
	fname = '{}/{}{:03d}0.{:02d}n'.format(dirname,staname,doy,yy)
	if (os.path.exists(fname)):
		return fname
	else:
		fname = '{}/{}{:03d}0.{:02d}N'.format(dirname,staname,doy,yy)
		if (os.path.exists(fname)):
			return fname
	if (reqd):
		print "Can't open",fname,"(or .N)"
		exit()
	return ''

# ------------------------------------------
# Main

parser = argparse.ArgumentParser(description='Edit a RINEX navigation file')
parser.add_argument('obssta',help='station name for RINEX observation files',type=str)
parser.add_argument('MJD',help='mjd')
parser.add_argument('obspath',help='path to RINEX observations',type=str)
parser.add_argument('navsta',help='station name for RINEX navigation files',type=str)
parser.add_argument('navpath',help='path to RINEX navigation files',type=str)
parser.add_argument('params',help='paramCGGTTS file',type=str)
parser.add_argument('cggttsout',help='CGGTTS output path',type=str)

parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--rename','-r',help='rename cggtts output',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

debug = args.debug

if (args.version):
	ShowVersion()
	exit()

# Clean up from last run
files = glob.glob('*.tmp')
for f in files:
	os.unlink(f)

files = glob.glob('rinex_')
for f in files:
	os.unlink(f)
	
mjd = int(args.MJD)

RNXOBSDIR = args.obspath
STA=args.obssta
RNXNAVDIR=args.navpath
NAV= args.navsta
PARS=args.params

# Convert MJD to YY and DOY
t=(mjd-40587)*86400.0
tod=time.gmtime(t)
yy1 = tod.tm_year - int(tod.tm_year/100)*100
doy1 = tod.tm_yday

# and for the following day
t=(mjd+1-40587)*86400.0;
tod=time.gmtime(t)
yy2 = tod.tm_year - int(tod.tm_year/100)*100
doy2 = tod.tm_yday


# Make RINEX file names
rnx1=MakeRINEXObservationName(RNXOBSDIR,STA,yy1,doy1,True);
nav1=MakeRINEXNavigationName(RNXNAVDIR,NAV,yy1,doy1,True);
rnx2=MakeRINEXObservationName(RNXOBSDIR,STA,yy2,doy2,False); #don't require next day
nav2=MakeRINEXNavigationName(RNXNAVDIR,NAV,yy2,doy2,False);

if (os.path.exists(PARS)):
	if (PARS != 'paramCGGTTS.dat'):
		shutil.copy(PARS,'paramCGGTTS.dat')
else:
	print "Can't open",PARS;
	exit()

# Got all the needed files so proceed

# Get the current number of leap seconds
nLeap = 0;
# Try the RINEX navigation file.
# Use the number of leap seconds in nav1
fin = open(nav1,'r')
for l in fin:
	m = re.search('\s+(\d+)\s+LEAP\s+SECONDS',l)
	if (m):
		nLeap = int(m.group(1))
		Debug('Leap seconds = '+str(nLeap))
		break
fin.close()

if (nLeap > 0):
	
	# Back up paramCGGTTS
	shutil.copyfile(PARS,PARS+'.bak')
	fin=open(PARS,'r')
	fout=open(PARS+'.tmp','w');
	for l in fin:
		m = re.search('LEAP\s+SECOND',l)
		if (m):
			fout.write(l)
			fout.write(str(nLeap)+'\n')
			fin.next()
		else:
			fout.write(l)
	
	fin.close();
	fout.close();
	shutil.copyfile(PARS+'.tmp',PARS)
  
shutil.copyfile(PARS+'.bak',PARS)
  
# Almost there ...
shutil.copy(rnx1,'rinex_obs')
shutil.copy(nav1,'rinex_nav_gps')
if (rnx2 != ''):
	shutil.copy(rnx2,'rinex_obs_p')
if (nav2 != ''):
	shutil.copy(nav2,'rinex_nav_p_gps')

subprocess.call(['r2cggttsv8'])

# Remove any empty files
files = glob.glob('CGGTTS_*')
for f in files:
	if (os.path.getsize(f) < 1024): # seems to be a reasonable threshold
		os.unlink(f)

files = glob.glob('CTTS_*')
for f in files:
	if (os.path.getsize(f) < 1024): # seems to be a reasonable threshold
		os.unlink(f)
		
# Kludge for the moment
if (args.rename):
	if (os.path.exists('CGGTTS_GPS_C1')):
		os.rename('CGGTTS_GPS_C1',args.cggttsout+'/'+str(mjd)+'.cctf')

if (os.path.exists('CTTS_GPS_30s_C1')):
	os.rename('CTTS_GPS_30s_C1',str(mjd)+'.gps.C1.30s.dat')
