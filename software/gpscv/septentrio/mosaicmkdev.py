#!/usr/bin/python3

# Script to run in cionjuntion with udev to create uniques device names
# for the Septentrio 
# Helpful stuff
# Use
# udevadm info /sys/...
# to examine device attributes as described by udev

# To check for errors 
# systemctl status systemd-udevd

# Note that when testing, the flash device may take a while to appear so the device won't be available for a while after plugging in

# This is meant to be used top down ie hub is matched bu udev then symlinks are made
# The other way to do this is bottom up - udev matches eg ttyACM and then the script figures out the device order 
 
# This will be called when plugged and unplugged

import argparse
import pyudev # Ubuntu 20.04 python3-pyudev
import os
import re
import sys
import time

sys.path.append("/usr/local/lib/python3.8/site-packages")
import ottplib

# Globals
debug = False
log   = False

VERSION = '0.1.2'
AUTHORS = "Michael Wouters"

LOGFILE = '/var/log/mosaicmkdev.log'

MODE_ADD    = 0
MODE_REMOVE = 1

# ------------------------------------------
def Debug(msg):
	if (debug):
		print(msg)
	if (log):
		fout = open(LOGFILE,'a')
		fout.write(time.strftime('%Y-%m-%d %H:%M:%S ',time.gmtime()) + msg+'\n')
		fout.close()
	return

# ------------------------------------------
def ErrorExit(msg):
	sys.stderr.write(msg + '\n')
	if (log):
		fout = open(LOGFILE,'a')
		fout.write(time.strftime('%Y-%m-%d %H:%M:%S ',time.gmtime()) + msg+'\n')
		fout.close()
	sys.exit(1)

# ------------------------------------------
def Initialise(configFile):
	cfg=ottplib.LoadConfig(configFile,{'tolower':True})
	if (cfg == None):
		ErrorExit('Error loading ' + configFile)
	
	# Check for required arguments
	reqd = ['main:receivers']
	
	for k in reqd:
		if (not k in cfg):
			ErrorExit('The required configuration entry "' + k + '" is undefined')
		
	return cfg

# ------------------------------------------
def MakeSymLink(dev,alias):
	dev = '/dev/' + dev
	symlink = '/dev/' + alias
	Debug('Syminking ' + dev + ' --> ' + symlink)
	if (os.path.exists(symlink)):
		# we have to handle the case where udev has been triggered again after creating a valid device
		# we don't want to remove it
		Debug('Symlink exists')
		if  (os.readlink(symlink) == dev):
			Debug('Already correctly symlinked')
		else:
			Debug('Removing stale symlink')
			os.remove(symlink)
			os.symlink(dev,symlink)
	else:
		os.symlink(dev,symlink)

# --------------------------------------------
def RemoveDeadSymLink(alias):
	symlink = '/dev/' + alias
	Debug('Checking ' + symlink)
	# check whether target actually is a link and whether it points to something
	if (os.path.islink(symlink) and not(os.path.exists(symlink))): 
		Debug('Removing dead symlink')
		os.remove(symlink)
			
# ------------------------------------------

configFile = '/usr/local/etc/mosaicmkdev.conf'
mode = MODE_ADD

parser = argparse.ArgumentParser(description='Detects Septentrio mosaicT and makes a device nodes based on the serial number')
parser.add_argument('devpath',help='device path',type=str) # supplied by udev
group = parser.add_mutually_exclusive_group()
group.add_argument('--add','-a',help='add devices',action='store_true')
group.add_argument('--remove','-r',help='remove devices',action='store_true')
parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--log','-l',help='create log file',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug
if args.log:
	log = True

if args.add:
	Debug('Device plugged ' + args.devpath)
	mode = MODE_ADD
	
if args.remove:
	Debug('Device unplugged ' + args.devpath)
	mode = MODE_REMOVE
	
configFile = args.config;

if (not os.path.isfile(configFile)):
	ErrorExit(configFile + ' not found')
	
cfg=Initialise(configFile)

receivers =cfg['main:receivers'].split(',')
ser2rx = {}

for rx in receivers:
	rx = rx.strip()
	sernum = cfg[rx + ':serial number']
	ser2rx[sernum] = rx
	if mode == MODE_REMOVE: # remove any dead symlinks
		if rx+':ttyacm0' in cfg:
			RemoveDeadSymLink(cfg[rx+':ttyacm0'])
		if rx+':ttyacm1' in cfg:
			RemoveDeadSymLink(cfg[rx+':ttyacm1'])

if mode == MODE_REMOVE: # all done
	sys.exit(0)
	
# The device is a USB hub
devRootPath = '/sys' + args.devpath # full path, two slashes is bad

try:
	context = pyudev.Context()
except:
	ErrorExit('No context')
	
# Get the serial number
try:
	rootDev = pyudev.Devices.from_path(context,devRootPath)
except:
	ErrorExit('No device ' + devRootPath)
	
serialNum = rootDev.properties['ID_SERIAL_SHORT']
Debug('Detected serial number = ' + serialNum)

try:
	rx = ser2rx[serialNum]
	Debug('Receiver is ' + rx)
except:
	ErrorExit('No match for serial number ' + serialNum + ' in ' + configFile)

# Check that ttyACM0 and ttyACM1 are defined
if not(rx+':ttyacm0' in cfg):
	ErrorExit( rx + ':ttyACM0 is not defined in ' + configFile)

if not(rx+':ttyacm1' in cfg):
	ErrorExit( rx + ':ttyACM1 is not defined in ' + configFile)
	
# Find the ttyACM ports
ports = []
for dev in context.list_devices(subsystem='usb',DRIVER='cdc_acm'):
	if (dev.parent.sys_path == devRootPath):
		Debug('')
		Debug(dev.sys_path)
		Debug(dev.sys_name)
		Debug(dev.device_path)
		Debug(dev.device_type)
		
		for c in dev.children:
			if (re.match('ttyACM',c.sys_name)):
				Debug('Matched ' + c.sys_name)
				ports.append(c.sys_name)

if not(len(ports) == 2):
	ErrorExit('Unable to find the two ttyACM port devices')
	
ports.sort()

# The convention will be that ttyACM0 --> ports[0]
#                             ttyACM1 --> ports[1]

MakeSymLink(ports[0],cfg[rx + ':ttyacm0'])
MakeSymLink(ports[1],cfg[rx + ':ttyacm1'])
