#!/usr/bin/python3
#

#
# The MIT License (MIT)
#
# Copyright (c) 2020 Michael J. Wouters
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the 'Software'), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

# 2022-08-01 ELM Added path info for Ubuntu 22, version now 0.1.2 (was 0.1.1)

import argparse
import datetime
import distro
import glob
import multiprocessing
import os
import platform
import re
import shutil
import subprocess
import sys

# This is where ottplib is installed
sys.path.append('/usr/local/lib/python3.6/site-packages')
sys.path.append('/usr/local/lib/python3.8/site-packages')
sys.path.append('/usr/local/lib/python3.10/dist-packages')
import ottplib

VERSION = '0.1.2'
AUTHORS = 'Michael Wouters, Louis Marais'

# init systems on Linux
UPSTART='upstart'
SYSTEMD='systemd'

DISTNAME = 0
DISTVER = 1
DISTSHORTNAME = 2
INITSYS = 3
PERLLIBDIR = 4
PY2LIBDIR = 5
PY3LIBDIR = 6

osinfo = [
	['RedHat','6','rhel6',UPSTART,'/usr/local/lib/site_perl','',''], 
	['Red Hat Enterprise Linux','8','rhel8',SYSTEMD,'/usr/local/lib64/perl5',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.6/site-packages'],
	['CentOS','6','centos6',UPSTART,'/usr/local/lib/site_perl','',''],
	['CentOS Linux','7','centos7',SYSTEMD,'/usr/local/lib64/perl5',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.6/site-packages'],
	['Ubuntu','14','ubuntu14',UPSTART,
		'/usr/local/lib/site_perl','/usr/local/lib/python2.7/site-packages',''],
	['Ubuntu','16','ubuntu16',UPSTART,
		'/usr/local/lib/site_perl','/usr/local/lib/python2.7/site-packages',''],
	['Ubuntu','18','ubuntu18',SYSTEMD,
		'/usr/local/lib/site_perl','/usr/local/lib/python2.7/site-packages',
		'/usr/local/lib/python3.6/site-packages'],
	['Ubuntu','20','ubuntu20',SYSTEMD,
		'/usr/local/lib/site_perl','/usr/local/lib/python2.7/site-packages',
		'/usr/local/lib/python3.8/site-packages'],
	['Ubuntu','22','ubuntu22',SYSTEMD,
		'/usr/local/lib/site_perl','/usr/local/lib/python2.7/site-packages',
		'/usr/local/lib/python3.10/dist-packages'],
	['Debian GNU/Linux','8','bbdebian8',SYSTEMD,'/usr/local/lib/site_perl',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.4/dist-packages/'],
	['Debian GNU/Linux','9','bbdebian9',SYSTEMD,'/usr/local/lib/site_perl',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.5/dist-packages/'],
	['Debian GNU/Linux','10','bbdebian10',SYSTEMD,'/usr/local/lib/site_perl',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.5/dist-packages/'],
	['Raspbian GNU/Linux','9','rpidebian9',SYSTEMD,'/usr/local/lib/site_perl',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.5/dist-packages/'],
	['Raspbian GNU/Linux','10','rpidebian10',SYSTEMD,'/usr/local/lib/site_perl',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.7/dist-packages/']]

# All available installation targets
basetargets = ['mktimetx','misc scripts']
alltargets  = ['mktimetx','gpsdo','javad','nvs','trimble','ublox','prs10','misc scripts']
ttsv5dirs   = ['raw/rest','raw/navspark'] # extra directories for TTS V5

receivers = [
	['Trimble','Resolution T','trimble'], # manufacturer, model, directory
	['Javad','any','javad'],
	['NVS','NV08C','nvs'],
	['ublox','NEOM8T','ublox'],
	['ublox','ZED-F9P','ublox'],
	['ublox','ZED-F9T','ublox'],
	['all', '', '']
]

# ------------------------------------------
def Debug(msg):
	
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ------------------------------------------
def Log(msg):
	print(msg)
	return

# ------------------------------------------
def ErrorExit(msg):
	
	print('\nError! ' + msg)
	print('Exited')
	sys.exit(1)

# ------------------------------------------
def GetYesNo(msg):
	
	done = False
	while (not done):
		val = input(msg)
		val=val.strip().lower()
		if (val == 'y' or val == 'n' or val == 'yes' or val == 'no'):
			return (val == 'y' or val == 'yes')
	return

# ------------------------------------------
def DetectOS():

	(dist,distrover,_)=distro.linux_distribution()
	Log('Detected ' + dist + ',ver ' + distrover)
	dist=dist.lower()
	ver=distrover.split('.')
	majorVer = ver[0]
	
	for os in osinfo:
		if (dist == os[DISTNAME].lower() and majorVer == os[DISTVER]):
			Debug('Matched ' + dist + ' ' + majorVer)
			Log('OS is supported')
			return os

	return []

# ------------------------------------------
def MakeDirectory(dir):
	
	if (os.path.isdir(dir)):
		Log(dir + ' exists ... nothing to do')
	else:
		if (os.path.exists(dir)):
			ErrorExit(dir + ' exists but is not a directory')
		else:
			Log('Creating ' + dir)
			try:
				os.makedirs(dir)
			except:
				ErrorExit('Failed to create ' + dir)
			# FIXME permissions ?
	return

#--------------------------------------------
def CompileTarget(target,targetDir,makeArgs=''):
	
	Log('--> Compiling ' + target)
	
	args = makeArgs.split()
	
	cwd = os.getcwd() # save current working directory
	os.chdir(targetDir)
	
	if (os.path.exists('configure.py')):
		Debug('Configuring ..')
		try:
			dbgout = subprocess.check_output(['./configure.py'])
			Debug(dbgout.decode('utf-8'))
		except:
			Log('configure.py failed on ' + target)
	elif (os.path.exists('configure.pl')):
		Debug('Configuring ...')
		try:
			dbgout = subprocess.check_output(['./configure.pl'])
			Debug(dbgout.decode('utf-8'))
		except:
			Log('configure.pl failed on ' + target)
	try:
		Debug('Cleaning ...')
		compOut = subprocess.check_output(['make','clean'])
		Debug(compOut.decode('utf-8'))
	except:
		Log('Make clean failed on ' + target)
		
	try:
		compOut = subprocess.check_output(['make']+args)
		Debug(compOut.decode('utf-8'))
	except:
		Log('Failed to compile ' + target)
		
	os.chdir(cwd) # back to the starting directory
	Debug('... done')
	return

#--------------------------------------------
def InstallExecutables(srcDir,dstDir,archiveDir):
	# If a single target is being installed, directory creation is skipped
	# so test for existence and warn the user
	if (not os.path.exists(dstDir)):
		Log(dstDir + ' is missing: ' + srcDir + '* was not installed')
		return
	if (not os.path.exists(archiveDir)): # test to trim logged messages a bit
		MakeDirectory(archiveDir)
	files = glob.glob(os.path.join(srcDir,'*'))
	for f in files:
		if (os.access(f, os.X_OK) and not os.path.isdir(f)):
			# backup the old one
			fold = os.path.join(dstDir,os.path.basename(f))
			if (os.path.exists(fold)): # may not be there yet
				shutil.copy2(fold,archiveDir)
			# install the new one
			shutil.copy2(f,dstDir) # preserve file attributes
	Log('Installed executables from ' + srcDir)
	return

#--------------------------------------------
def InstallConfigs(srcDir,dstDir):
	if (not os.path.exists(dstDir)):
		Log(dstDir + ' is missing: ' + srcDir + '*.conf was not installed')
		return
	files = glob.glob(os.path.join(srcDir,'*.conf'))
	for f in files:
		shutil.copy2(f,dstDir) # preserve file attributes
	Log('Installed configs from ' + srcDir)
	return

#--------------------------------------------
# This is a kludge to grab config files which do not have the extension .conf
# Remove it one day
def InstallAll(srcDir,dstDir):
	if (not os.path.exists(dstDir)):
		Log(dstDir + ' is missing: ' + srcDir + '* was not installed')
		return
	files = glob.glob(os.path.join(srcDir,'*'))
	for f in files:
		shutil.copy2(f,dstDir) # preserve file attributes
	Log('Installed all from ' + srcDir)
	return

#--------------------------------------------
# Returns an index into the receiver list
def DetectReceiver(cfg):
	if ('receiver:manufacturer' in cfg and 'receiver:model' in cfg):
		rxman = cfg['receiver:manufacturer']
		rxmod = cfg['receiver:model']
		i=0
		for r in receivers:
			if (r[0] == rxman and r[1] == rxmod):
				Log('Identified a supported receiver in gpscv.conf: ' + rxman + ' ' + rxmod)
				return i
			i=i+1
		Log('Couldn\'t match the receiver configured in gpscv.conf')
	else:
		Log('A receiver is not defined in gpscv.conf')
		return -1
	return

#--------------------------------------------
# Select the receiver to install software for
# Returns an index into the receiver list
def ChooseReceiver():

	print 
	print('Please select the receiver to install software for:')
	i=1
	for r in receivers:
		print(str(i) + '. ' + r[0] + ' ' + r[1])
		i += 1
	max = i-1
	
	while (True):
		sval = input('Choose (1-'+str(max)+'): ')
		try:
			val=int(sval)
			if (val >=1 and val <= max): 
				return val-1 # cos it's an index
		except:
			pass
	return

# ------------------------------------------
# Main
# ------------------------------------------

hints = '' # Hints to user after installation

examples =  'Usage examples\n'
examples += '1. Install mktimetx\n'
examples += '   install.py -i mktimetx \n'

parser = argparse.ArgumentParser(
	description='Install the OTTP GPSCV software',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

# No required arguments

# Optional arguments
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--install','-i',help='install a target')
parser.add_argument('--ttsversion','-t',help='tts version (5 only, currently)')
parser.add_argument('--list','-l',help='list targets for installation',
	action='store_true')
parser.add_argument('--version','-v',action='version',
	version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

ttsver = -1 # undefined

home = os.path.expanduser("~")

debug = args.debug

targets = basetargets

if args.list:
	print('Available targets for installation are:')
	for t in alltargets:
		print(t)
	sys.exit(0)

if args.install:
	if args.install in alltargets:
		targets = [args.install]
	else:
		ErrorExit(args.install + ' is not a known target')

if args.ttsversion:
	ttsver = int(args.ttsversion)
	if not(ttsver == 5):
		ErrorExit(args.install + ' only TTS version 5 is supported')
thisos = DetectOS()
if not thisos:
	print('Your Linux distribution has not been tested against.')
	print('The tested distributions are:')
	for os in osinfo:
		print(os[DISTNAME],os[DISTVER])
	print
	if (not GetYesNo('Do you want to proceed (y/n)? ')):
		ErrorExit('Unsupported OS')
	else:
		# FIXME better defaults
		thisos = ['Unsupported','?','unsupported','/usr/local/lib/site_perl']

(_,_,_,_,architecture,processor)=platform.uname()
if architecture.find('arm') != -1:
	processor = 'arm'

try:
	ncpus = multiprocessing.cpu_count()
except:
	ncpus = 1
makeArgs = '-j '+str(ncpus)

usingCfg = False

instRoot = home
dataRoot = instRoot

configDir  = os.path.join(instRoot, 'etc')
binDir =     os.path.join(instRoot, 'bin')
cggttsDir =  os.path.join(dataRoot, 'cggtts')
logDir =  os.path.join(dataRoot, 'logs')
rawDir = os.path.join(dataRoot, 'raw')
rinexDir = os.path.join(dataRoot, 'rinex')
tmpDir = os.path.join(dataRoot, 'tmp')
firmwareDir = os.path.join(instRoot, 'firmware')

config = os.path.join(configDir,'gpscv.conf')

if (not args.install):
	cfg = []
	if (os.path.exists(config)):
		Log('Detected an existing gpscv.conf')
		if GetYesNo('Do you want to use this to set various installation options (y/n)? '):
			Log('Using ' + config + ' for configuration')
			usingCfg = True
			cfg=ottplib.LoadConfig(config,{'tolower':True})
			if (cfg == None):
				ErrorExit("Error loading " + config)
		else:
			Log('The detected gpscv.conf will not be used')
	else:
		Log('An existing gpscv.conf was not detected')

	# Detect the receiver
	rx=-1
	if (usingCfg):
		rx = DetectReceiver(cfg) 
	if (rx < 0):
		rx = ChooseReceiver()

	if (rx == len(receivers) -1):
		for i in range(0,len(receivers)-1):
			if (not (receivers[i][2] in targets)):
				targets.append(receivers[i][2])		
	else:
		targets.append(receivers[rx][2])

	# PRS10 support is mainly for legacy systems
	# If it's in gpscv.conf recompile
	# Otherwise, ask nicely
	if (usingCfg):
		if ('counter:logger script' in cfg):
			if ('prs10log.pl' in cfg['counter:logger script']):
				targets.append('prs10')
		else:
			pass
	else:
		if GetYesNo('Do you want to install PRS10 support (y/n)? '):
			targets.append('prs10')
	
	# Detect if a GPSDO is the reference
	if (usingCfg):
		if ('reference:oscillator' in cfg):
			if ('gpsdo' in cfg['reference:oscillator']):
				targets.append('gpsdo')
		else:
			pass
	else:
		if GetYesNo('Do you want to install GPSDO support (y/n)? '):
			targets.append('gpsdo')
	
	# Create any missing directories

	if (instRoot == dataRoot):
		Log('\nCreating directories in ' + instRoot)
	else:
		Log('Creating directories in ' + instRoot + ' and ' + dataRoot)

	dirs = [binDir,configDir,firmwareDir]
	dataDirs = [cggttsDir,logDir,rawDir,rinexDir,tmpDir]

	for dir in dirs:
		MakeDirectory(dir)

	for dir in dataDirs:
		MakeDirectory(dir)

	# Make hardware dependent directories
	if (ttsver == 5):
		MakeDirector(ttsv5dirs)
	
# Make the archival directory
# Currently, only executables are archived
archiveRoot = os.path.join(instRoot,'archive')
archiveDir = os.path.join(archiveRoot,datetime.datetime.now().strftime('%Y%m%d'))
Log('Archiving to ' + archiveDir)
MakeDirectory(archiveRoot)
MakeDirectory(archiveDir)
	
# Installation targets

if ('mktimetx' in targets):
	CompileTarget('mktimetx','common/process/mktimetx',makeArgs)
	InstallExecutables('common/process/mktimetx',binDir,os.path.join(archiveDir,'bin'))

if ('gpsdo' in targets):
	InstallExecutables('gpsdo',binDir,os.path.join(archiveDir,'bin'))
	InstallConfigs('gpsdo',configDir)

if ('javad' in targets):
	InstallExecutables('javad',binDir,os.path.join(archiveDir,'bin'))
	InstallConfigs('javad',configDir)

if ('nvs' in targets):
	InstallExecutables('nvs',binDir,os.path.join(archiveDir,'bin'))
	InstallConfigs('nvs',configDir)

if ('trimble' in targets):
	InstallExecutables('trimble',binDir,os.path.join(archiveDir,'bin'))
	InstallConfigs('trimble',configDir)

if ('ublox' in targets):
	InstallExecutables('ublox',binDir,os.path.join(archiveDir,'bin'))
	InstallConfigs('ublox',configDir)

if ('prs10' in targets):
	CompileTarget('prs10','prs10')
	InstallExecutables('prs10',binDir,os.path.join(archiveDir,'bin'))

if ('misc scripts' in targets):
	InstallExecutables('common/bin',binDir,os.path.join(archiveDir,'bin'))
	InstallAll('common/etc',configDir)

Log('\nFinished installation :-)')

print('\n\nA log of the installation has been saved in ./install.log')
print('\t\t')
print('\t\t  o')
print('\t\t   o')
print('\t\t  o')  
print('\t\t____')
print('\t\t| o |')
print('\t\t|   |')  
print('\t\t|   |')
print('\t\t\\  /')
print('\t\t ||')
print('\t\t ||')
print('\t\t ||')
print('\t\t====')

# Print any post-installation hints
if (not hints == ''):
	print
	print('Post install hints:')
	print(hints)
