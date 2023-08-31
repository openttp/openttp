//
//
// The MIT License (MIT)
//
// Copyright (c) 2023  Michael J. Wouters
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <cmath>
#include <cstring>

#include <sstream>

#include <boost/algorithm/string.hpp>

#include "Application.h"
#include "Debug.h"
#include "GNSSSystem.h"
#include "GPS.h"
#include "Measurements.h"
#include "RINEXObsFile.h"
#include "Utility.h"

extern Application *app;
extern std::ostream *debugStream;

#define SBUFSIZE 160
#define MAXDAYS  2    // maximum number of days of data allowed in a file

//
// Public methods
//
		
RINEXObsFile::RINEXObsFile():RINEXFile()
{
	init();
}


RINEXObsFile::~RINEXObsFile()
{
}
	

	
bool RINEXObsFile::read(std::string fname,int tstart,int tstop)
{
	// It is presumed that the file can be read ...
	RINEXFile::readRINEXVersion(fname);
	
	if (version < 3){
		return false;
	}	
	else if (version < 4){
		readV3File(fname);
	}
	else if (version < 5){
		return false;
	}
	return true;
}


RINEXObsFile* RINEXObsFile::merge(RINEXObsFile &obs)
{
	DBGMSG(debugStream,TRACE,"merging");
	
	RINEXObsFile *mobs = NULL;
	
	// some basic compatibility checks
	if (obsInterval != obs.obsInterval){
		app->logMessage("Failed to merge observation file");
		return mobs;
	}
	
	
	// determine roughly how much data there is
	int startMJD = Utility::DateToMJD(tmFirstObs.tm_year + 1900, tmFirstObs.tm_mon + 1, tmFirstObs.tm_mday);
	int mjd = Utility::DateToMJD(obs.tmFirstObs.tm_year + 1900, obs.tmFirstObs.tm_mon + 1, obs.tmFirstObs.tm_mday);
	if (mjd < startMJD)
		startMJD = mjd;
	
	int stopMJD = Utility::DateToMJD(tmLastObs.tm_year + 1900, tmLastObs.tm_mon + 1, tmLastObs.tm_mday);
	mjd = Utility::DateToMJD(obs.tmLastObs.tm_year + 1900, obs.tmLastObs.tm_mon + 1, obs.tmLastObs.tm_mday);
	if (mjd > stopMJD)
		stopMJD = mjd;
	
	DBGMSG(debugStream,INFO, startMJD << " " << stopMJD);
	
	return mobs;
}

//
// Private methods
//
		
void RINEXObsFile::init()
{
	obsInterval = -1;
	ttFirstObs = 0; // flags invalid
	ttLastObs = 0; // flags invalid
}


