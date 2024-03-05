#!/usr/bin/python3
#

#
# The MIT License (MIT)
#
# Copyright (c) 2019 Michael J. Wouters
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
# This is where ottplib is installed
sys.path.append("/usr/local/lib/python3.8/site-packages")  # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22.04

import ottplib as ottp
import rinexlib as rinex

VERSION = "1.4.0"
AUTHORS = "Michael Wouters"
NMISSING  = 7 # number of days to look backwards for missing files


# ------------------------------------------
# Create a r2cggtts parameter file from scratch
def CreateParamsFile(fname,leapSecs):
	
	ottp.Debug('Creating the parameter file for r2cggtts')
	
	fout = open(fname,'w')
	
	fout.write('REV DATE\n')
	fout.write(cfg['params:rev date'] + '\n')
	
	fout.write('RCVR\n')
	fout.write(cfg['params:rcvr'] + '\n')
	
	fout.write('CH\n')
	fout.write(cfg['params:ch'] + '\n')
	
	fout.write('LAB NAME\n')
	fout.write(cfg['params:lab name'] + '\n')
	
	fout.write('X COORDINATE\n')
	fout.write(cfg['params:x'] + '\n')
	
	fout.write('Y COORDINATE\n')
	fout.write(cfg['params:y'] + '\n')
	
	fout.write('Z COORDINATE\n')
	fout.write(cfg['params:z'] + '\n')
	
	fout.write('COMMENTS\n')
	fout.write(cfg['params:comments'] + '\n')
	
	fout.write('REF\n')
	fout.write(cfg['params:ref'] + '\n')
	
	fout.write('CALIBRATION REFERENCE\n')
	fout.write(cfg['params:calibration reference'] + '\n')
	
	fout.write('ANT CAB DELAY\n')
	fout.write('CLOCK CAB DELAY XP+XO\n')
	
	fout.write('LEAP SECOND\n')
	fout.write(str(leapSecs)+'\n')
	
	fout.write('30s FILES\n')
	fout.write('NO\n')
	fout.close()

# ------------------------------------------
def SetLeapSeconds(paramsFile,newLeapSecs):
	
	ottp.Debug('Setting leap seconds')
	
	fin  = open(paramsFile,'r')
	fout = open(paramsFile + '.tmp','w')
	for l in fin:
		m = re.search('LEAP\s+SECOND',l)
		if (m):
			fout.write(l)
			fout.write(str(newLeapSecs)+'\n')
			next(fin)
		else:
			fout.write(l)
	
	fin.close()
	fout.close()
	shutil.copyfile(paramsFile + '.tmp',paramsFile)
	os.unlink(paramsFile + '.tmp')
	
# ------------------------------------------
# Main

home = os.environ['HOME'] 
root = home 
configFile = os.path.join(root,'etc','mkcggtts.conf')
nMissing = NMISSING

parser = argparse.ArgumentParser(description='')

examples =  'Usage examples\n'
examples += '1. Process data for MJDs 58418 - 58419\n'
examples += '    mkcggtts.py 58418 58419\n'
examples += '2. Process previous day, previous day - 1 (for complete tracks at day end) and look back 7 days for missing files\n'
examples += '    mkcggtts.py  --previousmjd --missing 7\n'

parser = argparse.ArgumentParser(description='Generate CGGTTS files from RINEX observations',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

parser.add_argument('mjd',nargs = '*',help='first MJD [last MJD] (if not given, the MJD of the previous day is used)')
parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)
parser.add_argument('--leapsecs',help='manually set the number of leap seconds')
parser.add_argument('--previousmjd',help='when no MJD (or one MJD) is given, MJD-1 is added to the MJDs to be processed',action='store_true')
parser.add_argument('--missing',help='generate missing files')

args = parser.parse_args()

debug = args.debug
ottp.SetDebugging(debug)
rinex.SetDebugging(debug)

configFile = args.config

if (not os.path.isfile(configFile)):
	ottp.ErrorExit(configFile + ' not found')
	
