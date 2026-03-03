#!/usr/bin/python3


#
# The MIT License (MIT)
#
# Copyright (c) 2021 Michael J. Wouters, Louis Marais
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
# Processing script for Septentrio and Javad receivers
# Based on runsbf2rnx.py, which it will replace. Same configuration file format, but with a few extra options.

# NOTE: a header replacement will be ignored if the value is empty
# 

import argparse
import binascii
import os
from pathlib import Path
import string
import shutil
import subprocess
import sys

# This is where ottp is installed

sys.path.append('/usr/local/lib/python3.6/site-packages')
sys.path.append('/usr/local/lib/python3.8/site-packages')
sys.path.append('/usr/local/lib/python3.10/site-packages')
import time

import ottplib as ottp

VERSION = '0.2.1'
AUTHORS = 'Michael Wouters'


# Some constants
SBF = 0 # SBF format data
JPS = 1 # JPS format data

# Some defaults

SBF2RIN='/usr/local/bin/sbf2rin'
JPS2RIN='/usr/local/bin/jps2rin'

#-----------------------------------------------------------------------------
def Cleanup():
	# Hmm ugly globals
	ottp.RemoveProcessLock(lockFile)

# ---------------------------------------------
def MJDtoYYYYDOY(mjd):
	tt = (mjd - 40587)*86400
	utctod = time.gmtime(tt)
	return (utctod.tm_year,utctod.tm_yday)
      
# ---------------------------------------------
def DecompressFile(basename,ext):
	if (ext == '.gz'):
		subprocess.check_output(['gunzip',basename + ext])
		ottp.Debug('Decompressed ' + basename)
	
# ---------------------------------------------
def CompressFile(basename,ext):
	if (ext == '.gz'):
		subprocess.check_output(['gzip',basename])
		ottp.Debug('Compressed ' + basename)

# ---------------------------------------------
# Main

home =os.environ['HOME'] 
root = home
tmpDir = os.path.join(home,'tmp')
rawDir = os.path.join(home,'raw')
rnxObsDir = os.path.join(home,'RINEX')
rnxNavDir = os.path.join(home,'RINEX')

rnxVersion = '3' # as a string
nameFormat = '3' # as a string
rnxObsInterval = '30'
rnxExclusions = 'ISJ'  # typically, we don't care about these
defRnxStation = 'SEPT' # default station name used by sbf2rin
fixHeader = False
bodgeSatCountBug = False
useRxCopy = False        # RINEX converters eg may not work happily with a live file. Workaround is to use a copy. 
rxFileFormat = SBF

configFile = os.path.join(home,'etc','runrx2rnx.conf')

