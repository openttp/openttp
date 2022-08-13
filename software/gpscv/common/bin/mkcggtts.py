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
sys.path.append("/usr/local/lib/python3.8/site-packages")
import ottplib

VERSION = "1.0.8"
AUTHORS = "Michael Wouters"
NMISSING  = 7 # number of days to look backwards for missing files

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
	reqd = ['tool:executable','tool:version',
		'rinex:obs sta',
		'cggtts:outputs']
	
	for k in reqd:
		if (not k in cfg):
			ErrorExit('The required configuration entry "' + k + '" is undefined')
		
	return cfg

# ------------------------------------------
# Create a r2cggtts parameter file from scratch
def CreateParamsFile(fname,leapSecs):
	
	Debug('Creating the parameter file for r2cggtts')
	
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
	
	Debug('Setting leap seconds')
	
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
# Convert Unix time to (truncated) MJD
def MJD(unixtime):
	return int(unixtime/86400) + 40587

# ------------------------------------------
# Find a file with basename and a list of potential extensions
# (usually compression extensions - .gz, .Z etc
def FindFile(basename,extensions):
	fname = basename
	Debug('Trying ' + fname )
	if (os.path.exists(fname)):
		Debug('Success')
		return (True,'')
	
	for ext in extensions:
		fname = basename + ext
		Debug('Trying ' + fname )
		if (os.path.exists(fname)):
			Debug('Success')
			return (True,ext)
	
	return (False,'') # flag failure

# ------------------------------------------
def FindRINEXObservationFile(dirname,staname,yyyy,doy,rnxver,reqd):
	if (rnxver == 2):
		yy = yyyy - int(yyyy/100)*100
		
		bname1 = os.path.join(dirname,'{}{:03d}0.{:02d}o'.format(staname,doy,yy))
		(found,ext) = FindFile(bname1,['.gz'])
		if (found):
			return (bname1,ext)
		
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}O'.format(staname,doy,yy))
		(found,ext) = FindFile(bname2,['.gz'])
		if (found):
			return (bname2,ext)
		
		if (reqd):	
			ErrorExit("Can't find obs file:\n\t" + bname1 + '\n\t' + bname2 )
			
	elif (rnxver==3):
		# Try a V3 name first
		bname1 = os.path.join(dirname,'{}_R_{:04d}{:03d}0000_01D_30S_MO.rnx'.format(staname,yyyy,doy))
		(found,ext) = FindFile(bname1,['.gz'])
		if (found):
			return (bname1,ext)
		
		# Try version 2
		yy = yyyy - int(yyyy/100)*100
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}o'.format(staname,doy,yy))
		(found,ext) = FindFile(bname2,['.gz'])
		if (found):
			return (bname2,ext)
		
		# Another try at version 2
		bname3 = os.path.join(dirname,'{}{:03d}0.{:02d}O'.format(staname,doy,yy))
		(found,ext) = FindFile(bname3,['.gz'])
		if (found):
			return (bname3,ext)
		
		if (reqd):	
			ErrorExit("Can't find obs file:\n\t" + bname1 + '\n\t' + bname2 + '\n\t' + bname3)
			
	return ('','')

# ------------------------------------------		
#
def FindRINEXNavigationFile(dirname,staname,yyyy,doy,rnxver,reqd):
	if (rnxver == 2):
		yy = yyyy - int(yyyy/100)*100
		
		bname1 = os.path.join(dirname,'{}{:03d}0.{:02d}n'.format(staname,doy,yy))
		(found,ext) = FindFile(bname1,['.gz'])
		if (found):
			return (bname1,ext)
		
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}N'.format(staname,doy,yy))
		(found,ext) = FindFile(bname2,['.gz'])
		if (found):
			return (bname2,ext)
		
		if (reqd):	
			ErrorExit("Can't find nav file:\n\t" + bname1 + '\n\t' + bname2)
			
	elif (rnxver == 3):
		# Mixed navigation files only
		bname1 = os.path.join(dirname,'{}_R_{:04d}{:03d}0000_01D_MN.rnx'.format(staname,yyyy,doy))
		(found,ext) = FindFile(bname1,['.gz'])
		if (found):
			return (bname1,ext)
		
		# Try version 2 name (typically produced by sbf2rin)
		yy = yyyy - int(yyyy/100)*100
		bname2 = os.path.join(dirname,'{}{:03d}0.{:02d}P'.format(staname,doy,yy))
		(found,ext) = FindFile(bname2,['.gz'])
		if (found):
			return (bname2,ext)
		
		if (reqd):	
			ErrorExit("Can't find nav file:\n\t" + bname1)
			
	return ('','')

