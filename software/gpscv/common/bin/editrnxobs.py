#!/usr/bin/python3
#

#
# The MIT License (MIT)
#
# Copyright (c) 2018 Michael J. Wouters
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
# Manipulates RINEX observation files


import argparse
import datetime
import math
import os
import re
import shutil
import subprocess
import sys
import time

VERSION = "2.1.0"
AUTHORS = "Michael Wouters"

# Compression algorithms

C_HATANAKA = 0x01
C_GZIP     = 0x02
C_BZIP2    = 0x04
C_COMPRESS = 0x08

# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ------------------------------------------
def ErrorExit(msg):
	print(msg)
	sys.exit(0)

# ------------------------------------------
def IsMJD(txt):
	return re.match(r'\d{5}',txt)

# ------------------------------------------
def Decompress(fin):
	
	algo = 0x00
	
	fBase,ext = os.path.splitext(fin)
	
	if not ext:
		return [fin,'','',algo]
	
	fout = fBase
	cmd = []
	
	if ext.lower() == '.gz':
		algo = C_GZIP
		cmd = ['gunzip',fin]
	elif ext.lower() == '.bz2':
		algo = C_BZIP2
		cmd = ['bunzip2',fin] 
	elif ext.lower() == '.z':
		algo = C_COMPRESS     # can do this with gunzip
		cmd = ['gunzip',fin]
	elif ext.lower() == '.crx' or re.match(r'\.\d{2}d',ext.lower()):
		algo = C_HATANAKA
		cmd  = ['CRX2RNX',fin,'-d'] # delete input file
		if ext == '.crx':
			fout  = fBase + '.rnx'
		elif ext == '.CRX':
			fout  = fBase + '.RNX'
		else:
			m = re.match(r'\.(\d{2})[dD]',ext)
			if m:
				fout = fBase + '.' + m.group(1)
				if ext[3] == 'd':
					fout += 'o'
				else:
					fout += 'O'
	else: # nothing to do, hopefully
		return [fin,algo]
	
	# and do it
	
	Debug('Decompressing {} (ext {})'.format(fin,ext))
	try:
		x = subprocess.check_output(cmd) # eat the output
	except Exception as e:
		ErrorExit('Failed to run ')
	print('Done')
	
	# CRX2RNX renames .crx to .rnx (and .CRX to .RNX)
	# CRX2RNX renames .xxD to .xxO (and .xxd to .xxo)
	
	# and check for Hatanaka again
	ext2=''
	fBase2,ext2 = os.path.splitext(fBase)
	if ext2.lower() == '.crx' or re.match(r'\.\d{2}d',ext2.lower()): 
		
		algo |= C_HATANAKA
		cmd = ['CRX2RNX',fBase,'-d'] # delete input file
		Debug('Decompressing {} format {}'.format(fBase,ext2))
		
		try:
			x = subprocess.check_output(cmd) # eat the output
		except Exception as e:
			ErrorExit('Failed to run ')
		
		if ext2 == '.crx':
			fout  = fBase2 + '.rnx'
		elif ext2 == '.CRX':
			fout  = fBase2 + '.RNX'
		else:
			m = re.match(r'\.(\d{2})[dD]',ext2)
			if m:
				fout = fBase2 + '.' + m.group(1)
				if ext2[3] == 'd':
					fout += 'o'
				else:
					fout += 'O'

	return [fout,algo] 


# ------------------------------------------
def Compress(fin,fOriginal,algo):
	
	if not algo:
		return
			
	fBase,ext = os.path.splitext(fin)
	
	if algo & C_HATANAKA: # HATANAKA first
		cmd = ['RNX2CRX',fin,'-d'] # delete input file
		Debug('Compressing (Hatanaka) {}'.format(fin))
		try:
			x = subprocess.check_output(cmd) # eat the output
		except Exception as e:
			ErrorExit('Failed to run ')
		
		if ext== '.rnx':
			fin  = fBase + '.crx'
		elif ext == '.RNX':
			fin = fBase + '.CRX'
		else:
			m = re.match(r'\.(\d{2})[oO]',ext)
			if m:
				fin = fBase + '.' + m.group(1)
				if ext[3] == 'o':
					fin += 'd'
				else:
					fin += 'D'
		
	if (algo & C_GZIP):
		cmd = ['gzip',fin]
		ext = '.gz'
	elif (algo & C_BZIP2):
		cmd = ['bzip2',fin] 
		ext = '.bz2'
	elif (algo & C_COMPRESS):
		pass
	
	Debug('Compressing {}'.format(fin))
	try:
		x = subprocess.check_output(cmd) # eat the output
	except Exception as e:
		ErrorExit('Failed to run ')
	
	fin += ext
	
	if not(fin == fOriginal): # Rename the file if necessary (to fix up case of file extensions)
		Debug('{} <- {}'.format(fOriginal,fin))
		os.rename(fin,fOriginal)
	
	return 
	
