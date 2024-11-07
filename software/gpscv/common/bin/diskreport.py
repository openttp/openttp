#!/usr/bin/python3
# diskreport.py

# Use smartmontools to check status of a disk drive
#
# -----------------------------------------------------------------------------
# Ver: 0.1.0
# Author: Louis Marais
# Start: 2024-11-01
# Last: 2024-??-??
#
# -----------------------------------------------------------------------------
# Ver: {Next}
# Author:
# Start:
# Last:
#
# Modifications:
# --------------
#
# -----------------------------------------------------------------------------

import time
import subprocess
import os
import sys
import argparse
import configparser
import signal

script = os.path.basename(__file__)
VERSION = "0.1.0"
AUTHORS = "Louis Marais"

running = True
DEBUG = False
runonce = True  # Flag to allow continuous running if user desires.
                # Note that this is not implemented yet.

# -----------------------------------------------------------------------------
# Subroutines
# -----------------------------------------------------------------------------
def ts():
	now = time.gmtime()
	tsStr = time.strftime('%Y-%m-%d %H:%M:%S ')
	return(tsStr)

# -----------------------------------------------------------------------------
def mjdts():
	mjd = (time.time()/86400) + 40587
	return(mjd)

# -----------------------------------------------------------------------------
def debug(msg):
	if DEBUG:
		print(ts(),msg)
	return

# -----------------------------------------------------------------------------
def errorExit(s):
	print('ERROR: '+s)
	sys.exit(1)

# -----------------------------------------------------------------------------
def makePath(s):
	if not s.startswith('/'):
		s = HOME + s
	if not s.endswith('/'):
		s = s + '/'
	return(s)

# -----------------------------------------------------------------------------
def checkPath(p):
	if not os.path.isdir(p):
		errorExit('The path '+p+' does not exist')
	return

# -----------------------------------------------------------------------------
def signalHandler(signal,frame):
	global running
	running = False
	return

# -----------------------------------------------------------------------------
def TestProcessLock(lockFile):
	if (os.path.isfile(lockFile)):
		flock=open(lockFile,'r')
		info = flock.readline().split()
		flock.close()
		if (len(info)==2):
			if (os.path.exists('/proc/'+str(info[1]))):
				return False
	return True

# -----------------------------------------------------------------------------
def CreateProcessLock(lockFile):
	if (not TestProcessLock(lockFile)):
		return False;
	flock=open(lockFile,'w')
	flock.write(os.path.basename(sys.argv[0]) + ' ' + str(os.getpid()))
	flock.close()
	return True

# -----------------------------------------------------------------------------
def RemoveProcessLock(lockFile):
	if (os.path.isfile(lockFile)):
		os.unlink(lockFile)
	return

# -----------------------------------------------------------------------------
def checkConfig(cfg, req):
	cnt = []
	for section in cfg.sections():
		s = section.lower()
		for key in cfg[section]:
			cnt.append(s+','+key.lower())
	for s in req:
		if not s in cnt:
			errorExit('Required key ({}) not in configuration file.'.format(s))
	debug("All required section:key pairs found in configuration file.")
	return(cnt)

# -----------------------------------------------------------------------------
def getDiskInfo(dev,tp):
	# tp == 0 means return Model number, Serial number, and Total NVM capacity
	# tp != 0 means return everything else
	
	cmd = ['smartctl','-a',dev]
	
	s = subprocess.run(cmd,capture_output = True)
	
	r = s.stdout.decode('ascii').split('\n')
	
	t = []
	for i in range(1,4):
		t.append('')
	
	v = [f"{mjdts():0.6f}"]
	for i in range(1,8):
		v.append('')
	
	for i in range(0,len(r)):
		#if r[i].find("Local Time is:") >= 0:
		#	v[0] = r[i][14:].strip()
		if r[i].find("Model Number:") >= 0:
			t[0] = r[i][13:].strip()
		if r[i].find("Serial Number:") >= 0:
			t[1] = r[i][14:].strip()
		if r[i].find("Total NVM Capacity:") >= 0:
			t[2] = r[i][19:].strip()
		if r[i].find("Critical Warning:") >= 0:
			v[1] = r[i][17:].strip()
		if r[i].find("Temperature:") >= 0:
			v[2] = r[i][12:].strip()
		if r[i].find("Data Units Read:") >= 0:
			v[3] = r[i][16:].strip()
		if r[i].find("Data Units Written:") >= 0:
			v[4] = r[i][19:].strip()
		if r[i].find("Power On Hours:") >= 0:
			v[5] = r[i][15:].strip()
		if r[i].find("Unsafe Shutdowns:") >= 0:
			v[6] = r[i][17:].strip()
		if r[i].find("Error Information (") >= 0:
			v[7] = r[i+1].strip()
	
	statusStr = ''
	
	if tp == 0:
		for i in range(0,len(t)):
			statusStr += '"'+t[i]+'"'
			if i < (len(t) - 1):
				statusStr += ','
	else:
		for i in range(0,len(v)):
			statusStr += '"'+v[i]+'"'
			if i < (len(v) - 1):
				statusStr += ','
	
	return(statusStr)

# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------

# Check if root is running the show, if not, tell user and exit
if not os.getuid() == 0:
	errorExit("This script must be run as root.")

HOME = os.path.expanduser('~')
if not(HOME.endswith('/')):
	HOME += '/'

parser = argparse.ArgumentParser(description="Logs critical NVMe SSD data. "+
																 "Runs as the root user, and should be "+
																 "installed as a system level cron job. "+
																 "Can be run as often as user requires.")
parser.add_argument("-v","--version",action="store_true",help="Show version "+
										"and exit.")
parser.add_argument("-c","--config",nargs=1,help="Specify alternative "+
										"configuration file. The default is "+
										f"{HOME}etc/diskreport.conf.")
parser.add_argument("-d","--debug",action="store_true",help="Turn debugging on")

args = parser.parse_args()

if args.debug:
	DEBUG = True

versionStr = script+" version "+VERSION+" written by "+AUTHORS

if args.version:
	print(versionStr)
	sys.exit(0)

debug(versionStr)

debug("Current user's home: "+HOME)

configfile = HOME+"etc/diskreport.conf"

if args.config:
	debug("Alternate config file specified: "+str(args.config[0]))
	configfile = str(args.config[0])
	if not configfile.startswith('/'):
		configfile = HOME+configfile

debug("Configuration file: "+configfile)

if not os.path.isfile(configfile):
	errorExit(configfile+' does not exist.')

conf = configparser.ConfigParser()
conf.read(configfile)

req = ['disk,device','path,data','path,file extension','path,lock file',
			 'user,name','check,runonce']

cfg = checkConfig(conf, req)

"""
fileheader = ('"Local time","Model number","Serial number",'+
						 '"Total NVM capacity","Critical warning","Temperature",'+
						 '"Data units read","Data units written","Power on hours",'+
						 '"Unsafe shutdowns","Error message"')
"""

fileheader = ('"Local time","Critical warning","Temperature",'+
						 '"Data units read","Data units written","Power on hours",'+
						 '"Unsafe shutdowns","Error message"')


dev = conf['disk']['device']

debug(f"Device to check: {dev}")

user = conf['user']['name']

debug(f"Owner of data files: {user}")

datapath = makePath(conf['path']['data'])
checkPath(datapath)
debug('Data will be saved to: {}'.format(datapath))

ext = conf['path']['file extension']

debug(f"Data file extension: {ext}")
if not ext.startswith('.'):
	ext = '.'+ext

lockfile = conf['path']['lock file']
if not lockfile.startswith('/'):
	lockfile = HOME+lockfile
if not CreateProcessLock(lockfile):
	errorExit('Unable to lock - '+script+' already running?')

debug(f"Lock file: {lockfile}")

if not conf['check']['runonce'].upper() == 'TRUE':
	runonce = False

debug(f"Runonce = {runonce}")

signal.signal(signal.SIGINT,signalHandler)
signal.signal(signal.SIGTERM,signalHandler)
signal.signal(signal.SIGHUP,signalHandler) # not usually run with a controlling TTY, but handle it anyway

oldmjd = -1
flnm = ""
# check every hour
checkinterval = 60 # minutes
if conf.has_option('check','interval'):
	try:
		dummy = int(conf['check']['interval'])
		if (dummy >= 1) and (dummy < 1440):
			checkinterval = dummy
	except:
		pass
nextcheck = int(time.time()/(checkinterval*60))*(checkinterval*60)
if not runonce:
	debug(f"Check disk every {checkinterval} minutes.")
else:
	debug("The check will be done once only.")

while running:
	mjd = int(mjdts())
	if mjd != oldmjd:
		flnm = datapath+str(mjd)+ext
		fileis = os.path.isfile(flnm)
		with open(flnm,'a') as f:
			if fileis:
				if not runonce:
					f.write(f"# Continuing {flnm}\n")
				debug(f"Continuing data file: {flnm}")
			else:
				if not runonce:
					f.write(f"# Starting {flnm}\n")
				debug(f"New data file: {flnm}")
				diskstatus = getDiskInfo(dev,0)
				f.write('#"Model number","Serial number","Total NVM capacity"\n')
				f.write(f"{diskstatus}\n")
				f.write(f"#{fileheader}\n")
			f.close()
		retv = subprocess.run(['chown',user+':',flnm],capture_output=True)
		if not retv.returncode == 0:
			# Acknowledge error but carry on regardless
			debug(f"Could not change ownership of {flnm}")
		oldmjd = mjd
	if time.time() >= nextcheck:
		diskstatus = getDiskInfo(dev,1)
		with open(flnm,'a') as f:
			f.write(f"{diskstatus}\n")
			f.close()
		debug("Status written to disk")
		nextcheck += (checkinterval*60)
	time.sleep(0.1)
	if runonce:
		running = False # Run one time only
		debug("Ran only once.")

RemoveProcessLock(lockfile)

print(ts(),script,'terminated.')

