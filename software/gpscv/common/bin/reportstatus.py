#!/usr/bin/python3
#

#
# The MIT License (MIT)
#
# Copyright (c) 2025 Michael J. Wouters
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
sys.path.append("/usr/local/lib/python3.12/site-packages") # Ubuntu 24.04

try: 
	import ottplib as ottp
except ImportError:
	sys.exit('ERROR: Must install ottplib\n eg openttp/software/system/installsys.py -i ottplib')

VERSION = "0.1.0"
AUTHORS = "Michael Wouters"

# -------------------------------------------------------
home = os.environ['HOME'] 
root = home 
configFile = os.path.join(root,'etc','gpscv.conf')
cggttsPath = os.path.join(root,'cggtts')

parser = argparse.ArgumentParser(description='')

examples =  'Usage examples\n'
examples += ''

parser = argparse.ArgumentParser(description='Basic status report on time-transfer system',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug
ottp.SetDebugging(debug)

configFile = args.config
	
if (not os.path.isfile(configFile)):
	ottp.ErrorExit(configFile + ' not found')
	
cfg=ottp.Initialise(configFile,[])

commands= [
 ['hostname','-f'],
 ['date'],
 ['df'],
 ['uptime'],
 ['find', cggttsPath, '-mtime', '-700', '-printf' , '%Ab %Ad %AH:%AM %s\t%p\n','|','sort','|','grep','cctf'],
 ['ps','x','|', 'grep','-E','plrxlog|ubloxlog','|','grep','-v','grep'],
 ['chronyc','sources']]

for cmd in commands:
	print('# ' + ' '.join(cmd))
	
	# The easy case - no pipes
	if not '|' in cmd:
		
		try:
			x = subprocess.check_output(cmd)
			print(x.decode('utf-8'))
		except Exception as e:
			ottp.ErrorExit('--- failed to run')

		continue
		
	# Handle pipes
	
	pos = cmd.index('|')
	
	procs = []
	procs.append(subprocess.Popen(cmd[:pos],  stdout=subprocess.PIPE))
	# Now we loop, adding processes
	while pos:
		prevPos = pos
		try:
			pos = cmd.index('|',pos+1)
		except:
			procs.append(subprocess.Popen(cmd[prevPos+1:], stdin =procs[len(procs)-1].stdout, stdout = subprocess.PIPE)) # last subprocess
			break
		# More to do
		# print(cmd[prevPos+1:pos])
		procs.append(subprocess.Popen(cmd[prevPos+1:pos], stdin =procs[len(procs)-1].stdout, stdout = subprocess.PIPE)) # next subprocess
		
	# Finally, get the output 
	output, errors = procs[len(procs) -1].communicate()
	
	print(output.decode())
	
