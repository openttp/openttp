#!/usr/bin/python3
# logfuruno.py

# Script to log Furuno GPSDO data and create a status file.

#
# The MIT License (MIT)
#
# Copyright (c) 2024 E. Louis Marais
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


# -----------------------------------------------------------------------------
# Modification history
# -----------------------------------------------------------------------------
#    DATE   AUTHOR   VER    COMMENTS
# -----------------------------------------------------------------------------
# 2022-07-27 ELM    0.0.1  Original version
# 2022-08-09 ELM    0.0.2  Forgot a linefeed at the end of one of the status
#                          lines. Fixed now.
# 2023-03-31 ELM    0.0.3  Fix swapped lat and lon in status file.
# 2024-04-23 ELM    0.0.4  Changed the way in which the ffe is reported. Not
#                          sure if this is better.
# 2024-10-16 ELM    0.0.5  Several changes while doing reset and power tests:
#                          Increased robustness around collection of serial
#                          data. If GPSDO shuts off or is reset, garbage on
#                          the serial connection can make the script bomb.
#                          Added a check on the serial output string to remove
#                          nonsense characters.
#                          Changed some 'format' directives to f-strings. It's
#                          neater and more compact.
# 2024-11-18 ELM    0.0.6  Added configuration options for GPSDO serial number,
#                          antenna current warning, PPS pulse length, PPS cable
#                          delay, and GCLK
#                          output frequency and on/off status.
#                          Added hardware serial number to status file.
# -----------------------------------------------------------------------------


# TODO:  Fix the way the frequency offset / error is reported
#        This is the 'ffe' number in the 'furuno.status' file.
#
# There is still an issue with how frequency drift data is presented. The 
# GPSDO reports the VCLK frequency error in the TPS4 message, but only to a
# resolution of 1 ppb (1E-9) while its short term stability is specified to
# be better than 1E-11, and its long term stability (24 hrs) is better than 
# 1E-12.
# 
# The TPS1 message reports the "Clock drift" to 0.001 ppb (1E-12) but this is
# related to the PPS error. Currently the software reports this value as the
# oscillator ffe, which is not correct. Averaging this value and centering it
# around zero may provide a better number...
#
# This needs to be investigated further.
# 
# It may be possible to integrate the PPS offset for a number of seconds to
# get a decent estimate of the frequency error, but this may just be the same
# as the number reported in the TPS1 message, although this will remove the 
# large offset reported in that value.
#


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
VERSION = "0.0.6"
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
def extractStatus(l,v,gpsdo_sn,flnm):
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
	saveStatus(flnm,gpsdo_sn,v,posdop,gpssats,glosats,galsats,gpsdo)
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
	lines.append("Clock drift: {} (see TPS1 message)\n".format(gpsdo[0][2]))  # TODO: Find a better way of calculating this.
	lines.append("Internal temperature: {} degC\n".format(gpsdo[0][3]))
	lines.append("PPS pulse width: {}\n".format(gpsdo[1][1]))
	return(lines)

# -----------------------------------------------------------------------------
def saveStatus(flnm,gpsdo_sn,verStr,posdop,gps,glo,gal,gpsdo):
	lines = [f"ID: {verStr}\n"]
	lines.append(f"Furuno serial number: {gpsdo_sn}\n")
	lines.extend(getSatParams("",gps))
	#lines.append("ffe: {}E-09\n".format(gpsdo[3][10].split(' ')[0]))
	lines.append("ffe: {}E-09\n".format(gpsdo[0][2]))
	lines.append("tie: {}E-09\n".format(gpsdo[3][9].split(' ')[0]))
	lines.append("lat: {}\n".format(posdop[0]))
	lines.append("lon: {}\n".format(posdop[1]))
	lines.append("alt: {}\n".format(posdop[2]))
	lines.append("Health: {}\n".format(healthreport(gpsdo)))
	lines.append('efc: 200\n') # canned value to preserve gpsdo status record format
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
def requestVERSION():
	sendcmd("PERDSYS,VERSION")
	return