# ------------------------------------------
def ParseRINEXFileName(fname):
	ver = 0
	p = os.path.dirname(fname)
	match = re.search('(\w{4})(\d{3})0\.(\d{2})([oOdD]|[oOdD].\w{2,3})$',fname) # version 2 [dD] for Hatanaka compression
	if match:
		st = match.group(1)
		doy = int(match.group(2))
		yy = int(match.group(3))
		yyyy = yy
		if (yyyy > 80):
			yyyy += 1900
		else:
			yyyy += 2000
		ext = match.group(4)
		ver=2
		return (p,ver,st,doy,yy,yyyy,ext,'','','','','')
	
	match = re.search('(\w{9})_(\w)_(\d{4})(\d{3})(\d{4})_(\w{3})_(\w{3})_(\w{2})\.(\w{3}|\w{3}.\w{2,3})$',fname) # version 3
	if match:
		st = match.group(1)
		dataSource = match.group(2)
		yy = int(match.group(3))
		yyyy=yy
		doy = int(match.group(4))
		hhmm = match.group(5)
		filePeriod=match.group(6)
		dataFrequency=match.group(7)
		ft = match.group(8)
		ext = match.group(9)
		ver=3
		return (p,ver,st,doy,yy,yyyy,ext,dataSource,hhmm,filePeriod,dataFrequency,ft)
	
	ErrorExit(fname + ' is not a standard RINEX file name')
	
# ------------------------------------------
def ReadHeader(fin):
	# The header is returned as a list of strings,with line terminators retained
	hdr=[]
	readingHeader = True
	while readingHeader:
		l = fin.readline()
		hdr.append(l)
		#if (l.find('TIME OF LAST OBS') > 0):
		#	lastObs=l # extract time system
		if (l.find('END OF HEADER') > 0):
			readingHeader = False
	return hdr

# ------------------------------------------
def UpdateHeader(hdr,nsv):
	
	newHeader=[]
	i=0
	while i < len(hdr):
		if args.excludegnss:
			if 'SYS / # / OBS TYPES' in hdr[i]:
				if hdr[i][0] in args.excludegnss: # may have continuation lines
					nsats = hdr[i][3:6].strip()
					if nsats:
						ncontinuation = int(math.ceil(int(nsats)/13)) - 1
					else:
						ncontinuation = 0
					i = i + 1 + ncontinuation
					continue
			if  'SYS / PHASE SHIFT' in hdr[i]:
				if hdr[i][0] in args.excludegnss: # may have continuation lines
					nsats = hdr[i][15:17].strip()
					if nsats:
						ncontinuation = int(math.ceil(int(nsats)/10)) - 1
					else:
						ncontinuation = 0
					i = i + 1 + ncontinuation
					continue
			if '# OF SATELLITES' in hdr[i]:
				newHeader.append('{:6d}{:54}{:20}\n'.format(nsv,' ','# OF SATELLITES'))
				i = i + 1
				continue
			if 'GLONASS' in hdr[i][60:-1]:
				if 'R' in args.excludegnss:
					i = i + 1
					continue
		newHeader.append(hdr[i])
		i = i + 1
		
	return newHeader

# ------------------------------------------
def WriteTmpHeaderFile(tmpHeaderFile,hdr):
	try:
		fout = open(tmpHeaderFile,'w')
	except:
		ErrorExit('Unable to create temporary file ' + tmpHeaderFile)
		
	for l in hdr:
		fout.write(l)
	fout.close()
	
# ------------------------------------------ 
# Note that this will return multiple lines
#
def GetHeaderField(hdr,key):
	return [ l[0:60] for l in hdr if (l.find(key) == 60)]

