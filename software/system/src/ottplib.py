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

import os
import re
import sys
import time
 
LIBVERSION = '0.0.3'

# ------------------------------------------
# Return the library version
#
def LibVersion():
	return LIBVERSION

# ------------------------------------------
# Read a text file to set up a dictionary for configuration information
# Options supported are:
# tolower=>[True,False] coonvert keys to lower case (but not values)
# defaults=>'filename'  load default configuration first and then update
#
def LoadConfig(fname,options={}):
	cfg={}
	if (options.get('defaults') != None):
		cfg=_LoadConfig(options.get('defaults'),options)
	custcfg=_LoadConfig(fname,options)
	if (len(cfg)>0):
		cfg.update(custcfg)
	else:
		cfg=custcfg
	return cfg

# ------------------------------------------
# Convert Unix time to (truncated) MJD
#
def MJD(unixtime):
	return int(unixtime/86400) + 40587

# ------------------------------------------
# Make a path relative to root (which must be absolute), if needed
# If a path starts with a '/' then it is not modified
# Adds a trailing separator
def MakeAbsolutePath(path,root):
	if (not os.path.isabs(path)):
		path = os.path.join(root,path) 
	path = os.path.join(path,'') # add trailing separator if necessary
	return path

# ------------------------------------------
# Make a file path relative to home, if needed
#
def MakeAbsoluteFilePath(fname,rootPath,defaultPath):
	
	ret = fname
	
	defaultPath = os.path.join(defaultPath,'') # add separator if necessary
	rootPath = os.path.join(rootPath,'')	
	sep=os.sep
	
	if (os.path.isabs(fname)): # absolute path already - nothing to do
		return ret
	elif (re.search(r'^\w+'+sep,fname)): # relative to rootPath
		ret = rootPath + fname
	else:
		ret = defaultPath + fname
	return ret

# ------------------------------------------
# Make a lock file for a process
#
def CreateProcessLock(lockFile):
	if (not TestProcessLock(lockFile)):
		return False;
	flock=open(lockFile,'w')
	flock.write(os.path.basename(sys.argv[0]) + ' ' + str(os.getpid()))
	flock.close()
	
	return True

# ------------------------------------------
# Remove a lock file for a process
#
def RemoveProcessLock(lockFile):
	if (os.path.isfile(lockFile)):
		os.unlink(lockFile)

# ------------------------------------------
# Test whether a lock can be obtained
#
def TestProcessLock(lockFile):
	if (os.path.isfile(lockFile)):
		
		# Check the system boot time
		# If the lock file was created before the reboot, it's stale (and the
		# system did not shut down cleanly) and it should be ignored. 
		# This could fail if the system time was/is unsynchronized.
		
		fup = open('/proc/uptime','r')
		upt = float(fup.readline().split()[0])
		fup.close();
		
		tboot = time.time() - upt;
		if os.path.getmtime(lockFile) < tboot:
			return True;
		
		flock=open(lockFile,'r')
		info = flock.readline().split()
		flock.close()
		if (len(info)==2):
			procDir = '/proc/'+str(info[1])
			if (os.path.exists(procDir)):
				fcmd = open(procDir + '/cmdline')
				cmdline = fcmd.readline()
				fcmd.close()
				if os.path.basename(sys.argv[0]) in cmdline:
					return False
				# otherwise, it's an unrelated process
	return True
		
# ------------------------------------------
# Internals - use these at your own peril
# ------------------------------------------

# ------------------------------------------
# This is the the guts of LoadConfig
#
def _LoadConfig(fname,options={}):

	cfg={};
	
	try:
		f = open(fname,'r')
	except:
		print('Unable to open '+fname)
		return cfg
		
	comment_re = re.compile(r'^\s*#')
	trailing_comment_re = re.compile(r'#.*$')
	section_re = re.compile(r'^\s*\[(.+)\]')
	keyval_re = re.compile(r'\s*=\s*')	
	
	section = ''
	value = ''
	key = ''
	
	tolower = options.get('tolower',False); 
	
	for line in f:
		
		if (comment_re.search(line)): # skip comments
			continue
		line = trailing_comment_re.sub('',line)
		line = line.strip() # remove leading and trailing spaces
		
		if (len(line) == 0): # nothing left - a blank line
			continue
		
		m = section_re.search(line)
		if (m): # new section
			if (len(value)>0): # found new section so save any the last dictionary key, if it exists
				cfg[key] = value
				key = ''
				value = ''
			section=m.group(1)
			if (tolower):
				section=section.lower()
		elif (None != keyval_re.search(line)): # found new key->value pair
			if (len(value)>0): # save the last key->value pair
				cfg[key]=value
				key = ''
				value = ''
			key_val =line.split('=') # FIXME a regex would be better
			if (len(key_val) == 2): # looks like a key->value pair
				key_val[0]=" ".join(key_val[0].split()) # remove any superfluous whitespace
				key_val[1]=key_val[1].strip()
				if (tolower):
					key_val[0]=key_val[0].lower()
				if (len(key_val[1]) > 0):
					key = section + ":" + key_val[0]
				value = key_val[1]
		else: # could be a multi-line value 
			if (len(value) > 0): 
				value = value + line
		
	if (len(value) > 0):
		cfg[key] = value
		
	f.close()
	
	return cfg
