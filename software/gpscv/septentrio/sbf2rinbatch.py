#!/usr/bin/python3
# Example
# sbf2rinbatch.py --debug --both --station MOS1 --outputdir ~/RINEX 59292 59292
import argparse
import datetime
import os
import subprocess
import sys
import time

SBF2RIN = '~/bin/sbf2rin'

VERSION = "0.0.1"
AUTHORS = "Michael Wouters"

# ------------------------------------------
def ShowVersion():
	print(os.path.basename(sys.argv[0])," ",VERSION)
	print("Written by",AUTHORS)
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ---------------------------------------------
def MJDtoYYYYDOY(mjd):
	tt = (mjd - 40587)*86400
	utctod = time.gmtime(tt)
	return (utctod.tm_year,utctod.tm_yday)
      
# ---------------------------------------------
def DecompressFile(basename,ext):
	if (ext == '.gz'):
		subprocess.check_output(['gunzip',basename + ext])
		Debug('Decompressed ' + basename)
	
# ---------------------------------------------
def RecompressFile(basename,ext):
	if (ext == '.gz'):
		subprocess.check_output(['gzip',basename])
		Debug('Recompressed ' + basename)


# ---------------------------------------------

home =os.environ['HOME']

parser = argparse.ArgumentParser(description='Batch process Septentrio SBF log files (V2 filenames only at present\n)' +
  'Example:\n' +
  'sbf2rinbatch.py --debug --rinexversion 3 --observation --outputdir tmp --station MOS1 --rawdir /mnt/gnss1/mosaict/raw/ 59291 59291' 
)
parser.add_argument('firstMJD',help='first mjd')
parser.add_argument('lastMJD',help='last mjd')
parser.add_argument('--station',help='station name ',default='SEPX')
parser.add_argument('--exclusions',help='excluded gnss',default='ERSCJI')
parser.add_argument('--rawdir',help='directory containing SBF files (full path)')
parser.add_argument('--outputdir',help='output directory (full path)')
parser.add_argument('--rinexversion',help='rinex version',default='2')
parser.add_argument('--interval',help='rinex version',default='30')
parser.add_argument('--extension',help='sbf file extension',default='rx')
parser.add_argument('--navigation',help='generate navigation file',action='store_true')
parser.add_argument('--observation',help='generate observation file',action='store_true')
parser.add_argument('--both',help='generate navigation and observation files',action='store_true')

parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
parser.add_argument('--version','-v',help='show version and exit',action='store_true')

args = parser.parse_args()

debug = args.debug

if (args.version):
	ShowVersion()
	exit()

rawdir = home + '/raw/'
outputdir = './'

if args.rawdir:
	rawdir = args.rawdir

if args.outputdir:
	outputdir = args.outputdir
	
firstMJD=int(args.firstMJD)
lastMJD=int(args.lastMJD)

nav = False
obs = False
nav = args.navigation
obs = args.observation
if (args.both):
  nav = True
  obs = True
 
for mjd in range(firstMJD,lastMJD+1):
	(yyyy,doy) = MJDtoYYYYDOY(mjd)
	yy = yyyy-int(yyyy/100)*100
	fin = rawdir + str(mjd) + '.' + args.extension
	recompress  = False
	if not(os.path.exists(fin)):
		if (os.path.exists(fin + '.gz')):
			DecompressFile(fin,'.gz')
			recompress = True
		else:
			sys.stderr.write(fin + ' is missing\n')
			continue
	if obs:
		fout = outputdir + '/' + '{}{:03d}0.{:02d}O'.format(args.station,doy,yy)
		cmd = SBF2RIN + ' -f ' + fin + ' -o ' + fout + ' -R' + args.rinexversion + ' -i' + args.interval + ' -x' + args.exclusions
		Debug(cmd)
		subprocess.check_call(cmd,shell=True)
	if nav:
		fout = outputdir + '/' + '{}{:03d}0.{:02d}N'.format(args.station,doy,yy)
		cmd = SBF2RIN + ' -f ' + fin + ' -o ' + fout + ' -R' + args.rinexversion + ' -nN' + ' -x' + args.exclusions
		Debug(cmd)
		subprocess.check_call(cmd,shell=True)
	if recompress:
		RecompressFile(fin,'.gz')
