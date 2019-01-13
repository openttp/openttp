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
#

import argparse
import datetime
import os
import sys
import time

VERSION = "0.1"
AUTHORS = "Michael Wouters"

BEIDOU='C'
GALILEO='E'
GLONASS='R'
GPS='G'

CLIGHT = 299792458.0

# ------------------------------------------
def ShowVersion():
	print  os.path.basename(sys.argv[0])," ",VERSION
	print "Written by",AUTHORS
	return

# ------------------------------------------
def Debug(msg):
	if (debug):
		sys.stderr.write(msg+'\n')
	return
	
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
def FixAmbiguity(meas,refmeas,gnss):
	measCol = obs[gnss+':'+'C2X']
	refMeasCol = refobs[gnss+':'+'C2I']
	try:
		pr = float(meas[3+(measCol-1)*16:(measCol-1)*16+17])
	except:
		return False
	try:
		refPr = float(refmeas[3+(refMeasCol-1)*16:(refMeasCol-1)*16+17])
	except:
		return False
	#print pr,refPr,1000*(refPr-pr)/CLIGHT
	return True
	
# ------------------------------------------
# Main

parser = argparse.ArgumentParser(description='Edit a RINEX observation file')
parser.add_argument('infile',help='input file',type=str)
parser.add_argument('--debug','-d',help='debug (to stderr)',action='store_true')
group = parser.add_mutually_exclusive_group()
group.add_argument('--output','-o',help='output to file/directory',default='')
group.add_argument('--replace','-r',help='replace edited file',action='store_true')
parser.add_argument('--system',help='satellite system (BeiDou,Galileo,GPS,GLONASS)')
parser.add_argument('--obstype',help='observation type (C2I,L2I,...)')
parser.add_argument('--fixms',help='fix ms ambiguities (ref RINEX file required)',action='store_true')
parser.add_argument('--refrinex',help='reference RINEX file for fixing ms ambiguities')
parser.add_argument('--version','-v',help='show version and exit',)


args = parser.parse_args()

debug = args.debug

if (args.version):
	ShowVersion()
	exit()

finName = args.infile
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

fixms = False
if (args.fixms):
	fixms = True
	if not(args.refrinex):
		print('You need to supply a reference RINEX file to fix ms ambiguities')
		exit()
	else:
		try:
			fref = open(args.refrinex,'r')
		except:
			print('Unable to open '+args.refrinex)
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
						refobs[gnss+':'+obscode]=i+1
		if (l.find('END OF HEADER',60)>0):
			Debug("Read REF header")
			break
	meas={}
	refmeas={}
	while True:
		
		l=fin.readline()
		if l:
			print l.rstrip()
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
							if FixAmbiguity(meas[m],refmeas[m],gnss):
								print meas[m].rstrip()
							else:
								pass # no REF to fix it so not output
					else:
						print meas[m].rstrip() # echo other GNSS
				break
			
		
fin.close()
fout.close()



if (args.replace):
	os.remove(finName)
	os.rename(foutName,finName)
	
