#!/usr/bin/python3
# pi5ttsgpio.py

# An application to manage the Raspberry Pi 5 GPIOs used for the TTS v6. Each
# GPIO has an associated file that holds the current value of that GPIO. Note
# that it does not hold transitional values, only steady state (0 or 1). For 
# the functionality of the Time Transfer System this is adequate.
#
# GPIOs that are configured as inputs have the current state of that input
# saved in the file. For GPIOs configured as outputs the file is monitored and
# the GPIO value is changed when the file modification time changes.
#
# The script can only run on a Raspberry Pi (tested on the model 5 only).
#
# The script must be run as the root user, ideally as a service.
#

#
# The MIT License (MIT)
#
# Copyright (c) 2025 E. Louis Marais
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

# ------------------------------------------------------------------------#
#                                                                         #
#      This requires the Raspberry Pi GPIO library                        #
#      For Pi5: sudo apt install python3-rpi-lgpio                        #
#      The 'python3-rpi.gpio' does not work on the Pi5 architecture       #
#                                                                         #
# ------------------------------------------------------------------------#

# -----------------------------------------------------------------------------
# GPIOs managed by this script with descriptions 
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
#              Input
# Header        or
#  pin   GPIO Output    Signal                Description
# ~~~~~~ ~~~~ ~~~~~~ ~~~~~~~~~~~~~~~~   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#   11    17    IN   GPSDO LOCK         Goes high when GPSDO is locked 
#   12    18   OUT   GPSDO RESET        Set to high to reset the GPSDO
#   15    22   OUT   MOS T RESET        Set to low to reset the Mosaic T
#   16    23    IN   REF CLK SIG2 MON   The two MON signals combined provide 
#   18    24    IN   EX CLK SIG1 MON    information on the clock in use.
#   22    25   OUT   GPSDO PWR ENABLE   Set to high to turn GPSDO power on 
#   13    27    IN   MOS T READY        Goes high when the Mosaic T is ready
#
# Deciphering the MON signals:
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#   EX CLK SIG1 MON  |  REF CLK SIG2 MON |  Description of indicated status
#  ------------------+-------------------+---------------------------------
#          0         |         0         |  Moscaic T internal clock in use *
#          0         |         1         |  GPSDO (REF) clock in use
#          1         |         0         |  UNDEFINED STATE: should never occur
#          1         |         1         |  External clock in use
#
#  * MOS T READY must be high too!
#
# Other GPIOs in use (excluding those for serial, SPI or I2C) are GPIO 19 and 
# GPIO26 for PPS signals (defined in /boot/firmware/config.txt) and GPIO20 
# that carries the RPI SHUTDOWN signal (also defined in
# /boot/firmware/config.txt).
#
# The script is designed to be configured for use with any GPIO or set of
# GPIOs. The ones in the table above are those used in the TTS v6 project.
# I suggest you change the name of the script if you do decide to manage other
# GPIOs with it.
#
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
#
# Authors: Louis Marais
# Version: 0.1
# Start date: 2025-06-25
# Last modifications: 2025-07-08
#
# Initial version
#
# -----------------------------------------------------------------------------
#
# Authors:
# Version: {Next}
# Start date:
# Last modifications:
#
# Modifications:
# ~~~~~~~~~~~~~~
#
# -----------------------------------------------------------------------------

import time
import subprocess
import os
import sys
import argparse
import configparser
import signal
import pwd

try:
	import RPi.GPIO as GPIO
except ImportError:
	sys.exit("Missing module 'RPi'!\nThis will only run on a Raspberry Pi with "+
					"the RPi gpio module installed. Use: sudo apt install "+
					"python3-rpi-lgpio")

script = os.path.basename(__file__)
VERSION = "0.1"
AUTHORS = "Louis Marais"

RPi_gpios = [i for i in range(2,28)] # 26 accessible GPIOs.
                                     # (GPIO 0 and 1 are reserved for HATs.)

DEBUG = False