parser = argparse.ArgumentParser(description='Generate RINEX files using vendor RINEX converters (sbf2rin, jps2rin)',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('mjd',nargs = '*',help='first MJD [last MJD] (if not given, the MJD of the previous day is used)')
parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--usecopy',help='debug',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug
ottp.SetDebugging(debug)

configFile = args.config;

if (not os.path.isfile(configFile)):
	ottp.ErrorExit(configFile + ' not found')
	
logPath = os.path.join(home,'log')
if (not os.path.isdir(logPath)):
	logPath = os.path.join(home,'logs')
if (not os.path.isdir(logPath)):
	ottp.ErrorExit(logPath + "not found")

cfg=ottp.Initialise(configFile,['paths:receiver data','receiver:file extension', \
		'rinex:obs sta','rinex:nav sta'])

firstMJD = ottp.MJD(time.time()) - 1; # two days ago
lastMJD  = firstMJD

if (args.mjd):
	if 1 == len(args.mjd):
		firstMJD = int(args.mjd[0])
		lastMJD  = firstMJD
	elif ( 2 == len(args.mjd)):
		firstMJD = int(args.mjd[0])
		lastMJD  = int(args.mjd[1])
		if (lastMJD < firstMJD):
			ottp.ErrorExit('Stop MJD is before start MJD')
	else:
		ottp.ErrorExit('Too many MJDs')

ottp.Debug('Processing MJDs ' + str(firstMJD) + ' to ' + str(lastMJD))


if 'paths:root' in cfg:
	root = cfg['paths:root']
	
rawDir = ottp.MakeAbsolutePath(cfg['paths:receiver data'],root)
rxExtension = '.sbf'

if 'receiver:file extension' in cfg:
	rxExtension = cfg['receiver:file extension']

if 'receiver:file format' in cfg:
	token  = cfg['receiver:file format'].lower()
	if token == 'sbf':
		rxFileFormat = SBF
	elif token == 'jps':
		rxFileFormat = JPS

if args.usecopy: # overrides configuration file
	useRxCopy = True
elif 'receiver:use copy' in cfg:
	tmp = cfg['receiver:use copy'].lower()
	useRxCopy = (tmp == '1') or (tmp == 'yes') or (tmp =='true')
	
if 'paths:tmp' in cfg:
	tmpDir = ottp.MakeAbsolutePath(cfg['paths:tmp'],root)
	
if 'rinex:version' in cfg:
	rnxVersion = cfg['rinex:version']

if rnxVersion[0] == '2':
	ottp.ErrorExit('Version 2 RINEX is not supported')

if 'main:exec' in cfg:
	if rxFileFormat == SBF:
		SBF2RIN = ottp.MakeAbsoluteFilePath(cfg['main:exec'],root,os.path.join(root,'bin'))
	elif rxFileFormat == JPS:
		JPS2RIN = ottp.MakeAbsoluteFilePath(cfg['main:exec'],root,os.path.join(root,'bin'))

if 'main:sbf station name' in cfg:
	defRnxStation = cfg['main:sbf station name']

if 'rinex:name format' in cfg:
	nameFormat = cfg['rinex:name format']
	
createNav = False
if 'rinex:create nav file' in cfg:
	token  = cfg['rinex:create nav file'].lower()
	if ('yes' == token or 'true' == token):
		createNav =True

if rxFileFormat == SBF:
	rnxFiles = 'O'
	if createNav:
		rnxFiles += 'P'
elif rxFileFormat == JPS:
	pass
	
if 'rinex:exclusions' in cfg:
	if rxFileFormat == SBF:
		rnxExclusions = cfg['rinex:exclusions']
	elif rxFileFormat == JPS:
		# We'll assume they know what they're doing
		rnxExclusions = []
		for g in cfg['rinex:exclusions']:
			rnxExclusions.append('-'+g)
		
if 'rinex:obs directory' in cfg:
	rnxObsDir = ottp.MakeAbsolutePath(cfg['rinex:obs directory'],root)

if 'rinex:nav directory' in cfg:
	rnxNavDir = ottp.MakeAbsolutePath(cfg['rinex:nav directory'],root)
	
rnxObsStation = cfg['rinex:obs sta']
rnxNavStation = cfg['rinex:nav sta']

if 'rinex:fix header' in cfg:
	token  = cfg['rinex:fix header'].lower()
	if ('yes' == token or 'true' == token):
		fixHeader =True

if 'rinex:fix satellite count' in cfg:
	token  = cfg['rinex:fix satellite count'].lower()
	if ('yes' == token or 'true' == token):
		bodgeSatCountBug =True

# Change to the tmp directory to create files
cwd = os.getcwd() # save old directory
os.chdir(tmpDir)

headerFixes = {}  # store as dictionary
if fixHeader:
	headerFile = ottp.MakeAbsoluteFilePath(cfg['rinex:header fixes'],root,os.path.join(root,'etc'))
	if os.path.exists(headerFile):
		fin = open(headerFile,'r')
		for l in fin:
			l = l.rstrip() # trailing whitespace only
			headerFixes[l[60:]] = l
		fin.close()
	else:
		ottp.ErrorExit(headerFile + ' is missing')
		
for mjd in range(firstMJD,lastMJD+1):
	(yyyy,doy) = MJDtoYYYYDOY(mjd)
	yy = yyyy-int(yyyy/100)*100
	frx = os.path.join(rawDir,str(mjd) + '.' + rxExtension)
	recompress  = False
	if not(os.path.exists(frx)):
		frxgz = frx + '.gz' 
		if (os.path.exists(frxgz)):
			
			if useRxCopy:
				ottp.Debug(f'Copying {frxgz}')
				shutil.copy(frxgz,'./')
				frx = Path(frx).name
				DecompressFile(frx,'.gz')
				# don't bother recompressing, because it will be deleted
			else:
				DecompressFile(frx,'.gz')
				recompress = True
		else:
			sys.stderr.write(frx + ' is missing\n')
			continue
	else:
		if useRxCopy:
			ottp.Debug(f'Copying {frx}')
			shutil.copy(frx,'./')
			frx = Path(frx).name # strip path - it's now in the working directory
			
	# sbf2rin defaults to file names in V2 format
	# sbf2rnx follows the same convention
	if rxFileFormat == SBF:
		fObs = '{}{:03d}0.{:02d}O'.format(defRnxStation,doy,yy) # as produced by sbf2rin
		fNav = '{}{:03d}0.{:02d}P'.format(defRnxStation,doy,yy) # mixed navigation file
		# Some ambiguity about whether spaces are allowed between option and value
		# but it works for the options used here
		cmd = [SBF2RIN,'-f',frx,'-R',rnxVersion,'-i',rnxObsInterval,'-n',rnxFiles,'-x',rnxExclusions]
	elif rxFileFormat == JPS:
		fObs = '{}.{:02d}o'.format(mjd,yy) # as produced by jps2rin
		fNav = '{}.{:02d}p'.format(mjd,yy) # mixed navigation file
		# Again, inconsistencies in the way option values are handled by jps2rin
		# Doppler and signal strength observations can be filtered out
		# We'll keep GPS L5 observations
		excludedObservations = '-b=D??,S??'
		navFileOption = '--mxd'
		cmd = [JPS2RIN,'-v='+rnxVersion] + rnxExclusions + ['--dt='+str(int(rnxObsInterval)*1000),'--fd',navFileOption,excludedObservations,frx]
	ottp.Debug('Running')
	try:
		x = subprocess.check_output(cmd) 
	except subprocess.CalledProcessError as e:
		print(f"Command that failed: {e.cmd}")
		print(f"Return code: {e.returncode}")
		print(f"Error output: {e.output}")
		ottp.ErrorExit('Failed to run')
		
	ottp.Debug(x.decode('utf-8'))
	
	# Rename the files
	oldFObs = fObs
	if nameFormat == '2':
		fObs = '{}{:03d}0.{:02d}O'.format(rnxObsStation,doy,yy)
	elif nameFormat == '3':
		fObs = '{}_R_{:04d}{:03d}0000_01D_30S_MO.rnx'.format(rnxObsStation,yyyy,doy) # FIXME observation interval ??

	# Fix the observation file header
	if os.path.exists(oldFObs):
		ottp.Debug('Renaming ' + oldFObs + ' to ' + fObs)
		os.rename(oldFObs,fObs)
		# Now fix the header
		if fixHeader:
			ottp.Debug('Fixing header')
			fout = open(fObs + '.tmp','w')
			fin =  open(fObs,'r',errors='replace')
			readingHeader = True
			for l in fin:
				if readingHeader:
					if 'END OF HEADER' in l:
						fout.write(l)
						readingHeader = False
					else:
						key = l[60:].rstrip() # trailing whitespace only
						if key in headerFixes:
							ottp.Debug('Fixing ' + key)
							fout.write(headerFixes[key]+'\n')
						elif (key == '# OF SATELLITES'):
							if bodgeSatCountBug:
								ottp.Debug('Dropping # OF SATELLITES')
								pass # skip the line since it is optional anyway
							else:
								fout.write(l)
						else:
							fout.write(l)
				else:
					fout.write(l)
			fin.close()
			fout.close()
			ottp.Debug('Moving ' + fObs + '.tmp' + ' to ' + os.path.join(rnxObsDir,fObs))
			shutil.move(fObs + '.tmp',os.path.join(rnxObsDir,fObs))
			os.unlink(fObs)
		else:
			ottp.Debug('Moving ' + fObs + ' to ' + os.path.join(rnxObsDir,fObs))
			shutil.move(fObs,os.path.join(rnxObsDir,fObs))
	else:
		ottp.Debug('The OBS file is missing')
		
	oldFNav = fNav
	if nameFormat == '2':
		fNav = '{}{:03d}0.{:02d}P'.format(rnxNavStation,doy,yy) # nb mixed GNSS
	elif nameFormat == '3':
		fNav = '{}_R_{:04d}{:03d}0000_01D_MN.rnx'.format(rnxNavStation,yyyy,doy)
		
	if os.path.exists(oldFNav):
		ottp.Debug('Moving ' + oldFNav + ' to ' + os.path.join(rnxNavDir,fNav))
		shutil.move(oldFNav,os.path.join(rnxNavDir,fNav))
	else:
		ottp.Debug('The NAV file is missing')
	
	if rxFileFormat == JPS:
		# jps2rin creates a directory jps2rin for temporary files
		# If there was an abnormal termination, then the files are not scrubbbed
		ottp.Debug('Scrubbing jps2rin temporary files')
		shutil.rmtree('jps2rin')
		
	if useRxCopy:
		ottp.Debug('Deleting receiver data file')
		os.unlink(frx)
	if recompress:
		CompressFile(frx,'.gz')
		
os.chdir(cwd) # back to starting directory
