#!/usr/bin/python
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
# In particular it can:
#   fix ms ambiguities given a reference file
#
# Output is written to stdout
# Example:
# editrnxobs.py -o MNAM3180.18O.fixed --fixms --obstype C2I --refrinex ../../septentrio2/rinex/SEP23180.18O --system beidou -d MNAM3180.18O

import argparse
import datetime
import os
import re
import sys
import time

VERSION = "0.1.3"
AUTHORS = "Michael Wouters"

BEIDOU='C'
GALILEO='E'
GLONASS='R'
GPS='G'

CLIGHT = 299792458.0

# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return

# ------------------------------------------
def ErrorExit(msg):
	print msg
	sys.exit(0)

# ------------------------------------------
def ParseRINEXFileName(fname):
	ver = 0
	p = os.path.dirname(fname)
	match = re.search('(\w{4})(\d{3})0\.(\d{2})([oO])',fname) # version 2
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
	
	match = re.search('(\w{9})_(\w)_(\d{4})(\d{3})(\d{4})_(\w{3})_(\w{3})_(\w{2})\.(\w{2,3})',fname) # version 3
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
	
	return (p,ver,'',0,0,'') 
	
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
def WriteHeader(fout,hdr):
	for l in hdr:
		fout.write(l)

# ------------------------------------------
def GetHeaderField(hdr,key):
	return [ l for l in hdr if (l.find(key) == 60)]

# ------------------------------------------
def EditHeader(hdr,key,newValue):
	for li,l in enumerate(hdr):
		if (l.find(key) == 60):
			hdr[li]=newValue
			break
		
# ------------------------------------------
def AddHeaderComments(hdr,comments):
	# hdr and comments are lists
	# comments are inserted before any existing comments
	for li,l in enumerate(hdr):
		if (l.find('PGM / RUN BY / DATE') == 60):
			for ci,c in enumerate(comments):
				hdr.insert(li+1+ci,'{:60}{:20}\n'.format(c,'COMMENT'));
			break

# ------------------------------------------
def WriteObservations(fout,obs):
	for o in obs:
		for l in o[2]:
			fout.write(l)
		for l in o[1]:
			fout.write(l)

# ------------------------------------------
def ReadV2Observations(fin,nobs):
	reading = True
	obs=[]
	while (reading):
		r = ReadV2Record(fin,nobs)
		if (not (r == None)):
			obs.append(r)
		else:
			reading = False
	Debug('Read ' + str(int(len(obs))) + ' observations')
	return obs

# ------------------------------------------
def ReadV2Record(fin,nobs):
	comments=''
	while True:
		l = fin.readline()
		if (len(l) == 0): #EOF
			return None
		#                    YY      MM      DD      HH      MM       SS.SSSSSS         num sats
		match = re.match('\s(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+\.\d+)\s+\d+\s+(\d+)',l)
		if (match): # got a record header
			# print match.group(4), match.group(5),match.group(6)
			yy = int(match.group(1))
			yyyy = yy
			if (yyyy > 80):
				yyyy += 1900
			else:
				yyyy += 2000
			tod = datetime.datetime(yyyy,int(match.group(2)),int(match.group(3)),
				int(match.group(4)),int(match.group(5)),int(float(match.group(6))))
			nsv = int(match.group(7))
			rec=[]
			rec.append(l)
			#  if nsv > 12, there are more SV identifiesr to read
			ntodo = nsv - 12
			while (ntodo > 0):
				l=fin.readline()
				rec.append(l)
				ntodo -= 12
			svcnt = 0
			while (svcnt < nsv): # assuming here that there are no blanks lines
				l = fin.readline()
				rec.append(l)
				ntodo = nobs - 5
				while (ntodo > 0):
					l = fin.readline()
					ntodo -= 5
				svcnt += 1
			return (tod,rec,comments)
		else:
			comments += l
	return (None)