cfg=ottp.Initialise(configFile,['tool:executable','tool:version','rinex:obs sta','cggtts:outputs'])

toolExec = ottp.MakeAbsoluteFilePath(cfg['tool:executable'],root,root + 'bin')

toolVersion = float(cfg['tool:version'])
if (toolVersion < 8):
	ottp.ErrorExit('r2cggtts version is unsupported (>=8 only)')
		
if 'paths:root' in cfg:
	root = ottp.MakeAbsolutePath(cfg['paths:root'],home)
	
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

if (startMJD == stopMJD and args.previousmjd): # add the previous day - this is to get both MJDs needed for a CGGTTS file with complete tracks at day end
	startMJD -= 1

paramFile = os.path.join(home,'etc','paramCGGTTS.dat')
if 'cggtts:parameter file' in cfg:
	paramFile = cfg['cggtts:parameter file']
	if not('auto' == paramFile):
		paramFile = ottp.MakeAbsoluteFilePath(paramFile,root,root + 'etc')
if (not('auto' == paramFile) and not(os.path.exists(paramFile))):
	ottp.ErrorExit(paramFile + " doesn't exist - check the configuration file")

tmpDir = os.path.join(root,'tmp')
if 'paths:tmp' in cfg:
	tmpDir = ottp.MakeAbsolutePath(cfg['paths:tmp'],root)
if not(os.path.exists(tmpDir)):
	ottp.ErrorExit(tmpDir + " doesn't exist - check the configuration file")
			
rnxVersion = 2

if 'rinex:version' in cfg:
	rnxVersion = int(cfg['rinex:version'])
	
rnxNavVersion = rnxVersion
rnxObsVersion = rnxVersion

if 'rinex:nav version' in cfg:
	rnxNavVersion = int(cfg['rinex:nav version'])

if 'rinex:obs version' in cfg:
	rnxObsVersion = int(cfg['rinex:obs version'])
	
rnxObsDir = os.path.join(home,'rinex')
if 'rinex:obs directory' in cfg:
	rnxObsDir = ottp.MakeAbsolutePath(cfg['rinex:obs directory'],root)
if not(os.path.exists(rnxObsDir)):
	ottp.ErrorExit(rnxObsDir + " doesn't exist - check the configuration file")
ottp.Debug('RINEX observation directory is ' + rnxObsDir)
			
rnxNavDir = os.path.join(home,'rinex')
if 'rinex:nav directory' in cfg:
	rnxNavDir = ottp.MakeAbsolutePath(cfg['rinex:nav directory'],root)
if not(os.path.exists(rnxNavDir)):
	ottp.ErrorExit(rnxNavDir + " doesn't exist - check the configuration file")
ottp.Debug('RINEX navigation directory is ' + rnxNavDir)

rnxObsSta = cfg['rinex:obs sta'] # required

rnxNavSta = rnxObsSta
if 'rinex:nav sta' in cfg:
	rnxNavSta = cfg['rinex:nav sta']
	
# Temporary files will be created in tmpDir 
os.chdir(tmpDir)

# Check output paths
outputs = cfg['cggtts:outputs'].split(',')
for o in outputs:
	lco = (o.lower()).strip()
	if not( (lco + ':directory') in cfg):
		ottp.ErrorExit('CGGTTS output section [' + lco + '] is missing')
	cggttsPath = ottp.MakeAbsolutePath(cfg[lco+':directory'],root)
	if not(os.path.exists(cggttsPath)):
		ottp.ErrorExit(cggttsPath + " doesn't exist - check the configuration file")

mjdsToDo = [*range(startMJD,stopMJD+1)]

