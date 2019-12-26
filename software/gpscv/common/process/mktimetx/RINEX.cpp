//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Michael J. Wouters
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

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <cmath>
#include <cstdio>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp> // nb C++11 has this

#include "Antenna.h"
#include "BeiDou.h"
#include "Application.h"
#include "Counter.h"
#include "CounterMeasurement.h"
#include "Debug.h"
#include "MeasurementPair.h"
#include "Receiver.h"
#include "ReceiverMeasurement.h"
#include "RINEX.h"
#include "Utility.h"

extern Application *app;
extern std::ostream *debugStream;

char gsbuf[256];

#define SBUFSIZE 160

//
// Public methods
//
		
RINEX::RINEX()
{
	init();
}

bool RINEX::writeObservationFile(Antenna *ant, Counter *cntr, Receiver *rx,int majorVer,int minorVer,std::string fname,int mjd,int interval, MeasurementPair **mpairs,bool TICenabled)
{
	char buf[81];
	FILE *fout;
	if (!(fout = std::fopen(fname.c_str(),"w"))){
		return false;
	}
	
	int useTIC = (TICenabled?1:0);
	DBGMSG(debugStream,1,"Writing  " << fname);
	DBGMSG(debugStream,1,"Using TIC = " << (TICenabled? "yes":"no"));
	
	char obs;
	switch (rx->constellations){
		case GNSSSystem::GPS: obs='G';break;
		case GNSSSystem::GLONASS: obs='R';break;
		case GNSSSystem::GALILEO: obs='E';break;
		case GNSSSystem::BEIDOU:obs='C';break; 
		default: obs= 'M';break;
	}
	
	std::fprintf(fout,"%9s%11s%-20s%c%-19s%-20s\n",makeVerName(majorVer,minorVer).c_str(),"","O",obs,"","RINEX VERSION / TYPE");
	
	time_t tnow = time(NULL);
	struct tm *tgmt = gmtime(&tnow);
	
	switch (majorVer){
		case V2:
		{
			std::strftime(buf,80,"%d-%b-%y %T",tgmt);
			std::fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			break;
		}
		case V3:
		{
			std::snprintf(buf,80,"%04d%02d%02d %02d%02d%02d UTC",tgmt->tm_year+1900,tgmt->tm_mon+1,tgmt->tm_mday,
					 tgmt->tm_hour,tgmt->tm_min,tgmt->tm_sec);
			std::fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			break;
		}
		default:break;
	}
	
	std::fprintf(fout,"%-60s%-20s\n",ant->markerName.c_str(),"MARKER NAME");
	std::fprintf(fout,"%-20s%40s%-20s\n",ant->markerNumber.c_str(),"","MARKER NUMBER");
    if (V3 == majorVer){
        std::fprintf(fout,"%-20s%40s%-20s\n",ant->markerType.c_str(),"","MARKER TYPE");
    }
	std::fprintf(fout,"%-20s%-40s%-20s\n",observer.c_str(),agency.c_str(),"OBSERVER / AGENCY");
	std::fprintf(fout,"%-20s%-20s%-20s%-20s\n",
		rx->serialNumber.c_str(),rx->modelName.c_str(),rx->version1.c_str(),"REC # / TYPE / VERS");
	std::fprintf(fout,"%-20s%-20s%-20s%-20s\n",ant->antennaNumber.c_str(),ant->antennaType.c_str()," ","ANT # / TYPE");
	std::fprintf(fout,"%14.4lf%14.4lf%14.4lf%-18s%-20s\n",ant->x,ant->y,ant->z," ","APPROX POSITION XYZ");
	std::fprintf(fout,"%14.4lf%14.4lf%14.4lf%-18s%-20s\n",ant->deltaH,ant->deltaE,ant->deltaN," ","ANTENNA: DELTA H/E/N");
	
	
	switch (majorVer){
		case V2:
		{
			int nobs=0;
			std::string obsTypes="";
			if (rx->constellations & GNSSSystem::GPS) {
				for (int c=GNSSSystem::C1C;c<GNSSSystem::NONE;c<<=1){
					if (rx->gps.codes & c){
						nobs++;
						obsTypes += "    " + rx->gps.observationCodeToStr(c,RINEX::V2);
					}
				}
			}
			if (rx->constellations & GNSSSystem::GLONASS) {
				if ( (std::string::npos == obsTypes.find("C1") ) ){  // if C1 not there
					obsTypes = "    C1" + obsTypes;
					nobs++;
				}
			}
			// V2.11 doesn't support the other constellations
			// FIXME linewrap ?
			std::fprintf(fout,"%6d%-54s%-20s\n",nobs,obsTypes.c_str(),"# / TYPES OF OBSERV");
			break;
		}
		case V3:
		{
			int nobs=0;
			std::string obsTypes="";
			if (rx->constellations & GNSSSystem::GPS) {
				for (int c=GNSSSystem::C1C;c<GNSSSystem::NONE;c<<=1){
					if (rx->gps.codes & c){
						nobs++;
						obsTypes += " " + rx->gps.observationCodeToStr(c,RINEX::V3);
					}
				}
				// Less than 13 observations so no linewrap
				if ((nobs) > 0)
					std::fprintf(fout,"%1s  %3d%-54s%-20s\n","G",nobs,obsTypes.c_str(),"SYS / # / OBS TYPES");
			}
			
			nobs=0;
			obsTypes="";
			if (rx->constellations & GNSSSystem::BEIDOU) {
				for (int c=GNSSSystem::C1C;c<GNSSSystem::NONE;c<<=1){
					if (rx->beidou.codes & c){
						nobs++;
						obsTypes += " " + rx->beidou.observationCodeToStr(c,RINEX::V3);
					}
				}
				if (nobs > 0)
					std::fprintf(fout,"%1s  %3d%-54s%-20s\n","C",nobs,obsTypes.c_str(),"SYS / # / OBS TYPES");
			}
		
			nobs=0;
			obsTypes="";
			if (rx->constellations & GNSSSystem::GLONASS) {
				for (int c=GNSSSystem::C1C;c<GNSSSystem::NONE;c<<=1){
					if (rx->glonass.codes & c){
						nobs++;
						obsTypes += " " + rx->glonass.observationCodeToStr(c,RINEX::V3);
					}
				}
				if ((nobs) > 0)
					std::fprintf(fout,"%1s  %3d%-54s%-20s\n","R",nobs,obsTypes.c_str(),"SYS / # / OBS TYPES");
			}
			
			nobs=0;
			obsTypes="";
			if (rx->constellations & GNSSSystem::GALILEO) {
				for (int c=GNSSSystem::C1C;c<GNSSSystem::NONE;c<<=1){
					if (rx->galileo.codes & c){
						nobs++;
						obsTypes += " " + rx->galileo.observationCodeToStr(c,RINEX::V3);
					}
				}
				if ((nobs) > 0)
					std::fprintf(fout,"%1s  %3d%-54s%-20s\n","E",nobs,obsTypes.c_str(),"SYS / # / OBS TYPES");
			}
			break;
		}
		default:break;
	}
	
	// Find the first observation
	// There is a problem here because we log in UTC time: GPS day starts N leapsceonds before UTC day
	// so the nominal first observation of the GPS day is in the previous file.
	int obsTime=0;
	int currMeas=0;
	int yday=-1; // to track day rollovers
	int nDayRollovers=0;
	while (currMeas < 86400 && obsTime <= 86400){
		if (mpairs[currMeas]->flags==0x03){
			ReceiverMeasurement *rm = mpairs[currMeas]->rm;
			if (yday==-1){
				yday=rm->tmGPS.tm_yday;
			}
			if (rm->tmGPS.tm_yday != yday){
				nDayRollovers++;
				yday = rm->tmGPS.tm_yday;
			}
			// Round the measurement time to the nearest second, accounting for any fractional part of the second)
			int tMeas= nDayRollovers*86400 + (int) rint(rm->tmGPS.tm_hour*3600+rm->tmGPS.tm_min*60+rm->tmGPS.tm_sec + rm->tmfracs);
			if (tMeas==obsTime){
				std::fprintf(fout,"%6d%6d%6d%6d%6d%13.7lf%-5s%3s%-9s%-20s\n",
					rm->tmGPS.tm_year+1900,rm->tmGPS.tm_mon+1,rm->tmGPS.tm_mday,rm->tmGPS.tm_hour,rm->tmGPS.tm_min,
					(double) (rm->tmGPS.tm_sec+rm->tmfracs),
					" ", "GPS"," ","TIME OF FIRST OBS");
				break;
			}
			else if (tMeas < obsTime)
				currMeas++;
			else if (tMeas > obsTime)
				obsTime += interval;
		}
		else
			currMeas++;
	}
	if (rx->leapsecs > 0){
        std::fprintf(fout,"%6d%54s%-20s\n",rx->leapsecs," ","LEAP SECONDS");
    }
    else{
        DBGMSG(debugStream,INFO,"LEAP SECONDS unknown so omitted");
    }
	std::fprintf(fout,"%60s%-20s\n","","END OF HEADER");
	
	obsTime=0;
	currMeas=0;
	yday=-1; 
	nDayRollovers=0;
	while (currMeas < 86400 && obsTime <= 86400){
		if (mpairs[currMeas]->flags==0x03){
			ReceiverMeasurement *rm = mpairs[currMeas]->rm;
			if (yday==-1){
				yday=rm->tmGPS.tm_yday;
			}
			if (rm->tmGPS.tm_yday != yday){
				nDayRollovers++;
				yday = rm->tmGPS.tm_yday;
			}
			// pps = counter measurement + sawtooth correction - programmed offset
			double ppsTime = useTIC*(rm->cm->rdg + rm->sawtooth - rx->ppsOffset*1.0E-9); // correction to the local clock
			
			// Round the measurement time to the nearest second, accounting for any fractional part of the second)
			int tMeas= nDayRollovers*86400 + (int) rint(rm->tmGPS.tm_hour*3600+rm->tmGPS.tm_min*60+rm->tmGPS.tm_sec + rm->tmfracs);
			if (tMeas==obsTime){
				
				// determine all space vehicle identifiers, noting that we may not have all measurements for all observation types
				std::vector<std::string> svids;
				std::vector<int>    svns;
				std::vector<int>    svsys;
				char sbuf[4];
				for (unsigned int i=0;i<rm->meas.size();i++){
					std::string svconst;
					switch (rm->meas.at(i)->constellation){
						case GNSSSystem::GPS: svconst='G';break;
						case GNSSSystem::GLONASS: svconst='R';break;
						case GNSSSystem::GALILEO: svconst='E';break;
						case GNSSSystem::BEIDOU: svconst='C';break;
						default:break;
					}
					// Need to check if this combination is already present
					bool gotIt = false;
					std::sprintf(sbuf,"%s%02d",svconst.c_str(),rm->meas.at(i)->svn);
					for (unsigned int id=0;id<svids.size();id++){
						if (NULL != strstr(sbuf,svids.at(id).c_str())){
							gotIt=true;
							break;
						}
					}
					if (!gotIt){
						svids.push_back(sbuf);
						svns.push_back(rm->meas.at(i)->svn);
						svsys.push_back(rm->meas.at(i)->constellation);
					}
				}
				
				// Record header
				switch (majorVer){
					case V2:
					{
						int allsv = svids.size();
						int nsv=0; // number to output
						
						for ( int sv=0;sv<allsv;sv++){ // RINEX v2.11 only supports GPS and GLONASS so only count those
							if (majorVer == RINEX::V2){
								if (svids[sv].c_str()[0]=='C') continue;
								if (svids[sv].c_str()[0]=='E') continue;
							}
							nsv++;
						}
						int yy = rm->tmGPS.tm_year - 100*(rm->tmGPS.tm_year/100);
						std::fprintf(fout," %02d %2d %2d %2d %2d%11.7lf  %1d%3d",
							yy,rm->tmGPS.tm_mon+1,rm->tmGPS.tm_mday,rm->tmGPS.tm_hour,rm->tmGPS.tm_min,
							(double) (rm->tmGPS.tm_sec+rm->tmfracs),
							rm->epochFlag,nsv);
			
						int svcount=0;
						for ( int sv=0;sv<allsv;sv++){
							if (majorVer == RINEX::V2){ // skip BeiDou and Galileo
								if (svids[sv].c_str()[0]=='C') continue;
								if (svids[sv].c_str()[0]=='E') continue;
							}
							svcount++;
							std::fprintf(fout,"%s",svids[sv].c_str());
							if ((nsv > 12) && ((svcount % 12)==0)){ // more to do, so start a continuation line
								std::fprintf(fout,"\n%32s","");
							}
						}
						std::fprintf(fout,"\n"); // CHECK does this work OK when there are no observations
						break;
					}
					case V3:
					{
						std::fprintf(fout,"> %4d %2.2d %2.2d %2.2d %2.2d%11.7f  %1d%3d%6s%15.12lf\n",
							rm->tmGPS.tm_year+1900,rm->tmGPS.tm_mon+1,rm->tmGPS.tm_mday,rm->tmGPS.tm_hour,rm->tmGPS.tm_min,(double) rm->tmGPS.tm_sec,
							rm->epochFlag,(int) svids.size()," ",0.0);
						
					} // case V3
				} // switch (RINEXversion)
				
				// SV measurements
				for (unsigned int sv=0;sv<svids.size();sv++){
					
					if (majorVer == V3)
						std::fprintf(fout,"%s",svids[sv].c_str());
					
					switch (svsys[sv])
					{
						case GNSSSystem::BEIDOU:
						{	
							if (majorVer==V2) continue;
							for (unsigned int c=GNSSSystem::C1C;c<GNSSSystem::NONE;c<<=1){
								bool foundit=false;
								for (unsigned int svc=0;svc<rm->meas.size();svc++){
									if (rm->meas[svc]->svn == svns[sv] && rm->meas[svc]->constellation == svsys[sv] &&  rm->meas[svc]->code == c){
										std::fprintf(fout,"%14.3lf%2s",(rm->meas[svc]->meas+ppsTime)*CVACUUM,formatFlags(rm->meas[svc]->lli,rm->meas[svc]->signal));
										foundit=true;
										break;
									}
								}
								if ((c & rx->beidou.codes) && !foundit) std::fprintf(fout,"%16s"," "); 
							}
							std::fprintf(fout,"\n");
							break;
						} // case GNSSSystem::BEIDOU:
						
						case GNSSSystem::GALILEO:
						{	
							if (majorVer==V2) continue;
							for (unsigned int c=GNSSSystem::C1C;c<GNSSSystem::NONE;c<<=1){
								bool foundit=false;
								for (unsigned int svc=0;svc<rm->meas.size();svc++){
									if (rm->meas[svc]->svn == svns[sv] && rm->meas[svc]->constellation == svsys[sv] &&  rm->meas[svc]->code == c){
										std::fprintf(fout,"%14.3lf%2s",(rm->meas[svc]->meas+ppsTime)*CVACUUM,formatFlags(rm->meas[svc]->lli,rm->meas[svc]->signal));
										foundit=true;
										break;
									}
								}
								if ((c & rx->galileo.codes) && !foundit) std::fprintf(fout,"%16s"," "); 
							}
							
							std::fprintf(fout,"\n");
							break;
						} // case GNSSSystem::GALILEO
						
						case GNSSSystem::GLONASS:
						{	
							for (unsigned int c=GNSSSystem::C1C;c<GNSSSystem::NONE;c<<=1){
								bool foundit=false;
								double freq = rx->glonass.codeToFreq(c);
								for (unsigned int svc=0;svc<rm->meas.size();svc++){
									if (rm->meas[svc]->svn == svns[sv] && rm->meas[svc]->constellation == svsys[sv] &&  rm->meas[svc]->code == c){
										if (c < 0x10000){ // code observations
											std::fprintf(fout,"%14.3lf%2s",(rm->meas[svc]->meas+ppsTime)*CVACUUM,formatFlags(rm->meas[svc]->lli,rm->meas[svc]->signal));
										}
										else if (c >= 0x10000 && allObservations){ // phase observations, which can be masked out
											std::fprintf(fout,"%14.3lf%2s",rm->meas[svc]->meas+ppsTime*freq,formatFlags(rm->meas[svc]->lli,rm->meas[svc]->signal));
										}
										foundit=true;
										break;
									}
								}
								if ((c & rx->glonass.codes) && !foundit) std::fprintf(fout,"%16s"," "); 
							}
							
							std::fprintf(fout,"\n");
							break;
						} // case GNSSSystem::GLONASS
						
						case GNSSSystem::GPS:
						{
							for (unsigned int c=GNSSSystem::C1C;c<GNSSSystem::NONE;c<<=1){
								bool foundit=false;
								double freq = rx->gps.codeToFreq(c);
								for (unsigned int svc=0;svc<rm->meas.size();svc++){
									if (rm->meas[svc]->svn == svns[sv] && rm->meas[svc]->constellation == svsys[sv] &&  rm->meas[svc]->code == c){
										//std::fprintf(fout,"%14.3lf%1i%1i",(rm->meas[svc]->meas+ppsTime)*CVACUUM,rm->meas[svc]->lli,rm->meas[svc]->signal);
										if (c < 0x10000){ // code observations
											std::fprintf(fout,"%14.3lf%2s",(rm->meas[svc]->meas+ppsTime)*CVACUUM,formatFlags(rm->meas[svc]->lli,rm->meas[svc]->signal));
										}
										else if (c >= 0x10000 && allObservations){ // phase observations, which can be masked out
											
											std::fprintf(fout,"%14.3lf%2s",rm->meas[svc]->meas+ppsTime*freq,formatFlags(rm->meas[svc]->lli,rm->meas[svc]->signal));
										}
										foundit=true;
										break;
									}
								}
								if ((c & rx->gps.codes) && !foundit) std::fprintf(fout,"%16s"," "); 
							}
							std::fprintf(fout,"\n");
							break;
						}
					}
				}
				obsTime+=interval;
				currMeas++;
			}
			else if (tMeas < obsTime){
				currMeas++;
			}
			else if (tMeas > obsTime){
				obsTime += interval;
			}
		}
		else{
			currMeas++;
		}
	}
	std::fclose(fout);

	return true;
}