# ------------------------------------------
def ReadV3Observations(fin):
	reading = True
	obs=[]
	while (reading):
		r = ReadV3Rec(fin)
		if (not (r == None)):
			obs.append(r)
		else:
			reading = False
	Debug('Read ' + str(int(len(obs))) + ' observations')
	return obs

# ------------------------------------------
def ReadV3Rec(fin):
	comments=''
	while True:
		l = fin.readline()
		if (len(l) == 0): #EOF
			return None
		if (l[0]=='>'):
			year = int(l[2:6])
			mon = int(l[6:10])
			day = int(l[9:13])
			hours= int(l[12:16])
			mins = int(l[15:19])
			secs = float(l[19:30])
			tod = datetime.datetime(year,mon,day,hours,mins,int(secs)) # FIXME whatabout non-integer secs - use microseconds field ?
			nmeas = int(l[32:36]) # cols 32-35
			rec=[]
			for m in range(0,nmeas):
				l=fin.readline()
				rec.append(l)
			return (tod,rec,comments)
		else:
			comments += l
	return None
			
# ------------------------------------------
def ReadV3Record(l,f):
	if (l[0]=='>'):
		year = int(l[2:6])
		mon = int(l[6:10])
		day = int(l[9:13])
		hours= int(l[12:16])
		mins = int(l[15:19])
		secs = float(l[19:30])
		tod = datetime.datetime(year,mon,day,hours,mins,int(secs)) # FIXME whatabout non-integer secs - use microseconds field ?
		nmeas = int(l[32:36]) # cols 32-35
		meas={}
		for m in range(0,nmeas):
			l=f.readline()
			sv =l[0:3]
			meas[sv]=l
		return(time.mktime(tod.timetuple()),meas)

# ------------------------------------------
def FixAmbiguity(meas,refmeas,gnss,obsType):
	measCol = obs[gnss+':'+obsType]
	refMeasCol = refobs[gnss+':'+obsType]
	try:
		prstr = meas[3+(measCol-1)*16:(measCol-1)*16+17]
		pr = float(prstr)
	except:
		return (False,meas)
	try:
		refPr = float(refmeas[3+(refMeasCol-1)*16:(refMeasCol-1)*16+17])
	except:
		return (False,meas)
	#print '   ',pr,refPr,1000*(refPr-pr)/CLIGHT
	pr = pr + int(round(1000*(refPr-pr)/CLIGHT))*CLIGHT/1000.0
	newprstr = '{:14.3f}'.format(pr)
	return (True,meas.replace(prstr,newprstr,1))

	fi=0

