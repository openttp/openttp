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
#include "Measurements.h"
#include "RINEXObsFile.h"

extern Application *app;
extern std::ostream *debugStream;

#define SBUFSIZE 160

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
		readV2File(fname);
	}	
	else if (version < 4){
		readV3File(fname);
	}
	else if (version < 1){
		return false;
	}
	
	return true;
}

//
// Private methods
//
		
void RINEXObsFile::init()
{
	obsInterval = -1;
	leapSecs = -1;
}


bool RINEXObsFile::readV2File(std::string)
{
	return false;
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
					gps.gnss = GNSSSystem::GPS; gps.maxSVN = 32; // FIXME
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
				std::vector<std::string> codes;
				std::vector<int >   cols;
				for (int i=0;i<nObs;i++){
					std::string code = obsCodes.substr(i*4,3);
					// We only care about code observations
					if (code[0] == 'C'){
						DBGMSG(debugStream,INFO,code);
						meas->codes.push_back(code);
						meas->cols.push_back(i);
					}
				}
				// Now we can allocate memory for the observation data
				meas->allocateStorage(1440*2);// memory is cheap
				meas->nCodeObs = meas->codes.size();
				DBGMSG(debugStream,INFO,"read SYS/OBS TYP: " << satSysCode);
				DBGMSG(debugStream,INFO,obsCodes << "<-" );
			}
		}
		
	
	//		
// 		if (strfind(l,'SYS / # / OBS TYP'))
// 					satSysCode=l(1);
// 					nobs = sscanf(l(4:6),'%i');
// 					obsl = strtrim(l(7:58));% remove leading blank as well for use with strsplit
// 					nlines = ceil(nobs/14); % 13 + 1 = 14 :-)
// 					for line=2:nlines % first one done
// 						nl = fgetl(fobs);
// 						nl = deblank(nl(7:58));
// 						obsl= [obsl,nl];
// 					end
					
		if (std::string::npos != line.find("INTERVAL",60)){ // optional
			readParam(line,1,10, &obsInterval); // F10.3
			DBGMSG(debugStream,TRACE,"read INTERVAL: " << obsInterval);
			continue;
		}
		
		if (std::string::npos != line.find("LEAP SECONDS",60)){ // optional
			readParam(line,1,6, &leapSecs); // I6 
			DBGMSG(debugStream,TRACE,"read LEAP SECONDS:" << leapSecs);
			continue;
		}
		
		if (std::string::npos != line.find("TIME OF FIRST OBS",60)){ // format is 5I6,F12.6,6X,A3 
			
			readParam(line,1,6,&obs1yr);
			yrOffset = (obs1yr/100)*100;
			readParam(line,7,6,&obs1mon);
			readParam(line,13,6,&obs1day);
			readParam(line,19,6,&obs1hr);
			readParam(line,25,6,&obs1min);
			readParam(line,31,12,&obs1sec);
			//std::strncpy(sbuf,line+48,3);// remember, subtract 1 from start index !
			if (std::string::npos != line.find("GPS"))
				timeSystem = GNSSSystem::GPS;
			else if (std::string::npos != line.find("GLONASS"))
				timeSystem = GNSSSystem::GLONASS;
			DBGMSG(debugStream,TRACE,"read TIME OF FIRST OBS: " << obs1yr << "-" << obs1mon << "-" << obs1day);
			continue;
		}
		
		if (std::string::npos != line.find("END OF HEADER",60)){
			DBGMSG(debugStream,TRACE,"Finished reading header");
			break;
		}
		
	}
	
	//for (int i=0;i<gps.codes.size();i++){
	//	DBGMSG(debugStream,INFO,gps.cols[i] << " " << gps.codes[i]  );
	//}
		
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
			
			int itod = (hour *3600 + mins* 60 + rint(secs))/30;
			// FIXME should I test epochFlag ?
			for (int o = 1; o<= nObs; o++){
				std::getline(fin,line);
				lineCount++;
				if (line[0] == 'G'){
					readParam(line,2,2,&svn);
					//DBGMSG(debugStream,INFO,tod);
					gps.meas[itod][svn][gps.nCodeObs]= hour *3600 + mins* 60 + secs; // non-integer measurement epoch has to be taken into account!
					readV3Obs(gps,itod,svn,line);
					continue;
				}
				// ... and so on for other GNSS
			}
		}
	}
	
	fin.close();
	gps.dump();
	
	return true;
}

void RINEXObsFile::readV3Obs(Measurements &m, int itod,int svn,std::string l)
{
	int nFields = m.nAllObs;
	// A field can be empty or have a value of 0.0 to indicate 'no measurement'
	// Format is A1,I2.2,m(14.3.I1,I1)
	double dbuf;
	for (int i=0;i<m.cols.size();i++){
		int stop = 3 + m.cols[i]*(16+1);
		if (stop > l.length()){ // empty fields at the end
			//DBGMSG(debugStream,INFO,l);
			//DBGMSG(debugStream,INFO,"missing at end " << m.cols[i]);
			m.meas[itod][svn][i]=0.0; 
			continue;
		}
		if (readParam(l,1+3+m.cols[i]*16,14,&dbuf)){
			//DBGMSG(debugStream,INFO,dbuf);
			m.meas[itod][svn][i]=dbuf;
		}
		else{
			//DBGMSG(debugStream,INFO,"missing");
			m.meas[itod][svn][i]=0.0;
		}
	}
}

		