bool  RINEX::writeNavigationFile(Receiver *rx,int constellation,int majorVer,int minorVer,std::string fname,int mjd)
{
	switch (constellation)
	{
		case GNSSSystem::BEIDOU:return writeBeiDouNavigationFile(rx,majorVer,minorVer,fname,mjd);break;
		case GNSSSystem::GPS:return writeGPSNavigationFile(rx,majorVer,minorVer,fname,mjd);break;
		default: break;
	}
	return false;
}

bool RINEX::readNavigationFile(Receiver *rx,int constellation,std::string fname){
	
	unsigned int lineCount=0;
	
	FILE *fin;
	char line[SBUFSIZE];
	
	// Test for directory name since fopen() will return  non-NULL on a directory
	struct stat sstat;
	if (0==stat(fname.c_str(),&sstat)){
		if (S_ISDIR(sstat.st_mode)){
			app->logMessage(fname + " is not a regular file");
			return false;
		}
	}
	else{
		app->logMessage("Unable to stat the navigation file " + fname);
		return false;
	}
	if (NULL == (fin=std::fopen(fname.c_str(),"r"))){
		app->logMessage("Unable to open the navigation file " + fname);
		return false;
	}

	// First, determine the version
	double RINEXver;
	
	while (!std::feof(fin)){
		std::fgets(line,SBUFSIZE,fin);
		lineCount++;
		if (NULL != strstr(line,"RINEX VERSION")){
			parseParam(line,1,12,&RINEXver);
			break;
		}
	}

	if (std::feof(fin)){
		app->logMessage("Unable to determine the RINEX version in " + fname);
		return false;
	}
	
	DBGMSG(debugStream,TRACE,"RINEX version is " << RINEXver);
	
	std::fclose(fin); // the first line needs to be reread so just close the file (could rewind ..)
	
	if (RINEXver < 3){
		readV2NavigationFile(rx,constellation,fname);
	}	
	else if (RINEXver < 4){
		readV3NavigationFile(rx,constellation,fname);
	}
	else{
	}
	
	//if (ephemeris.size() == 0){
	//	app->logMessage("Empty navigation file " + fname);
	//	return false;
	//}
	DBGMSG(debugStream,INFO,"Read " << rx->gps.ephemeris.size() << " GPS entries");
	DBGMSG(debugStream,INFO,"Read " << rx->beidou.ephemeris.size() << " BeiDou entries");
	return true;
}