# ------------------------------------------
def FixMissing(infiles):

	fi = 0
	
	for f in infiles:
		
		finName = f
		finNextName = ''
		try:
			fin = open(finName,'r')
		except:
			ErrorExit('Unable to open ' + finName)
			
		if (f == infiles[-1]):
			continue # we're done
		else:
			fi = fi+1
			finNextName = infiles[fi]
			try:
				finNext = open(finNextName,'r')
			except:
				ErrorExit('Unable to open ' + finNextName)
				
			tmpFinNextName = finNextName + '.tmp'
		
			# Two good file names
			(path1,ver1,st1,doy1,yy1,yyyy1,ext1,dataSource1,hhmm1,filePeriod1,dataFrequency1,ft1)=ParseRINEXFileName(finName)
			(path2,ver2,st2,doy2,yy2,yyyy2,ext2,dataSource2,hhmm2,filePeriod2,dataFrequency2,ft2)=ParseRINEXFileName(finNextName)
			
			date1=datetime.datetime(yyyy1, 1, 1) + datetime.timedelta(doy1 - 1)
			date2=datetime.datetime(yyyy2, 1, 1) + datetime.timedelta(doy2- 1)
			
			hdr1 = ReadHeader(fin)
			nobs = 1
			if (ver1 == 2):
				hdrField = GetHeaderField(hdr1,'# / TYPES OF OBSERV')
				nobs = int(str(hdrField[0][0:6]))
				Debug('Number of observations ' + str(nobs))
			else:
				hdrField = GetHeaderField(hdr1,'SYS / # / OBS TYPES')
				# FIXME
				
			hdr2 = ReadHeader(finNext)
			nobs2 = 1
			if (ver1 == 2):
				hdrField = GetHeaderField(hdr2,'# / TYPES OF OBSERV')
				nobs2 = int(str(hdrField[0][0:6]))
				Debug('Number of observations ' + str(nobs2))
			else:
				hdrField = GetHeaderField(hdr2,'SYS / # / OBS TYPES')
			
			if (not nobs == nobs2):
				ErrorExit('# of observations do not match')
			obs1=[]
			if (ver1==2):	
				obs1 = ReadV2Observations(fin,nobs)
			else:
				obs1 = ReadV3Observations(fin)
				
			obsTmp = []
			
			# Find any records in the first file which are in the next day
			# They have to be removed and added to the next file 
			delRecs=[]
			for o in obs1:
				if (o[0].date() > date1.date()):
					delRecs.append(o)
				else:
					obsTmp.append(o)
			
			fin.close()
		
			Debug(str(len(delRecs)) + ' record(s) to be removed from ' + finName)
			if (len(delRecs) > 0):
				tmpName = finName + '.tmp'
				tmpFout = open(tmpName,'w')
				AddHeaderComments(hdr1,['Edited by ' + progName]);
				# Fixup time of the last observation, if the field is present
				hdrField = GetHeaderField(hdr1,'TIME OF LAST OBS')
				if hdrField:
					tLast = obsTmp[-1][0]
					timeRef = hdrField[0][48:51]
					lastObs = '{:6d}{:6d}{:6d}{:6d}{:6d}{:13.7f}{:5}{:3}{:9}{:20}\n'.format(tLast.date().year,tLast.date().month,tLast.date().day,
						tLast.time().hour,tLast.time().minute,tLast.time().second,' ',timeRef,' ',
						'TIME OF LAST OBS')
					EditHeader(hdr1,'TIME OF LAST OBS',lastObs)
				WriteHeader(tmpFout,hdr1)
				WriteObservations(tmpFout,obsTmp)
				tmpFout.close()
				
				# Rename the old file
				# if (args.replace):
				# 	os.rename(finName,finName+'.bak')
				# 	os.rename(tmpName,finName)
				#		if not args.keep:
				# 		os.remove(finName+'.bak')
				
				# Now examine the next file
				# Already read the header to do some checks
				obs2=[]
				if (ver1 == 2):
					obs2 = ReadV2Observations(finNext,nobs)
				else:
					obs2 = ReadV3Observations(finNext)
					
				# Find the first observation time
				# If we have any observations that can be inserted before this, print them
				firstObs = obs2[0][0]
				insRecs = [d for d in delRecs if (d[0] < firstObs)]
				Debug(str(len(insRecs)) + ' record(s) to be inserted in ' + finNextName)
				if (len(insRecs)>0): # something to do
					tmpName = finNextName + '.tmp'
					tmpFout = open(tmpName,'w')
					AddHeaderComments(hdr2,['Edited by ' + progName])
					hdrField = GetHeaderField(hdr1,'TIME OF FIRST OBS')
					if not hdrField:
						ErrorExit('Invalid RINEX - missing TIME OF FIRST OBS' + finNextName)
					timeRef = hdrField[0][48:51]
					tFirst = insRecs[0][0]
					fObs = '{:6d}{:6d}{:6d}{:6d}{:6d}{:13.7f}{:5}{:3}{:9}{:20}\n'.format(tFirst.date().year,tFirst.date().month,tFirst.date().day,
						tFirst.time().hour,tFirst.time().minute,tFirst.time().second,' ',timeRef,' ',
						'TIME OF FIRST OBS')
					EditHeader(hdr2,'TIME OF FIRST OBS',fObs)
					WriteHeader(tmpFout,hdr2)
					WriteObservations(tmpFout,insRecs)
					WriteObservations(tmpFout,obs2)
					tmpFout.close()
					# Rename files
					# if (args.replace):
					# 	os.rename(finName,finName+'.bak')
					# 	os.rename(tmpName,finName)
					# 	if not args.keep:
					#			os.remove(finName+'.bak')
					