# -----------------------------------------------------------------------------
# Subroutines
# -----------------------------------------------------------------------------
def ts():
	now = time.gmtime()
	tsStr = time.strftime('%Y-%m-%d %H:%M:%S ')
	return(tsStr)

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
def makePath(hm,s):
	if not s.startswith('/'):
		s = hm + s
	if not s.endswith('/'):
		s = s + '/'
	return(s)

# -----------------------------------------------------------------------------
def makeFilePath(hm,s):
	if not s.startswith('/'):
		s = hm + s
	return(s)

# -----------------------------------------------------------------------------
def checkPath(p):
	if not os.path.isdir(p):
		errorExit(f'checkPath: The path {p} does not exist')
	return(True)

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
	debug("checkConfig: All required section:key pairs found in configuration "+
			 "file.")
	return(cnt)

# -----------------------------------------------------------------------------
def checkIO(s,gp):
	s = s.upper()
	if not s in ['INPUT','OUTPUT']:
		errorExit(f"Invalid value for 'io' key in GPIO{gp} configuration.")
	v = 'IN'
	if s == 'OUTPUT':
		v = 'OUT'
	return(v)

# -----------------------------------------------------------------------------
# Is this active high?  [1 | True | Yes] == True, [0 | False | No] == False
def checkHL(s,gp):
	s = s.lower()
	if not s in ['1','true','yes','0','false','no']:
		errorExit(f"checkHL: Invalid value for 'active high' key in GPIO{gp} "+
						"configuration.")
	v = True
	if s in ['0','false','no']:
		v = False
	return(v)

# -----------------------------------------------------------------------------
def isinteger(s):
	try:
		int(s)
	except ValueError:
		return False
	else:
		return True

# -----------------------------------------------------------------------------
def checkIV(s,gp):
	s = s.lower()
	if not s in ['0','1','low','high']:
		errorExit(f"checkIV: Invalid value for 'active high' key in GPIO{gp} "+
						"configuration.")
	d = 0
	if s in ['1','high']:
		d = 1
	return(d)

# -----------------------------------------------------------------------------
def checkGPIOconfig(hm,cnf,req):
	gp = [g.strip() for g in conf['main']['gpios'].split(',')]
	ret = []
	for i in range(0,len(gp)):
		s = []
		for j in range(0,len(req)):
			s.append(f"{gp[i].lower()},{req[j]}")
		debug(f"checkGPIOconfig: Checking configuration for [{gp[i]}]")
		cfgi = checkConfig(cnf,s)
		ret.append([])
		s = cnf[gp[i]]['gpio']
		if isinteger(s):
			d = int(s)
			if d in RPi_gpios:
				ret[-1].append(d)
			else:
				errorExit(f"checkGPIOconfig: The configured value for 'gpio' ({d}) "+
							f"in [{gp[i]}] is not a valid Raspberry Pi GPIO.")
		else:
			errorExit(f"checkGPIOconfig: The configured 'gpio' value for [{gp[i]}] "+
						 f"is not an integer: '{s}'")
		ret[-1].append(cnf[gp[i]]['description'])
		io = checkIO(cnf[gp[i]]['io'],ret[-1][0])
		ret[-1].append(io)
		ah = checkHL(cnf[gp[i]]['active high'],ret[-1][0])
		ret[-1].append(ah)
		fl = makeFilePath(hm,cnf[gp[i]]['file'])
		pth = os.path.split(fl)[0]
		if checkPath(os.path.split(fl)[0]):
			ret[-1].append(fl)
		else:
			errorExit("checkGPIOconfig: The path for the configured 'file' value "+
						 f"of [{gp[i]}] ({fl}) is not valid. Make sure the path exists.")
		init_val = 0
		if cnf.has_option(gp[i],'initial value'): # most likely only outputs
			init_val = checkIV(cnf[gp[i]]['initial value'],ret[-1][0])
		ret[-1].append(init_val)
	return(ret) 

