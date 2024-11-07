#!/usr/bin/python3
# logfuruno.py

# Script to log Furuno GPSDO data and create a status file.

# -----------------------------------------------------------------------------
# Ver: 0.0.1
# Author: Louis Marais
# Start: 2022-07-27
# Last: 2022-07-27
#
# Modifications:
# --------------
# Original version
#
# -----------------------------------------------------------------------------
# Ver: 0.0.2
# Author: Louis Marais
# Start: 2022-08-09
# Last: 2022-08-09
#
# Modifications:
# --------------
# 1. Forgot a linefeed at the end of one of the status lines. Fixed now.
#
# -----------------------------------------------------------------------------
# Ver: 0.0.3
# Author: Louis Marais
# Start: 2023-03-31
# Last: 2023-03-31
#
# Modifications:
# --------------
# 1. Fix swapped lat and lon in status file.
#
# -----------------------------------------------------------------------------
# Ver: 0.0.4
# Author: Louis Marais
# Start: 2024-04-23
# Last: 2024-04-23
#
# Modifications:
# --------------
# 1. Changed the way in which the ffe is reported. Not sure if this is better.
#
# -----------------------------------------------------------------------------
# Ver: 0.0.5
# Author: Louis Marais
# Start: 2024-10-16
# Last: 2024-1?-??
#
# Modifications:
# --------------
# 1. Increased robustness around collection of serial data. If GPSDO shuts off
#    or is reset, garbage on the serial connection can make the program bomb
#    out.
# 2. Added a check on the serial output string to remove nonsense characters.
# 3. Changed some 'format' directives to f-strings. Neater and more compact.
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

import os
import serial
import sys
import argparse
import configparser
import subprocess
import signal
import re
import datetime
import time

script = os.path.basename(__file__)
VERSION = "0.0.5"
AUTHORS = "Louis Marais"

running = True
DEBUG = False

# -----------------------------------------------------------------------------
# Sub routines
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
def ts():
	now = datetime.datetime.now()
	tsStr = now.strftime('%Y-%m-%d %H:%M:%S ')
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
def checkConfig(cfg, req):
	cnt = []
	for section in cfg.sections():
		s = section.lower()
		for key in cfg[section]:
			cnt.append(s+','+key.lower())
	for s in req:
		if not s in cnt:
			errorExit(f'Required key ({s}) not in configuration file.')
	debug("All required section:key pairs found in configuration file.")
	return(cnt)

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
		errorExit(f'The path {p} does not exist')
	return

# -----------------------------------------------------------------------------
def getMJD():
	mjd = int(time.time()/86400) + 40587
	return(mjd)

# -----------------------------------------------------------------------------
def extractVersionInfo(s):
	b = s.split(',')
	mdl = b[5].split('*')[0]
	dev = b[2]
	ver = b[3]
	retVal = f"Furuno {mdl}, device {dev}, version {ver}"
	return(retVal)

# -----------------------------------------------------------------------------
# value in (d)ddmm.mmmm converted to dd.dddddd
# and returned as a string
def calcdeg(d):
	p = d.split('.')
	dd = float(p[0][:-2])
	mm = float(p[0][-2:]+'.'+p[1]) 
	ddd = dd+mm/60
	return(f"{ddd:0.7f}")

# -----------------------------------------------------------------------------
def getPosDOP(s): # decode G*GNS message
	d = s.split(',')
	lat = calcdeg(d[2])
	if d[3] == 'S':
		lat = '-'+lat
	lng = calcdeg(d[4])
	if d[5] == 'W':
		lng = '-'+lng
	hgt = d[9]
	sts = d[7]
	hdp = d[8]
	retVal = [lat,lng,hgt,sts,hdp]
	return(retVal)