# ------------------------------------------
# Note that newValue needs to correctly formatted

def ReplaceHeaderField(hdr,key,newValue):
	for li,l in enumerate(hdr):
		if (l.find(key) == 60):
			hdr[li]='{:<60}{:<20}\n'.format(newValue,key) # since other stuff assumes retention of the newline
			break

# ------------------------------------------
def AddHeaderComments(hdr,comments):
	# hdr and comments are lists
	# comments are inserted before any existing comments
	for li,l in enumerate(hdr):
		if (l.find('PGM / RUN BY / DATE') == 60):
			for ci,c in enumerate(comments):
				hdr.insert(li+1+ci,'{:<60}{:<20}\n'.format(c,'COMMENT'));
			break
		
# -------------------------------------------
def GetRinexVersion(hdr):
	vMajor = None
	vMinor = None
	hdrField = GetHeaderField(hdr,'RINEX VERSION / TYPE')
	if hdrField:
		match = re.search('(\d+)\.(\d+)',hdrField[0][0:9])
		if match:
			vMajor = int(match.group(1))
			vMinor = int(match.group(2))
			Debug('GetRinexVersion {:d}.{:02d}'.format(vMajor,vMinor))
	return [vMajor,vMinor]
	

# ------------------------------------------
def MJDtoRINEXObsName(mjd,template):
	
	fname = ''
	t    = (mjd-40587)*86400.0
	tod  = time.gmtime(t)
	yyyy = tod.tm_year
	yy   = tod.tm_year - int(tod.tm_year/100)*100
	doy  = tod.tm_yday
	
	# Version 2 names are of the format
	# xxxxDDDx.YY[oO]
	m = re.match(r'(\w{4})DDD(\d)\.YY([oO])',template)
	if m:
		fname = '{}{:03d}{}.{:02d}{}'.format(m.group(1),doy,m.group(2),yy,m.group(3))
		return fname
	
	# Next, try V3
	# See the format spec!
	m = re.match('(\w{9})_(\w)_YYYYDDD(\d{4})_(\w{3})_(\w{3})_(\w{2})\.(rnx|RNX)',template)
	if m:
		fname='{}_{}_{:04d}{:03d}{}_{}_{}_{}.{}'.format(m.group(1),m.group(2),yyyy,doy,m.group(3),m.group(4),m.group(5),m.group(6),m.group(7))
		return fname

	return fname

# ------------------------------------------
def WriteRINEXFile(tmpDir,tmpDataFile,tmpHeaderFile,tmpRnxFile,finName):
	
	Debug('Writing RINEX')
	with open(os.path.join(tmpDir,tmpRnxFile),'wb') as fout:
		with open(tmpHeaderFile,'rb') as f:
			fout.write(f.read())
		with open(tmpDataFile,'rb') as f:
			fout.write(f.read())
			
	os.unlink(tmpHeaderFile)
	os.unlink(tmpDataFile)
	
	if finName:
		if args.replace:
			if args.backup: 
				fBackup = finName + '.original'
				shutil.copyfile(finName,fBackup)
				Debug('{} backed up to {}'.format(finName,fBackup))
			shutil.copyfile(tmpRnxFile,finName)
		else:
			shutil.copyfile(tmpRnxFile,os.path.join(args.output,os.path.basename(finName)))
	
	if args.output:
		shutil.copy(tmpRnxFile,args.output)
		os.unlink(tmpRnxFile)
	
	Debug('Writing RINEX done')
		
# ------------------------------------------
# Main

appName= os.path.basename(sys.argv[0])+ ' ' + VERSION

examples =  'Usage examples\n'
examples += '  \n'

parser = argparse.ArgumentParser(description='Edit a V3 RINEX observation file',
	formatter_class=argparse.RawDescriptionHelpFormatter,epilog = examples)

parser.add_argument('infile',nargs='+',help='input file or MJD',type=str)

parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')

parser.add_argument('--catenate','-c',help='catenate input files',action='store_true')
parser.add_argument('--excludegnss','-x',help='remove specified GNSS (BEGRJI)',default='')
parser.add_argument('--fixmissing','-f',help='fix missing observations due to UTC/GPS day rollover mismatch',action='store_true')

parser.add_argument('--template',help='template for RINEX file names',default='')
parser.add_argument('--obsdir',help='RINEX file directory',default='./')
parser.add_argument('--tmpdir',help='directory for temporary files',default='./')