bool RINEXObsFile::readV3File(std::string fname)
{
	
	unsigned int lineCount=0;	
	std::ifstream fin;
	fin.open(fname.c_str()); // already tested 'good'
	
	// Parse the header
	int gnss = -1;
	int ibuf;
	double dbuf;
	std::string line;
	GPS gpsSys;
	
	while (!fin.eof()){
		
		std::getline(fin,line);
		lineCount++;
		
		if (std::string::npos != line.find("RINEX VERSION/TYPE",60)){
			char satSystem = line[40]; //assuming length is OK
			switch (satSystem){
				case 'M':gnss = 0; break;
				case 'G':gnss = GNSSSystem::GPS;break;
				case 'R':gnss = GNSSSystem::GLONASS;break;
				case 'E':gnss = GNSSSystem::GALILEO;break;
				case 'C':gnss = GNSSSystem::BEIDOU;break;
				default:break;
			}
			continue;
		}
		
		if (std::string::npos != line.find("SYS / # / OBS TYP",60)){
			
			int nObs;
			readParam(line,4,3, &nObs);
			int nObsLines = ceil(nObs/14.0);
			
			char satSysCode = line[0];
			Measurements *meas = NULL; // to flag that we don't need to read the record
			switch (satSysCode)
			{
				case 'G':
				{
					gps.gnss = GNSSSystem::GPS; gps.maxSVN = gpsSys.maxSVN(); // FIXME
					meas = &gps;
					break;
				}
				default:
				{
					DBGMSG(debugStream,INFO,"Ignoring " << satSysCode << "  ... pfftt!");
					for (int i=2;i<=nObsLines;i++){
						std::getline(fin,line);
					}
					break;
				}
			}
			
			if (meas){
				meas->nAllObs = nObs;
				std::string obsCodes = line.substr(7,52);
				for (int i=2;i<=nObsLines;i++){
					std::getline(fin,line);
					obsCodes += line.substr(7,52);
				}
				boost::trim(obsCodes);

				for (int i=0;i<nObs;i++){
					std::string code = obsCodes.substr(i*4,3);
					
					// We only care about code observations - ignore everything else
					if (code[0] == 'C'){
						DBGMSG(debugStream,INFO,code);
						meas->codes.push_back(code);
						rnxCodes.push_back(code);
						rnxCols.push_back(i);
					}
				}
				// Now we can allocate memory for the observation data
				meas->allocateStorage(1440*2*MAXDAYS);// memory is cheap - allow 2 days of data. 1440 minutes per day * 2 observations per minute
				meas->nCodeObs = meas->codes.size();
				DBGMSG(debugStream,INFO,"read SYS/OBS TYP: " << satSysCode);
				DBGMSG(debugStream,INFO,obsCodes );
			}
		}
		
		if (std::string::npos != line.find("INTERVAL",60)){ // optional
			readParam(line,1,10, &obsInterval); // F10.3
			DBGMSG(debugStream,TRACE,"read INTERVAL: " << obsInterval);
			continue;
		}
		
		if (std::string::npos != line.find("LEAP SECONDS",60)){ // optional
			readParam(line,1,6, &leapsecs); // I6 
			DBGMSG(debugStream,TRACE,"read LEAP SECONDS:" << leapsecs);
			continue;
		}
		
		if (std::string::npos != line.find("TIME OF FIRST OBS",60)){ // format is 5I6,F13.7,5X,A3 
			
			// we're going to determine the first (and last) observation time ourselves
			// but we need the time system
			if (std::string::npos != line.find("GPS"))
				timeSystem = GNSSSystem::GPS;
			else if (std::string::npos != line.find("GLONASS"))
				timeSystem = GNSSSystem::GLONASS;
			DBGMSG(debugStream,TRACE,"read TIME OF FIRST OBS");
			continue;
		}
		
		if (std::string::npos != line.find("END OF HEADER",60)){
			DBGMSG(debugStream,TRACE,"Finished reading header");
			break;
		}
		
	}
	
	std::string dummy,satNum;
	int year,mon,mday,hour,mins,epochFlag,nObs,svn;
	double secs;
	
	while (!fin.eof()){
		std::getline(fin,line);
		lineCount++;
		if (line[0] == '>'){
			
			std::stringstream ss(line); // this line has has white space delimited fields for TOD so we can read it with less fuss
			ss >> dummy >> year >> mon >> mday >> hour >> mins >> secs; // hard to see why there shouldn't always be a space before 'secs'
			
			readParam(line,32,1,&epochFlag);
			readParam(line,33,3,&nObs);
			DBGMSG(debugStream,TRACE,nObs << " obs at " << hour << ":"<< mins << ":" << secs);
			
			if (nObs > 0){
				if (ttFirstObs == 0){
					tmFirstObs.tm_year = year - 1900;
					tmFirstObs.tm_mon = mon - 1 ;
					tmFirstObs.tm_mday = mday;
					tmFirstObs.tm_hour = hour;
					tmFirstObs.tm_min = mins;
					tmFirstObs.tm_sec = rint(secs); /// but .. we need a double
					firstObsSecs = secs;
					tmFirstObs.tm_isdst = - 1;
					
					if (-1 == (ttFirstObs = mktime(&tmFirstObs))){
						app->fatalError("mktime failed on first observation");
					}
			
				}
				
				tmLastObs.tm_year = year - 1900;
				tmLastObs.tm_mon = mon - 1 ;
				tmLastObs.tm_mday = mday;
				tmLastObs.tm_hour = hour;
				tmLastObs.tm_min = mins;
				tmLastObs.tm_sec = rint(secs); /// but .. we need a double
				lastObsSecs = secs;
				tmLastObs.tm_isdst = - 1;
				
			}
			
			int itod = (hour *3600 + mins* 60 + rint(secs))/30; // GPS time, usually
			int mjd  = Utility::DateToMJD(year,mon,mday);       // ditto
			// FIXME should I test epochFlag ?
			for (int o = 1; o<= nObs; o++){
				std::getline(fin,line);
				lineCount++;
				if (line[0] == 'G'){
					readParam(line,2,2,&svn);
					gps.meas[itod][svn][gps.nCodeObs] = mjd;
					gps.meas[itod][svn][gps.nCodeObs+1]= hour *3600 + mins* 60 + secs; // non-integer measurement epoch has to be taken into account!
					readV3Obs(gps,itod,svn,line);
					continue;
				}
				// ... and so on for other GNSS
			}
		}
	}
	
	fin.close();
	//gps.dump();
	if (-1 == (ttLastObs = mktime(&tmLastObs))){
		app->fatalError("mktime failed on last observation");
	}
	
	return true;
}

int RINEXObsFile::readV3Obs(Measurements &m, int itod,int svn,std::string l)
{
	int nMeas = 0;
	// A field can be empty or have a value of 0.0 to indicate 'no measurement'
	// Format is A1,I2.2,m(14.3.I1,I1)
	double dbuf;
	
	for (unsigned int i=0;i<rnxCols.size();i++)
		m.meas[itod][svn][i] = NAN;

	for (unsigned int i=0;i<rnxCols.size();i++){
		unsigned int stop = 3 + rnxCols[i]*(16+1);
		if (stop > l.length()) // empty fields at the end
			break;
		if (readParam(l,1+3+rnxCols[i]*16,14,&dbuf)){
			m.meas[itod][svn][i]=dbuf;
			nMeas += 1;
		}
	}
	return nMeas;
}


//  This is just used during parsing of the RINEX file
int RINEXObsFile::code2RNXcol(std::string c){
	for (unsigned int i=0;i<rnxCodes.size();i++){
		if (rnxCodes[i] == c){
			return rnxCols[i];
		}
	}
	return -1; // FIXME probably should sanity check RINEX file before we get this far
}
		