# -----------------------------------------------------------------------------
# TODO: Add more options, see Furuno documentation.
def configureReceiver(plen,cdelay,emask,antw,gclko,gclkf):
	# PPS configuration
	# Mode = 1 : PPS always on
	# Polarity = 0 : Rising edge
	cmd=f"PERDAPI,PPS,VCLK,1,0,{plen:0d},{cdelay:0d},0"
	debug(f"PPS formatting command sent: {cmd}")
	sendcmd(cmd)
	# NOTE: There is no query message for PPS configuration.
	# Positioning and elevation mask setting
	cmd = f"PERDAPI,FIXMASK,USER,{emask:0d},0,00,0,0x0,0x0,0x0,0x0,0x0"
	debug(f"FIXMASK command sent: {cmd}")
	sendcmd(cmd)
	# Query setting
	cmd = "PERDAPI,FIXMASK,QUERY"
	sendcmd(cmd)
	# Antenna alarm setting
	if antw == 0:
		cmd = "PERDAPI,ALMSET,0x00,0xFE"  # Masks antenna open warning
		sendcmd(cmd)
	# Query setting
	cmd = "PERDAPI,ALMSET,QUERY"
	sendcmd(cmd)
	# GCLK frequency setting
	mode = 0
	if gclko:
		mode = 1
	cmd = f"PERDAPI,GCLK,{mode},{gclkf:0d}"
	debug(f"GCLK formatting command sent: {cmd}")
	sendcmd(cmd)
	# Query setting
	cmd = "PERDAPI,GCLK,QUERY"
	sendcmd(cmd)
	return

# -----------------------------------------------------------------------------
def setAntPos(lat,lon,alt):
	cmd = f"PERDAPI,SURVEY,3,0,0,{lat:0.7f},{lon:0.7f},{alt:0.2f}"
	debug(f"SURVEY command sent: {cmd}")
	sendcmd(cmd)
	# NOTE: There is no query command for the SURVEY command
	return

# -----------------------------------------------------------------------------
def setSiteSurvey():
	cmd = f"PERDAPI,SURVEY,1"
	debug(f"SURVEY command sent: {cmd}")
	sendcmd(cmd)
	# NOTE: There is no query command for the SURVEY command
	return

# -----------------------------------------------------------------------------
def checkSatSys(s):
	v = [False,'','','','']
	satsystems = ['GPS', 'GLONASS', 'GALILEO', 'QZSS']
	t = list(a.strip().upper() for a in s.split(','))
	allValid = True
	for l in t:
		if not l in satsystems:
			allValid = False
			break
	if not allValid:
		return v
	v[0] = True
	if 'GPS' in t:     v[1] = 'GPS'
	if 'GLONASS' in t: v[2] = 'GLONASS'
	if 'GALILEO' in t: v[3] = 'GALILEO'
	if 'QZSS' in t:    v[4] = 'QZSS'
	return v

# -----------------------------------------------------------------------------
def setLeapSeconds(nls):
	cmd = f"PERDAPI,DEFLS,{nls}"
	debug(f"DEFLS command sent: {cmd}")
	sendcmd(cmd)
	cmd = "PERDAPI,DEFLS,QUERY"
	sendcmd(cmd)
	return

# -----------------------------------------------------------------------------
def setDefaultTimeAlign():
	cmd = f"PERDAPI,TIMEALIGN,2"
	debug(f"TIMEALIGN command to be sent: {cmd}")
	sendcmd(cmd)
	#time.sleep(1)
	cmd = "PERDAPI,TIMEALIGN,QUERY"
	sendcmd(cmd)
	return

# -----------------------------------------------------------------------------
def setTimeAlign(ta,ls):
	mode = "2"
	if ta == 'USNO': mode = "2"
	if ta == 'SU':
		mode = "3"
		setLeapSeconds(ls)
	if ta == 'EU':   mode = "4"
	if ta == 'NICT': mode = "5"
	cmd = f"PERDAPI,TIMEALIGN,{mode}"
	debug(f"TIMEALIGN command to be sent: {cmd}")
	sendcmd(cmd)
	#time.sleep(1)
	cmd = "PERDAPI,TIMEALIGN,QUERY"
	sendcmd(cmd)
	return

# -----------------------------------------------------------------------------
def setDefaultSatelliteSystems():
	cmd = "PERDAPI,GNSS,GN,2,2,2,2,1"
	debug(f"GNSS command sent: {cmd}")
	sendcmd(cmd)
	#time.sleep(1)
	cmd = "PERDAPI,GNSS,QUERY"
	sendcmd(cmd)
	#time.sleep(1)
	setDefaultTimeAlign()
	return

