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

import argparse
import distro
import os
import platform
import re
import shutil
import subprocess

import sys

VERSION = '1.0.1'
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
	['CentOS Linux','7','centos7',SYSTEMD,'/usr/local/lib64/perl5',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.6/site-packages'],
	['CentOS Linux','8','centos8',SYSTEMD,'/usr/local/lib64/perl5',
                '/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.6/site-packages'],
	['Red Hat Enterprise Linux','8','rhel8',SYSTEMD,'/usr/local/lib64/perl5',
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
	['Debian GNU/Linux','8','bbdebian8',SYSTEMD,'/usr/local/lib/site_perl',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.4/dist-packages/'],
	['Debian GNU/Linux','9','bbdebian9',SYSTEMD,'/usr/local/lib/site_perl',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.5/dist-packages/'],
	['Debian GNU/Linux','10','bbdebian10',SYSTEMD,'/usr/local/lib/site_perl',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.7/dist-packages/'],
	['Raspbian GNU/Linux','9','rpidebian9',SYSTEMD,'/usr/local/lib/site_perl',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.5/dist-packages/'],
	['Raspbian GNU/Linux','10','rpidebian10',SYSTEMD,'/usr/local/lib/site_perl',
		'/usr/local/lib/python2.7/site-packages','/usr/local/lib/python3.7/dist-packages/']]

# All available installation targets
alltargets = ['libconfigurator','dioctrl','lcdmon','ppsd',
	'sysmonitor','tflibrary','kickstart','gziplogs','misc','ottplib','cggttslib',
	'okcounterd','okbitloader','udevrules','gpscvperllibs']

# Targets for a minimal installation
mintargets = ['libconfigurator','tflibrary','kickstart','gziplogs','misc','ottplib','cggttslib']

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

# ------------------------------------------
def DetectOS():

	(dist,distrover,_)=distro.linux_distribution()
	Debug('Detected ' + dist + ' ' + distrover)
	dist=dist.lower()
	ver=distrover.split('.')
	majorVer = ver[0]
	
	for os in osinfo:
		if (dist == os[DISTNAME].lower() and majorVer == os[DISTVER]):
			Debug('Matched ' + dist + ' ' + majorVer)
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
	except subprocess.CalledProcessError as e:
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
def InstallPyModule(modname,srcdir,py2libdir,py3libdir):
	
	Debug('--> Installing ' + modname)
	try:
		py2 = subprocess.check_output(['which','python2']).strip().decode('utf-8');
	except:
		py2=''
	try:
		py3 = subprocess.check_output(['which','python3']).strip().decode('utf-8');
	except:
		py3=''
	src = srcdir + '/' + modname + '.py'
	
	if ('python2' in py2):
		ver = subprocess.check_output([py2,'-V'],stderr=subprocess.STDOUT).strip().decode('utf-8');
		Debug('python2 is ' + ver)
		match = re.search('^Python\s+(\d+)\.(\d+)',ver)
		minorVer = -1
		if match:
			minorVer = int(match.group(2))
		else:
			ErrorExit('Unable to determine python2 version')
		if (minorVer >= 7):
			MakeDirectory(py2libdir)
			try:
				cmd = 'import py_compile;py_compile.compile(\'' + src + '\')'
				subprocess.check_output([py2,'-c',cmd],stderr=subprocess.STDOUT).strip().decode('utf-8');
				shutil.copy(src,py2libdir)
				shutil.copy(srcdir + '/' + modname + '.pyc',py2libdir)
			except:
				ErrorExit('Failed to compil/install ' + modname + ' (python2)')
			Log('Installed ' + modname + ' to ' + py2libdir)
		else:
			ErrorExit('Python2 version is ' + str(minorVer) + ': unsupported')
		
	if ('python3' in py3):
		ver = subprocess.check_output([py3,'-V'],stderr=subprocess.STDOUT).strip().decode('utf-8');
		Debug('python3 is ' + ver)
		match = re.search('^Python\s+(\d+)\.(\d+)',ver)
		minorVer = -1
		if match:
			minorVer = int(match.group(2))
		else:
			ErrorExit('Unable to determine python3 version')
		if (minorVer >= 6):
			MakeDirectory(py3libdir)
			MakeDirectory(py3libdir + '/__pycache__')
			try:
				cmd = 'import py_compile;py_compile.compile(\'' + src + '\')'
				subprocess.check_output([py3,'-c',cmd],stderr=subprocess.STDOUT).strip().decode('utf-8');
				shutil.copy(src,py3libdir)
				shutil.copy(srcdir + '/__pycache__/' + modname + '.cpython-3' + str(minorVer)+ '.pyc',
					py3libdir + '/__pycache__')
			except:
				ErrorExit('Failed to compile/install ' + modname + ' (python3)')
			Log('Installed ' + modname + ' to ' + py3libdir)
		else:
			ErrorExit('Python3 version is ' + str(minorVer) + ': unsupported')
	return

#--------------------------------------------
def InstallScript(src,dst):
	shutil.copy(src,dst)
	Log('Installed ' +src + ' to ' + dst)
	return

#--------------------------------------------
def EnableService(service):
	try:
		subprocess.check_output(['systemctl','enable',service])
		Log('Enabled the service ' + service)
	except:
		Log('Failed to enable the service ' + service)
	return

# ------------------------------------------
# Main
# ------------------------------------------

hints = '' # Hints to user after installation

examples =  'Usage examples\n'
examples += '1. Install libconfigurator\n'
examples += '   installsys.py -i libconfigurator \n'

parser = argparse.ArgumentParser(description='Install the OTTP system software',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

# No required arguments

# Optional arguments
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--install','-i',metavar = 'TARGET',help='install TARGET (see -l option)')
parser.add_argument('--list','-l',help='list targets for installation (see -i option)',
	action='store_true')
parser.add_argument('--minimal','-m',help='do a minimal installation',action='store_true')
parser.add_argument('--version','-v',action='version',
	version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

targets = alltargets

if (os.geteuid() > 0):
	ErrorExit('This script must be run with superuser privileges')

if args.list:
	print('Available targets for installation are:')
	for t in targets:
		print(t)
	sys.exit(0)

if args.install:
	if args.install in alltargets:
		targets = [args.install]
	else:
		ErrorExit(args.install + ' is not a known target')
elif args.minimal:
	targets = mintargets
	print('Minimal installation:')
	for t in targets:
		print(t)

thisos = DetectOS()
if not thisos:
	print('Your Linux distribution has not been tested against.')
	print('The tested distributions are:')
	for o in osinfo:
		print(o[DISTNAME]+' ' + o[DISTVER])
	print()
	if (not GetYesNo('Do you want to proceed (y/n)?')):
		ErrorExit('Unsupported OS')
	else:
		# FIXME better defaults
		thisos = ['Unsupported','?','unsupported','/usr/local/lib/site_perl']

(_,_,_,_,architecture,processor)=platform.uname()
if architecture.find('arm') == 0:
	processor = 'arm'

initSys = thisos[INITSYS]

# The nuts and bolts of it

if ('udevrules' in targets):
	InstallScript('udev/60-opalkelly.rules','/etc/udev/rules.d')
	#FIXME trigger ?

if (not args.install):
	Log('Creating any miscellaneous directories')
	MakeDirectory('/usr/local/lib/bitfiles')

# Installation of TFLibrary (Perl module)
if ('tflibrary' in targets):
	MakeDirectory(thisos[PERLLIBDIR])
	shutil.copy('src/TFLibrary.pm',thisos[PERLLIBDIR])
	Log('Installed TFLibrary to ' + thisos[PERLLIBDIR])

# Installation of miscellaneous GPSCV Perl libraries
if ('gpscvperllibs' in targets):
	libdir = thisos[PERLLIBDIR] + '/' + 'LTELite'
	MakeDirectory(libdir)
	shutil.copy('../gpscv/gpsdo/LTELite/DecodeNMEA.pm',libdir)
	Log('Installed gpsdo/LTELite/DecodeNMEA.pm to ' + libdir) 
	
	libdir = thisos[PERLLIBDIR] + '/' + 'NV08C'
	MakeDirectory(libdir)
	shutil.copy('../gpscv/nvs/lib/DecodeMsg.pm',libdir)
	Log('Installed nvs/lib/DecodeMsg.pm to ' + libdir) 
	
# Installation of Python modules
if ('ottplib' in targets):
	InstallPyModule('ottplib','src',thisos[PY2LIBDIR],thisos[PY3LIBDIR])

if ('cggttslib' in targets):
	InstallPyModule('cggttslib','src',thisos[PY2LIBDIR],thisos[PY3LIBDIR])

# Installation of C/C++ libraries
if ('libconfigurator' in targets):
	CompileTarget('libconfigurator','src/libconfigurator','install')

if ('dioctrl' in targets):
	if (processor == 'arm'):
		Log('dioctrl is not available on ARM')
	else:
		CompileTarget('dioctrl','src/dioctrl','install')

if ('lcdmon' in targets):
	CompileTarget('lcdmon','src/lcdmon','install')
	if (initSys == SYSTEMD):
		EnableService('lcdmonitor.service')
		hints += 'To start lcdmon, run: systemctl start lcdmonitor.service\n'
	elif (initSys == UPSTART):
		hints += 'To start lcdmon, run: start lcdmonitor\n'

if ('okbitloader' in targets):
	CompileTarget('okbitloader','src/okbitloader','install')
	
if ('okcounterd' in targets):
	CompileTarget('okcounterd','src/okcounterd','install')
	if (initSys == SYSTEMD):
		EnableService('okcounterd.service')
		hints += 'To start okcounterd, run: systemctl start okcounterd.service\n'
	elif (initSys == UPSTART):
		hints += 'To start okcounterd, run: start okcounterd\n'

if ('ppsd' in targets):
	if (processor == 'arm'):
		Log('ppsd is not available on ARM')
	else:
		CompileTarget('ppsd','src/ppsd','install')
		if (initSys == SYSTEMD):
			EnableService('ppsd.service')
			hints += 'To start ppsd, run: systemctl start ppsd.service\n'
		elif (initSys == UPSTART):
			hints += 'To start ppsd, run: start ppsd\n'

if ('misc' in targets):
	CompileTarget('misc','src','install')

# Installation of Perl and Python scripts

if ('kickstart' in targets):
	InstallScript('src/kickstart.pl','/usr/local/bin')

if ('gziplogs' in targets):
	InstallScript('src/gziplogs.pl','/usr/local/bin')

# Installation of sysmonitor
if ('sysmonitor' in targets):
	MakeDirectory('/usr/local/log')
	MakeDirectory('/usr/local/log/alarms')
	InstallScript('src/sysmonitor/sysmonitor.pl','/usr/local/bin')
	InstallScript('src/sysmonitor/sysmonitor.conf','/usr/local/etc')
	if (initSys == SYSTEMD):
		InstallScript('src/sysmonitor/sysmonitor.service','/lib/systemd/system')
		EnableService('sysmonitor.service')
		hints += 'To start sysmonitor, run: systemctl start sysmonitor.service\n'
	elif (initSys == UPSTART):
		InstallScript('src/sysmonitor/sysmonitor.upstart.conf','/etc/init/sysmonitor.conf')
		hints += 'To start sysmonitor, run: start sysmonitor\n'
		
# Print any post-installation hints
if (not hints == ''):
	print()
	print('Post install hints:')
	print(hints)