std::string RINEX::makeFileName(std::string pattern,int mjd)
{
	std::string ret="";
	int year,mon,mday,yday;
	Utility::MJDtoDate(mjd,&year,&mon,&mday,&yday);
	int yy = year - (year/100)*100;
	
	boost::regex rnx1("(\\w{4})[D|d]{3}(0\\.)[Y|y]{2}([nNoO])");
	boost::smatch matches;
	if (boost::regex_search(pattern,matches,rnx1)){
		char tmp[16];
		// this is ugly but C++ stream manipulators are even uglier
		std::snprintf(tmp,15,"%s%03d%s%02d%s",matches[1].str().c_str(),yday,matches[2].str().c_str(),yy,matches[3].str().c_str());
		ret = tmp;
	}
	return ret;
}



//
// Private members
//

bool  RINEX::writeBeiDouNavigationFile(Receiver *rx,int majorVer,int minorVer,std::string fname,int mjd)
{
	if (majorVer != RINEX::V3) return false;
	
	char buf[81];
	FILE *fout;
	if (!(fout = std::fopen(fname.c_str(),"w"))){
		return false;
	}
	
	std::fprintf(fout,"%9s%11s%-20s%-20s%-20s\n",makeVerName(majorVer,minorVer).c_str(),"","N: GNSS NAV DATA","C: BDS","RINEX VERSION / TYPE");
	time_t tnow = time(NULL);
	struct tm *tgmt = gmtime(&tnow);
	std::snprintf(buf,80,"%04d%02d%02d %02d%02d%02d UTC",tgmt->tm_year+1900,tgmt->tm_mon+1,tgmt->tm_mday,
		tgmt->tm_hour,tgmt->tm_min,tgmt->tm_sec);
	std::fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
	
	// FIXME incomplete - need SV supplying ionosphere corrections
	std::fprintf(fout,"BDSA %12.4e%12.4e%12.4e%12.4e%7s%-20s\n",
		rx->beidou.ionoData.a0,rx->beidou.ionoData.a1,rx->beidou.ionoData.a2,rx->beidou.ionoData.a3,"","IONOSPHERIC CORR");
	std::fprintf(fout,"BDSB %12.4e%12.4e%12.4e%12.4e%7s%-20s\n",
		rx->beidou.ionoData.b0,rx->beidou.ionoData.b1,rx->beidou.ionoData.b2,rx->beidou.ionoData.b3,"","IONOSPHERIC CORR");
	std::fprintf(fout,"%6d%54s%-20s\n",rx->leapsecs," ","LEAP SECONDS");
	std::fprintf(fout,"%60s%-20s\n"," ","END OF HEADER");
	
	for (unsigned int i=0;i<rx->beidou.ephemeris.size();i++){
		BeiDou::EphemerisData *ed = rx->beidou.ephemeris[i];
		
		std::fprintf(fout,"C%02d %4d %02d %02d %02d %02d %02d%19.12e%19.12e%19.12e\n",ed->SVN,
					ed->year,ed->mon,ed->mday,ed->hour,ed->mins,ed->secs,
				ed->a_0,ed->a_1,ed->a_2);
				
		std::snprintf(buf,80,"%%4s%%19.12e%%19.12e%%19.12e%%19.12e\n");
		
		std::fprintf(fout,buf," ", // broadcast orbit 1
			(double) ed->AODE,ed->C_rs,ed->delta_N,ed->M_0);
				
		std::fprintf(fout,buf," ", // broadcast orbit 2
			ed->C_uc,ed->e,ed->C_us,ed->sqrtA);
		
		std::fprintf(fout,buf," ", // broadcast orbit 3
			ed->t_oe,ed->C_ic,ed->OMEGA_0,ed->C_is);
		
		std::fprintf(fout,buf," ", // broadcast orbit 4
			ed->i_0,ed->C_rc,ed->OMEGA,ed->OMEGADOT);
		
		std::fprintf(fout,buf," ", // broadcast orbit 5
			ed->IDOT,0.0,(double) ed->WN,0.0);
	
		std::fprintf(fout,buf," ", // broadcast orbit 6
			(double) ed->URAI,(double) ed->SatH1,ed->t_GD1,ed->t_GD2);

		std::fprintf(fout,buf," ", // broadcast orbit 7
			ed->tx_e,ed->AODC,0.0,0.0);
		
	}
	
	std::fclose(fout);
	
	return true;
}

