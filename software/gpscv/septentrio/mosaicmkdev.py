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

VERSION = '0.1.0'
AUTHORS = "Michael Wouters"

# ------------------------------------------
def Debug(msg):
	if (debug):
		#fout = open('/tmp/debug.txt','a')
		#fout.write(time.strftime('%H:%M:%S ',time.gmtime()) + msg+'\n')
		#fout.close()
		print(msg)
	return

# ------------------------------------------
def ErrorExit(msg):
	sys.stderr.write(msg + '\n')
	#fout = open('/tmp/error.txt','a')
	#fout.write(time.strftime('%H:%M:%S ',time.gmtime()) + msg+'\n')
	#fout.close()
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

# ------------------------------------------

configFile = '/usr/local/etc/mosaicmkdev.conf'

parser = argparse.ArgumentParser(description='Detects Septentrio mosaicT and makes a device nodes based on the serial number')
parser.add_argument('devpath',help='device path',type=str) # supplied by udev
parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

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

# The device path is a USB hub
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