#  --missing overrides everything
if (args.missing):
	nMissing = int(args.missing)
	stopMJD  = ottp.MJD(time.time()) - 1
	startMJD = stopMJD - nMissing
	if not(args.previousmjd): # this is so that we retain the MJD defined by --previousmjd
		mjdsToDo = []
	for mjd in range(startMJD,stopMJD+1):
		if mjd  in mjdsToDo: 
			continue
		outputs = cfg['cggtts:outputs'].split(',')
		for o in outputs:
			lco = (o.lower()).strip()
			constellation = cfg[lco + ':constellation'].upper()
			code = cfg[lco + ':code'].upper() 
			cggttsPath = ottp.MakeAbsolutePath(cfg[lco+':directory'],root)
			if (cfg['cggtts:naming convention'].lower() == 'plain'):
				fout = cggttsPath + str(mjd) + '.cctf'
			elif (cfg['cggtts:naming convention'].upper() == 'BIPM'):
				X = 'G'
				F = 'Z' # dual frequency is default
				if ('GPS'==constellation):
					X='G'
					if (code in ['C1','P1','P2','L5']):
						F='M'
				elif ('GLO'==constellation):
					X='R'
					if (code in ['C1','P1','P2']):
						F='M'
				elif ('BDS'==constellation):
					X='C'
					if ('B1I' == code or 'B2I' ==code):
						F='M'
				elif ('GAL'==constellation):
					X='E'
					if ('E1' == code or 'E5a' == code):
						F='M'
				mjdDD = int(mjd/1000)
				mjdDDD = mjd - 1000*mjdDD
				fout = cggttsPath + '{}{}{}{}{:02d}.{:03d}'.format(X,F,cfg['cggtts:lab id'].upper(),
					cfg['cggtts:receiver id'].upper(),mjdDD,mjdDDD)
			if not(os.path.exists(fout)):
				ottp.Debug(fout + ' is missing')
				# Don't be too fussy - one missing file means redo the lot
				mjdsToDo.append(mjd)
				break

