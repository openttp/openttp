#!/usr/bin/python
#

#
# The MIT License (MIT)
#
# Copyright (c) 2017 Michael J. Wouters
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
# ubloxextract.py - a script to decode OpenTTP ublox log files
#
# Modification history
# 2017-05-09 MJW First version
#

import argparse
import binascii
import glob
import os
import re
import select
import signal
import subprocess
import sys
import struct

# This is where ottplib is installed
sys.path.append("/usr/local/lib/python2.7/site-packages")
import time

import ottplib

VERSION = '0.1.2'
AUTHORS = "Michael Wouters"

# Time stamp formats
TS_UNIX = 0
TS_TOD  = 1

# Globals
debug = False
killed = False
tt=0

# ------------------------------------------
def SignalHandler(signal,frame):
	global killed
	killed=True
	return

# ------------------------------------------
def ShowVersion():
	print  os.path.basename(sys.argv[0])," ",VERSION
	print "Written by",AUTHORS
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		print msg
	return

# ------------------------------------------
def ErrorExit(msg):
	print msg
	sys.exit(0)
	
# ------------------------------------------
def Initialise(configFile):
	cfg=ottplib.LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit("Error loading " + configFile)
		
	# Check for required arguments
	reqd = ['paths:receiver data','receiver:file extension']
	for k in reqd:
		if (not cfg.has_key(k)):
			ErrorExit("The required configuration entry " + k + " is undefined")
		
	return cfg

# ------------------------------------------
def DecodeUBX_MON_VER(msg):
	nExtended = int((len(msg) - 2*(2+40))/60)
	if (not(len(msg) == 2*(40+2+nExtended*30))):
		return ['?','?']
	packed=binascii.unhexlify(msg)
	unpacked=struct.unpack_from('30s10s',packed)
	# TO DO parse extended version strings
	return unpacked
	
# ------------------------------------------
def DecodeUBX_NAV_CLOCK(msg):
	if not(len(msg)==(20+2)*2):
		return ''
	packed=binascii.unhexlify(msg)
	# iTOW,clkB,clkD,tAcc,fAcc
	unpacked=struct.unpack('I2i2I2B',packed)
	return unpacked

def SaveSV(gnssID,svdata):
	svFile = gnssID + str(svdata[1])+'.sv.dat'
	sd=open(svFile,'a')
	sd.write('{0} {1} {2} {3} {4} {5} {6} {7} {8}\n'.format(tt,svdata[2],svdata[3],svdata[4],svdata[5],svdata[6],svdata[7],svdata[8],svdata[9]))
	sd.close()
	
# ------------------------------------------
def DecodeUBX_NAV_SAT(msg):
	if (len(msg)<(8+2)*2):
		return [0,0,0,0]
	packed=binascii.unhexlify(msg)
	unpacked=struct.unpack_from('I4B',packed)
	ngps=0
	nglonass=0
	nbeidou=0
	ngalileo=0
	nsbas=0
	nqzss=0
	nimes =0
	nsv = unpacked[2]
	for sv in xrange(0,nsv):
		unpacked=struct.unpack_from('3Bb2h4B',packed,8+12*sv)
		if (unpacked[0]==0):
			ngps += 1
			SaveSV('G',unpacked)
		elif (unpacked[0]==1):
			nsbas += 1
		elif (unpacked[0]==2):
			ngalileo += 1
			SaveSV('E',unpacked)
		elif (unpacked[0]==3):
			nbeidou += 1
			SaveSV('B',unpacked)
		elif (unpacked[0]==4):
			nimes += 1
		elif (unpacked[0]==5):
			nqzss += 1
		elif (unpacked[0]==6):
			nglonass += 1
			SaveSV('R',unpacked)
	return [ngps,nglonass,nbeidou,ngalileo]

# ------------------------------------------
def DecodeUBX_NAV_TIMEUTC(msg):
	if not(len(msg)==(20+2)*2):
		return [0,0,0,0,0,0,0,0,0,0,0,0]
	packed=binascii.unhexlify(msg)
	# (iTOW,tAcc,nano,year,month,day,hour,min,sec,valid)
	unpacked=struct.unpack('2IiH5BB2B',packed)
	return unpacked

# ------------------------------------------
def DecodeUBX_RXM_RAWX(msg):
	if (len(msg)<(16+2)*2):
		return [0]
	packed=binascii.unhexlify(msg)
	nmeas = (len(msg)/2 -18)/32
	unpacked=struct.unpack_from('d',packed)
	return unpacked

# ------------------------------------------
def DecodeUBX_SEC_UNIQID(msg):
	if not(len(msg)==(9+2)*2):
		return ''
	packed=binascii.unhexlify(msg)
	unpacked=struct.unpack('B3B5B2B',packed)
	return [unpacked[0],(unpacked[4] << 32)+(unpacked[5] << 24)+(unpacked[6] << 16)+(unpacked[7] << 8)+unpacked[8]]

