#!/usr/bin/python3
#

#
# The MIT License (MIT)
#
# Copyright (c) 2023 Michael J. Wouters
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
# Compares receiver time-transfer files for the purpose of tracking performance

import argparse
import copy
from   datetime import datetime

import numpy as np
import os
import re
import socket
import subprocess
import sys
import time

import smtplib
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from email.mime.image import MIMEImage

import allantools

# This is where OpenTTP libraries are installed
sys.path.append("/usr/local/lib/python3.6/site-packages")  # Ubuntu 18.04
sys.path.append("/usr/local/lib/python3.8/site-packages")  # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22.04

try: 
	import ottplib as ottp
except ImportError:
	sys.exit('ERROR: Must install ottplib\n eg openttp/software/system/installsys.py -i ottplib')

VERSION = "0.2.4"
AUTHORS = "Michael Wouters"

MODE_CMPALL = 0
MODE_CMPREF = 1

TRACKS_PER_DAY = 78

# ------------------------------------------
def ParseDataSpec(dataSpec):
	path = None
	prefix = None
	if '*' == dataSpec[-1]: 
		path,prefix = os.path.split(dataSpec)
		path = ottp.MakeAbsolutePath(path,root)
		prefix = prefix[:-1] # chop off the asterix
	else:
		path = ottp.MakeAbsolutePath(dataSpec,root)
	return path,prefix
	
# ------------------------------------------
def LoadRefCalAvMatches(fName):
	t=[]
	delta=[]
	try:
		fin = open(fName,'r')
	except:
		ErrorExit('Failed to open ' + fName) # shouldn't happen
	
	for l in fin:
		# format is 
		# MJD TOD(s) REFSYS_1 REFSYS_2 Delta NSATS
		data = l.split()
		t.append(float(data[0]) + float(data[1])/86400)
		delta.append(float(data[4]))
	
	fin.close()
	return np.array([t,delta])

# ------------------------------------------
def ProcessEvents(d,rxev,sgn):
	
	# Events
	if (rxev):
		for ev in rxev:
			if (ev[1] == 'STEP'):
				tStop = float(ev[0])
				evStep = float(ev[2])
				for i in range(0,len(d[0])):
					if d[0][i] <= tStop:
						d[1][i] = d[1][i] + sgn*evStep
		for ev in rxev:
			if (ev[1] == 'BAD'):
				tStart = float(ev[0])
				tStop  = float(ev[2])
				for i in range(0,len(d[0])):
					if d[0][i] >= tStart and d[0][i] <= tStop:
						d[0][i]=float('nan')
						d[1][i]=float('nan')
	
	cd0 = d[0][~np.isnan(d[0])]
	cd1 = d[1][~np.isnan(d[1])]
	
	return np.array([cd0,cd1])

# ------------------------------------------
def RemoveOutliers(d):

	mdn = np.nanmedian(d[1])
	outlierCnt = 0
	for i in range(0,len(d[0])):
		if not (np.isnan(d[0][i])):
			if abs(d[1][i] - mdn) > outlierThreshold:
				d[0][i]=float('nan')
				d[1][i]=float('nan')
				outlierCnt += 1	
	cd0 = d[0][~np.isnan(d[0])]
	cd1 = d[1][~np.isnan(d[1])]
	ottp.Debug('Removed {:d} outliers'.format(outlierCnt))
	return np.array([cd0,cd1]),outlierCnt