# ------------------------------------------
def ReadLeapSeconds(nav,navExt,rnxVers):
	DecompressFile(nav,navExt)
	nLeap = 0
	fin = open(nav,'r')
	for l in fin:
		m = re.search('\s+(\d+)(\s+\d+\s+\d+\s+\d+)?\s+LEAP\s+SECONDS',l) # RINEX V3 has extra leap second information 
		if (m):
			nLeap = int(m.group(1))
			Debug('Leap seconds = '+str(nLeap))
			break
	fin.close()
	return nLeap

# ------------------------------------------
def DecompressFile(basename,ext):
	if (ext == '.gz'):
		subprocess.check_output(['gunzip',basename + ext])
		Debug('Decompressed ' + basename)

# ------------------------------------------
def RecompressFile(basename,ext):
	if (ext == '.gz'):
		subprocess.check_output(['gzip',basename])
		Debug('Recompressed ' + basename)

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

configFile = args.config;

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + ' not found')
	
cfg=Initialise(configFile)
	
toolVersion = float(cfg['tool:version'])
if (toolVersion < 8):
	ErrorExit('r2cggtts version is unsupported (>=8 only)')
		
if 'paths:root' in cfg:
	root = ottplib.MakeAbsolutePath(cfg['paths:root'],home)
	
startMJD = ottplib.MJD(time.time()) - 1 # previous day
stopMJD  = startMJD
	
if (args.mjd):
	if 1 == len(args.mjd):
		startMJD = int(args.mjd[0])
		stopMJD  = startMJD
	elif ( 2 == len(args.mjd)):
		startMJD = int(args.mjd[0])
		stopMJD  = int(args.mjd[1])
		if (stopMJD < startMJD):
			ErrorExit('Stop MJD is before start MJD')
	else:
		ErrorExit('Too many MJDs')

if (startMJD == stopMJD and args.previousmjd): # add the previous day - this is to get both MJDs needed for a CGGTTS file with complete tracks at day end
	startMJD -= 1

paramFile = os.path.join(home,'etc','paramCGGTTS.dat')
if 'cggtts:parameter file' in cfg:
	paramFile = cfg['cggtts:parameter file']
	if not('auto' == paramFile):
		paramFile = ottplib.MakeAbsoluteFilePath(paramFile,root,root + 'etc')
if (not('auto' == paramFile) and not(os.path.exists(paramFile))):
	ErrorExit(paramFile + " doesn't exist - check the configuration file")

tmpDir = os.path.join(root,'tmp')
if 'paths:tmp' in cfg:
	tmpDir = ottplib.MakeAbsolutePath(cfg['paths:tmp'],root)
if not(os.path.exists(tmpDir)):
	ErrorExit(tmpDir + " doesn't exist - check the configuration file")
			
rnxVersion = 2
if 'rinex:version' in cfg:
	rnxVersion = int(cfg['rinex:version'])
	
rnxObsDir = os.path.join(home,'rinex')
if 'rinex:obs directory' in cfg:
	rnxObsDir = ottplib.MakeAbsolutePath(cfg['rinex:obs directory'],root)
if not(os.path.exists(rnxObsDir)):
	ErrorExit(rnxObsDir + " doesn't exist - check the configuration file")
Debug('RINEX observation directory is ' + rnxObsDir)
			
rnxNavDir = os.path.join(home,'rinex')
if 'rinex:nav directory' in cfg:
	rnxNavDir = ottplib.MakeAbsolutePath(cfg['rinex:nav directory'],root)
if not(os.path.exists(rnxNavDir)):
	ErrorExit(rnxNavDir + " doesn't exist - check the configuration file")
Debug('RINEX navigation directory is ' + rnxNavDir)

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
		ErrorExit('CGGTTS output section [' + lco + '] is missing')
	cggttsPath = ottplib.MakeAbsolutePath(cfg[lco+':directory'],root)
	if not(os.path.exists(cggttsPath)):
		ErrorExit(cggttsPath + " doesn't exist - check the configuration file")

mjdsToDo = [*range(startMJD,stopMJD+1)]