bool  RINEX::writeGPSNavigationFile(Receiver *rx,int majorVer,int minorVer,std::string fname,int mjd)
{
	char buf[81];
	FILE *fout;
	if (!(fout = std::fopen(fname.c_str(),"w"))){
		return false;
	}
	
	switch (majorVer)
	{
		case V2:
			std::fprintf(fout,"%9s%11s%1s%-39s%-20s\n",makeVerName(majorVer,minorVer).c_str(),"","N","","RINEX VERSION / TYPE");
			break;
		case V3:
			std::fprintf(fout,"%9s%11s%-20s%-20s%-20s\n",makeVerName(majorVer,minorVer).c_str(),"","N: GNSS NAV DATA","G: GPS","RINEX VERSION / TYPE");
			break;
		default:
			break;
	}
	
	time_t tnow = std::time(NULL);
	struct tm *tgmt = std::gmtime(&tnow);
	
	// Determine the GPS week number FIXME why am I not using the receiver-provided WN_t ?
	// GPS week 0 begins midnight 5/6 Jan 1980, MJD 44244
	int gpsWeek=int ((mjd-44244)/7);
	switch (majorVer)
	{
		case V2:
		{
			std::strftime(buf,80,"%d-%b-%y %T",tgmt);
			std::fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			std::fprintf(fout,"%2s%12.4e%12.4e%12.4e%12.4e%10s%-20s\n","",
				rx->gps.ionoData.a0,rx->gps.ionoData.a1,rx->gps.ionoData.a2,rx->gps.ionoData.a3,"","ION ALPHA");
			std::fprintf(fout,"%2s%12.4e%12.4e%12.4e%12.4e%10s%-20s\n","",
				rx->gps.ionoData.B0,rx->gps.ionoData.B1,rx->gps.ionoData.B2,rx->gps.ionoData.B3,"","ION BETA");
			std::fprintf(fout,"%3s%19.12e%19.12e%9d%9d %-20s\n","",
				rx->gps.UTCdata.A0,rx->gps.UTCdata.A1,(int) rx->gps.UTCdata.t_ot, gpsWeek,"DELTA-UTC: A0,A1,T,W"); // FIXME gpsWeek is NOT right !
			break;
		}
		case V3:
		{
			std::snprintf(buf,80,"%04d%02d%02d %02d%02d%02d UTC",tgmt->tm_year+1900,tgmt->tm_mon+1,tgmt->tm_mday,
					 tgmt->tm_hour,tgmt->tm_min,tgmt->tm_sec);
			std::fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			std::fprintf(fout,"GPSA %12.4e%12.4e%12.4e%12.4e%7s%-20s\n",
					rx->gps.ionoData.a0,rx->gps.ionoData.a1,rx->gps.ionoData.a2,rx->gps.ionoData.a3,"","IONOSPHERIC CORR");
			std::fprintf(fout,"GPSB %12.4e%12.4e%12.4e%12.4e%7s%-20s\n",
					rx->gps.ionoData.B0,rx->gps.ionoData.B1,rx->gps.ionoData.B2,rx->gps.ionoData.B3,"","IONOSPHERIC CORR");
			std::fprintf(fout,"GPUT %17.10e%16.9e%7d%5d %5s %2d %-20s\n",rx->gps.UTCdata.A0,rx->gps.UTCdata.A1,(int) rx->gps.UTCdata.t_ot,gpsWeek," ", 0,"TIME SYSTEM CORR");
			break;
		}
	}
	
	std::fprintf(fout,"%6d%54s%-20s\n",rx->leapsecs," ","LEAP SECONDS");
	std::fprintf(fout,"%60s%-20s\n"," ","END OF HEADER");
	
	// Write out the ephemeris entries
	int lastGPSWeek=-1;
	int lastToc=-1;
	int weekRollovers=0;
	struct tm tmGPS0;
	tmGPS0.tm_sec=tmGPS0.tm_min=tmGPS0.tm_hour=0;
	tmGPS0.tm_mday=6;tmGPS0.tm_mon=0;tmGPS0.tm_year=1980-1900,tmGPS0.tm_isdst=0;
	time_t tGPS0=std::mktime(&tmGPS0);
	for (unsigned int i=0;i<rx->gps.ephemeris.size();i++){
			
		// Account for GPS rollover:
		// GPS week 0 begins midnight 5/6 Jan 1980, MJD 44244
		// GPS week 1024 begins midnight 21/22 Aug 1999, MJD 51412
		// GPS week 2048 begins midnight 6/7 Apr 2019, MJD 58580
		int tmjd=mjd;
		int GPSWeek=rx->gps.ephemeris[i]->week_number;
		
		while (tmjd>=51412) {
			GPSWeek+=1024;
			tmjd-=(7*1024);
		}
		if (-1==lastGPSWeek){lastGPSWeek=GPSWeek;}
		// FIXME if we have read in a GPS navigation file then we should use Toc as given in the file
		// (but why would you read it in and then write it out, except for debugging ??)
		// Convert GPS week + $Toc to epoch as year, month, day, hour, min, sec
		// Note that the epoch should be specified in GPS time
		double Toc=rx->gps.ephemeris[i]->t_OC;
		if (-1==lastToc) {lastToc = Toc;}
		// If GPS week is unchanged and Toc has gone backwards by more than 2 days, increment GPS week
		// It is assumed that ephemeris entries have been correctly ordered (using fixWeeKRollovers() prior to writing out
		if ((GPSWeek == lastGPSWeek) && (Toc-lastToc < -2*86400)){
			weekRollovers=1;
		}
		else if (GPSWeek == lastGPSWeek+1){//OK now 
			weekRollovers=0; 	
		}
		
		lastGPSWeek=GPSWeek;
		lastToc=Toc;
		
		GPSWeek = GPSWeek + weekRollovers;
		
		double t=Toc;
		int day=(int) t/86400;
		t-=86400*day;
		int hour=(int) t/3600;
		t-=3600*hour;
		int minute=(int) t/60;
		t-=60*minute;
		int second=t;
	
		time_t tgps = tGPS0+GPSWeek*86400*7+Toc;
		struct tm *tmGPS = std::gmtime(&tgps);
		
		switch (majorVer)
		{
			case V2:
			{
				int yy = tmGPS->tm_year-100*(tmGPS->tm_year/100);
				std::fprintf(fout,"%02d %02d %02d %02d %02d %02d%5.1f%19.12e%19.12e%19.12e\n",rx->gps.ephemeris[i]->SVN,
					yy,tmGPS->tm_mon+1,tmGPS->tm_mday,hour,minute,(float) second,
					rx->gps.ephemeris[i]->a_f0,rx->gps.ephemeris[i]->a_f1,rx->gps.ephemeris[i]->a_f2);
				
				std::snprintf(buf,80,"%%3s%%19.12e%%19.12e%%19.12e%%19.12e\n");
			
				break;
			}
			case V3:
			{
				std::fprintf(fout,"G%02d %4d %02d %02d %02d %02d %02d%19.12e%19.12e%19.12e\n",rx->gps.ephemeris[i]->SVN,
					tmGPS->tm_year+1900,tmGPS->tm_mon+1,tmGPS->tm_mday,hour,minute,second,
					rx->gps.ephemeris[i]->a_f0,rx->gps.ephemeris[i]->a_f1,rx->gps.ephemeris[i]->a_f2);
				
				std::snprintf(buf,80,"%%4s%%19.12e%%19.12e%%19.12e%%19.12e\n");
				
				break;
			}
		}
		
		std::fprintf(fout,buf," ", // broadcast orbit 1
					(double) rx->gps.ephemeris[i]->IODE,rx->gps.ephemeris[i]->C_rs,rx->gps.ephemeris[i]->delta_N,rx->gps.ephemeris[i]->M_0);
				
		std::fprintf(fout,buf," ", // broadcast orbit 2
			rx->gps.ephemeris[i]->C_uc,rx->gps.ephemeris[i]->e,rx->gps.ephemeris[i]->C_us,rx->gps.ephemeris[i]->sqrtA);
		
		std::fprintf(fout,buf," ", // broadcast orbit 3
			rx->gps.ephemeris[i]->t_0e,rx->gps.ephemeris[i]->C_ic,rx->gps.ephemeris[i]->OMEGA_0,rx->gps.ephemeris[i]->C_is);
		
		std::fprintf(fout,buf," ", // broadcast orbit 4
			rx->gps.ephemeris[i]->i_0,rx->gps.ephemeris[i]->C_rc,rx->gps.ephemeris[i]->OMEGA,rx->gps.ephemeris[i]->OMEGADOT);
		
		std::fprintf(fout,buf," ", // broadcast orbit 5
			rx->gps.ephemeris[i]->IDOT,1.0,(double) GPSWeek,0.0);
	
		std::fprintf(fout,buf," ", // broadcast orbit 6
			GPS::URA[rx->gps.ephemeris[i]->SV_accuracy_raw],(double) rx->gps.ephemeris[i]->SV_health,rx->gps.ephemeris[i]->t_GD,(double) rx->gps.ephemeris[i]->IODC);
		
		std::fprintf(fout,buf," ", // broadcast orbit 7
			rx->gps.ephemeris[i]->t_ephem,4.0,0.0,0.0);
	}
	
	std::fclose(fout);
	
	return true;
}