# -----------------------------------------------------------------------------
def getuser(d):
	cmd = ["id","-nu",f"{d}"]
	retval = subprocess.run(cmd,capture_output=True)
	u = retval.stdout.decode('ascii').strip()
	return(u)

# -----------------------------------------------------------------------------
def gethome(u):
	success = False
	hm = pwd.getpwnam(u).pw_dir
	if not hm.endswith('/'):
		hm += '/'
	if os.path.isdir(hm):
		success = True
	return(success,hm)

# -----------------------------------------------------------------------------
def changeOwnerAttributes(flnm,user):
	# everyone can read, everyone can write
	os.chmod(flnm,0o666) # Sets file mode to: -rw-rw-rw-
	# get user id and group id
	udets = pwd.getpwnam(user)
	uid = udets.pw_uid
	gid = udets.pw_gid
	# change owner if necessary
	current_owner = pwd.getpwuid(os.stat(flnm).st_uid).pw_name
	if not current_owner == user:
		os.chown(flnm, uid, gid)
		new_owner = pwd.getpwuid(os.stat(flnm).st_uid).pw_name
		debug(f"changeOwnerAttributes: {flnm} was owned by {current_owner} and "+
				  f"is now owned by {new_owner}")
	return

# -----------------------------------------------------------------------------
def logChange(flnm,ioStr,gpio,v,user,desc):
	if not os.path.isfile(flnm):
		with open(flnm,'w') as f:
			f.write("# Log of changes made to configured GPIOs.\n")
			f.close()
		changeOwnerAttributes(flnm,user)
	s = f'{ts()} {ioStr:7s} GPIO{gpio:<2d} = {v}  "{desc}"\n'
	with open(flnm,'a') as f:
		f.write(s)
		f.close()
	debug(f"logChange: Change to GPIO{gpio} logged in {flnm}")
	return

# -----------------------------------------------------------------------------
def writeGPIO(gpio,flnm,logflnm,user,desc):
	value = 0
	with open(flnm,'r') as f:
		lines = f.readlines()
		f.close()
	s = lines[0].strip()
	if not s in ['0','1']:
		debug(f"writeGPIO: Invalid value in {flnm} for GPIO{gpio}: {s}")
		return(-1)
	value = int(lines[0])
	GPIO.output(gpio,value)
	debug(f"writeGPIO: Wrote {value} to GPIO{gpio}")
	logChange(logflnm,'OUTPUT',gpio,value,user,desc)
	return(value)

# -----------------------------------------------------------------------------
def readGPIO(gpio,flnm, oldval,logflnm,user,desc):
	value = GPIO.input(gpio)
	if value != oldval:
		debug(f"readGPIO: Read GPIO{gpio}: {value}")
		with open(flnm,'w') as f:
			f.write(f"{value:d}\n")
			f.close()
		debug(f"readGPIO: Wrote new value to {flnm}")
		logChange(logflnm,'INPUT',gpio,value,user,desc)
	return(value)

# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------

uid = os.getuid()
if not uid == 0:
	errorExit("This script must be run by the 'root' user.")

parser = argparse.ArgumentParser(description="Manages Pi 5 GPIOs for TTS v6")
parser.add_argument("-v","--version",action="store_true",help="Show version "+
										"and exit.")
parser.add_argument("-c","--config",nargs=1,help="Specify alternative "+
										"configuration file. The default is "+
										"~/etc/pi5ttsgpio.conf.")
parser.add_argument("-d","--debug",action="store_true",help="Turn debugging "+
										"on")

args = parser.parse_args()

if args.debug:
	DEBUG = True

versionStr = script+" version "+VERSION+" written by "+AUTHORS

if args.version:
	print(versionStr)
	sys.exit(0)

debug(versionStr)

HOME = os.path.expanduser('~')
if not(HOME.endswith('/')):
	HOME += '/'

debug("Current user's home: "+HOME)

configfile = HOME+"etc/pi5ttsgpio.conf"

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

req = ['main,gpios','main,user','main,lock file']

cfg = checkConfig(conf, req)

configured_user = conf['main']['user']
debug(f"Configured user: {configured_user}")