# -----------------------------------------------------------------------------
def getSats(s):
	#print(s)
	sats = []
	sats.append([]) # PRN numbers
	sats.append([]) # elevation (degrees)
	sats.append([]) # azimuth (degrees)
	sats.append([]) # C/NO (dB-Hz)
	for i in range(0,len(s)):
		itms = s[i].split(',')
		for j in range(0,4):
			prn = itms[4+j*4]
			elv = itms[5+j*4]
			azm = itms[6+j*4]
			cno = itms[7+j*4]
			if not prn == '' and not elv == '' and not azm == '' and not cno == '':
				sats[0].append(prn)
				sats[1].append(elv)
				sats[2].append(azm)
				sats[3].append(cno)
	#print(sats)
	return(sats)

# -----------------------------------------------------------------------------
def decodeTPS1(s):
	l = s.split(',')
	leapsecs = "{:d}".format(int(l[5]))
	p = l[7]
	pps_source = ''
	if p == '0':
		pps_source = 'RTC'
	elif p == '1':
		pps_source = 'GPS'
	elif p == '2':
		pps_source = 'UTC(USNO)'
	elif p == '3':
		pps_source = 'UTC(SU)'
	elif p == '4':
		pps_source = 'UTC(EU)'
	elif p == '5':
		pps_source = 'UTC(NICT)'
	else:
		pps_source = 'TPS1 MSG DECODE ERROR!'
	clock_drift = "{:.3f}".format(float(l[8]))
	k = l[9].split('*')
	int_temp = "{:.2f}".format(float(k[0])/100)
	tps1 = [leapsecs,pps_source,clock_drift,int_temp]
	#print(tps1)
	return(tps1)

# -----------------------------------------------------------------------------
def decodeTPS2(s):
	l = s.split(',')
	ppsmode = ""
	if l[2] == "0":
		ppsmode = "Off"
	else:
		if l[3] == "0":
			ppsmode = "Always off"
		elif l[3] == "1":
			ppsmode = "Always on"
		elif l[3] == "2":
			ppsmode = "On when time and pos fixed"
		else:
			ppsmode = "On when TRAIM is OK"
	pulsewidth = l[5]+" ms"
	ppsdelay = "{:.0f} ns".format(float(l[6]))
	if l[7] == "0":
		ppsedge = "rising"
	else:
		ppsedge = "falling"
	ppsacc = "{:.0f} ns".format(float(l[9]))
	# I thought this may be the EFC voltage but it is not.
	#vefc = "{:0.3f} V".format(float(l[10])) # Shows as reserved on specification
	tps2 = [ppsmode,pulsewidth,ppsdelay,ppsedge,ppsacc] #,vefc)
	#print(tps2)
	return(tps2) #,vefc)

# -----------------------------------------------------------------------------
def decodeReceiverStatus(s):
	s = s[2:]
	if s[7] == "0":
		antcur = "Normal"
	elif s[7] == "1":
		antcur = "Short"
	elif s[7] == "2":
		antcur = "Open"
	else:
		antcur = "None"
	if s[6] == "0":
		spoofing = "Not detected"
	else:
		spoofing = "Detected"
	if s[5] == "0":
		nlosmaskmode = "OFF"
	else:
		nlosmaskmode = "Step {}".format(s[5])
	if s[4] == "0":
		ontime = "Less than 1 hour"
	elif s[4] == "1":
		ontime = "> 1 hour, < 1 day"
	elif s[4] == "2":
		ontime = "between 1 and 7 days"
	elif s[4] == "3":
		ontime = "between 7 and 30 days"
	else:
		ontime = "more than 30 days"
	if(s[0]) == "0":
		antenv = "No position fix"
	elif(s[0]) == "1":
		antenv = "Open sky"
	elif(s[0]) == "2":
		antenv = "Semi-shielded"
	else:
		antenv = "High shielding"
	s = "{}, {}, {}, {}, {}".format(antcur,spoofing,nlosmaskmode,ontime,antenv)
	return([antcur,spoofing,nlosmaskmode,ontime,antenv])

