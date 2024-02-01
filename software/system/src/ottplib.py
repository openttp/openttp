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
import subprocess
import sys
import time
 
LIB_MAJOR_VERSION  = 0
LIB_MINOR_VERSION = 4
LIB_PATCH_VERSION = 0

debug=False

# ------------------------------------------
# Return the library version
#
def LibVersion():
	return '{:d}.{:d}.{:d}'.format(LIB_MAJOR_VERSION,LIB_MINOR_VERSION,LIB_PATCH_VERSION)

def LibMajorVersion():
	return LIB_MAJOR_VERSION

def LibMinorVersion():
	return LIB_MINOR_VERSION

def LibPatchVersion():
	return LIB_PATCH_VERSION

# ------------------------------------------
# Miscellaneous
# ------------------------------------------

def SetDebugging(dbgOn):
	global debug
	debug = dbgOn
	
def Debug(msg):
	global debug
	if (debug):
		sys.stderr.write(msg + '\n')
	return

# ------------------------------------------
def ErrorExit(msg):
	print (msg)
	sys.exit(0)
	
# ------------------------------------------
def DecompressFile(basename,ext):
	if (ext.lower() == '.gz' or ext.lower() == '.z'):
		subprocess.check_output(['gunzip',basename + ext])
		Debug('Decompressed ' + basename)

# ------------------------------------------
def RecompressFile(basename,ext):
	if (ext.lower() == '.gz'):
		subprocess.check_output(['gzip',basename])
		Debug('Recompressed ' + basename)
	elif (ext.lower() == '.z'):
		subprocess.check_output(['compress',basename])
		Debug('Recompressed ' + basename)

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
def Initialise(configFile,reqd):
	cfg = LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit("Error loading " + configFile)
		
	# Check for required arguments
	for k in reqd:
		if (not k in cfg):
			ErrorExit('The required configuration entry "' + k + '" is undefined')
		
	return cfg

# ------------------------------------------
# Convert Unix time to (truncated) MJD
#
def MJD(unixtime):
	return int(unixtime/86400) + 40587


# ---------------------------------------------
def MJDtoYYYYDOY(mjd):
	tt = (mjd - 40587)*86400
	utctod = time.gmtime(tt)
	return (utctod.tm_year,utctod.tm_yday,utctod.tm_mon)

# ---------------------------------------------
def MJDtoGPSWeekDay(mjd):
	ttGPS = (mjd - 40587)*86400 - 315964800 # ignores leap seconds but this won't cause problems for a while :-)
	GPSWn = int(ttGPS/(7*86400))
	GPSday = int((ttGPS - GPSWn*86400*7)/86400)
	return (GPSWn, GPSday)
	
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
	pid = os.getpid() # process ID
	stime = int(open("/proc/{:d}/stat".format(pid)).read().split()[21]) # process start time. in ticks
	flock.write(os.path.basename(sys.argv[0]) + ' ' + str(pid) + ' ' + str(stime))
	flock.close()
	
	return True

# ------------------------------------------
# Remove a lock file for a process
#
def RemoveProcessLock(lockFile):
	if (os.path.isfile(lockFile)):
		os.unlink(lockFile)

# ------------------------------------------
# Returns True if a lock can be obtained
# The principal test is a match between PID, process name and process start time

def TestProcessLock(lockFile):
	if (os.path.isfile(lockFile)):
			
		# The following test is retained for the moment but can be removed when all kickstart-compatible 
		# executables write stime in the lock (C,C++, Perl)
		
		# Check the system boot time
		# If the lock file was created before the reboot, it's stale (and the
		# system did not shut down cleanly) and should be ignored. 
		# This could fail if the system time is not yet set.
		
		tup = float(open('/proc/uptime').read().split()[0])
		mtime = float(os.stat(lockFile).st_mtime)
		if (mtime < time.time() - tup): # old and stinky
			return True
		
		flock=open(lockFile,'r')
		info = flock.readline().split()
		flock.close()
		# Deprecated format is:
		# executable PID
		# New format is:
		# executable PID stime
		# For the present, we'll maintain backwards compatibility with the deprecated format
		if len(info)>=2:
			pid = info[1]
			procDir = '/proc/' + pid
			if (os.path.exists(procDir)):
				fcmd = open(procDir + '/cmdline')
				# We don't try to parse the command line since it could be a script (with the interpreter as first argument)
				# or a compiled executable with arguments or ...
				cmdline = fcmd.readline()
				fcmd.close()
				if info[0] in cmdline: # so we just look for the script/executable name
					if len(info) == 3: # should have stime
						stime = open("/proc/{}/stat".format(pid)).read().split()[21] # starttime field, in clock ticks
						return not(stime == info[2])
					else:
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
