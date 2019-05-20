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

VERSION = "0.1.1"
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
def ParseRINEXFileName(fname):
	ver = 0
	p = os.path.dirname(fname)
	print fname
	match = re.search('(\w{4})(\d{3})0\.(\d{2})([oO])',fname)
	if match:
		st = match.group(1)
		doy = int(match.group(2))
		yy = int(match.group(3))
		ft = match.group(4)
		ver=2
		#print ver,st,doy,yy
	return (p,ver,st,doy,yy,ft)
		
# ------------------------------------------
def ParseDataRecord(l,f):
	if (l[0]=='>'):
		year = int(l[2:6])
		mon = int(l[6:10])
		day = int(l[9:13])
		hours= int(l[12:16])
		mins = int(l[15:19])
		secs = float(l[19:30])
		tod = datetime.datetime(year,mon,day,hours,mins,int(secs)) # FIXME whatabout non-nteger secs - use microseconds field ?
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
	
# ------------------------------------------
# Main


parser = argparse.ArgumentParser(description='Edit a RINEX observation file',
	formatter_class=argparse.RawDescriptionHelpFormatter)

parser.add_argument('infile',nargs='+',help='input file',type=str)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
group = parser.add_mutually_exclusive_group()
group.add_argument('--output','-o',help='output to file/directory',default='')
group.add_argument('--replace','-r',help='replace edited file',action='store_true')
parser.add_argument('--system',help='satellite system (BeiDou,Galileo,GPS,GLONASS)')
parser.add_argument('--obstype',help='observation type (C2I,L2I,...)')
parser.add_argument('--fixms',help='fix ms ambiguities (ref RINEX file required)',action='store_true')
parser.add_argument('--sequence','-s',help='interpret input files as a sequence',action='store_true')
parser.add_argument('--refrinex',help='reference RINEX file for fixing ms ambiguities (name of first file if multiple input files are specified)')
parser.add_argument('--version','-v',action='version',version = os.path.basename(sys.argv[0])+ ' ' + VERSION + '\n' + 'Written by ' + AUTHORS)

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

if (args.sequence): # determine whether the file names form a valid sequence
	if (2==len(args.infile)):
		(path1,ver1,st1,doy1,yy1,ft1)=ParseRINEXFileName(args.infile[0])
		(path2,ver2,st2,doy2,yy2,ft2)=ParseRINEXFileName(args.infile[1])
		if not(path1 == path2):
			sys.stderr.write('The files must be in the same directory --sequence option\n')
			exit()
		if (yy1 > 80):
			yy1 += 1900
		else:
			yy1 += 2000
		if (yy2 > 80):
			yy2 += 1900
		else:
			yy2 += 2000
		if (st1 == st2):
			if ((yy1 > yy2) or (yy1 == yy2 and doy1 > doy2)):
				sys.stderr.write('The dates appear to be in the wrong order for the --sequence option\n')
				exit()
			else: # it appears we have a valid sequence so generate it
				date1=datetime.datetime(yy1, 1, 1) + datetime.timedelta(doy1 - 1)
				date2=datetime.datetime(yy2, 1, 1) + datetime.timedelta(doy2- 1)
				td =  date2-date1
				refver = 0 # lazy flag
				if (args.refrinex):
					(refpath,refver,refst,refdoy,refyy,refft)=ParseRINEXFileName(args.refrinex)
				
				for d in range(0,td.days+1):
					ddate = date1 +  datetime.timedelta(d)
					if (ver1 == 2):
						yystr = ddate.strftime('%y')
						doystr=ddate.strftime('%j')
						fname = '{}{}0.{}{}'.format(st1,doystr,yystr,ft1) 
						infiles.append(os.path.join(path1,fname))
						
					elif (ver1 == 3):
						pass
				
					if (refver == 2):
						yystr = ddate.strftime('%y')
						doystr=ddate.strftime('%j')
						fname = '{}{}0.{}{}'.format(refst,doystr,yystr,refft) 
						reffiles.append(os.path.join(refpath,fname))
					elif (refver ==3):
						pass
		else:
			sys.stderr.write('The station names must match with the --sequence option\n')
			exit()
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

fi=0

for f in infiles:
	
	finName = f
	foutName = finName + '.tmp'
	
	if (args.output):
		if (os.path.isdir(args.output)):# if it's a directory, write output there
			foutName = os.path.join(args.output,os.path.basename(finName))
		else:# otherwise, write to the specified file
			foutName = args.output
		
	try:
		fin = open(finName,'r')
	except:
		print('Unable to open '+finName)
		exit()

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
				(tod,meas) = ParseDataRecord(l,fin)
			else: # end of file
				break
			
			while True:
				lref=fref.readline()
				if lref: # end of file
					(reftod,refmeas) = ParseDataRecord(lref,fref)
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