# -----------------------------------------------------------------------------
def decodeTPS3(s):
	l = s.split(',')
	if l[2] == "0":
		posmode = "NAV"
	elif l[2] == "1":
		posmode = "SS"
	elif l[2] == "2":
		posmode = "CSS"
	else:
		posmode = "TO"
	posdif = "{:d} m".format(int(l[3]))
	if l[7] == "0":
		traim = "OK"
	elif l[7] == "1":
		traim = "ALARM"
	else:
		traim = "Insufficient sats"
	recvsts = decodeReceiverStatus(l[10])
	tps3 = [posmode,posdif,traim]
	tps3.extend(recvsts)
	#print(tps3)
	return(tps3)

# -----------------------------------------------------------------------------
def decodeAlarm(s):
	t = int(s,16)
	if t & 0x03 == 0:
		antcur = "Normal"
	elif t & 0x03 == 1:
		antcur = "Open"
	elif t & 0x03 == 2:
		antcur = "Short"
	else:
		antcur = "None"
	if t & 0x04 == 0:
		oscerror = "Normal"
	else:
		oscerror = "Output error"
	if t & 0x08 == 0:
		oscctrl = "Normal"
	else:
		oscctrl = "control impossible (lifetime error)"
	alarm = [antcur,oscerror,oscctrl]
	return(alarm)

# -----------------------------------------------------------------------------
def decodeStatus(s):
	t = int(s,16)
	if t & 0x01 == 0:
		antpwr = "OFF"
	else:
		antpwr = "ON"
	if t & 0x02 == 0:
		syncsource = "GNSS"
	else:
		syncsource = "EPPS"
	if t & 0x04 == 0:
		eppsdetect = "No"
	else:
		eppsdetect = "Yes"
	if t & 0x40 == 0:
		debugging = "Off"
	else:
		debugging = "On"
	if t & 0x80 == 0:
		tempcor = "Available"
	else:
		tempcor = "None"
	status = [antpwr,syncsource,eppsdetect,debugging,tempcor]
	return(status)

# -----------------------------------------------------------------------------
def decodeTPS4(s):
	#print(s)
	l = s.split(',')
	if l[2] == "0":
		freqmode = "Warm up"
	elif l[2] == "1":
		freqmode = "Pull-in"
	elif l[2] == "2":
		freqmode = "Coarse lock"
	elif l[2] == "3":
		freqmode = "Fine lock"
	elif l[2] == "4":
		freqmode = "Holdover"
	else:
		freqmode = "Out of holdover"
	tps4 = [freqmode]
	tps4.extend(decodeAlarm(l[4]))
	tps4.extend(decodeStatus(l[5]))
	tps4.append("{:.0f} ns".format(float(l[6]))) # pps_err
	tps4.append("{:.0f} ppb".format(float(l[7]))) # freq_err
	tps4.append("{:.0f} s".format(float(l[10]))) # holdover_time
	#print(tps4)
	return(tps4)

# -----------------------------------------------------------------------------
def getGPSDOstatus(p):
	tps1 = []
	tps2 = []
	tps3 = []
	tps4 = []
	for i in range(0,len(p)):
		if p[i][9:13] == "TPS1":
			tps1 = decodeTPS1(p[i])
			#[leapsecs,pps_source,clock_drift,int_temp]
		elif p[i][9:13] == "TPS2":
			tps2 = decodeTPS2(p[i])
			#[ppsmode,pulsewidth,ppsdelay,ppsedge,ppsacc]
		elif p[i][9:13] == "TPS3":
			tps3 = decodeTPS3(p[i])
			# [posmode,posdif,traim,antcur,spoofing,nlosmaskmode,ontime,antenv]
		elif p[i][9:13] == "TPS4":
			tps4 = decodeTPS4(p[i])
			# [freqmode,antcur,oscerror,oscctrl,antpwr,syncsource,eppsdetect,
			#  debugging,tempcor,ppserr,ferr,holdovertm]
	gpsdo = [tps1,tps2,tps3,tps4]
	return(gpsdo)
	