#  --missing overrides everything
if (args.missing):
	nMissing = int(args.missing)
	stopMJD  = ottplib.MJD(time.time()) - 1
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
			cggttsPath = ottplib.MakeAbsolutePath(cfg[lco+':directory'],root)
			if (cfg['cggtts:naming convention'].lower() == 'plain'):
				fout = cggttsPath + str(mjd) + '.cctf'
			elif (cfg['cggtts:naming convention'].upper() == 'BIPM'):
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
				fout = cggttsPath + '{}{}{}{}{:02d}.{:03d}'.format(X,F,cfg['cggtts:lab id'].upper(),
					cfg['cggtts:receiver id'].upper(),mjdDD,mjdDDD)
			if not(os.path.exists(fout)):
				Debug(fout + ' is missing')
				# Don't be too fussy - one missing file means redo the lot
				mjdsToDo.append(mjd)
				break

# Main loop
for mjd in mjdsToDo:
	
	Debug('Processing ' + str(mjd))
	
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
	(rnx1,rnx1ext)=FindRINEXObservationFile(rnxObsDir,rnxObsSta,yyyy1,doy1,rnxVersion,True);
	ftmp = 'rinex_obs' 
	shutil.copy(rnx1 + rnx1ext,ftmp + rnx1ext)
	DecompressFile(ftmp,rnx1ext)
	
	(nav1,nav1ext)=FindRINEXNavigationFile(rnxNavDir,rnxNavSta,yyyy1,doy1,rnxVersion,True);
	# Need to copy the file to the current directory, decompress it if necessary and rename it
	ftmp = 'rinex_nav_gps'  # version 2 is default
	if (rnxVersion == 3):
		ftmp = 'rinex_nav_mix' 
	shutil.copy(nav1 + nav1ext,ftmp + nav1ext)
	DecompressFile(ftmp,nav1ext)
	
	(rnx2,rnx2ext)=FindRINEXObservationFile(rnxObsDir,rnxObsSta,yyyy2,doy2,rnxVersion,False); #don't require next day
	if not(rnx2 == ''):
		ftmp = 'rinex_obs_p' 
		shutil.copy(rnx2 + rnx2ext,ftmp + rnx2ext)
		DecompressFile(ftmp,rnx2ext)
	
	(nav2,nav2ext)=FindRINEXNavigationFile(rnxNavDir,rnxNavSta,yyyy2,doy2,rnxVersion,False);
	if not(nav2 == ''):
		ftmp = 'rinex_nav_p_gps'  # version 2 is default
		if (rnxVersion == 3):
			ftmp = 'rinex_nav_p_mix' 
		shutil.copy(nav2 + nav2ext,ftmp + nav2ext)
		DecompressFile(ftmp,nav2ext)
	
	leapSecs =0
	if (args.leapsecs):
		leapSecs = int(args.leapsecs)
	else:
		leapSecs = ReadLeapSeconds(nav1,nav1ext,rnxVersion)
		
	if (leapSecs <= 0):
		ErrorExit('Invalid number of leap seconds')
		
	if (paramFile == 'auto'):
		CreateParamsFile('paramCGGTTS.dat',leapSecs)
	else:
		shutil.copy(paramFile,'paramCGGTTS.dat')
		SetLeapSeconds('paramCGGTTS.dat',leapSecs)
	
	# Do it
	Debug('Running ' + cfg['tool:executable'])
	try:
		x = subprocess.check_output([cfg['tool:executable']]) # eat the output
	except:
		ErrorExit('Failed to run ' + cfg['tool:executable'])
	Debug(x.decode('utf-8'))
		
	# Copy and rename files
	outputs = cfg['cggtts:outputs'].split(',')
	for o in outputs:
		Debug('Processing ' + o)
		lco = (o.lower()).strip()
		constellation = cfg[lco + ':constellation'].upper()
		code = cfg[lco + ':code'].upper() 
		if (toolVersion >= 8):
			fin = 'CGGTTS_' + constellation + '_' + code 
		
		if not(os.path.exists(fin)):
			Debug(fin + ' is missing')
			continue
		
		cggttsPath = ottplib.MakeAbsolutePath(cfg[lco+':directory'],root)
		
		if (cfg['cggtts:naming convention'].lower() == 'plain'):
			fout = cggttsPath + str(mjd) + '.cctf'
		elif (cfg['cggtts:naming convention'].upper() == 'BIPM'):
			
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
			fout = cggttsPath + '{}{}{}{}{:02d}.{:03d}'.format(X,F,cfg['cggtts:lab id'].upper(),
				cfg['cggtts:receiver id'].upper(),mjdDD,mjdDDD)
		
		shutil.copy(fin,fout)
		Debug('Generated ' + fout)
