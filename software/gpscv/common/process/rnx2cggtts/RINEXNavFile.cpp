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

#include <cstring>

#include <boost/algorithm/string.hpp>

#include "Application.h"
#include "Debug.h"
#include "GNSSSystem.h"
#include "GPS.h"
#include "RINEXNavFile.h"

extern Application *app;
extern std::ostream *debugStream;


char gsbuf[256];
#define SBUFSIZE 160


//
// Public
//		

RINEXNavFile::RINEXNavFile():RINEXFile()
{
	init();
}

RINEXNavFile::~RINEXNavFile()
{
}

bool RINEXNavFile::read(std::string fname)
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
	
	//if (ephemeris.size() == 0){
	//	app->logMessage("Empty navigation file " + fname);
	//	return false;
	//}
	DBGMSG(debugStream,INFO,"Read " << gps.ephemeris.size() << " GPS entries");
	//DBGMSG(debugStream,INFO,"Read " << beidou.ephemeris.size() << " BeiDou entries");
	
	return true;
}

void RINEXNavFile::dump()
{
}

//
//	Private
//		

void RINEXNavFile::init()
{
}

		
bool RINEXNavFile::readV2File(std::string)
{
	return false;
}

bool RINEXNavFile::readV3File(std::string fname)
{
	unsigned int lineCount=0;	
	std::ifstream fin;
	fin.open(fname.c_str()); // already tested 'good'
	
	// Parse the header
	int gnss = -1;
	int ibuf;
	std::string line,str;
	
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
		
		if (std::string::npos != line.find("IONOSPHERIC CORR",60)){
			if (std::string::npos != line.find("GPSA")){
					readParam(line,6,12, &(gps.ionoData.a0));
					readParam(line,18,12,&(gps.ionoData.a1));
					readParam(line,30,12,&(gps.ionoData.a2));
					readParam(line,42,12,&(gps.ionoData.a3));
					DBGMSG(debugStream,TRACE,"read GPS ION ALPHA " << gps.ionoData.a0 << " " << gps.ionoData.a1 << " " << gps.ionoData.a2 << " " << gps.ionoData.a3);
					continue;
			}
			if (std::string::npos != line.find("GPSB")){
				readParam(line,6,12, &(gps.ionoData.B0));
				readParam(line,18,12,&(gps.ionoData.B1));
				readParam(line,30,12,&(gps.ionoData.B2));
				readParam(line,42,12,&(gps.ionoData.B3));
				DBGMSG(debugStream,TRACE,"read GPS ION BETA " << gps.ionoData.B0 << " " << gps.ionoData.B1 << " " << gps.ionoData.B2 << " " << gps.ionoData.B3);
				continue;
			}
			continue;
		}

		if (std::string::npos != line.find("TIME SYSTEM CORR",60)){
			
			if (std::string::npos != line.find("GPUT")){
				readParam(line,6,17,&(gps.UTCdata.A0));
				readParam(line,23,16,&(gps.UTCdata.A1));
				readParam(line,39,7,&(gps.UTCdata.t_ot));
				readParam(line,46,5,&ibuf);gps.UTCdata.WN_t = ibuf; // this is a full week number
				DBGMSG(debugStream,TRACE,"read GPS UT: " << "WN " << gps.UTCdata.WN_t);
				continue;
			}
			continue;
		}
		
		if  (std::string::npos != line.find("LEAP SECONDS",60)){
			readParam(line,1,6,&leapsecs);
			DBGMSG(debugStream,TRACE,"read LEAP SECONDS: " << leapsecs);
			continue;
		}
		
		if (std::string::npos != line.find("END OF HEADER",60)){ 
			break;
		}
	} // reading header
	
	// Parse the data
	if (fin.eof()){
		app->logMessage("Format error (no END OF HEADER) in " + fname);
		return false;
	}
	
	while (!fin.eof()){
		
		std::getline(fin,line);
		lineCount++;
		
		DBGMSG(debugStream,TRACE,line);
		
		// skip blank lines
		std::string tst(line);
		boost::trim(tst);
		if (tst.length()==0)
			continue;
		
		//if (strlen(line) < 79) // FIXME why did I have this
		//	continue;
		
		char satSys = line[0];
		switch (satSys){
			case 'G':
				{GPSEphemeris *ed = readGPSEphemeris(fin,line,&lineCount); gps.addEphemeris(ed);}
				break;
			case 'E':
				{ for (int i=0;i<7;i++){ std::getline(fin,line);} lineCount += 7; continue;} // a big TODO
			case 'R':
				{ for (int i=0;i<3;i++){ std::getline(fin,line);} lineCount += 3; continue;}
			case 'C': // BDS
				{ for (int i=0;i<7;i++){ std::getline(fin,line);} lineCount += 7; continue;}
			case 'J': // QZSS
				{ for (int i=0;i<7;i++){ std::getline(fin,line);} lineCount += 7; continue;}
			case 'S': // SBAS
				{ for (int i=0;i<3;i++){ std::getline(fin,line);} lineCount += 3; continue;}
			case 'I': // IRNS
				{ for (int i=0;i<7;i++){ std::getline(fin,line);} lineCount += 7; continue;}
			default:
				break;
		}
	}
	return true;
}