bool RINEX::readV2NavigationFile(Receiver *rx,int constellation,std::string fname)
{
	unsigned int lineCount=0;
	
	FILE *fin;
	char line[SBUFSIZE];
				 
	if (NULL == (fin= std::fopen(fname.c_str(),"r"))){
		app->logMessage("Unable to open the navigation file " + fname);
		return false;
	}
	
	while (!std::feof(fin)){
		
		std::fgets(line,SBUFSIZE,fin);
		lineCount++;
		
		if (constellation == GNSSSystem::GPS){
			if (NULL != strstr(line,"ION ALPHA")){
				parseParam(line,3,12, &(rx->gps.ionoData.a0));
				parseParam(line,15,26,&(rx->gps.ionoData.a1));
				parseParam(line,27,38,&(rx->gps.ionoData.a2));
				parseParam(line,39,50,&(rx->gps.ionoData.a3));
				DBGMSG(debugStream,TRACE,"read ION ALPHA " << rx->gps.ionoData.a0 << " " << rx->gps.ionoData.a1 << " " << rx->gps.ionoData.a2 << " " << rx->gps.ionoData.a3);
				
			}
			else if (NULL != strstr(line,"ION BETA")){
				parseParam(line,3,14, &(rx->gps.ionoData.B0));
				parseParam(line,15,26,&(rx->gps.ionoData.B1));
				parseParam(line,27,38,&(rx->gps.ionoData.B2));
				parseParam(line,39,50,&(rx->gps.ionoData.B3));
				DBGMSG(debugStream,TRACE,"read ION BETA " << rx->gps.ionoData.B0 << " " << rx->gps.ionoData.B1 << " " << rx->gps.ionoData.B2 << " " << rx->gps.ionoData.B3);
			}
			else if (NULL != strstr(line,"DELTA-UTC:")){
				parseParam(line,4,22,&(rx->gps.UTCdata.A0));
				parseParam(line,23,41,&(rx->gps.UTCdata.A1));
				parseParam(line,42,50,&(rx->gps.UTCdata.t_ot));
			}
			else if  (NULL != strstr(line,"LEAP SECONDS")){
				parseParam(line,1,6,&(rx->leapsecs));
				DBGMSG(debugStream,TRACE,"read LEAP SECONDS=" << rx->leapsecs);
			}
			else if (NULL != strstr(line,"END OF HEADER")) 
				break;
		} // if constellation == GNSSSystem::GPS
		else if (constellation == GNSSSystem::GLONASS){
			// FIXME coming soon
		}
		
	}
	
	if (feof(fin)){
		app->logMessage("Format error (no END OF HEADER) in " + fname);
		return false;
	}
	
	while (!feof(fin)){
		
		switch (constellation){
			case GNSSSystem::GPS:
			{
				GPS::EphemerisData *ed = getGPSEphemeris(2,fin,&lineCount);
				if (NULL != ed) rx->gps.addEphemeris(ed);
				break;
			}
			case GNSSSystem::GLONASS:
			{
				// FIXME coming soon
				break;
			}
			default:
				break;
		}
		
	}
	
	return true;
}

