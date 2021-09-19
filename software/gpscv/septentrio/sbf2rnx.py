#!/usr/bin/python3

#
# Processing script for Septentrio receivers
# 

import argparse
import binascii
import os
import string
import shutil
import subprocess
import sys

# This is where ottplib is installed

sys.path.append('/usr/local/lib/python3.6/site-packages')
sys.path.append('/usr/local/lib/python3.8/site-packages')
import time

import ottplib

VERSION = '0.0.1'
AUTHORS = 'Michael Wouters'

# Some defaults

SBF2RIN='/usr/local/bin/sbf2rin'

#-----------------------------------------------------------------------------
def Debug(msg):
	if (debug):
		print(msg)
	return

#-----------------------------------------------------------------------------
def ErrorExit(msg):
	print (msg)
	sys.exit(0)

#-----------------------------------------------------------------------------
def Initialise(configFile):
	cfg=ottplib.LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit("Error loading " + configFile)
		
	# Check for required arguments
	reqd = ['paths:receiver data','receiver:file extension', \
		       'rinex:obs sta','rinex:nav sta']
	
	for k in reqd:
		if (not k in cfg):
			ErrorExit('The required configuration entry "' + k + '" is undefined')
	
	return cfg

#-----------------------------------------------------------------------------
def Cleanup():
	# Hmm ugly globals
	ottplib.RemoveProcessLock(lockFile)


# ---------------------------------------------
def MJDtoYYYYDOY(mjd):
	tt = (mjd - 40587)*86400
	utctod = time.gmtime(tt)
	return (utctod.tm_year,utctod.tm_yday)
      
# ---------------------------------------------
def DecompressFile(basename,ext):
	if (ext == '.gz'):
		subprocess.check_output(['gunzip',basename + ext])
		Debug('Decompressed ' + basename)
	
# ---------------------------------------------
def CompressFile(basename,ext):
	if (ext == '.gz'):
		subprocess.check_output(['gzip',basename])
		Debug('Compressed ' + basename)

# ---------------------------------------------
# Main


home =os.environ['HOME'] 
root = home
tmpDir = os.path.join(home,'tmp')
rawDir = os.path.join(home,'raw')
rnxDir = os.path.join(home,'RINEX')

rnxVersion = '3' # as a string
rnxObsInterval = '30'
rnxExclusions = 'ISJ'
defRnxStation = 'SEPT' # deulat station name used by sbf2rin
fixHeader = False

configFile = os.path.join(home,'etc','sbf2rnx.conf')

parser = argparse.ArgumentParser(description='Generate RINEX files from Septentrio SBF (wrapper for sbf2rin) ',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('mjd',nargs = '*',help='first MJD [last MJD] (if not given, the MJD of the previous day is used)')
parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug
configFile = args.config;

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + ' not found')
	
logPath = os.path.join(home,'logs')
if (not os.path.isdir(logPath)):
	ErrorExit(logPath + "not found")

cfg=Initialise(configFile)

firstMJD = ottplib.MJD(time.time()) - 1; # two days ago
lastMJD  = firstMJD

if (args.mjd):
	if 1 == len(args.mjd):
		firstMJD = int(args.mjd[0])
		lastMJD  = firstMJD
	elif ( 2 == len(args.mjd)):
		firstMJD = int(args.mjd[0])
		lastMJD  = int(args.mjd[1])
		if (lastMJD < firstMJD):
			ErrorExit('Stop MJD is before start MJD')
	else:
		ErrorExit('Too many MJDs')

Debug('Processing MJDs ' + str(firstMJD) + ' to ' + str(lastMJD))

rawDir = ottplib.MakeAbsolutePath(cfg['paths:receiver data'],root)
rxExtension = '.sbf'

if 'receiver:file extension' in cfg:
	rxExtension = cfg['receiver:file extension']

if 'paths:tmp' in cfg:
	tmpDir = ottplib.MakeAbsolutePath(cfg['paths:tmp'],root)
	
if 'rinex:version' in cfg:
	rnxVersion = cfg['rinex:version']
