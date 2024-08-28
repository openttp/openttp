#!/usr/bin/python3


#
# The MIT License (MIT)
#
# Copyright (c) 2015 Michael J. Wouters
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
# Modification history
# 2016-03-30 MJW Replaces zip_data. First version.
# 2017-12-11 MJW Extended capabilities. More extensible. Compressed files can be moved. 
# 2022-05-30 MJW Move compressed files
# 2023-02-05 MJW python3 port. Version 1.x.x


import argparse
import datetime
import os
import shutil
import subprocess
import sys
import time

# This is where ottplib is installed
# 
sys.path.append("/usr/local/lib/python3.6/site-packages")  # Ubuntu 18.04
sys.path.append("/usr/local/lib/python3.8/site-packages")  # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22.04
sys.path.append("/usr/local/lib/python3.12/site-packages") # Ubuntu 24.04

try: 
	import ottplib as ottp
except ImportError:
	sys.exit('ERROR: Must install ottplib\n eg openttp/software/system/installsys.py -i ottplib')

VERSION = "1.1.0"
AUTHORS = "Michael Wouters"

# ------------------------------------------
def ShowVersion():
	print (os.path.basename(sys.argv[0])+" "+VERSION)
	print ('Written by ' + AUTHORS)
	return

# ------------------------------------------
def CompressFile(f):
	ottp.Debug('Checking ' + f)
	if os.path.isfile(f):
		try:
			ottp.Debug('Compressing ' + f)
			x = subprocess.check_output(['gzip','-f',fname]) # -q to suppress warnings?
		except Exception as e:
			print(e) # not fatal
	
# ------------------------------------------
# Main body 
# ------------------------------------------

home = os.environ['HOME'] 
root = home 
configFile = os.path.join(root,'etc','gziplogs.conf')
appName = os.path.basename(sys.argv[0])
examples=''

#if ottplib.LibMinorVersion() < 1:
#	print('ottplib major version < 1')
#	sys.exit(1)

parser = argparse.ArgumentParser(description='Compresses daily log files',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

parser.add_argument('mjd',nargs = '*',help='first MJD [last MJD] (if not given, the MJD of the previous day is used)')
parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

ottp.SetDebugging(args.debug)

if (args.version):
	ShowVersion()
	exit()

configFile = args.config;
if (not os.path.isfile(configFile)):
	ottp.ErrorExit(configFile + ' not found')
cfg=ottp.Initialise(configFile,[])

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

for mjd in range(startMJD,stopMJD + 1):
	
	tmjd =  (mjd - 40587)*86400
	utc = datetime.datetime.fromtimestamp(tmjd,tz = datetime.timezone.utc)
	ymd = utc.strftime('%Y%m%d')
	doy = utc.strftime('%j') # zero padded
	yy  = utc.strftime('%y') # ditto
	
	if ':files' in cfg: # old style, single entry
		files = cfg[':files'].split(',')
		files = [f.strip() for f in files]
		for f in files:
			f=f.strip() # trim white space from beginning and end
			f=f.replace('{MJD}',str(mjd))
			f=f.replace('{YYYYMMDD}',ymd)
			f=f.replace('{DOY}',doy) # this
			f=f.replace('{YY}',yy)   # plus this is useful for V2 RINEX file names
			fname = ottp.MakeAbsoluteFilePath(f,root,os.path.join(root,'raw')) # default is just GPSCV raw data files
			CompressFile(fname)
			# nuffink more to do
	elif ':targets' in cfg: # new style, extensible and more flexible 
		targets = cfg[':targets'].split(',')
		for t in targets:
			t = t.strip()
			if t+':files' in cfg:
				files = cfg[t + ':files'].split(',')
				for f in files:
					f=f.strip() # trim white space from beginning and end
					f=f.replace('{MJD}',str(mjd))
					f=f.replace('{YYYYMMDD}',ymd)
					f=f.replace('{DOY}',doy)
					f=f.replace('{YY}',yy)
					fname = ottp.MakeAbsoluteFilePath(f,root,os.path.join(root,'raw')) 
					CompressFile(fname)
					# The following moves any matching compressed file, if a destination is defined
					fgz = fname + '.gz'
					if os.path.isfile(fgz) and (t+':destination' in cfg) :
						dst = ottp.MakeAbsolutePath(cfg[t+':destination'],home)
						ottp.Debug('Moving ' + fgz + ' to ' + dst)
						try:
							shutil.move(fgz,dst)
						except Exception as e:
							print(e) # it's not the end of the world
			else:
				ottp.Debug('No files defined for target ' + t)
	else:
		pass

# That's all folks!