# -----------------------------------------------------------------------------
def extractStatus(l,v,flnm):
	idx = 0
	for s in l:
		if s[3:6] == "RMC":
			if len(s) > 30:
				#if not s[11:13] == "00": # Save status every minute
				if not s[12:13] == "0":   # Save status every 10 seconds
					return                  # gpsdo collection saves every second... 
				else:
					idx = l.index(s)
					break
	#if not l[idx][11:13] == "00":
	if not l[idx][12:13] == "0":
		return
	if v == "":
		return
	for i in range(idx,len(l)):
		if l[i][3:6] == "GNS":
			posdop = getPosDOP(l[i])
			# posdop = [latitude,longitude,heigt,no of sats,hdop]
			break
	# collect G*GSA messages to find satellites being tracked
	# collect GPGSV messages to get GPS satellite information
	# collect GLGSV messages to get GLONASS satellite information
	# collect GAGSV messages to get GALILEO satellite information
	gps = []
	glo = []
	gal = []
	for i in range(idx,len(l)):
		if l[i][1:6] == 'GPGSV':
			gps.append(l[i])
		elif l[i][1:6] == 'GLGSV':
			glo.append(l[i])
		elif l[i][1:6] == 'GAGSV':
			gal.append(l[i])
	if len(gps) > 0:
		gpssats = getSats(gps)
	if len(glo) > 0:
		glosats = getSats(glo)
	if len(gal) > 0:
		galsats = getSats(gal)
	# collect PERDCR* messages to get GPSDO status information
	p = []
	for i in range(idx,len(l)):
		if l[i][1:7] == 'PERDCR':
			p.append(l[i])
	if len(p) > 0:
		gpsdo = getGPSDOstatus(p)
	saveStatus(flnm,v,posdop,gpssats,glosats,galsats,gpsdo)
	return(posdop,gpssats,glosats,galsats,gpsdo)

# -----------------------------------------------------------------------------
def healthreport(g):
	traim = g[2][2]
	spoofing = g[2][4]
	ontime = g[2][6]
	freqmode = g[3][0]
	antcur = g[3][1]
	oscerror = g[3][2]
	oscctrl = g[3][3]
	holdovertime = g[3][11]
	if (traim == "OK" and spoofing == "Not detected" and antcur == "Normal" and 
		 oscerror == "Normal" and oscctrl == "Normal"):
		s = f"Healthy - Frequency mode: {freqmode}, "
		s += f"on for {ontime}, "
		s += f"available holdover time: {holdovertime}"
	else:
		s = "Unhealthy |"
		if not traim == "OK":
			s += " TRAIM ALARM |"
		if not spoofing == "Not detected":
			s += " Spoofing detected |"
		if not antcur == "Normal":
			s += f" Antenna current: {antcur} |"
		if not oscerror == "Normal":
			s += f" Oscillator: {oscerror} |"
		if not oscctrl == "Normal":
			s += f" Oscillator: {oscctrl} |"
	return(s)

# -----------------------------------------------------------------------------
def getSatParams(preamble,s):
	lines = []
	sats = preamble+"Sats: "
	ss = preamble+"SS: "
	elv = preamble+"Elv: "
	azim = preamble+"Azim: "
	for i in range(0,len(s[0])):
		sats += "{}".format(int(s[0][i]))
		ss += "{}".format(int(s[3][i]))
		elv += "{}".format(int(s[1][i]))
		azim += "{}".format(int(s[2][i]))
		if i != (len(s[0])-1):
			sats += ','
			ss += ','
			elv += ','
			azim += ','
	sats += '\n'
	ss += '\n'
	elv += '\n'
	azim += '\n'
	lines.append(sats)
	lines.append(ss)
	lines.append(elv)
	lines.append(azim)
	return(lines)