group = parser.add_mutually_exclusive_group()
group.add_argument('--output','-o',help='output to file/directory',default='')
group.add_argument('--replace','-r',help='replace edited file',action='store_true')

parser.add_argument('--backup','-b',help='create backup of edited file',action='store_true')

parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

# Check arguments
if not(args.catenate or args.excludegnss):
	ErrorExit('Nothing to do!')

tmpDir =args.tmpdir
tmpDataFile =   os.path.join(tmpDir,'rnxmeas.tmp')
tmpHeaderFile = os.path.join(tmpDir,'rnxhdr.tmp')
tmpRnxFile = os.path.join(tmpDir,'rnx.tmp')

infiles = []

# Create a list of input files to process

# First, check for MJDs in the 'file list'
if (1==len(args.infile)):
	if IsMJD(args.infile[0]):
		mjdStart = int(args.infile[0])
		infiles.append(str(mjdStart)) # we will process MJDs later
		if args.fixmissing:
			mjdStop = mjdStart
			mjdStart = mjdStart -1
			infiles = [mjdStart] + infiles
elif (2==len(args.infile)):
	if IsMJD(args.infile[0]):
		mjdStart = int(args.infile[0])
		mjdStop = int(args.infile[1])
		if args.fixmissing:
			mjdStart -= 1
		if (mjdStop < mjdStart):
			ErrorExit('Stop MJD is before Start MJD')
		for m in range(mjdStart,mjdStop+1):
			infiles.append(str(m))
else:
	ErrorExit('Too many files!')
	
if infiles:
	if not(args.template):
		ErrorExit('You need to define a template for the RINEX file names (--template)')
	if not(MJDtoRINEXObsName(60000,args.template)):
		ErrorExit('Bad --template')
	for i in range(0,len(infiles)):
		fName = MJDtoRINEXObsName(int(infiles[i]),args.template)
		infiles[i] = os.path.join(args.obsdir,fName)
		print(infiles[i])

# No ? Then check for a file sequence
if not(infiles):
	if (1==len(args.infile)):
		infiles.append(os.path.join(args.obsdir,args.infile[0]))
		if args.fixmissing:
			(path1,ver1,st1,doy1,yy1,yyyy1,ext1,dataSource1,hhmm1,filePeriod1,dataFrequency1,ft1)=ParseRINEXFileName(args.infile[0])
			date1=datetime.datetime(yyyy1, 1, 1) + datetime.timedelta(doy1 - 1)
			
	elif (2==len(args.infile)):
		
		(path1,ver1,st1,doy1,yy1,yyyy1,ext1,dataSource1,hhmm1,filePeriod1,dataFrequency1,ft1)=ParseRINEXFileName(args.infile[0]) # version here is naming convention
		(path2,ver2,st2,doy2,yy2,yyyy2,ext2,dataSource2,hhmm2,filePeriod2,dataFrequency2,ft2)=ParseRINEXFileName(args.infile[1])
		if not(path1 == path2):
			ErrorExit('The files must be in the same directory for the --sequence/--fixmissing  option\n')
		if not(ver1 == ver2):
			ErrorExit('The RINEX files must have the same naming convention for the --sequence/--fixmissing option\n')
		if not(st1==st2):
			ErrorExit('The station names must match with the --sequence/--fixmissing option\n')
		if not(ft1==ft2):
			ErrorExit('The file types must match with the --sequence/--fixmissing option\n')
			
		if ((yyyy1 > yyyy2) or (yyyy1 == yyyy2 and doy1 > doy2)):
			ErrorExit('The files appear to be in the wrong order for the --sequence/--fixmissing option\n')
			
		# it appears we have a valid sequence so generate it
		if args.fixmissing:
			date1=datetime.datetime(yyyy1, 1, 1) + datetime.timedelta(doy1 - 2)
		else:
			date1=datetime.datetime(yyyy1, 1, 1) + datetime.timedelta(doy1 - 1)
		date2=datetime.datetime(yyyy2, 1, 1) + datetime.timedelta(doy2 - 1)
		td =  date2-date1
		
		obsdir = path1
		if args.obsdir:
			obsdir = args.obsdir
			
		for d in range(0,td.days+1):
			ddate = date1 +  datetime.timedelta(d)
			if (ver1 == 2):
				yystr = ddate.strftime('%y')
				doystr=ddate.strftime('%j')
				fname = '{}{}0.{}{}'.format(st1,doystr,yystr,ext1) 
				infiles.append(os.path.join(obsdir,fname))	
			elif (ver1 == 3):
				yystr = ddate.strftime('%Y')
				doystr= ddate.strftime('%j')
				fname = '{}_{}_{}{:>03d}{}_{}_{}_{}.{}'.format(st1,dataSource1,yystr,int(doystr),hhmm1,filePeriod1,dataFrequency1,ft1,ext1) 
				infiles.append(os.path.join(obsdir,fname))
	else:
		ErrorExit('Too many files!')