# ------------------------------------------
def PlotEventMarkers(ax,rxev,evcol):
	ymin,ymax = ax.get_ylim()
	badEvColour = mplt.colors.to_rgb(evcol)
	# Can't do transparency in postscript so do DIY alpha composite
	# NB now using PNGs so don't need this 
	fgR = badEvColour[0]
	fgG = badEvColour[1]
	fgB = badEvColour[2]
	bgR = 1
	bgG = 1
	bgB = 1
	fgA = 0.3
	bgA = 1
	rA = 1 - (1 -fgA)*(1-bgA)
	rR = fgR * fgA / rA + bgR * bgA * (1 - fgA) / rA
	rG = fgG * fgA / rA + bgG * bgA * (1 - fgA) / rA
	rB = fgB * fgA / rA + bgB * bgA * (1 - fgA) / rA
	rBad = (rR,rG,rB,1)
	for ev in rxev:
		if (ev[1] == 'STEP'):
			tStep = float(ev[0])
			ax.plot([tStep,tStep],[ymin,ymax],color=evcol,linestyle='dashed')
			
		if (ev[1] == 'BAD'):
			tStart = float(ev[0])
			tStop  = float(ev[2])
			ax.add_patch(mplt.patches.Rectangle([tStart,ymin],tStop-tStart,ymax - ymin,facecolor=rBad))
			
# ------------------------------------------

appName = os.path.basename(sys.argv[0])
home = os.environ['HOME']
user =os.environ['USER']
root = home 
configFile = os.path.join(root,'etc','cmprxreport.conf')
tmpDir = './' 
repDir = tmpDir
emailRecipients = ''
emailSender = ''
smtpServer = ''

cggttsTool = '/usr/local/bin/cmpcggtts.py'
mode  = MODE_CMPALL

outlierThreshold = 10.0 # in ns

examples =  'Usage examples:\n'

parser = argparse.ArgumentParser(description='Compare and report stability of multiple receivers (with a common clock) mv via CGGGTS',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog=examples)

parser.add_argument('mjd',nargs = '*',help='first MJD [last MJD]')
parser.add_argument('--ndays',help='number of days (prior to the current MJD) to analyze',default='')

group = parser.add_mutually_exclusive_group()
group.add_argument('--cmpall',help='compute all comparisons (default)',action='store_true')
group.add_argument('--cmpref',help='compute only comparisons with reference receiver',action='store_true')

group = parser.add_mutually_exclusive_group()
group.add_argument('--monthly',help='report for past month',action='store_true')
group.add_argument('--quarterly',help='report for past quarter',action='store_true')
group.add_argument('--yearly',help='report for past year',action='store_true')

parser.add_argument('--config','-c',help='use an alternate configuration file',default=configFile)

parser.add_argument('--display',help='display plots',action='store_true')
parser.add_argument('--report',help='create a report',action='store_true')
parser.add_argument('--email',help='email report',action='store_true')
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--nowarn',help='suppress warnings',action='store_true')
parser.add_argument('--quiet',help='suppress all output to the terminal',action='store_true')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug
ottp.SetDebugging(debug)

if args.display:
	import matplotlib as mplt
	import matplotlib.pyplot as plt
	import matplotlib.ticker as mticker
else:
	import matplotlib as mplt # this stops warnings about being unable to connect to a display
	mplt.use('Agg') 
	import matplotlib.pyplot as plt
	import matplotlib.ticker as mticker
	
configFile = args.config

if (not os.path.isfile(configFile)):
	ottp.ErrorExit(configFile + ' not found')
	
cfg = ottp.Initialise(configFile,['main:receivers','main:codes'])

if 'paths:root' in cfg:
	root = ottp.MakeAbsolutePath(cfg['paths:root'],home)

if 'paths:tmp' in cfg:
	tmpDir = ottp.MakeAbsolutePath(cfg['paths:tmp'],root)

if 'paths:reports' in cfg:
	repDir = ottp.MakeAbsolutePath(cfg['paths:reports'],root)

if 'main:email sender' in cfg:
	emailSender = cfg['main:email sender']

if 'main:email recipients' in cfg:
	emailRecipients = cfg['main:email recipients']

if 'main:smtp server' in cfg:
	smtpServer = cfg['main:smtp server']
	
startMJD = ottp.MJD(time.time()) - 1 # previous day
stopMJD  = startMJD
	