bool RINEX::readV3NavigationFile(Receiver *rx,int constellation,std::string fname)
{
	unsigned int lineCount=0;
	
	FILE *fin;
	char line[SBUFSIZE];
				 
	if (NULL == (fin= std::fopen(fname.c_str(),"r"))){
		app->logMessage("Unable to open the navigation file " + fname);
		return false;
	}
	
	// Parse the header
	int ibuf;
	
	while (!feof(fin)){
		
		std::fgets(line,SBUFSIZE,fin);
		lineCount++;
		
		if (NULL != strstr(line,"RINEX VERSION/TYPE")){
			char satSystem = line[40]; //assuming length is OK
			int gnss = -1; 
			switch (satSystem){
				case 'M':gnss =0;break;
				case 'G':gnss = GNSSSystem::GPS;break;
				case 'R':gnss = GNSSSystem::GLONASS;break;
				case 'E':gnss = GNSSSystem::GALILEO;break;
				case 'C':gnss = GNSSSystem::BEIDOU;break;
				default:break;
			}
			if (gnss != 0){
				if (gnss != constellation){
					app->logMessage("No data for satellite system " + std::string(1,satSystem) + " in " + fname);
					return false;
				}
			}
		}
		else if (NULL != strstr(line,"IONOSPHERIC CORR")){
			switch (constellation){
				case GNSSSystem::GPS :
					if (NULL != strstr(line,"GPSA")){
						parseParam(line,6,12, &(rx->gps.ionoData.a0));
						parseParam(line,18,12,&(rx->gps.ionoData.a1));
						parseParam(line,30,12,&(rx->gps.ionoData.a2));
						parseParam(line,42,12,&(rx->gps.ionoData.a3));
						DBGMSG(debugStream,TRACE,"read GPS ION ALPHA " << rx->gps.ionoData.a0 << " " << rx->gps.ionoData.a1 << " " << rx->gps.ionoData.a2 << " " << rx->gps.ionoData.a3);
					}
					else if (NULL != strstr(line,"GPSB")){
						parseParam(line,6,12, &(rx->gps.ionoData.B0));
						parseParam(line,18,12,&(rx->gps.ionoData.B1));
						parseParam(line,30,12,&(rx->gps.ionoData.B2));
						parseParam(line,42,12,&(rx->gps.ionoData.B3));
						DBGMSG(debugStream,TRACE,"read GPS ION BETA " << rx->gps.ionoData.B0 << " " << rx->gps.ionoData.B1 << " " << rx->gps.ionoData.B2 << " " << rx->gps.ionoData.B3);
					}
					break;
				case GNSSSystem::GLONASS :
					// nothing to find
					break;
				case GNSSSystem::GALILEO :
					if (NULL != strstr(line,"GAL")){
						parseParam(line,6,12, &(rx->galileo.ionoData.ai0));
						parseParam(line,18,12,&(rx->galileo.ionoData.ai1));
						parseParam(line,30,12,&(rx->galileo.ionoData.ai2));
						DBGMSG(debugStream,TRACE,"read GAL ION AI " << rx->galileo.ionoData.ai0 << " " << rx->galileo.ionoData.ai1 << " " << rx->galileo.ionoData.ai2);
					}
					break;
				case GNSSSystem::BEIDOU :
					if (NULL != strstr(line,"BDSA")){
						parseParam(line,6,12, &(rx->beidou.ionoData.a0));
						parseParam(line,18,12,&(rx->beidou.ionoData.a1));
						parseParam(line,30,12,&(rx->beidou.ionoData.a2));
						parseParam(line,42,12,&(rx->beidou.ionoData.a3));
						DBGMSG(debugStream,TRACE,"read BDS ION ALPHA " << rx->beidou.ionoData.a0 << " " << rx->beidou.ionoData.a1 << " " << rx->beidou.ionoData.a2 << " " << rx->beidou.ionoData.a3);
					}
					else if (NULL != strstr(line,"BDSB")){
						parseParam(line,6,12, &(rx->beidou.ionoData.b0));
						parseParam(line,18,12,&(rx->beidou.ionoData.b1));
						parseParam(line,30,12,&(rx->beidou.ionoData.b2));
						parseParam(line,42,12,&(rx->beidou.ionoData.b3));
						DBGMSG(debugStream,TRACE,"read GPS ION BETA " << rx->beidou.ionoData.b0 << " " << rx->beidou.ionoData.b1 << " " << rx->beidou.ionoData.b3 << " " << rx->beidou.ionoData.b3);
					}
					break;
				
				default:break;
			}
		}
		else if (NULL != strstr(line,"TIME SYSTEM CORR")){
				switch (constellation){
					case GNSSSystem::GPS :
						if (NULL != strstr(line,"GPUT")){
							parseParam(line,6,17,&(rx->gps.UTCdata.A0));
							parseParam(line,23,16,&(rx->gps.UTCdata.A1));
							parseParam(line,39,7,&(rx->gps.UTCdata.t_ot));
							parseParam(line,46,5,&ibuf);rx->gps.UTCdata.WN_t = ibuf; // this is a full week number
						}
						break;
					case GNSSSystem::BEIDOU :
						if (NULL != strstr(line,"GPUT")){
							parseParam(line,6,17,&(rx->beidou.UTCdata.A_0UTC));
							parseParam(line,23,16,&(rx->beidou.UTCdata.A_1UTC));
							parseParam(line,39,7,&(rx->beidou.UTCdata.t_ot));
							parseParam(line,46,5,&ibuf);rx->beidou.UTCdata.WN_t = ibuf; // this is a full week number
						}
					default:break;
				}
		}
		else if  (NULL != strstr(line,"LEAP SECONDS")){
			parseParam(line,1,6,&(rx->leapsecs));
			DBGMSG(debugStream,TRACE,"read LEAP SECONDS=" << rx->leapsecs);
		}
		else if (NULL != strstr(line,"END OF HEADER")){ 
			break;
		}
	}
	
	// Parse the data
	if (feof(fin)){
		app->logMessage("Format error (no END OF HEADER) in " + fname);
		return false;
	}
	
	while (!feof(fin)){
		
		switch (constellation){
			case GNSSSystem::GPS:
			{
				GPS::EphemerisData *ed = getGPSEphemeris(3,fin,&lineCount);
				if (NULL != ed) rx->gps.addEphemeris(ed);
				break;
			}
			case GNSSSystem::GLONASS:
			{
				// FIXME coming soon
				break;
			}
			case GNSSSystem::BEIDOU: 
			{
				BeiDou::EphemerisData *ed = getBeiDouEphemeris(fin,&lineCount);
				if (NULL != ed) rx->beidou.addEphemeris(ed);
				break;
			}
			default:
				break;
		}
		
	}
	
	return true;
}