# -----------------------------------------------------------------------------
def getmoreInfo(glo,gal,gpsdo):
	lines = ["Additional information:\n"]
	lines.extend(getSatParams("GLONASS - ",glo))
	lines.extend(getSatParams("GALILEO - ",gal))
	lines.append("Leap seconds: {}\n".format(gpsdo[0][0]))
	lines.append("Time reference: {}\n".format(gpsdo[0][1]))
	lines.append("Clock drift: {} (see TPS1 message)\n".format(gpsdo[0][2]))
	lines.append("Internal temperature: {} degC\n".format(gpsdo[0][3]))
	lines.append("PPS pulse width: {}\n".format(gpsdo[1][1]))
	return(lines)

# -----------------------------------------------------------------------------
def saveStatus(flnm,verStr,posdop,gps,glo,gal,gpsdo):
	lines = ['ID: {}\n'.format(verStr)]
	lines.extend(getSatParams("",gps))
	#lines.append("ffe: {}E-09\n".format(gpsdo[3][10].split(' ')[0]))
	lines.append("ffe: {}E-09\n".format(gpsdo[0][2]))
	lines.append("tie: {}E-09\n".format(gpsdo[3][9].split(' ')[0]))
	lines.append("lat: {}\n".format(posdop[0]))
	lines.append("lon: {}\n".format(posdop[1]))
	lines.append("alt: {}\n".format(posdop[2]))
	lines.append("Health: {}\n".format(healthreport(gpsdo)))
	lines.append('efc: 200\n')
	additionalInfo = getmoreInfo(glo,gal,gpsdo)
	lines.extend(additionalInfo)
	# For debugging
	debug(f"Information saved to status file({flnm}).")
	if DEBUG:
		for l in lines:
			print(l.strip())
	with open(flnm,'w') as f:
		f.writelines(lines)
		f.close()
	return

# -----------------------------------------------------------------------------
def calcChkSum(s):
	cs = ord(s[0])
	for i in range(1,len(s)):
		cs = cs ^ ord(s[i])
	return("{:02X}".format(cs))

# -----------------------------------------------------------------------------
def sendcmd(cmd):
	chksum = calcChkSum(cmd)
	s = '$'+cmd+'*'+chksum+'\r\n'
	b = bytearray(s.encode('utf-8'))
	ser.write(b)
	return

# -----------------------------------------------------------------------------
def requestSN():
	sendcmd("PERDSYS,VERSION")
	return

# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------

parser = argparse.ArgumentParser(description="Logs data from Furuno GPSDO "+
																 "and creates status file")
parser.add_argument("-v","--version",action="store_true",help="Show version "+
										"and exit.")
parser.add_argument("-c","--config",nargs=1,help="Specify alternative "+
										"configuration file. The default is "+
										"~/etc/furuno.conf.")
parser.add_argument("-d","--debug",action="store_true",help="Turn debugging on")

args = parser.parse_args()

if args.debug:
	DEBUG = True

versionStr = f"{script} version {VERSION} written by {AUTHORS}"

if args.version:
	print(versionStr)
	sys.exit(0)

debug(versionStr)

HOME = os.path.expanduser('~')
if not(HOME.endswith('/')):
	HOME += '/'

debug(f"Current user's home: {HOME}")

configfile = HOME+"etc/furuno.conf"

if args.config:
	debug("Alternate config file specified: "+str(args.config[0]))
	configfile = str(args.config[0])
	if not configfile.startswith('/'):
		configfile = HOME+configfile

debug(f"Configuration file: {configfile}")

if not os.path.isfile(configfile):
	errorExit(f"{configfile} does not exist.")

conf = configparser.ConfigParser()
conf.read(configfile)

req = ['main,lock file','comms,port','data,path','data,ext','status,path',
			 'status,file name']

cfg = checkConfig(conf, req)