# -----------------------------------------------------------------------------
# Note that selecting certain GNSS may required changes in time alignment and 
# may require specification of the leap second offset.
def setSatelliteSystems(ss):
	leaps = ss[4]
	setleap = False
	timealign = 'USNO'           # Align PPS output to UTC(USNO)
	if not 'GPS' in ss:
		timealign = 'EU'           # Align PPS output to UTC(EU)
		if not 'GALILEO' in ss:
			timealign = 'NICT'       # Align PPS output to UTC(NICT)
			if not 'QZSS' in ss:
				timealign = 'SU'       # Align PPS output to UTC(SU)
				setleap = True
	debug(f"Output PPS aligned with UTC({timealign})")
	cmd = "PERDAPI,GNSS,GN,"
	if 'GPS' in ss: 
		cmd += "2,"
	else: 
		cmd += "0,"
	if 'GLONASS' in ss: 
		cmd += "2," 
	else: 
		cmd += "0,"
	if 'GALILEO' in ss: 
		cmd += "2,"
	else: 
		cmd += "0,"
	if 'QZSS' in ss: 
		cmd += "2,"
	else: 
		cmd += "0,"
	cmd += "1"
	debug(f"GNSS command sent: {cmd}")
	sendcmd(cmd)
	#time.sleep(1)
	cmd = "PERDAPI,GNSS,QUERY"
	sendcmd(cmd)
	#time.sleep(1)
	setTimeAlign(timealign,leaps)
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

req = ['main,lock file','main,serial number','comms,port','data,path',
			 'data,ext','status,path','status,file name']

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

furuno_sn = conf['main']['serial number']

debug(f"The Furuno GPSDO serial number is {furuno_sn}")

antwarn = True
pulselen = 500 # milliseconds
cabledelay = 0 # ns
elvmask = 5 # degrees
gclkon = False
gclkfreq = int(10E6) # Hz
leapsecs = 18 # correct on 2024-11-19
satsys = [False,'','','','']
fixant = False
latitude = 0
longitude = 0
altitude = 0

if 'receiver,antenna open warning' in cfg:
	try:
		if int(conf['receiver']['antenna open warning']) == 0:
			antwarn = False
		debug("User request: Ignore antenna open warning")
	except:
		debug("There is an issue with the '[receiver] antenna open warning' "+
				  "configuration entry")
		pass

debug(f"Antenna open warning set to {antwarn}")

if 'receiver,pps length' in cfg:
	try:
		d = int(conf['receiver']['pps length'])
		if d >= 1 and d <= 500:
			pulselen = d
		debug(f"User request: PPS length = {pulselen} ms")
	except:
		debug("There is an issue with the '[receiver] pps length' "+
				  "configuration entry")
		pass

debug(f"PPS pulse length: {pulselen} ms")

if 'receiver,pps cable delay' in cfg:
	try:
		d = int(conf['receiver']['pps cable delay'])
		if d >= -100000 and d <= 100000:
			cabledelay = d
			debug(f"User request: PPS cable delay = {cabledelay} ns")
	except:
		debug("There is a issue with the '[receiver] PPS cable delay' "+
				  "configuration entry.")
		pass

debug(f"PPS cable delay is {cabledelay} ns")

if 'receiver,elevation mask' in cfg:
	try:
		e = int(conf['receiver']['elevation mask'])
		if e >= 0 and e <=90:
			elvmask = e
			debug(f"User request: Elevation mask = {elvmask} degrees")
	except:
		debug("There is a issue with the '[receiver] Elevation mask' "+
				  "configuration entry.")
		pass

debug(f"Elevation mask is {elvmask} degrees.")

c = [0,0,0]

if 'antenna position,latitude' in cfg:
	try:
		l = float(conf['antenna position']['latitude'])
		if l >= -90 and l <= 90:
			latitude = l
			c[0] = 1
			debug(f"User request: Latitute = {latitude:0.7f} degrees")
	except:
		debug("There is a issue with the '[antenna] latitude' "+
				  "configuration entry.")
		pass