#if (args.output and len(args.infile) >1 and not(args.catenate)):
	#if not(os.path.isdir(args.output)):
		#sys.stderr.write('The --output option must specify a directory  when there is more than one input file\n')
		#exit()

# The way we handle fixmissing is to catenate the files
# and then disassemble into the individual files. This is a simple way of ensuring that everything is in sequence.
# For this to work, we add the file previous to the first in the nominal sequence
# It will be rewritten as well, with entries belonging to the next day moved to the succeeding day
# Entries after the end of the day in the last file are not touched.

if args.fixmissing:
	args.catenate = True
	
# Preliminary stuff done.

# First, a quick check on RINEX version
fDe,algo = Decompress(infiles[0])
try:
	fin = open(fDe,'r')
except:
	ErrorExit('Unable to open ' + fDe)
	
hdr = ReadHeader(fin)
majorVer,minorVer = GetRinexVersion(hdr)
if (majorVer < 3):
	ErrorExit('RINEX version {:d} detected in {}. Only V3 is supported'.format(majorVer,infiles[0]))
Compress(fDe,infiles[0],algo)


# Now do stuff!
headers = []
svn     = []

if args.catenate: # open the measurement file for output
	try:
		fout = open(tmpDataFile,'w')
	except:
		ErrorExit('Unable to create temporary file ' + tmpDataFile)

compressionJobs =[]

for f in infiles:
	
	finName,algo = Decompress(f)
	compressionJobs.append([finName,f,algo])
	
	try:
		fin = open(finName,'r')
		svn=[]
	except:
		ErrorExit('Unable to open ' + finName)
	
	Debug('Opened ' + finName)
	
	if not(args.catenate): # open file for output of measurements
		try:
			fout = open(tmpDataFile,'w')
		except:
			ErrorExit('Unable to create temporary file ' + tmpDataFile)
			
	hdr = ReadHeader(fin)
	headers.append(hdr)
	
	reading = True
	while reading:
		l = fin.readline()
		
		if (len(l) == 0): #EOF
			reading = False
			continue
		
		rec = []
		if (l[0]=='>'):
			nmeas = int(l[32:36]) # cols 32-35
			epochFlag = int(l[31])
			rec.append(l)
			for m in range(0,nmeas):
				l = fin.readline()
				if epochFlag < 2: # there will be a SV identifier
					svid = l[0:3]
					if args.excludegnss:
						if not(l[0] in args.excludegnss):
							rec.append(l)
							if not(svid in svn): # this is SLOW
								svn.append(svid)
					else:
						rec.append(l)
						if not(svid in svn): # this is SLOW
							svn.append(svid)
				else:
					rec.append(l)
		else:
			pass # shouldn't happen I think FIXME should test!
		
		# Now output the measurement block
		if rec:
			if args.excludegnss: # may have to fix the measurement count
				fout.write(rec[0][0:32] + '{:3d}'.format(len(rec)-1) + rec[0][36:-1] + '\n')
			else:
				fout.write(rec[0])
			for i in range(1,len(rec)):
				fout.write(rec[i])
	
	if not(args.catenate): # writing individual files ...
		fout.close()
		newHdr = UpdateHeader(hdr,len(svn))
		AddHeaderComments(newHdr,['Processed by {}'.format(appName)])
		WriteTmpHeaderFile(tmpHeaderFile,newHdr)
		WriteRINEXFile(tmpDir,tmpDataFile,tmpHeaderFile,tmpRnxFile,finName)

if args.catenate:
	fout.close() # this is tmpDataFile
	
