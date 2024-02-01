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
# Fetches GNSS products
#

import argparse
import calendar
import magic
import os
import re
import sys
import time
import requests
import pycurl

# A fudge
sys.path.append("/usr/local/lib/python3.6/site-packages")  # Ubuntu 18.04
sys.path.append("/usr/local/lib/python3.8/site-packages")  # Ubuntu 20.04
sys.path.append("/usr/local/lib/python3.10/site-packages") # Ubuntu 22.04

try: 
	import ottplib as ottp
except ImportError:
	sys.exit('ERROR: Must install ottplib\n eg openttp/software/system/installsys.py -i ottplib')

VERSION = "0.10.0"
AUTHORS = "Michael Wouters"

# RINEX V3 constellation identifiers
BEIDOU='C'
GALILEO='E'
GLONASS='R'
GPS='G'
MIXED='M'

# ---------------------------------------------
def DateToMJD(d):
	
	if (re.match('^\d{5}$',d)): # just check the format
		return int(d)
	match = re.match('^\d{4}-\d{1,3}$',d)
	if (match): # YYYY-DOY
		utctod = time.strptime(d,'%Y-%j')
		return ottp.MJD(calendar.timegm(utctod))
	match = re.match('^\d{4}-\d{1,2}-\d{1,2}$',d) # YYYY-MM-DD
	if (match): 
		utctod = time.strptime(d,'%Y-%m-%d')
		return ottp.MJD(calendar.timegm(utctod))
	return -1

# ---------------------------------------------
def GNSStoNavDirectory(gnss):
	if (BEIDOU == gnss): # CN in 'f'
		return 'f'
	elif (GALILEO == gnss): # EN in 'l'
		return 'l'
	elif (GLONASS == gnss): 	# RN in 'g'
		return 'g'
	elif (GPS == gnss): # GN in 'n'
		return 'n'
	elif (MIXED == gnss): # MN in 'p'
		return 'p'

# ---------------------------------------------
def FetchFile(session,url,destination,magicTag):
	
	if url[-1] =='/': # ugly hack to determine if we failed to contrsuct a file name
		return False
	
	if not(args.force):
		if os.path.isfile(destination):
			if os.path.getsize(destination):
				ottp.Debug('File exists! Download skipped, tra-la!')
				return True
		
	ottp.Debug('Downloading '+ url)
	ottp.Debug('Destination = ' + destination)

	r = session.get(url)
	with open(destination,'wb') as fd:
		for chunk in r.iter_content(chunk_size=1000):
			fd.write(chunk)

	fd.close()
	if magicTag in magic.from_file(destination):
		return True
	else:
		ottp.Debug('Bad download - got ' + magic.from_file(destination))
		return False

# ---------------------------------------------
def  IGSBiasFile(prefCentre,product,yyyy,doy):
	fname = ''
	
	# Rapid are CAS/DFZ (2023-), available daily
	# CAS0MGXRAP_20222790000_01D_01D_DCB.BSX.gz
	# GFZ0MGXRAP_20223610000_01D_01D_DCB.BSX.gz
	
	# Final are DLR, available every three months
	# Weekly _07D and daily _01D  files are available
	# DLR0MGXFIN_20220910000_03L_01D_DCB.BSX.gz 
	# # 1, 91, 182, 274
	
	if product == 'rapid':
		fname = '{}0MGXRAP_{:04d}{:03d}0000_01D_01D_DCB.BSX.gz'.format(prefCentre,yyyy,doy)
	else:
		# This needs a bit more work
		if (doy <= 90):
			pass
		elif (doy <= 181): # 182 in 2016
			pass
		elif (doy <= 273): # 274 in 2016
			pass 
		elif (doy <= 366):
			pass
		ottp.Debug('IGS final BSX not yet implemented!')
	return fname