if (args.mjd):
	if 1 == len(args.mjd):
		startMJD = int(args.mjd[0])
		stopMJD  = startMJD
	elif ( 2 == len(args.mjd)):
		startMJD = int(args.mjd[0])
		stopMJD  = int(args.mjd[1])
		if (stopMJD < startMJD):
			ottp.ErrorExit('Stop MJD is before start MJD')
	else:
		ottp.ErrorExit('Too many MJDs')

repInterval = ''
if (args.monthly):
	stopMJD = ottp.MJD(time.time()) - 1
	startMJD = stopMJD - 30
	repInterval = 'last month'
	
if (args.quarterly):
	stopMJD = ottp.MJD(time.time()) - 1
	startMJD = stopMJD - 90
	repInterval = 'last quarter'
	
if (args.yearly):
	stopMJD = ottp.MJD(time.time()) - 1
	startMJD = stopMJD - 365
	repInterval = 'last year'
	
if args. # overrides args.mjd
	currMJD = ottp.MJD(time.time()) - 1
	startMJD = currMJD - 1 - int(args.ndays) - 1
	stopMJD =  currMJD - 1

rx2list = cfg['main:receivers'].split(',')
rx2list = [rx.lower() for rx in rx2list]
rx1list = rx2list # since the default is MODE_CMPALL

if args.cmpall:
	mode = MODE_CMPALL
	rx1list =rx2list  
	
if args.cmpref:
	mode = MODE_CMPREF
	if not('main:reference receiver' in cfg):
		ErrorExit('"Main:reference receiver" is not defined in config')
	rx1list = [cfg['main:reference receiver'].lower()]

ottp.Debug('Running for {:d} - {:d}'.format(startMJD,stopMJD))

codes = cfg['main:codes'].split(',')
codes = [c.lower() for c in codes]

cmd = [cggttsTool]
cmd.append('--delaycal')
cmd.append('--matchephemeris')
cmd.append('--nowarn')
cmd.append('--quiet')
cmd.append('--acceptdelays')
cmd.append('--outputdir')
cmd.append(tmpDir)

rx1evcol = 'tab:green'
rx2evcol = 'tab:purple'

tmpFiles = []
plotFiles = []

