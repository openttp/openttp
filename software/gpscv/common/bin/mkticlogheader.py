#!/usr/bin/python3
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

# mkticlogheader - create the header for a TIC log file
#
# Modification history
# 2018-04-216 MJW First version
#

import argparse
import os
import re
import socket
import string
import subprocess
import sys
# This is where ottplib is installed
sys.path.append('/usr/local/lib/python3.6/site-packages')
sys.path.append('/usr/local/lib/python3.8/site-packages')
import ottplib

VERSION = "0.2.0"
AUTHORS = "Michael Wouters"

# ------------------------------------------
def ErrorExit(msg):
	print(msg)
	sys.exit(0)
	
# ------------------------------------------
def Initialise(configFile):
	cfg=ottplib.LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit("Error loading " + configFile)
		
	# Check for required arguments
	reqd = ['counter:logger','receiver:pps offset','delays:reference cable','delays:antenna cable']
	for k in reqd:
		if (not k in cfg):
			ErrorExit("The required configuration entry " + k + " is undefined")
		
	return cfg	
# ------------------------------------------
# Main 
# ------------------------------------------

home =os.environ['HOME'] + os.sep
configFile = os.path.join(home,'etc','gpscv.conf')
logger = os.path.join(home,'bin','ticclog.py')

parser = argparse.ArgumentParser(description='Output a header for a TIC log file',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

configFile = args.config;

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + " not found")
	
logPath = os.path.join(home,'logs')
if (not os.path.isdir(logPath)):
	ErrorExit(logPath + "not found")

cfg=Initialise(configFile)

dataPath= ottplib.MakeAbsolutePath(cfg['paths:counter data'], home)

logger = ottplib.MakeAbsoluteFilePath(cfg['counter:logger'],home,'/usr/local/bin')


if not(os.access(logger,os.X_OK)):
	ErrorExit(logger + ' is not runnable')

# The header is written to stdout

# First line is software version

swver=subprocess.check_output([logger,'-v'],stderr=subprocess.STDOUT)
logger = os.path.basename(logger) # strip path 
swver = swver.decode('utf-8').splitlines()
ver = ''
for l in swver:
	match = re.search(logger+'\s+(version)?\s*\d',l) # at least one digit for a match
	if match:
		ver = l
		break

print('# TIC log,',ver)

# Second line is the delay information
print('# Delays = [','CAB DLY =',cfg['delays:antenna cable'],', REF DLY = ',cfg['delays:reference cable'], \
			 ', pps offset = ',cfg['receiver:pps offset'],']')