# Main loop
for mjd in mjdsToDo:
	
	ottp.Debug('Processing ' + str(mjd))
	
	# Clean up the leftovers
	misc = ['paramCGGTTS.dat','biasC1P1.dat','biasC2P2.dat','r2cggtts.log']
	for f in misc:
		if (os.path.isfile(f)):
			os.unlink(f)

	files = glob.glob('*.tmp')
	for f in files:
		os.unlink(f)

	files = glob.glob('rinex_*')
	for f in files:
		os.unlink(f)
	
	files = glob.glob('CGGTTS_*')
	for f in files:
		os.unlink(f)
		
	t=(mjd-40587)*86400.0
	tod=time.gmtime(t)
	yyyy1 =  tod.tm_year
	#yy1 = tod.tm_year - int(tod.tm_year/100)*100
	doy1 = tod.tm_yday

	# and for the following day
	t=(mjd+1-40587)*86400.0;
	tod=time.gmtime(t)
	yyyy2 =  tod.tm_year
	#yy2 = tod.tm_year - int(tod.tm_year/100)*100
	doy2 = tod.tm_yday
	
	# Make RINEX file names
	(rnx1,rnx1ext)=rinex.FindObservationFile(rnxObsDir,rnxObsSta,yyyy1,doy1,rnxObsVersion,False);
	if not(rnx1):
		ottp.Debug('Missing observation file')
		continue
	rnx1 = rnx1+rnx1ext
	
	shutil.copy(rnx1,'./')
	rnx1 = os.path.basename(rnx1)
	rnx1,algo = rinex.Decompress(rnx1)
	ftmp = 'rinex_obs' 
	os.rename(rnx1,ftmp)
	
	(nav1,nav1ext)=rinex.FindNavigationFile(rnxNavDir,rnxNavSta,yyyy1,doy1,rnxNavVersion,False);
	if not(nav1):
		ottp.Debug('Missing navigation file')
		continue
	# Need to copy the file to the current directory, decompress it if necessary and rename it
	ftmp = 'rinex_nav_gps'  # version 2 is default
	if (rnxNavVersion == 3):
		ftmp = 'rinex_nav_mix' 
	tmpNavFile = ftmp
	shutil.copy(nav1 + nav1ext,ftmp + nav1ext)
	ottp.DecompressFile(ftmp,nav1ext)
	
	(rnx2,rnx2ext)=rinex.FindObservationFile(rnxObsDir,rnxObsSta,yyyy2,doy2,rnxObsVersion,False); 
	if rnx2: # don't require the next day's data
		rnx2 = rnx2+rnx2ext
		shutil.copy(rnx2,'./')
		rnx2 = os.path.basename(rnx2)
		rnx2,algo = rinex.Decompress(os.path.join(rnx2))
		ftmp = 'rinex_obs_p' 
		os.rename(rnx2,ftmp)
	
	(nav2,nav2ext)=rinex.FindNavigationFile(rnxNavDir,rnxNavSta,yyyy2,doy2,rnxNavVersion,False);
	if nav2:
		ftmp = 'rinex_nav_p_gps'  # version 2 is default
		if (rnxNavVersion == 3):
			ftmp = 'rinex_nav_p_mix' 
		shutil.copy(nav2 + nav2ext,ftmp + nav2ext)
		ottp.DecompressFile(ftmp,nav2ext)
	
	leapSecs =0
	if (args.leapsecs):
		leapSecs = int(args.leapsecs)
	else:
		leapSecs = rinex.GetLeapSeconds(tmpNavFile,rnxNavVersion) # already decompressed
		
	if (leapSecs <= 0):
		ottp.ErrorExit('Invalid number of leap seconds')
		
	if (paramFile == 'auto'):
		CreateParamsFile('paramCGGTTS.dat',leapSecs)
	else:
		shutil.copy(paramFile,'paramCGGTTS.dat')
		SetLeapSeconds('paramCGGTTS.dat',leapSecs)
	
	# Do it
	ottp.Debug('Running ' + toolExec)
	try:
		x = subprocess.check_output([toolExec]) # eat the output
	except Exception as e:
		ottp.ErrorExit('Failed to run ' + toolExec)
	ottp.Debug(x.decode('utf-8'))
		
	# Copy and rename files
	outputs = cfg['cggtts:outputs'].split(',')
	for o in outputs:
		ottp.Debug('Processing ' + o)
		lco = (o.lower()).strip()
		constellation = cfg[lco + ':constellation'].upper()
		code = cfg[lco + ':code']
		
		# This list defines the input file and the output path
		inputs = [ ['CGGTTS_' + constellation + '_' + code, ottp.MakeAbsolutePath(cfg[lco+':directory'],root)] ]
		
		if lco +':process ctts' in cfg:
			cfgVal = cfg[lco +':process ctts'].lower()
			if ( cfgVal in ['yes','true','1']):
				if lco+':ctts directory' in cfg:
					inputs.append(['CTTS_' + constellation + '_30s_' + code, ottp.MakeAbsolutePath(cfg[lco+':ctts directory'],root)])
				else:
					ottp.Debug(lco + ':ctts directory' + ' is unconfigured')
		
		for inp in inputs:
			fin = inp[0]
			if not(os.path.exists(fin)):
				ottp.Debug(fin + ' is missing')
				continue
			
			cggttsPath = inp[1]
			
			if (cfg['cggtts:naming convention'].lower() == 'plain'):
				fout = cggttsPath + str(mjd) + '.cctf'
			elif (cfg['cggtts:naming convention'].upper() == 'BIPM'):
				
				X = 'G'
				F = 'Z' # dual frequency is default
				if ('GPS'==constellation):
					X='G'
					if (code in ['C1','P1','P2','L5']):
						F='M'
				elif ('GLO'==constellation):
					X='R'
					if (code in ['C1','P1','P2']):
						F='M'
				elif ('BDS'==constellation):
					X='C'
					if ('B1I' == code or 'B2I' == code):
						F='M'
				elif ('GAL'==constellation):
					X='E'
					if ('E1' == code or 'E5a' == code):
						F='M'
				mjdDD = int(mjd/1000)
				mjdDDD = mjd - 1000*mjdDD
				fout = cggttsPath + '{}{}{}{}{:02d}.{:03d}'.format(X,F,cfg['cggtts:lab id'].upper(),
					cfg['cggtts:receiver id'].upper(),mjdDD,mjdDDD)
			
			shutil.copy(fin,fout)
			ottp.Debug('Generated ' + fout)