debug(f"Current user's id: {uid}")

current_user = getuser(uid)
debug(f"Current user: {current_user}")

if not configured_user == current_user:
	debug("Not running as configured user")
	r = gethome(configured_user)
	if r[0]:
		debug(f"Found the home directory for '{configured_user}': {r[1]}")
		HOME = r[1]
	else:
		debug(f"Could not find user '{configured_user}'. Home directory remains "+
				"as is.")
else:
	debug("Running as configured user")

log_changes = False
logfile = ""
if conf.has_option('main','log file'):
	logfile = makeFilePath(HOME,conf['main']['log file'])
	checkPath(os.path.split(logfile)[0])
	log_changes = True
	debug(f"GPIO changes will be written to {logfile}")
else:
	debug("GPIO changes will not be logged.")

# Read GPIO definitions from configuration file
gpio_req = ['description','gpio','io','active high','file']
gpios = checkGPIOconfig(HOME,conf,gpio_req)


# Set up the GPIO
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)

debug("GPIOs set to BCM mode")

# Check each GPIO file and store the value. Create the file if it does not 
# exist, and put an initial value in the file (from the configuration). If
# the value is an output, write it. If it is an input, read the value.

file_times = []
old_values = []
for i in range(0,len(gpios)):
	if not os.path.isfile(gpios[i][4]):
		with open(gpios[i][4],'w') as f:
			f.write(f"{gpios[i][5]:d}\n")
			f.close()
		debug(f"Created file for GPIO{gpios[i][0]}: {gpios[i][4]}")
		changeOwnerAttributes(gpios[i][4],configured_user)
	if gpios[i][2] == 'OUT':
		GPIO.setup(gpios[i][0],GPIO.OUT)
		v = writeGPIO(gpios[i][0],gpios[i][4],logfile,configured_user,gpios[i][1])
		debug(f"Written {v} to GPIO{gpios[i][0]}")
	else:  # Inputs
		GPIO.setup(gpios[i][0],GPIO.IN)
		# Set old value = -1 to force file write
		v = readGPIO(gpios[i][0],gpios[i][4],-1,logfile,configured_user,
							 gpios[i][1]) 
		debug(f"Read {v} from GPIO{gpios[i][0]}")
	old_values.append(v)
	file_times.append(os.path.getmtime(gpios[i][4]))

# Loop - check INPUT GPIOs and OUTPUT GPIO files

lockfile = makeFilePath(HOME,conf['main']['lock file'])
if not CreateProcessLock(lockfile):
	errorExit(f'Unable to lock - {script} already running?')

debug(f"Lock file: {lockfile}")

signal.signal(signal.SIGINT,signalHandler)
signal.signal(signal.SIGTERM,signalHandler)
signal.signal(signal.SIGHUP,signalHandler) # not usually run with a controlling
                                           # TTY, but handle it anyway

running = True

debug("")
debug("Starting main loop.")
debug("")

cursec = int(time.time())
newsec = cursec
startsec = cursec

while running:
	# run checks every second
	if not newsec == cursec:
		for i in range(0,len(gpios)):
			# Check file times for outputs and write new value to output if there is
			if gpios[i][2] == 'OUT':                                      # a change
				ft = os.path.getmtime(gpios[i][4])
				if not ft == file_times[i]:
					old_values[i] = writeGPIO(gpios[i][0],gpios[i][4],logfile,
															 configured_user,gpios[i][1])
					file_times[i] = ft
			else: # Read inputs
				old_values[i] = readGPIO(gpios[i][0],gpios[i][4],old_values[i],logfile,
														 configured_user,gpios[i][1])
		cursec = newsec
		if DEBUG:
			print(f"Active for {cursec - startsec} seconds.               ",end='\r')
	time.sleep(0.1)
	newsec = int(time.time())

# NOTE: No cleanup (as in GPIO.cleanup()) because we want to preserve the GPIOs
#       on exit

RemoveProcessLock(lockfile)

debug(f"{script} terminated.")