if 'antenna position,longitude' in cfg:
	try:
		l = float(conf['antenna position']['longitude'])
		if l >= -180 and l <= 180:
			longitude = l
			c[1] = 1
			debug(f"User request: Longitude = {longitude:0.7f} degrees")
	except:
		debug("There is a issue with the '[antenna] longitude' "+
				  "configuration entry.")
		pass

if 'antenna position,altitude' in cfg:
	try:
		l = float(conf['antenna position']['altitude'])
		if l >= -1000 and l <= 18000:
			altitude = l
			c[2] = 1
			debug(f"User request: Altitude = {altitude:0.2f} meters")
	except:
		debug("There is a issue with the '[antenna] altitude' "+
				  "configuration entry.")
		pass

if c[0] == 1 and c[1] == 1 and c[2] == 1:
	fixant = True
	debug("User provided fixed antenna coordinates.")
	debug(f"Longitude = {longitude:0.7f} degrees.")
	debug(f"Latitude  = {latitude:0.7f} degrees.")
	debug(f"Altitude  =  {altitude:0.2f} meters.")
else:
	debug("Either user did not request fixed antenna coordinates, or there "+
			  "is an issue with the coordinates in the configuration file")

if 'receiver,satellite systems' in cfg:
	satsys = checkSatSys(conf['receiver']['satellite systems'])
	debug(f"Use user defined selection of satellite systems? {satsys[0]}")
	debug(f"User request: Uses these satellite systems: {satsys[1:]}")
else:
	debug("User did not specify selection of satellite systems")

if 'receiver,leap seconds' in cfg:
	try:
		l = int(conf['receiver']['leap seconds'])
		if l >= -100 and l < 100:
			leapsecs = l
			satsys[5] = leapsecs
			debug(f"User specified: Number of leap seconds = {leapsecs} seconds")
	except:
		debug("There is an issue with the '[receiver] leap seconds' "+
				  "configuration entry.")
		pass

debug(f"Number of pre-configured leap seconds is {leapsecs} seconds.")

satsys.append(leapsecs)

if 'receiver,gclk on' in cfg:
	try:
		if int(conf['receiver']['gclk on']) == 1:
			gclkon = True
		debug("User request: Turn GCLK on")
	except:
		debug("There is an issue with the '[receiver] gclk on' "+
				  "configuration entry")
		pass

debug("GCLK output is %s." %("on" if gclkon else "off"))

if 'receiver,gclk freq' in cfg:
	try:
		d = int(float(conf['receiver']['gclk freq']))
		if d >= 10 and d <= 40E6:
			gclkfreq = d
		debug(f"User request: Set GCLK frequency to {gclkfreq:0d} Hz")
	except:
		debug("There is an issue with the '[receiver] gclk freq' "+
				  "configuration entry")
		pass

debug(f"GCLK frequency: {gclkfreq} Hz")

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

t_out = 10.0 # seconds
if 'comms,timeout' in cfg:
	t_out = float(conf['comms']['timeout'])

debug(f"Serial communications timeout is {t_out:.1f} s.")

debug(f'Opening {port}')

oldmjd = 0
oldflnm = ""

lines = []
gotVERSION = False
verStr = ""

with serial.Serial(port,38400,timeout = t_out) as ser:
	try:
		s = ser.readline() # throw first (likely incomplete) string away
	except:
		debug("Initial serial read exception.")
		pass
	readtime = time.time()
	configureReceiver(pulselen,cabledelay,elvmask,antwarn,gclkon,gclkfreq)
	if fixant:
		setAntPos(latitude,longitude,altitude)
	else: # Site Survey is the default, but we do not want a stale position to be set.
		setSiteSurvey()
	if satsys[0]:
		setSatelliteSystems(satsys[1:])
	else:
		setDefaultSatelliteSystems()
	while running:
		if not gotVERSION:
			requestVERSION()
			gotVERSION = True
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
				gotVERSION = False  # Make sure every data file has a response for this.
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
				extractStatus(lines,verStr,furuno_sn,statusfile)
	ser.close()
	f.write(f"# {ts()} : Logging process stopped.\n")
	f.close()

subprocess.check_output(['/usr/local/bin/lockport','-r',port])

RemoveProcessLock(lockfile)

print(ts(),script,'terminated.')