GPS::EphemerisData* RINEX::getGPSEphemeris(int ver,FILE *fin,unsigned int *lineCount){
	GPS::EphemerisData *ed = NULL;
	
	char line[SBUFSIZE];
	
	(*lineCount)++;
	if (!feof(fin)){ 
		std::fgets(line,SBUFSIZE,fin);
	}
	
	// skip blank lines
	char *pch = line;
	while (*pch != '\0') {
    if (!isspace((unsigned char)*pch))
      break;
    pch++;
  }
	if (*pch == '\0')
		return NULL;
	
	if (strlen(line) < 79)
		return NULL;
	
	ed = new GPS::EphemerisData();
	
	int ibuf;
	double dbuf;
	
	int year,mon,mday,hour,mins;
	double secs;
	int startCol;
	
	if (ver==2){
		startCol=4;
		// Line 1: format is I2,5I3,F5.1,3D19.12
		parseParam(line,1,2,&ibuf); ed->SVN = ibuf;	
		parseParam(line,3,3,&year);
		parseParam(line,6,3,&mon);
		parseParam(line,9,3,&mday);
		parseParam(line,12,3,&hour);
		parseParam(line,15,3,&mins);
		parseParam(line,18,5,&secs);
		parseParam(line,23,19,&dbuf);ed->a_f0=dbuf;
		parseParam(line,42,19,&dbuf);ed->a_f1=dbuf;
		parseParam(line,61,19,&dbuf);ed->a_f2=dbuf;
	}
	else if (ver==3){
		startCol=5;
		char satSys = line[0];
		switch (satSys){
			case 'G':
				parseParam(line,2,2,&ibuf); ed->SVN = ibuf;	
				parseParam(line,5,4,&year);
				parseParam(line,9,3,&mon);
				parseParam(line,12,3,&mday);
				parseParam(line,15,3,&hour);
				parseParam(line,18,3,&mins);
				parseParam(line,21,3,&secs);
				parseParam(line,24,19,&dbuf);ed->a_f0=dbuf;
				parseParam(line,43,19,&dbuf);ed->a_f1=dbuf;
				parseParam(line,62,19,&dbuf);ed->a_f2=dbuf;
				break;
			case 'E':
				{ for (int i=0;i<7;i++){ std::fgets(line,SBUFSIZE,fin);} (*lineCount) += 7; return NULL;}
				break;
			case 'R':
				{ for (int i=0;i<3;i++){ std::fgets(line,SBUFSIZE,fin);}   (*lineCount) += 3;return NULL;}
				break;
			case 'C': // BDS
				{ for (int i=0;i<7;i++){ std::fgets(line,SBUFSIZE,fin);}   (*lineCount) += 7; return NULL;}
				break;
			case 'J': // QZSS
				{ for (int i=0;i<7;i++){ std::fgets(line,SBUFSIZE,fin);}   (*lineCount) += 7; return NULL;}
				break;
			case 'S': // SBAS
				{ for (int i=0;i<3;i++){ std::fgets(line,SBUFSIZE,fin);}  (*lineCount) += 3; return NULL;}
				break;
			case 'I': // IRNS
				{ for (int i=0;i<7;i++){ std::fgets(line,SBUFSIZE,fin);}  (*lineCount) += 7; return NULL;}
				break;
			default:break;
		}
	}
	
	DBGMSG(debugStream,TRACE,"ephemeris for SVN " << (int) ed->SVN << " " << hour << ":" << mins << ":" <<  secs);
	
	// Lines 2-8: 3X,4D19.12
	double dbuf1,dbuf2,dbuf3,dbuf4;
	
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->IODE=dbuf1; ed->C_rs=dbuf2; ed->delta_N=dbuf3; ed->M_0=dbuf4;
	
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->C_uc=dbuf1; ed->e=dbuf2; ed->C_us=dbuf3; ed->sqrtA=dbuf4;;
	
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->t_0e=dbuf1; ed->C_ic=dbuf2; ed->OMEGA_0=dbuf3; ed->C_is=dbuf4;
	
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->i_0=dbuf1; ed->C_rc=dbuf2; ed->OMEGA=dbuf3; ed->OMEGADOT=dbuf4; // note OMEGADOT read in as DOUBLE but stored as SINGLE so in != out
	
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->IDOT=dbuf1; ed->week_number= dbuf3; // don't truncate WN just yet
	
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
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
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
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

BeiDou::EphemerisData*  RINEX::getBeiDouEphemeris(FILE *fin,unsigned int *lineCount)
{
	BeiDou::EphemerisData *ed=NULL;
	
	char line[SBUFSIZE];
	
	(*lineCount)++;
	if (!feof(fin)){ 
		std::fgets(line,SBUFSIZE,fin);
	}
	
	// skip blank lines
	char *pch = line;
	while (*pch != '\0') {
    if (!isspace((unsigned char)*pch))
      break;
    pch++;
  }
	if (*pch == '\0')
		return NULL;
	
	if (strlen(line) < 79)
		return NULL;
	
	ed = new BeiDou::EphemerisData();
	
	int ibuf;
	double dbuf;
	
	int startCol=5;
	
	char satSys = line[0];
	switch (satSys){
		case 'G':
			{ for (int i=0;i<7;i++){ std::fgets(line,SBUFSIZE,fin);} (*lineCount) += 7; return NULL;}
			break;
		case 'E':
			{ for (int i=0;i<7;i++){ std::fgets(line,SBUFSIZE,fin);} (*lineCount) += 7; return NULL;}
			break;
		case 'R':
			{ for (int i=0;i<3;i++){ std::fgets(line,SBUFSIZE,fin);}   (*lineCount) += 3;return NULL;}
			break;
		case 'C': // BDS
			parseParam(line,2,2,&ibuf); ed->SVN = ibuf;	
			parseParam(line,5,4,&(ed->year));
			parseParam(line,9,3,&(ed->mon));
			parseParam(line,12,3,&(ed->mday));
			parseParam(line,15,3,&(ed->hour));
			parseParam(line,18,3,&(ed->mins));
			parseParam(line,21,3,&(ed->secs));
			parseParam(line,24,19,&dbuf);ed->a_0=dbuf;
			parseParam(line,43,19,&dbuf);ed->a_1=dbuf;
			parseParam(line,62,19,&dbuf);ed->a_2=dbuf;
			break;
		case 'J': // QZSS
			{ for (int i=0;i<7;i++){ std::fgets(line,SBUFSIZE,fin);}   (*lineCount) += 7; return NULL;}
			break;
		case 'S': // SBAS
			{ for (int i=0;i<3;i++){ std::fgets(line,SBUFSIZE,fin);}  (*lineCount) += 3; return NULL;}
			break;
		case 'I': // IRNS
			{ for (int i=0;i<7;i++){ std::fgets(line,SBUFSIZE,fin);}  (*lineCount) += 7; return NULL;}
			break;
		default:break;
	}
		
	DBGMSG(debugStream,TRACE,"ephemeris for SVN " << (int) ed->SVN << " " << ed->hour << ":" << ed->mins << ":" <<  ed->secs);
	
	// Lines 2-8: 3X,4D19.12
	double dbuf1,dbuf2,dbuf3,dbuf4;
	
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->AODE=dbuf1; ed->C_rs=dbuf2; ed->delta_N=dbuf3; ed->M_0=dbuf4;
	
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->C_uc=dbuf1; ed->e=dbuf2; ed->C_us=dbuf3; ed->sqrtA=dbuf4;
		
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->t_oe=dbuf1; ed->C_ic=dbuf2; ed->OMEGA_0=dbuf3; ed->C_is=dbuf4;
	
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->i_0=dbuf1; ed->C_rc=dbuf2; ed->OMEGA=dbuf3; ed->OMEGADOT=dbuf4; // note OMEGADOT read in as DOUBLE but stored as SINGLE so in != out
	
	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->IDOT=dbuf1; ed->WN= dbuf3; 

	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->URAI=dbuf1; ed->SatH1=dbuf2;ed->t_GD1=dbuf3; ed->t_GD2=dbuf4;

	get4DParams(fin,startCol,&dbuf1,&dbuf2,&dbuf3,&dbuf4,lineCount);
	ed->tx_e=dbuf1;ed->AODC=dbuf2;
	
	DBGMSG(debugStream,TRACE,"ephemeris for SVN " << (int) ed->SVN << " " << ed->hour << ":" << ed->mins << ":" <<  ed->secs << " " 
		<< ed->hour*3600+ed->mins*60+ed->secs << " " << ed->t_oe);
		return ed;
}

void RINEX::init()
{
	agency = "KAOS";
	observer = "Siegfried";
	allObservations=false; 
}

char * RINEX::formatFlags(int lli,int sn)
{
	if (lli != 0 && sn!=0)
		std::sprintf(gsbuf,"%1i%1i",lli,sn);
	else if (lli != 0 && sn == 0)
		std::sprintf(gsbuf,"%1i ",lli);
	else if (lli ==0 && sn !=0)
		std::sprintf(gsbuf," %1i",sn);
	else
		std::sprintf(gsbuf,"  ");
	return gsbuf;
}

// Note: these subtract one from the index !
void RINEX::parseParam(char *str,int start,int len,int *val)
{
	char sbuf[SBUFSIZE];
	memset(sbuf,0,SBUFSIZE);
	*val=0;
	strncpy(sbuf,&(str[start-1]),len);
	*val = strtol(sbuf,NULL,10);
}

void RINEX::parseParam(char *str,int start,int len,float *val)
{
	//std::replace(str.begin(), str.end(), 'D', 'E'); // filthy FORTRAN
	char *pch;
	if ((pch=strstr(str,"D"))){
		(*pch)='E';
	}
	char sbuf[SBUFSIZE];
	memset(sbuf,0,SBUFSIZE);
	*val=0.0;
	strncpy(sbuf,&(str[start-1]),len);
	*val = strtof(sbuf,NULL);
	
}

void RINEX::parseParam(char *str,int start,int len,double *val)
{
	char *pch;
	if ((pch=strstr(str,"D"))){
		(*pch)='E';
	}
	char sbuf[SBUFSIZE];
	memset(sbuf,0,SBUFSIZE);
	*val=0.0;
	strncpy(sbuf,&(str[start-1]),len);
	*val = strtod(sbuf,NULL);
}

bool RINEX::get4DParams(FILE *fin,int startCol,
	double *darg1,double *darg2,double *darg3,double *darg4,
	unsigned int *lineCount)
{
	char sbuf[SBUFSIZE];
	
	(*lineCount)++;
	if (!std::feof(fin)) 
		std::fgets(sbuf,SBUFSIZE,fin);
	else
		return false;

	int slen = strlen(sbuf);
	*darg1 = *darg2= *darg3 = *darg4 = 0.0;
	if (slen >= startCol + 19 -1)
		parseParam(sbuf,startCol,19,darg1);
	if (slen >= startCol + 38 -1)
		parseParam(sbuf,startCol+19,19,darg2);
	if (slen >= startCol + 57 -1)
		parseParam(sbuf,startCol+2*19,19,darg3);
	if (slen >= startCol + 76 -1)
		parseParam(sbuf,startCol+3*19,19,darg4);
	
	return true;
	
}

std::string RINEX::makeVerName(int majorVer,int minorVer)
{
    char sbuf[SBUFSIZE];
    std::string n;
    sprintf(sbuf,"%i.%02d",majorVer,minorVer);
    n=sbuf;
    return n;
}