# ------------------------------------------
# Main


progName = os.path.basename(sys.argv[0])+ ' ' + VERSION

parser = argparse.ArgumentParser(description='Edit a RINEX observation file',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('infile',nargs='+',help='input file',type=str)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
group = parser.add_mutually_exclusive_group()
group.add_argument('--output','-o',help='output to file/directory',default='')
group.add_argument('--keep','-k',help='keep intermediate files',action='store_true')
group.add_argument('--replace','-r',help='replace edited file',action='store_true')
parser.add_argument('--system',help='satellite system (BeiDou,Galileo,GPS,GLONASS)')
parser.add_argument('--obstype',help='observation type (C2I,L2I,...)')
parser.add_argument('--fixms',help='fix ms ambiguities (ref RINEX file required)',action='store_true')
parser.add_argument('--fixmissing',help='add observations missing at the beginning of the day',action='store_true')
parser.add_argument('--sequence','-s',help='interpret input files as a sequence',action='store_true')
parser.add_argument('--refrinex',help='reference RINEX file for fixing ms ambiguities (name of first file if multiple input files are specified)')
parser.add_argument('--version','-v',action='version',version =  progName + '\n' + 'Written by ' + AUTHORS)

args = parser.parse_args()

debug = args.debug

fixms = False
if (args.fixms):
	fixms = True
	if not(args.refrinex):
		print('You need to supply a reference RINEX file to fix ms ambiguities')
		exit()
			
infiles = []
reffiles = []

if (args.sequence or args.fixmissing): # determine whether the file names form a valid sequence
	if (2==len(args.infile)):
		(path1,ver1,st1,doy1,yy1,yyyy1,ext1,dataSource1,hhmm1,filePeriod1,dataFrequency1,ft1)=ParseRINEXFileName(args.infile[0])
		(path2,ver2,st2,doy2,yy2,yyyy2,ext2,dataSource2,hhmm2,filePeriod2,dataFrequency2,ft2)=ParseRINEXFileName(args.infile[1])
		if not(path1 == path2):
			sys.stderr.write('The files must be in the same directory for the --sequence/--fixmissing  option\n')
			exit()
		if not(ver1 == ver2):
			sys.stderr.write('The RINEX files must have the same version for the --sequence/--fixmissing option\n')
			exit()
		if not(st1==st2):
			sys.stderr.write('The station names must match with the --sequence/--fixmissing option\n')
			exit()
		if not(ft1==ft2):
			sys.stderr.write('The file types must match with the --sequence/--fixmissing option\n')
			exit()
			
		if ((yyyy1 > yyyy2) or (yyyy1 == yyyy2 and doy1 > doy2)):
			sys.stderr.write('The dates appear to be in the wrong order for the --sequence/--fixmissing option\n')
			exit()
			
		# it appears we have a valid sequence so generate it
		date1=datetime.datetime(yyyy1, 1, 1) + datetime.timedelta(doy1 - 1)
		date2=datetime.datetime(yyyy2, 1, 1) + datetime.timedelta(doy2 - 1)
		td =  date2-date1
		refver = 0 # lazy flag
		if (args.refrinex):
			(refpath,refver,refst,refdoy,refyy,refext,refdataSource,refhhmm,reffilePeriod,refdataFrequency,refft)=ParseRINEXFileName(args.refrinex)
		
		for d in range(0,td.days+1):
			ddate = date1 +  datetime.timedelta(d)
			if (ver1 == 2):
				yystr = ddate.strftime('%y')
				doystr=ddate.strftime('%j')
				fname = '{}{}0.{}{}'.format(st1,doystr,yystr,ext1) 
				infiles.append(os.path.join(path1,fname))	
			elif (ver1 == 3):
				yystr = ddate.strftime('%Y')
				doystr= ddate.strftime('%j')
				fname = '{}_{}_{}{:>03d}{}_{}_{}_{}.{}'.format(st1,dataSource1,yystr,int(doystr),hhmm1,filePeriod1,dataFrequency1,ft1,ext1) 
				infiles.append(os.path.join(path1,fname))
				
			if (refver == 2):
				yystr = ddate.strftime('%y')
				doystr= ddate.strftime('%j')
				fname = '{}{}0.{}{}'.format(refst,doystr,yystr,refext) 
				reffiles.append(os.path.join(refpath,fname))
			elif (refver == 3):
				yystr = ddate.strftime('%Y')
				doystr= ddate.strftime('%j')
				fname = '{}_{}_{}{:>03d}{}_{}_{}_{}.{}'.format(refst,refdataSource,yystr,int(doystr),refhhmm,reffilePeriod,refdataFrequency,refft,refext) 
				reffiles.append(os.path.join(refpath,fname))
				
	else:
		sys.stderr.write('Only two file names are allowed with the --sequence option\n')
		exit()
else:
	if (1==len(args.infile)):
		infiles.append(args.infile[0])
		if (args.refrinex):
			reffiles.append(args.refrinex)
	else:
		pass

if (args.output and len(args.infile) >1):
	if not(os.path.isdir(args.output)):
		sys.stderr.write('The --output option must specify a directory  when there is more than one input file\n')
		exit()

if not(args.fixmissing or args.fixms):
	sys.stderr.write('Nothing to do!\n')
	sys.exit()

# Preliminary stuff done. 
# Now do stuff!

if (args.fixmissing):
	FixMissing(infiles)
	sys.exit(0)
	
fi=0

for f in infiles:
	
	finName = f
	
	try:
		fin = open(finName,'r')
	except:
		print('Unable to open '+finName)
		exit()
	
	foutName = finName + '.tmp'
	
	if (args.output):
		if (os.path.isdir(args.output)):# if it's a directory, write output there
			foutName = os.path.join(args.output,os.path.basename(finName))
		else:# otherwise, write to the specified file
			foutName = args.output
		
	if (args.replace or args.output):
		try:
			fout = open(foutName,'w')
		except:
			print('Unable to open ' + foutName)
			exit()
	else:
		fout = sys.stdout
			
	if (args.system):
		gnss = args.system.lower()
		if (gnss =='beidou'):
			gnss =BEIDOU
		elif (gnss == 'galileo'):
			gnss = GALILEO
		elif (gnss == 'glonass'):
			gnss = GLONASS
		elif (gnss == 'gps'):
			gnss = GPS
		else:
			print('Unknown GNSS system')
			exit()

	
	if (fixms):
		try:
			fref = open(reffiles[fi],'r')
		except:
			print('Unable to open '+reffiles[i])
			exit()
				
		if not(args.obstype):
			print(' You need to supply the observation to correct')
			exit()
			
	# Read the header
	obs={} # satsys/observation code stored in dict
	while True:
		l=fin.readline()
		fout.write(l)
		if (l.find('PGM / RUN BY / DATE',60)>0):
			comment = '{:60}{:20}\n'.format('Edited with ' + os.path.basename(sys.argv[0]),'COMMENT')
			fout.write(comment)
			
		if (l.find('RINEX VERSION',60)>0):
			try:
				ver = float(l[:8])
				Debug('RINEX version is {}'.format(ver))
				if (ver <3):
					print('Only V3 supported')
					exit()
			except:
				sys.stderr.write('Unable to determine the RINEX version - continuing anyway\n')
		
		if (ver >= 3):
			if (l.find('SYS / # / OBS TYPES',60)>0):
				# Store signals in a dictionary where the key is like 
				# SYS_ID:SIGNAL eg C:C2I 
				# and value is the column
				syscode = l[0]
				if (syscode == gnss):
					nobs = int(l[3:6])
					lastLine = 0
					for i in range(0,nobs):
						currLine = int(i/13) # 13 obs per line
						if not(lastLine == currLine):
							l=fin.readline()
							fout.write(l)
							lastLine=currLine
						indx = 6+1+(i -currLine*13)*4
						obscode = l[indx:indx+3]
						# Fixup BeiDou for v3.02 vs v3.03 - add extra lookups
						if (gnss == BEIDOU):
							if (obscode[1] == '1'):
								newcode = obscode.replace('1','2')
								obs[gnss+':'+newcode]=i+1
							if (obscode[1] == '2'):
								newcode = obscode.replace('2','1')
								obs[gnss+':'+newcode]=i+1
						obs[gnss+':'+obscode]=i+1
		if (l.find('END OF HEADER',60)>0):
			Debug("Read header")
			break
	# 

	if fixms:
		# Read the REF header
		refobs={} # satsys/observation code stored in dict
		while True:
			l=fref.readline()			
			if (l.find('RINEX VERSION',60)>0):
				try:
					refver = float(l[:8])
					Debug('RINEX version is {}'.format(ver))
					if (ver <3):
						print('Only V3 supported')
						exit()
				except:
					sys.stderr.write('Unable to determine the RINEX version for REF - continuing anyway\n')
			
			if (refver >= 3):
				if (l.find('SYS / # / OBS TYPES',60)>0):
					# Store signals in a dictionary where the key is like 
					# SYS_ID:SIGNAL eg C:C2I 
					# and value is the column
					syscode = l[0]
					if (syscode == gnss):
						nobs = int(l[3:6])
						lastLine = 0
						for i in range(0,nobs):
							currLine = int(i/13) # 13 obs per line
							if not(lastLine == currLine):
								l=fin.readline()
								fout.write(l)
								lastLine=currLine
							indx = 6+1+(i -currLine*13)*4
							obscode = l[indx:indx+3]
							if (gnss == BEIDOU):
								if (obscode[1] == '1'):
									newcode = obscode.replace('1','2')
									refobs[gnss+':'+newcode]=i+1
								if (obscode[1] == '2'):
									newcode = obscode.replace('2','1')
									refobs[gnss+':'+newcode]=i+1
							refobs[gnss+':'+obscode]=i+1
			if (l.find('END OF HEADER',60)>0):
				Debug("Read REF header")
				break
		
		meas={}
		refmeas={}
		while True:
			
			l=fin.readline()
			if l:
				fout.write(l)
				(tod,meas) = ReadV3Record(l,fin)
			else: # end of file
				break
			
			while True:
				lref=fref.readline()
				if lref: # end of file
					(reftod,refmeas) = ReadV3Record(lref,fref)
				else: # end of file
					break
				if (reftod < tod):
					pass # move to next record in REF file
				elif (reftod > tod):
					break # move to next record in edited file
				else: 
					for m in meas:
						mgnss = meas[m][0]
						if (mgnss == gnss):
							if m in refmeas:
								(ok,corrMeas) = FixAmbiguity(meas[m],refmeas[m],gnss,args.obstype)
								if ok :
									meas[m]=corrMeas
									fout.write(meas[m])
								else:
									# if we can't fix it then output the SV only
									fout.write(meas[m][0:3]+'\n')
							else:
								fout.write(meas[m][0:3]+'\n')
						else:
							fout.write(meas[m]) # echo other GNSS
					break
				
			
	fin.close()
	fout.close()



	if (args.replace):
		os.remove(finName)
		os.rename(foutName,finName)
	
	fi=fi+1