for i in range(0,len(rx1list)):
	rx1 = rx1list[i]
	rx1events = []
	
	if (rx1+':events') in cfg:
		ev = cfg[rx1+':events'].split(',')
		for e in ev:
			rx1events.append(e.split())
		
	for j in range (i+1,len(rx2list)): # this skips unwanted permutations
		rx2 = rx2list[j]
		rx2events = []
		
		if (rx2+':events') in cfg:
			ev = cfg[rx2+':events'].split(',')
			for e in ev:
				rx2events.append(e.split())
		
		for c in codes:
			ottp.Debug('Processing {}-{} {} '.format(rx1,rx2,c))
			rx1c = rx1 + ':' + c
			rx2c = rx2 + ':' + c
			if (rx1c in cfg and rx2c in cfg):
				
				rx1Prefix = None
				rx2Prefix = None
				rx1cPath,rx1Prefix = ParseDataSpec(cfg[rx1c])
				rx2cPath,rx2Prefix = ParseDataSpec(cfg[rx2c])
				
				tcmd = copy.deepcopy(cmd)
				tcmd.append(rx1cPath)
				tcmd.append(rx2cPath)
				if rx1Prefix:
					tcmd.append('--refprefix')
					tcmd.append(rx1Prefix)
				if rx2Prefix:
					tcmd.append('--calprefix')
					tcmd.append(rx2Prefix)
					
				tcmd.append(str(startMJD))
				tcmd.append(str(stopMJD))
				
				if debug:
					print(tcmd)
				
				ottp.Debug('Running ' + cggttsTool)
	
				try:
					x = subprocess.check_output(tcmd) # eat the output
				except Exception as e:
					ottp.ErrorExit('Failed to run ' + cggttsTool)
				ottp.Debug(x.decode('utf-8'))
				
				# Output files are 
				# ref.cal.matches.txt
				# ref.cal.av.matches.txt
				# ref.cal.ps

				# but we only want the second one which we will rename so that the files can be used for debugging
				
				# cmppcggtts can produce no output
				matchFile = os.path.join(tmpDir,'ref.cal.av.matches.txt')
				sufficientData = True
				if os.path.isfile(matchFile):
					dataFile  = os.path.join(tmpDir,'{}.{}.{}.av.matches.txt'.format(rx1,rx2,c))
					tmpFiles.append(dataFile)
					os.rename(matchFile,dataFile)
					td = LoadRefCalAvMatches(dataFile)
					sufficientData = len(td[0]) >= 2*TRACKS_PER_DAY/3
					ottp.Debug('Matched tracks = {:d}'.format(len(td[0])))
						else:
					ottp.Debug('No output from cmpcggtts.py')
					sufficientData = False
					
				if not(sufficientData): # not much we can do with that (why 2/3? - so the convolve() for running mean doesn't result in truncation of the output)
					fig,(ax1,ax2)= plt.subplots(2,sharex=False,figsize=(8,11))
					title = rx1.upper() + ' - ' + rx2.upper() + ' ' + c.upper() + 'NO DATA'
					fig.suptitle(title,ha='left',x=0.02,size='medium')	
					plt.savefig(os.path.join(tmpDir,rx1 + '.' + rx2 + '.' + c + '.png'))
					continue
					
				# Now we fix up for known events 
				td = ProcessEvents(td,rx1events, 1)
				td = ProcessEvents(td,rx2events,-1)
				# and then remove outliers, since known steps have been fixed and known bad data removed
				td,nOutliers = RemoveOutliers(td)
				
				# The timescales might be
				# week = 7*80 = 560
				# month = 30 * 80 = 2400 
				# year  = 365 * 80 = 
				
				# Calculate a running mean
				winLen = int(TRACKS_PER_DAY/2)
				mm = np.convolve(td[1],np.ones(winLen),'same')/winLen
				mm[0:winLen]   = None # remove the invalid bits of the convolution
				mm[-winLen:] = None
				
				nTDEV   = int(len(td[0])/3)
				dT = 86400*(td[0][-1] - td[0][0])/len(td[0]) # average time between measurements
				
				(tdtaus, tddevs, tderrors, tdns) = allantools.tdev(td[1],rate = 1.0,taus='all')
				
				fig,(ax1,ax2)= plt.subplots(2,sharex=False,figsize=(8,9))
				plt.subplots_adjust(bottom=0.07,right=0.85)
				title = '{}-{} {}    OUTLIERS = {:d}'.format(rx1.upper(),rx2.upper(),c.upper(),nOutliers) 
				fig.suptitle(title,ha='left',x=0.02,size='medium')
				
				ax1.set_title(r'$\Delta$ REFSYS (unweighted track average)')
				ax1.set_ylabel(r'$\Delta$(ns)')
				ax1.set_xlabel('MJD')
				ax1.plot(td[0],td[1],ls='None',marker='.')
				ax1.plot(td[0],mm)
				ax1.set_xlim([startMJD,stopMJD+1]) # mask out unmwanted events
				PlotEventMarkers(ax1,rx1events,rx1evcol)
				PlotEventMarkers(ax1,rx2events,rx2evcol)
				
				ax2.set_title(r'TDEV of $\Delta$ REFSYS')
				ax2.set_ylabel('TDEV (ns)')
				ax2.set_xlabel('averaging time (s)')
				ax2.loglog(tdtaus*dT,tddevs,'.-')
				
				ax2.yaxis.set_major_formatter(mticker.ScalarFormatter())

				ax2.grid(which='both')
				
				plotFile = os.path.join(tmpDir,rx1 + '.' + rx2 + '.' + c + '.png')
				tmpFiles.append(plotFile)
				plotFiles.append([plotFile,rx1.upper(),rx2.upper(),c.upper()]) # may as well save stuff
				plt.savefig(plotFile)
				