# ---------------------------------------------
def CODEBiasFile(yy,mm):
	fnames = []
	fnames.append('P1C1{:02d}{:02d}.DCB.Z'.format(yy,mm))
	fnames.append('P1P2{:02d}{:02d}.DCB.Z'.format(yy,mm))
	return fnames

# ---------------------------------------------
# Main

home = os.environ['HOME'] + '/';
root = home
configFile = os.path.join(home,'etc/getgnssproducts.conf');

parser = argparse.ArgumentParser(description='Downloads GNSS products',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('start',nargs='?',help='start (MJD/yyyy-doy/yyyy-mm-dd, default = today)',type=str)
parser.add_argument('stop',nargs='?',help='stop (MJD/yyyy-doy/yyyy-mm-dd, default = start)',type=str)

parser.add_argument('--ndays',help='download previous n days',default='0')

parser.add_argument('--config','-c',help='use this configuration file',default=configFile)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--outputdir',help='set output directory')

parser.add_argument('--centre',help='set the data centre',default='cddis')
parser.add_argument('--listcentres','-l',help='list the configured IGS data centres',action='store_true')

# RINEX files
parser.add_argument('--ephemeris',help='get broadcast ephemeris (nb if v2, only the GPS ephemeris is fetched',action='store_true')
parser.add_argument('--observations',help='get station observations',action='store_true')
parser.add_argument('--statid',help='station identifier (eg V3 SYDN00AUS, V2 sydn)')
parser.add_argument('--rinexversion',help='rinex version of station observation')
parser.add_argument('--system',help='gnss system (GLONASS,BEIDOU,GPS,GALILEO,MIXED')

# IGS products
parser.add_argument('--clocks',help='get clock products (.clk)',action='store_true')
parser.add_argument('--orbits',help='get orbit products (.sp3)',action='store_true')
parser.add_argument('--erp',help='get ERP products (.erp)',action='store_true')
parser.add_argument('--bias',help='get differential code bias products (.bsx/.dcb)',action='store_true')
parser.add_argument('--biasformat',help='format of downloaded bias products (BSX/DCB)',default='dcb')

parser.add_argument('--ppp',help='get products needed for PPP',action='store_true')
parser.add_argument('--rapiddir',help='output directory for rapid products',default ='')
parser.add_argument('--finaldir',help='output directory for final products',default ='')
# separate directory for bias products, since IGS does this too
parser.add_argument('--biasdir',help='output directory for bias products',default ='')

parser.add_argument('--force','-f',help='force download, overwriting existing files',action='store_true')

group = parser.add_mutually_exclusive_group()
group.add_argument('--rapid',help='get rapid products',action='store_true')
group.add_argument('--final',help='get final products',action='store_true')

parser.add_argument('--noproxy',help='disable use of proxy server',action='store_true')
parser.add_argument('--proxy',help='set the proxy server (server:port)',type=str)
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug
ottp.SetDebugging(debug)

configFile = args.config

proxy = ''
port = 0
outputdir = './'


if (not os.path.isfile(configFile)):
	ottp.ErrorExit(configFile + ' not found')

cfg=ottp.Initialise(configFile,['main:data centres'])

centres = cfg['main:data centres'].split(',')

if (args.listcentres):	
	print('Available data centres:')
	for c in centres:
		print(c, cfg[c.lower() + ':base url'])
	sys.exit(0)

proxies = {}

if (args.noproxy): #FIXME UNTESTED!!!!
	# FIXME this does not override the default proxy
	
	pass
elif (args.proxy):
	(server,port)=args.proxy.split(':')
	server.strip()
	port.strip()
	proxies['http'] = 'http://{}:{}'.format(server,port)
	proxies['https'] = 'http://{}:{}'.format(server,port)
	
elif('main:proxy server' in cfg):
	server = cfg['main:proxy server']
	port  =  cfg['main:proxy port']
	proxies['http'] = 'http://{}:{}'.format(server,port)
	proxies['https'] = 'http://{}:{}'.format(server,port)

dataCentre = args.centre.lower()
# Check that we've got this
found = False
for c in centres:
	if (c.lower() == dataCentre):
		found=True
		break
if (not found):
	ottp.ErrorExit('The data centre ' + dataCentre + ' is not defined in ' + configFile)

if 'paths:root' in cfg:
	root =  ottp.MakeAbsolutePath(cfg['paths:root'],home)
	print(root)
	
if (args.outputdir):
	outputdir = args.outputdir
elif ('main:output directory' in cfg):
	outputdir = ottp.MakeAbsolutePath(cfg['main:output directory'],root)
	
rapiddir = outputdir
finaldir = outputdir
biasdir  = outputdir

if (args.rapiddir):
	rapiddir = args.rapiddir
elif ('paths:rapid directory' in cfg):
	rapiddir = ottp.MakeAbsolutePath(cfg['paths:rapid directory'],root)

if (args.finaldir):
	finaldir = args.finaldir
elif ('paths:final directory' in cfg):
	finaldir = ottp.MakeAbsolutePath(cfg['paths:final directory'],root)

if (args.biasdir):
	biasdir = args.biasdir
elif ('paths:bias directory' in cfg):
	biasdir = ottp.MakeAbsolutePath(cfg['paths:bias directory'],root)

rnxVersion = 3
if args.rinexversion:
	rnxVersion = int(args.rinexversion)

if (args.statid):
	stationID = args.statid
		
if (args.observations): # station observations
	if not args.statid:
		ottp.ErrorExit('You must define the station identifier (--statid)')

if (args.ephemeris and rnxVersion == 3): # station observations
	if not args.statid:
		ottp.ErrorExit('You must define the station identifier (--statid)')
		
# Defaults for broadcast ephemeris
if (rnxVersion == 2):
	gnss = GPS # actually, this does nothing ...
elif (rnxVersion == 3):
	gnss = MIXED
	
if (rnxVersion == 3 and args.system):
	gnss = args.system.lower()
	if (gnss =='beidou'):
		gnss =BEIDOU
	elif (gnss == 'galileo'):
		gnss = GALILEO
	elif (gnss == 'glonass'):
		gnss = GLONASS
	elif (gnss == 'gps'):
		gnss = GPS
	elif (gnss == 'mixed'):
		gnss = MIXED
	else:
		print('Unknown GNSS system')
		exit()

if args.clocks or args.orbits or args.erp or args.bias or args.ppp:
	if not(args.rapid or args.final):
		if args.bias and args.biasformat == 'bsx':
			ottp.ErrorExit("You need to specify 'rapid' or 'final' products")
		else:
			ottp.ErrorExit("You need to specify 'rapid' or 'final' products")
			
if args.ppp: # configure download of all required products
	args.clocks = True
	args.orbits = True
	args.erp    = True

# Now that we've defined the file types to download, set the MJD range for download
nprev = int(args.ndays)

today = time.time() # save this in case the day rolls over while the script is running
mjdToday = ottp.MJD(today)

# If observations or broadcast ephemeris, at best we can only get yesterday's files
# 
start = mjdToday - 1 - nprev # 
stop  = mjdToday - 1

# For IGS rapid products, the latemcy is 17 hours so subtract another day
if args.rapid:
	start = start - 1
	stop  = stop  - 1
	
if args.final:
	start = start - 14
	stop  = stop  - 14

# Otherwise, the user has defined the download range, and we assume that they know what they're doing :-)
if (args.start):
	start = DateToMJD(args.start)
	if not(args.stop):
		start = start -  nprev
		stop  = start
	
if (args.stop):
	stop = DateToMJD(args.stop)

ottp.Debug('start = {},stop = {} '.format(start,stop))

biasFormat = args.biasformat.lower()
prefBiasCentre = 'CAS'

# Daily data
baseURL = cfg[dataCentre + ':base url']
brdcPath = cfg[dataCentre + ':broadcast ephemeris']
stationDataPath = cfg[dataCentre + ':station data']

# Products
productPath = cfg[dataCentre + ':products']

session = requests.Session()
session.proxies.update(proxies)

for m in range(start,stop+1):
	
	(yyyy,doy,mm) = ottp.MJDtoYYYYDOY(m)
	(GPSWn,GPSday) = ottp.MJDtoGPSWeekDay(m)
	yy = yyyy-100*int(yyyy/100)
	
	ottp.Debug('fetching files for MJD {:d}, Y {:d} DOY {:d}, Wn {:d} Dn {:d}'.format(m,yyyy,doy,GPSWn,GPSday))
	
	# IGS products needed for PPP 
	# CDDIS changed to RINEXV3 style names  after WN 2237
	
	if (args.clocks):
		magicTag = 'gzip compressed'
		
		if (args.rapid):
			dstdir = rapiddir
			if GPSWn > 2237:  
				fname = 'IGS0OPSRAP_{:04d}{:03d}0000_01D_05M_CLK.CLK.gz'.format(yyyy,doy) 
			else:	
				fname = 'igr{:04d}{:1d}.clk.Z'.format(GPSWn,GPSday) # 5 minute clocks
				magicTag = 'compress\'d'
		elif (args.final):
			dstdir = finaldir
			if GPSWn > 2237:
				fname = 'IGS0OPSFIN_{:04d}{:03d}0000_01D_05M_CLK.CLK.gz'.format(yyyy,doy) 
			else:
				fname = 'igs{:04d}{:1d}.clk.Z'.format(GPSWn,GPSday)
				magicTag = 'compress\'d'
				
		url = '{}/{}/{:04d}/{}'.format(baseURL,productPath,GPSWn,fname)
		FetchFile(session,url,'{}/{}'.format(dstdir,fname),magicTag)
		
	if (args.orbits):
		magicTag = 'gzip compressed'
		
		if (args.rapid):
			dstdir = rapiddir
			if GPSWn > 2237:
				fname = 'IGS0OPSRAP_{:04d}{:03d}0000_01D_15M_ORB.SP3.gz'.format(yyyy,doy)
			else:
				fname = 'igr{:04d}{:1d}.sp3.Z'.format(GPSWn,GPSday)
				magicTag = 'compress\'d'
		elif (args.final):
			dstdir = finaldir
			if GPSWn > 2237:
				fname = 'IGS0OPSFIN_{:04d}{:03d}0000_01D_15M_ORB.SP3.gz'.format(yyyy,doy)
			else:
				fname = 'igs{:04d}{:1d}.sp3.Z'.format(GPSWn,GPSday)
				magicTag = 'compress\'d'
				
		url = '{}/{}/{:04d}/{}'.format(baseURL,productPath,GPSWn,fname)
		FetchFile(session,url,'{}/{}'.format(dstdir,fname),magicTag)
	
	if (args.erp):
		magicTag = 'gzip compressed'
		
		if (args.rapid): # published each day
			dstdir = rapiddir
			if GPSWn > 2237:
				fname = 'IGS0OPSRAP_{:04d}{:03d}0000_01D_01D_ERP.ERP.gz'.format(yyyy,doy)
			else:
				fname = 'igr{:04d}{:1d}.erp.Z'.format(GPSWn,GPSday)
				magicTag = 'compress\'d'
		elif (args.final):
			dstdir = finaldir
			if GPSWn > 2237: # published for first day of GPS week
				(tmpyyyy,tmpdoy) = ottp.MJDtoYYYYDOY(m - GPSday)
				fname = 'IGS0OPSFIN_{:04d}{:03d}0000_07D_01D_ERP.ERP.gz'.format(tmpyyyy,tmpdoy)
			else:
				fname = 'igs{:04d}{:1d}.erp.Z'.format(GPSWn,7) # published at end of week (GPSday == 7)
				magicTag = 'compress\'d'
				
		url = '{}/{}/{:04d}/{}'.format(baseURL,productPath,GPSWn,fname)
		FetchFile(session,url,'{}/{}'.format(dstdir,fname),magicTag)
		
	if (args.bias):
		
		if biasFormat== 'bsx': # these are downloaded from the IGS global centre
			fname = IGSBiasFile(prefBiasCentre,'rapid' if args.rapid  else 'final',yyyy,doy)
			url = '{}/{}/{:04d}/{}'.format(baseURL,cfg[dataCentre + ':bias'],yyyy,fname)
			FetchFile(session,url,'{}/{}'.format(biasdir,fname),'gzip compressed')
		elif biasFormat == 'dcb':
			prevmm = mm - 1
			prevyy = yy
			if prevmm == 0:
				prevmm = 12
				prevyy = prevyy -1
			fnames = CODEBiasFile(prevyy,prevmm)
			for f in fnames:
				url = '{}/{:04d}/{}'.format(cfg['bias:code'],yyyy,f)
				FetchFile(session,url,'{}/{}'.format(biasdir,f),'compress\'d')
				
	# Miscellanea - IGS broadcast ephemeris
	if (args.ephemeris):
		magicTag = 'gzip compressed'
		if (rnxVersion == 2): # GPS only!
			
			brdcName = 'brdc'
			if (args.statid):
				brdcName = args.statid
			fname = '{}{:03d}0.{:02d}n.Z'.format(brdcName,doy,yy)
			if brdcName == 'brdc':
				fname = '{}{:03d}0.{:02d}n.gz'.format(brdcName,doy,yy)
				url = '{}/{}/{:04d}/brdc/{}'.format(baseURL,brdcPath,yyyy,fname)
			else:  
				url = '{}/{}/{:04d}/{:03d}/{:02d}n/{}'.format(baseURL,brdcPath,yyyy,doy,yy,fname)
				magicTag = 'compress\'d'
				
			FetchFile(session,url,'{}/{}'.format(outputdir,fname),magicTag)
		
		elif (rnxVersion == 3):
			fname = '{}_R_{:04d}{:03d}0000_01D_{}N.rnx.gz'.format(stationID,yyyy,doy,gnss)
			yy = yyyy-100*int(yyyy/100)
			url = '{}/{}/{:04d}/{:03d}/{:02d}{}/{}'.format(baseURL,stationDataPath,yyyy,doy,yy,
				GNSStoNavDirectory(gnss),fname)
			
			FetchFile(session,url,'{}/{}'.format(outputdir,fname),magicTag)
	
	# Miscellanea - station observations
	if (args.observations):
		magicTag = 'gzip compressed'
		gnss = MIXED # FIXME maybe
		if (rnxVersion == 2):
			if (gnss == MIXED):
				yy = yyyy-100*int(yyyy/100)
				fname = '{}{:03d}0.{}o.Z'.format(stationID,doy,yy)
				url = '{}/{}/{:04d}/{:03d}/{:02d}o/{}'.format(baseURL,stationDataPath,yyyy,doy,yy,fname)
				FetchFile(session,url,'{}/{}'.format(outputdir,fname),magicTag)
			else:
				print('Warning: only mixed observation files are downloaded - skipping ...')
		elif (rnxVersion == 3):
			if (gnss == MIXED):
				fname = '{}_R_{:04d}{:03d}0000_01D_30S_{}O.crx.gz'.format(stationID,yyyy,doy,gnss)
				# MO in 'd'
				yy = yyyy-100*int(yyyy/100)
				url = '{}/{}/{:04d}/{:03d}/{:02d}d/{}'.format(baseURL,stationDataPath,yyyy,doy,yy,fname)
				FetchFile(session,url,'{}/{}'.format(outputdir,fname),magicTag)
			else:
				print('Warning: only mixed observation files are downloaded - skipping ...')

ottp.Debug('Downloads completed!')

# All  done!!!