if (args.catenate and not(args.fixmissing)):
	
	# Write a new header, using the first as a template
	newHdr = UpdateHeader(headers[0],len(svn))
	AddHeaderComments(newHdr,['Processed by {}'.format(appName)])
	# Now we need to update the time of the last observation, but only do it if it's defined
	if GetHeaderField(newHdr,'TIME OF LAST OBS'):
		lastObs = GetHeaderField(headers[-1],'TIME OF LAST OBS') # remember, this returns a list
		ReplaceHeaderField(newHdr,'TIME OF LAST OBS',lastObs[0])
	
	WriteTmpHeaderFile(tmpHeaderFile,newHdr)
	WriteRINEXFile(tmpDir,tmpDataFile,tmpHeaderFile,tmpRnxFile,'')
	
if args.fixmissing:
	
	fin = open(tmpDataFile,'r') # this is catenated data

	rolloverTOD = datetime.datetime(1980,1,6,0,0,0,tzinfo=datetime.timezone.utc) # no data before this !
	skipRead = False
	fileCount = 0
	reading = True
	
	for c in compressionJobs: 
		
		f = c[0] # decompressed file
		svn = [] 
		firstObs = []
		
		tmpDailyDataFile = os.path.join(tmpDir,'rnxdailymeas.{:d}.tmp'.format(fileCount))
		try:
			fout = open(tmpDailyDataFile,'w')
			ftmp = open(f,'r') # we know it exists
			hdr = ReadHeader(ftmp)
			ftmp.close() 
		except:
			ErrorExit('Unable to create temporary file ' + tmpDailyDataFile)
		fileCount += 1
		
		while reading:
		
			if not skipRead:
				l = fin.readline()
			skipRead = False
			if (len(l) == 0): #EOF
				break
			
			if (l[0]=='>'): # process a measurement block
				year = int(l[2:6])
				mon  = int(l[6:10])
				day  = int(l[9:13])
				hours= int(l[12:16])
				mins = int(l[15:19])
				secs = float(l[19:30])
				tod  = datetime.datetime(year,mon,day,hours,mins,int(secs),tzinfo=datetime.timezone.utc)
			
				if not firstObs:
					firstObs = [year,mon,day,hours,mins,secs]
					rolloverTOD = datetime.datetime(year,mon,day,tzinfo=datetime.timezone.utc) + datetime.timedelta(days=1)
		
				if (tod >= rolloverTOD and fileCount < len(infiles)): # time to write!
					#print(tod,rolloverTOD)
					skipRead = True # we want to process this line again for the new file
					break
				
				fout.write(l)
				
				nmeas = int(l[32:36]) # cols 32-35
				epochFlag = int(l[31])
				for m in range(0,nmeas):
					l = fin.readline()
					fout.write(l)
					if epochFlag < 2: # there will be a SV identifier
						svid = l[0:3]
						if not(svid in svn):
							svn.append(svid)
						
				lastObs = [year,mon,day,hours,mins,secs]
				
		fout.close()
		
		newHdr = UpdateHeader(hdr,len(svn))
		AddHeaderComments(newHdr,['Processed by {}'.format(appName)])
		hdrField = GetHeaderField(newHdr,'TIME OF FIRST OBS') # mandatory field
		timeSys = hdrField[0][48:51]
		ReplaceHeaderField(newHdr,'TIME OF FIRST OBS',
				'{:6d}{:6d}{:6d}{:6d}{:6d}{:13.7f}     {}'.format(firstObs[0],firstObs[1],firstObs[2],firstObs[3],firstObs[4],firstObs[5],timeSys))
		
		if GetHeaderField(newHdr,'TIME OF LAST OBS'):
			ReplaceHeaderField(newHdr,'TIME OF LAST OBS',
				'{:6d}{:6d}{:6d}{:6d}{:6d}{:13.7f}     {}'.format(lastObs[0],lastObs[1],lastObs[2],lastObs[3],lastObs[4],lastObs[5],timeSys))
		WriteTmpHeaderFile(tmpHeaderFile,newHdr)
		WriteRINEXFile(tmpDir,tmpDailyDataFile,tmpHeaderFile,tmpRnxFile,f)
		
				
	fin.close()
	
# ... and recompress anything we decompressed
for c in compressionJobs:
	Compress(c[0],c[1],c[2])

	
