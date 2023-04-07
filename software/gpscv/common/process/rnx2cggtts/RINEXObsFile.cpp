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

#include "Application.h"
#include "Debug.h"
#include "GNSSSystem.h"
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
	

	
bool RINEXObsFile::read(std::string fname)
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
		
		if (std::string::npos != line.find("RINEX VERSION/TYPE")){
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
		
		if (std::string::npos != line.find("SYS / # / OBS TYP")){
			char satSysCode = line[0];
			int nObs;
			readParam(line,4,3, &nObs);
			int nObsLines = ceil(nObs/14.0);
			for (int i=2;i<=nObsLines;i++){
				std::getline(fin,line);
			}
			DBGMSG(debugStream,TRACE,"read SYS/OBS TYP: " << satSysCode);
		}
		
			
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
					
		if (std::string::npos != line.find("INTERVAL")){ // optional
			readParam(line,1,10, &obsInterval); // F10.3
			DBGMSG(debugStream,TRACE,"read INTERVAL: " << obsInterval);
			continue;
		}
		
		if (std::string::npos != line.find("LEAP SECONDS")){ // optional
			readParam(line,1,6, &leapSecs); // I6 
			DBGMSG(debugStream,TRACE,"read LEAP SECONDS:" << leapSecs);
			continue;
		}
		
		if (std::string::npos != line.find("TIME OF FIRST OBS")){ // format is 5I6,F12.6,6X,A3 
			
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
		
		if (std::string::npos != line.find("END OF HEADER")){
			break;
		}
		
	}
	
	fin.close();
	
	return true;
}
		
