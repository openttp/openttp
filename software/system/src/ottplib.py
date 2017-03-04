#
# The MIT License (MIT)
#
# Copyright (c) 2015  R. Bruce Warrington, Michael J. Wouters
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
# Make a path relative to root, if needed
# If a path starts with a '/' then it is not modified
#
def MakeAbsolutePath(path,root):
	if (re.search(r'^/',path) == None):
		path = os.path.join(root,path)
	if (re.search(r'/$',path) == None): # add trailing '/' if needed
		path = path + '/'
	return path

# ------------------------------------------
# Make a file path relative to home, if needed
#
def MakeAbsoluteFilePath(fname,home,defaultPath):
	ret = fname
	
	if (re.search(r'/$',defaultPath) == None): # add trailing '/' if needed
		defaultPath = defaultPath + '/'
		
	if (re.search(r'^/',fname)):
		# absolute path already - nothing to do
		return ret
	elif (re.search(r"\w+/",fname)): # relative to $HOME
		ret = home + fname
	else:
		ret = defaultPath + fname
	return ret

# ------------------------------------------
# Make a lock file for a process
#
def CreateProcessLock(lockFile):
	if (os.path.isfile(lockFile)):
		flock=open(lockFile,'r')
		info = flock.readline().split()
		flock.close()
		if (len(info)==2):
			if (os.path.isfile('/proc/'+str(info[1]))):
				#ErrorExit('Process ' + str(info[1]) + ' is running');
				return False
			else:
				flock=open(lockFile,'w')
				flock.write(os.path.basename(sys.argv[0]) + ' ' + str(os.getpid()))
				flock.close()
		else:
			print 'Bad lock file'
			return False
	else:
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
		
	comment_re = re.compile('^\s*#')
	trailing_comment_re = re.compile('#.*$')
	section_re = re.compile('^\s*\[(.+)\]')
	keyval_re = re.compile('\s*=\s*')	
	
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