if 'rinex:version' in cfg:
	rnxVersion = cfg['rinex:version']

if 'main:exec' in cfg:
	SBF2RIN = cfg['main:exec']

createNav = False
if 'rinex:create nav file' in cfg:
	token  = cfg['rinex:create nav file'].lower()
	if ('yes' == token or 'true' == token):
		createNav =True
	
rnxFiles = ' -nO'
if createNav:
	rnxFiles += 'P'

rnxObsStation = cfg['rinex:obs sta']
rnxNavStation = cfg['rinex:nav sta']

if 'rinex:fix header' in cfg:
	token  = cfg['rinex:fix header'].lower()
	if ('yes' == token or 'true' == token):
		fixHeader =True
		
# Change to the tmp directory to create files
cwd = os.getcwd() # save old directory
os.chdir(tmpDir)

headerFixes = {}  # store as dictionary
if fixHeader:
	headerFile = ottplib.MakeAbsoluteFilePath(cfg['rinex:header fixes'],root,os.path.join(root,'etc'))
	if os.path.exists(headerFile):
		fin = open(headerFile,'r')
		for l in fin:
			l = l.strip()
			headerFixes[l[60:]] = l
		fin.close()
	else:
		ErrorExit(headerFile + ' is missing')
		
for mjd in range(firstMJD,lastMJD+1):
	(yyyy,doy) = MJDtoYYYYDOY(mjd)
	yy = yyyy-int(yyyy/100)*100
	fin = os.path.join(rawDir,str(mjd) + '.' + rxExtension)
	recompress  = False
	if not(os.path.exists(fin)):
		if (os.path.exists(fin + '.gz')):
			DecompressFile(fin,'.gz')
			recompress = True
		else:
			sys.stderr.write(fin + ' is missing\n')
			continue
	
	# sbf2rin defaults to file names in V2 format
	
	fObs = '{}{:03d}0.{:02d}O'.format(defRnxStation,doy,yy)
	fNav = '{}{:03d}0.{:02d}P'.format(defRnxStation,doy,yy) # mixed navigation file
	cmd = SBF2RIN + ' -f ' + fin  + ' -R' + rnxVersion + ' -i' + rnxObsInterval + rnxFiles + ' -x' + rnxExclusions
	Debug('Running: ' + cmd)
	subprocess.check_call(cmd,shell=True)
	
	# Rename the files
	oldFObs = fObs
	fObs = '{}{:03d}0.{:02d}O'.format(rnxNavStation,doy,yy)
	
	# Fix the observation file header
	if os.path.exists(oldFObs):
		Debug('Renaming ' + oldFObs + ' to ' + fObs)
		os.rename(oldFObs,fObs)
		# Now fix the header
		if fixHeader:
			Debug('Fixing header')
			fout = open(fObs + '.tmp','w')
			fin =  open(fObs,'r')
			readingHeader = True
			for l in fin:
				if readingHeader:
					if 'END OF HEADER' in l:
						fout.write(l)
						readingHeader = False
					else:
						key = l[60:].strip()
						if key in headerFixes:
							Debug('Fixing ' + key)
							fout.write(headerFixes[key]+'\n')
						else:
							fout.write(l)
				else:
					fout.write(l)
			fin.close()
			fout.close()
			Debug('Moving ' + fObs + '.tmp' + ' to ' + os.path.join(rnxDir,fObs))
			shutil.move(fObs + '.tmp',os.path.join(rnxDir,fObs))
			os.unlink(fObs)
	else:
			pass
	
	oldFNav = fNav
	fNav = '{}{:03d}0.{:02d}P'.format(rnxNavStation,doy,yy)
	
	if os.path.exists(oldFNav):
		Debug('Moving ' + oldFNav + ' to ' + os.path.join(rnxDir,fNav))
		shutil.move(oldFNav,os.path.join(rnxDir,fNav))
		
	if recompress:
		CompressFile(fin,'.gz')
		
os.chdir(cwd) # back to starting directory