port = conf['comms']['port']

datapath = makePath(conf['data']['path'])
checkPath(datapath)
debug(f'Data will be saved to: {datapath}')

ext = conf['data']['ext']
if not ext.startswith('.'):
	ext = '.'+ext
debug(f"Data files will have a '{ext}' extension.")

statuspath = makePath(conf['status']['path'])
checkPath(statuspath)

statusfile = statuspath + conf['status']['file name']
debug(f'GPSDO status will be saved to {statusfile}')

# Create UUCP lock for the serial port
uucpLockPath='/var/lock'
if ('paths,uucp lock' in cfg):
	uucpLockPath = conf['paths']['uucp lock']

ret = subprocess.check_output(['/usr/local/bin/lockport','-d',uucpLockPath,
									 '-p',str(os.getpid()),port,sys.argv[0]]).decode('utf-8')

if (re.match('1',ret)==None):
	errorExit(f'Could not obtain a lock on {port}.')

lockfile = conf['main']['lock file']
if not lockfile.startswith('/'):
	lockfile = HOME+lockfile
if not CreateProcessLock(lockfile):
	errorExit(f'Unable to lock - {script} already running?')

signal.signal(signal.SIGINT,signalHandler)
signal.signal(signal.SIGTERM,signalHandler)
signal.signal(signal.SIGHUP,signalHandler) # not usually run with a controlling TTY, but handle it anyway

t_out = 10.0
if ('comms,timeout' in cfg):
	t_out = float(conf['comms']['timeout'])

debug(f"Serial communications timeout is {t_out:.1f} s.")

debug(f'Opening {port}')

oldmjd = 0
oldflnm = ""

lines = []
gotSN = False
verStr = ""

# Format receiver

with serial.Serial(port,38400,timeout = t_out) as ser:
	try:
		s = ser.readline() # throw first (likely incomplete) string away
	except:
		debug("Initial serial read exception.")
		pass
	readtime = time.time()
	# TODO: Format receiver pps pulse width, sat system(s), etc
	#       Read desired values from the configuration file
	#       Coldstart?
	#       Change baud rate? can be done on the fly: ser.baudrate = {baudrate}
	while running:
		if not gotSN:
			requestSN()
			gotSN = True
		try:
			s = ser.readline().decode().strip()
		except:
			debug("Serial read exception.")
			s = ""
			continue
		if (time.time() - readtime) > t_out:
			print("Error! Serial timeout waiting for data.")
			ser.close()
			break
		readtime = time.time()
		mjd = getMJD()
		if mjd != oldmjd:
			flnm = datapath+str(mjd)+ext
			if os.path.isfile(flnm):
				t = "Continuing"
				debug("Continuing data file: {}".format(flnm))
			else:
				t = "Starting"
				debug("New data file: {}".format(flnm))
			if oldflnm != "":
				f.close()
			f = open(flnm,'a')
			f.write(f"# {ts()} : {t} {flnm}\n")
			oldflnm = flnm
			oldmjd = mjd
		
		# Remove special characters from the string. When the GPSDO powers on a
		# bunch of null characters can be placed in a serial output string.
		t = ""
		for i in range(0,len(s)):
			if (ord(s[i])) in range(32,129):
				t += s[i]
		if t.strip() == '':
			continue
		s = t
		
		f.write(s+'\n')
		f.flush()
		if "VERSION" in s:
			if not "PERDACK" in s:
				verStr = extractVersionInfo(s)
		lines.append(s)
		if len(lines) > 25:
			lines.pop(0)
			if "TPS4" in lines[-1]:
				extractStatus(lines,verStr,statusfile)
	ser.close()
	f.write(f"# {ts()} : Logging process stopped.\n")
	f.close()

subprocess.check_output(['/usr/local/bin/lockport','-r',port])

RemoveProcessLock(lockfile)

print(ts(),script,'terminated.')