GPSEphemeris * RINEXNavFile::readGPSEphemeris(std::ifstream &fin,std::string &line, unsigned int *lineCount){
	GPSEphemeris *ed = NULL;
	
	ed = new GPSEphemeris();
	
	int ibuf;
	double dbuf;
	
	int year,mon,mday,hour,mins;
	double secs;
	int startCol;
	
	if (version  < 3){
		startCol=4;
		// Line 1: format is I2,5I3,F5.1,3D19.12
		readParam(line,1,2,&ibuf); ed->SVN = ibuf;	
		readParam(line,3,3,&year);
		readParam(line,6,3,&mon);
		readParam(line,9,3,&mday);
		readParam(line,12,3,&hour);
		readParam(line,15,3,&mins);
		readParam(line,18,5,&secs);
		readParam(line,23,19,&dbuf);ed->a_f0=dbuf;
		readParam(line,42,19,&dbuf);ed->a_f1=dbuf;
		readParam(line,61,19,&dbuf);ed->a_f2=dbuf;
	}
	else if (version < 4){
		startCol=5;
		readParam(line,2,2,&ibuf); ed->SVN = ibuf;	
		readParam(line,5,4,&year);
		readParam(line,9,3,&mon);
		readParam(line,12,3,&mday);
		readParam(line,15,3,&hour);
		readParam(line,18,3,&mins);
		readParam(line,21,3,&secs);
		readParam(line,24,19,&dbuf);ed->a_f0=dbuf;
		readParam(line,43,19,&dbuf);ed->a_f1=dbuf;
		readParam(line,62,19,&dbuf);ed->a_f2=dbuf;
	
	}
	
	DBGMSG(debugStream,TRACE,"ephemeris for SVN " << (int) ed->SVN << " " << hour << ":" << mins << ":" <<  secs);
	
	// Lines 2-8: 3X,4D19.12
	double dbuf1,dbuf2,dbuf3,dbuf4;
	
	read4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->IODE=dbuf1; ed->C_rs=dbuf2; ed->delta_N=dbuf3; ed->M_0=dbuf4; // variables are a mix of SINGLE and DOUBLE
	
	read4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->C_uc=dbuf1; ed->e=dbuf2; ed->C_us=dbuf3; ed->sqrtA=dbuf4;;
	
	read4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->t_0e=dbuf1; ed->C_ic=dbuf2; ed->OMEGA_0=dbuf3; ed->C_is=dbuf4;
	
	read4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->i_0=dbuf1; ed->C_rc=dbuf2; ed->OMEGA=dbuf3; ed->OMEGADOT=dbuf4; // note OMEGADOT read in as DOUBLE but stored as SINGLE so in != out
	
	read4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->IDOT=dbuf1; ed->week_number= dbuf3; // don't truncate WN just yet
	
	read4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->SV_health=dbuf2; ed->t_GD=dbuf3; ed->IODC=dbuf4;
	int i=0;
	ed->SV_accuracy_raw=0.0;
	ed->SV_accuracy=dbuf1;
	while (GPS::URA[i] > 0){
		if (GPS::URA[i] == dbuf1){
			ed->SV_accuracy_raw=i;
			break;
		}
		i++;
	}
	read4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->t_ephem=dbuf1;
	
	// Calculate t_OC - the clock data reference time
	
	// First, work out the century.
	struct tm tmGPS; // origin of GPS time
	tmGPS.tm_sec=tmGPS.tm_min=tmGPS.tm_hour=0;
	tmGPS.tm_mday=6;tmGPS.tm_mon=0;tmGPS.tm_year=1980-1900,tmGPS.tm_isdst=0;
	time_t tGPS0=mktime(&tmGPS);
	time_t ttmp= tGPS0 + ed->week_number*7*86400;
	struct tm *tmtmp=gmtime(&ttmp);
	int century=(tmtmp->tm_year/100)*100+1900;
	
	// Then, 'full' t_OC so we can get wday
	tmGPS.tm_sec=(int) secs; 
	tmGPS.tm_min=mins;
	tmGPS.tm_hour=hour;
	tmGPS.tm_mday=mday;
	tmGPS.tm_mon=mon-1;
	tmGPS.tm_year=year+century-1900;
	tmGPS.tm_isdst=0;
	mktime(&tmGPS); // this sets wday
	
	// Then 
	ed->t_OC = secs+mins*60+hour*3600+tmGPS.tm_wday*86400;
	
	// Now truncate WN
	ed->week_number = ed->week_number - 1024*(ed->week_number/1024);
	
	return ed;
}