# ------------------------------------------
def DecodeUBX_TIM_TP(msg):
	if not(len(msg)==(16+2)*2):
		return ['']
	packed=binascii.unhexlify(msg)
	unpacked=struct.unpack('IIiH4B',packed)
	return unpacked

# ------------------------------------------
# Main 
# ------------------------------------------

home =os.environ['HOME'] + '/'
configFile = os.path.join(home,'etc/gpscv.conf')
tt = time.time()
mjd = ottplib.MJD(tt) - 1 # previous day
compress=False

parser = argparse.ArgumentParser(description='Extract messages from a ublox data file')
parser.add_argument('file',nargs='?',help='input file',type=str,default='')
parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--mjd','-m',help='mjd',default=mjd)
parser.add_argument('--uniqid',help='chip id',action='store_true')
parser.add_argument('--monver',help='hardware and software versions',action='store_true')
parser.add_argument('--navclock',help='nav-clock',action='store_true')
parser.add_argument('--navsat',help='nav-sat',action='store_true')
parser.add_argument('--navtimeutc',help='nav-timeutc',action='store_true')
parser.add_argument('--rawx',help='raw measurement data',action='store_true')
parser.add_argument('--timtp',help='sawtooth correction',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')
args = parser.parse_args()

debug = args.debug
mjd = args.mjd

if (args.version):
	ShowVersion()
	sys.exit(0)

if (args.navsat): # remove all old SV files
	map(os.unlink,glob.glob(u'./G*.sv.dat'))
	map(os.unlink,glob.glob(u'./R*.sv.dat'))
	map(os.unlink,glob.glob(u'./E*.sv.dat'))
	map(os.unlink,glob.glob(u'./B*.sv.dat'))

configFile = args.config

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + " not found")
	
logPath = os.path.join(home,'logs')
if (not os.path.isdir(logPath)):
	ErrorExit(logPath + "not found")

cfg=Initialise(configFile)

if (args.file):
	if not (os.path.isfile(args.file)):
		ErrorExit('Unable to open ' + args.file)
	dataFile = args.file
	path,ext = os.path.splitext(dataFile)
	if (ext == '.gz'):
		gzDataFile = dataFile
		dataFile = path
		compress = True
else:
	dataPath = ottplib.MakeAbsolutePath(cfg['paths:receiver data'], home)

	rxExt = cfg['receiver:file extension']
	if (None == re.search(r'\.$',rxExt)):
		rxExt = '.' + rxExt

	# File may be compressed so decompress it if required
	dataFile = dataPath + str(mjd) + rxExt

	if not (os.path.isfile(dataFile)):
		gzDataFile = dataFile + '.gz'
	if not (os.path.isfile(gzDataFile)):
		ErrorExit('Unable to open ' + dataFile + '(.gz)')
	else:
		compress=True
		
if (compress):
	Debug('Decompressing '+gzDataFile)
	subprocess.check_output(['gunzip',gzDataFile])
		
lasttstamp='-1'

fdata=open(dataFile,'r')

for line in fdata:
	line=line.rstrip('\r\n') # chomp
	lfields = line.split()
	if not (len(lfields)==3):
		continue
	msgid=lfields[0]
	tstamp=lfields[1]
	msg=lfields[2]
	if not(tstamp == lasttstamp):
		lasttstamp = tstamp
		hhmmss = tstamp.split(':')
		tt = int(hhmmss[0])*3600 + int(hhmmss[1])*60 + int(hhmmss[2])

	if (msgid == '0a04'):
		if (args.monver):
			msgf=DecodeUBX_MON_VER(msg)
			print 'SW:',msgf[0],'HW:',msgf[1]
	elif (msgid == '0d01'):
		if (args.timtp):
			msgf=DecodeUBX_TIM_TP(msg)
			print tt,msgf[2] # in ps
	elif (msgid == '0121'):
		if (args.navtimeutc):
			msgf=DecodeUBX_NAV_TIMEUTC(msg)
			print tt,msgf[0],msgf[1],msgf[3],msgf[4]
	elif (msgid == '0122'):
		if (args.navclock):
			msgf=DecodeUBX_NAV_CLOCK(msg)
			print tt,msgf[0],msgf[1],msgf[2],msgf[3],msgf[4]
	elif (msgid == '0135'):
		if (args.navsat):
			msgf=DecodeUBX_NAV_SAT(msg)
			print tt,msgf[0],msgf[1],msgf[2],msgf[3]
	elif (msgid == '0215'):
		if (args.rawx):
			msgf=DecodeUBX_RXM_RAWX(msg)
			print tt,msgf[0]
	elif (msgid == '2703'):
		if (args.uniqid):
			msgf=DecodeUBX_SEC_UNIQID(msg)
			print tt,msgf
			
fdata.close()

if (compress):
	Debug('Recompressing ...',)
	subprocess.check_output(['gzip',dataFile])
	
sys.exit(0)