if (args.display):
	plt.show()

if (args.report):
	
	dt = datetime.today()
	yyyy = dt.year
	mm  = dt.month
	dd = dt.day

	html =  '<html>'
	html += '<head></head>'
	html += '<body>'
	html += '<H2>' + 'GNSS receiver common clock comparison report for MJD {:d} to {:d}'.format(startMJD,stopMJD) + '</H2>'
	html += '<br>'
	html += 'Notes:<br>'
	html += '<ol>'
	html += '<li>Running average (in orange) is over 12 hours.</li>'
	html += '<li>Outliers greater than {:g} ns from the median have been removed.</li>'.format(outlierThreshold)
	html += '<li>Known  steps have been fixed using event data.</li>'
	html += '<li>Events are marked with green dashed lines for the first receiver, and with purple dashed lines for the second receiver.</li>'
	html += '<li>Excluded data are marked as shaded rectangles, in lighter shades of green and purple.</li>'
	html += '</ol>'
	html += '<br> <em>Generated by ' + appName + ' ' + VERSION + ' (' + user + '@' + socket.gethostname() + ' </em>)</br>\n' 
	
	# Add the plots
	for pf in plotFiles:
		plotCID = '{}{}{}'.format(pf[1],pf[2],pf[3])
		plotAltText = 'Receivers {}-{} signal {}'.format(pf[1],pf[2],pf[3])
		html += '<img src="cid:' + plotCID  +'" alt="' + plotAltText + '">'
	
	html += '</body>'
	html += '</html>'
	
	repBody = html
	# Hacky fixup - replace CIDs with references to image files for generating the report
	for pf in plotFiles:
		plotCID = '{}{}{}'.format(pf[1],pf[2],pf[3])
		repBody = repBody.replace('cid:'+plotCID,pf[0])
	repName = 'rep.{:d}{:02d}{:02d}.{:d}{:d}'.format(yyyy,mm,dd,startMJD,stopMJD)
	repHTML = os.path.join(tmpDir,repName + '.html')
	repPDF  = os.path.join(repDir,repName + '.pdf')
	fout = open(repHTML,'w')
	fout.write(repBody)
	fout.close()
	
	# Create a pdf from html
	cmd = '/usr/bin/htmldoc --quiet --webpage -f ' + repPDF + ' ' + repHTML
	os.system(cmd)

if args.email and args.report:
	

	msg = MIMEMultipart('related')
	msg['Subject'] = '[REPORT] GNSS receiver time-transfer performance for {} (MJD {:d} to {:d})'.format(repInterval,startMJD,stopMJD) 
	msg['From'] = emailSender
	msg['To'] = emailRecipients
	msg['Reply-To'] = emailSender

	body = MIMEText(html,'html')
	msg.attach(body)

	for pf in plotFiles:
		fp = open(pf[0], 'rb')
		msgImage = MIMEImage(fp.read())
		fp.close()
		plotCID = '{}{}{}'.format(pf[1],pf[2],pf[3])
		msgImage.add_header('Content-ID', '<'+plotCID+'>')
		msg.attach(msgImage)

	# Send the message via local SMTP server.
	try:
		s = smtplib.SMTP(smtpServer)
	except Exception as e:
		print('Exception:',e)
		ottp.ErrorExit('Unable to connect to ' + smtpServer)
			
	s.sendmail(emailSender,emailRecipients.split(','), msg.as_string())
	s.quit()

	
if not(debug):
	tmpFiles.append(os.path.join(tmpDir,'ref.cal.matches.txt'))
	tmpFiles.append(os.path.join(tmpDir,'ref.cal.ps'))
	for f in tmpFiles:
		os.unlink(f)
	if args.report:
		os.unlink(repHTML)
