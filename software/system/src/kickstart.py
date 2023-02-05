#!/usr/bin/python3


# The MIT License (MIT)
#
# Copyright (c) 2023 Michael J. Wouters
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

# kickstart.py
# Restarts processes listed in the file kickstart.conf
# Should be run periodically as a cron job
# Rewrite of kickstart.pl
#
# Shamelessly cut&pasted from check_rx and siblings
# 17-07-2002 MJW first version
# 02-08-2002 MJW pedantic filter run over code
#                processes with /usr/bin/perl -w now recognised
# 30-06-2009 MJW Path fixups
# 23-03-2016 MJW Pipe stdout & stderr from a process to its own file, rather than a common file.
#                Rip the guts out of this. Multiple problems - checking for processes with the same name,
#                different behaviour of ps across O/Ss, long process names, imperfect detection of different startup methods ....
#								 Demand use of a lock and check whether the process is running via this.
# 30-03-2016 MJW New configuration file format. Use the 'standard' .conf format
# 10-05-2017 ELM moved location of check files to ~/lockStatusCheck directory - on the BBB system this is a RAM disk.
# 2017-12-11 MJW Backwards compatibility for checkStatusPath
# 2023-01-31 MJW python3 port. Version 2.x.x


import argparse
from datetime import datetime
import glob
import os
import re
import shutil
import socket
import subprocess
import sys
import time

# This is where ottplib is installed
sys.path.append("/usr/local/lib/python3.6/site-packages")  # Ubuntu 18.04
sys.path.append("/usr/local/lib/python3.8/site-packages")  # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22.04
import ottplib

VERSION = "2.0.0"
AUTHORS = "Michael Wouters"

debug = False

# ------------------------------------------
def ShowVersion():
	print (os.path.basename(sys.argv[0])+" "+VERSION)
	print ('Written by ' + AUTHORS)
	return

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
	reqd = [':targets'] # note empty main ...
	for k in reqd:
		if (not k in cfg):
			ErrorExit('The required configuration entry "' + k + '" is undefined')
		
	return cfg

# ------------------------------------------
# Main body 
# ------------------------------------------

home = os.environ['HOME'] 
root = home 
configFile = os.path.join(root,'etc','kickstart.conf')
logPath = os.path.join(root,'logs')
logFile = os.path.join(logPath,'kickstart.log')
# This is for V1 of OTTP platform ...
checkPath = os.path.join(root,'lockStatusCheck')
if not(os.path.isdir(checkPath)):
	checkPath = logPath
appName = os.path.basename(sys.argv[0])
hostName = socket.gethostname()
examples=''

if ottplib.LibMinorVersion() < 1: # a bit redundant since this will fail anyway on older versions of ottplib ...
	print('ottplib major version < 1')
	sys.exit(1)

parser = argparse.ArgumentParser(description='Start/restart user processes',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

debug = args.debug

if (args.version):
	ShowVersion()
	exit()

configFile = args.config;

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + ' not found')
	
cfg=Initialise(configFile)

targets = cfg[':targets'].split(',')
targets = [t.strip() for t in targets] # trim that whitespace

for t in targets:
	target = cfg[ t + ':target']
	lockFile = ottplib.MakeAbsoluteFilePath(cfg[t + ':lock file'],root,logPath)
	#  Fiddle with the path to the executable, if this is not absolute
	#  Since there can be command line arguments we need to extract the first part of the command
	cmdArgs = cfg[t + ':command'].split()
	if len(cmdArgs) > 1:
		cmd = ottplib.MakeAbsoluteFilePath(cmdArgs[0],root,os.path.join(root,'bin')) 
		for j in range(1,len(cmdArgs)):
			cmd.append(cmdArgs[j])
	else:
		cmd = ottplib.MakeAbsoluteFilePath(cfg[t + ':command'],root,os.path.join(root,'bin'))
	
	Debug('Testing ' + target + ' for ' + lockFile)
	running = not(ottplib.TestProcessLock(lockFile)) # reverse logic to Perl function!
	checkFile = os.path.join(checkPath,'kickstart.' + t + '.check')
	if running:
		Debug('Process is running')
		fout = open(checkFile,'w') # easiest way to update the modification time
		fout.close()
	else:
		Debug('Process is not running')
		
		targetOutputFile = os.path.join(logPath,target + '.log')
		
		# The nitty gritty
		try:
			x = subprocess.Popen('nohup ' + cmd + ' >>' + targetOutputFile + ' 2>&1 &',shell=True) # this is how you can start a background process
			Debug('Restarted using ' + cmd);
			msg = '{} {} restarted'.format(datetime.utcnow().strftime('%Y/%m/%d %H:%M:%S'),t)
			if os.path.isfile(checkFile):
				mtime = int(os.stat(checkFile).st_mtime) # chop off fractional bit
				msg += ' (last OK check {})'.format(datetime.fromtimestamp(mtime))
			msg += '\n'
		except Exception as e:
			msg = '{} {} restart failed'.format(datetime.utcnow().strftime('%Y/%m/%d %H:%M:%S'),t)
			print(e)
		
		Debug(msg)
		
		try:
			fout = open(logFile,'a')
			fout.write(msg)
			fout.close()
		except Exception as e:
			print(e) # not fatal 
		
		try:
			fout = open(targetOutputFile,'a')
			fout.write(msg)
			fout.close()
		except Exception as e:
			print(e) # similarly, not fatal.  
			
