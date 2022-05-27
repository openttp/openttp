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

#include <stdio.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cmath>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <boost/concept_check.hpp>

#include "Antenna.h"
#include "Application.h"
#include "Debug.h"
#include "GPS.h"
#include "HexBin.h"
#include "Ublox.h"
#include "ReceiverMeasurement.h"
#include "SVMeasurement.h"
#include "Timer.h"

extern std::ostream *debugStream;
extern Application *app;

extern int verbosity; // FIXME temporary

#define CLIGHT 299792458.0
#define ICD_PI 3.1415926535898

#define MAX_CHANNELS 16 // max channels per constellation
#define SLOPPINESS 0.1
#define CLOCKSTEP  0.001
#define MAX_GAP    3   // maximum permissible gap in observation sequence before ambiguity resolution is triggered
// types used by ublox receivers     
typedef signed char    I1;
typedef unsigned char  U1;
typedef unsigned char  X1;
typedef unsigned short U2;
typedef short          I2;
typedef int            I4;
typedef unsigned int   U4;
typedef float          R4;
typedef double         R8;

// messages that are parsed
#define MSG0121 0x01
#define MSG0122 0x02
#define MSG0215 0x04
#define MSG0D01 0x08
#define MSG0213 0x10 # ublox9 only



Ublox::Ublox(Antenna *ant,std::string m):Receiver(ant)
{
	modelName=m;
	manufacturer="ublox";
	swversion="0.1";
	// defaults are for NEO-M8T
	constellations=GNSSSystem::GPS | GNSSSystem::GLONASS | GNSSSystem::GALILEO | GNSSSystem::BEIDOU;
	gps.codes = GNSSSystem::C1C;
	galileo.codes = GNSSSystem::C1C;
	beidou.codes = GNSSSystem::C2I;
	glonass.codes = GNSSSystem::C1C;
	codes = beidou.codes | galileo.codes | glonass.codes | gps.codes;
	channels=72;
	
	alertPagesCnt = 0;
	
	if (modelName == "LEA-M8T"){
		// For the future
	}
	else if (modelName == "NEO-M8T"){
		// defaults
		model = UBLOX_NE08MT;
	}
	else if (modelName == "ZED-F9P" or modelName == "ZED-F9T"){
		if (modelName == "ZED-F9P"){
			model = UBLOX_ZEDF9P;
		}
		else{
			model = UBLOX_ZEDF9T;
		}
	  channels=184;
		constellations=GNSSSystem::GPS | GNSSSystem::GLONASS | GNSSSystem::GALILEO | GNSSSystem::BEIDOU;
		// Appendix B in the ZED-F9P interface description identifies the GPS signals available as L1 C/A, L2 CL and L2 CM
		gps.codes = GNSSSystem::C1C | GNSSSystem::C2L | GNSSSystem::L1C | GNSSSystem::L2L ;
		galileo.codes = GNSSSystem::C1C| GNSSSystem::C1B | GNSSSystem::C7I | GNSSSystem::C7Q;
		beidou.codes = GNSSSystem::C2I | GNSSSystem::C7I;
		glonass.codes = GNSSSystem::C1C| GNSSSystem::C2C | GNSSSystem::L1C |  GNSSSystem::L2C;
		codes = beidou.codes | galileo.codes | glonass.codes | gps.codes;
		dualFrequency = true;
	}
	else{
		app->logMessage("Unknown receiver model: " + modelName);
		app->logMessage("Assuming NEO-M8T");
	}
	for (int i=1;i<=gps.maxSVN();i++){
		GPSEphemeris *ed = new GPSEphemeris();
		gpsEph[i]=ed;
	}
	for (int i=1;i<=galileo.maxSVN();i++){
		GalEphemeris *ed = new GalEphemeris();
		galEph[i]=ed;
	}
}

Ublox::~Ublox()
{
}

void Ublox::addConstellation(int constellation)
{
	constellations |= constellation;
	switch (constellation)
	{
		case GNSSSystem::BEIDOU:
			if (modelName == "LEA-M8T")
				beidou.codes = GNSSSystem::C2I;
			else if (modelName == "NEO-M8T")
				beidou.codes = GNSSSystem::C2I;
			else if (modelName == "ZED-F9P" or modelName == "ZED-F9T")
				beidou.codes =GNSSSystem::C2I | GNSSSystem::C7I;
			codes |= beidou.codes;
			break;
		case GNSSSystem::GALILEO:
			if (modelName == "LEA-M8T")
				galileo.codes = GNSSSystem::C1C;
			else if (modelName == "NEO-M8T")
				galileo.codes = GNSSSystem::C1C;
			else if (modelName == "ZED-F9P" or modelName == "ZED-F9T")
				galileo.codes = GNSSSystem::C1C | GNSSSystem::C1B | GNSSSystem::C7I | GNSSSystem::C7Q;
			codes |= galileo.codes;
			break;
		case GNSSSystem::GLONASS:
			if (modelName == "LEA-M8T")
				glonass.codes = GNSSSystem::C1C;
			else if (modelName == "NEO-M8T")
				glonass.codes = GNSSSystem::C1C;
			else if (modelName == "ZED-F9P" or modelName == "ZED-F9T")
				glonass.codes = GNSSSystem::C1C | GNSSSystem::C2C;
			codes |= glonass.codes;
			break;
		case GNSSSystem::GPS:
			if (modelName == "LEA-M8T")
				gps.codes = GNSSSystem::C1C;
			else if (modelName == "NEO-M8T")
				gps.codes = GNSSSystem::C1C;
			else if (modelName == "ZED-F9P" or modelName == "ZED-F9T")
				gps.codes = GNSSSystem::C1C | GNSSSystem::C2L | GNSSSystem::L1C | GNSSSystem::L2L;
			codes |= gps.codes;
			break;
	}
}

bool Ublox::readLog(std::string fname,int mjd,int startTime,int stopTime,int rinexObsInterval)
{
	DBGMSG(debugStream,INFO,"reading " << fname << ", constellations = " << (int) constellations);	
	
	std::ifstream infile (fname.c_str());
	std::string line;
	int linecount=0;
	
	std::string msgid,currpctime,pctime,msg,gpstime;
	
	I4 sawtooth;
	I4 clockBias;
	R8 measTOW=-1; 
	U2 measGPSWN;
	I1 measLeapSecs=0;
	
	U2 UTCyear;
	U1 UTCmon,UTCday,UTChour,UTCmin,UTCsec,UTCvalid;
	
	U1 u1buf;
	I2 i2buf;
	I4 i4buf;
	U4 u4buf;
	R4 r4buf;
	R8 r8buf;
	
	float rxTimeOffset; // single
	
	time_t tgps; 
	
	std::vector<SVMeasurement *> svmeas;
	
	gps.gotUTCdata=false;
	gps.gotIonoData=false;
	
	unsigned int currentMsgs=0;
	unsigned int reqdMsgs =  MSG0121 | MSG0122 | MSG0215 | MSG0D01 ;
	
	Timer timer;
	timer.start();
	
  if (infile.is_open()){
    while (std::getline (infile,line) ){
			linecount++;
			
			if (line.size()==0) continue; // skip empty line
			if ('#' == line.at(0)) continue; // skip comments
			if ('%' == line.at(0)) continue;
			if ('@' == line.at(0)) continue;
			
			std::stringstream sstr(line);
			sstr >> msgid >> currpctime >> msg;
			if (sstr.fail()){
				DBGMSG(debugStream,WARNING," bad data at line " << linecount);
				currentMsgs=0;
				deleteMeasurements(svmeas);
				continue;
			}
			
			// The 0x0215 message starts each second
			if(msgid == "0215"){ // raw measurements 
				
				int nGPS=0;
				int nBeidou=0;
				
				if (currentMsgs == reqdMsgs){ // save the measurements from the previous second
					if (svmeas.size() > 0){
						ReceiverMeasurement *rmeas = new ReceiverMeasurement();
						measurements.push_back(rmeas);
						
						rmeas->sawtooth=sawtooth*1.0E-12; // units are ps, must be added to TIC measurement
						rmeas->timeOffset=clockBias*1.0E-9; // units are ns WARNING no sign convention defined yet ...
						
						int pchh,pcmm,pcss;
						if ((3==std::sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss))){
							rmeas->pchh=pchh;
							rmeas->pcmm=pcmm;
							rmeas->pcss=pcss;
						}
						
						// GPSTOW is used for pseudorange estimations
						// Note: this is rounded because that's what works for time-transfer :-)
						if (app->positioningMode)
							rmeas->gpstow = measTOW; // truncate fractional part
						else
							rmeas->gpstow = rint(measTOW); 
						rmeas->gpswn=measGPSWN % 1024; // Converted to truncated WN.  
						
						// UTC time of measurement
						// We could use other time information to calculate this eg gpstow,gpswn and leap seconds
						rmeas->tmUTC.tm_sec=UTCsec;
						rmeas->tmUTC.tm_min=UTCmin;
						rmeas->tmUTC.tm_hour=UTChour;
						rmeas->tmUTC.tm_mday=UTCday;
						rmeas->tmUTC.tm_mon=UTCmon-1;
						rmeas->tmUTC.tm_year=UTCyear-1900;
						rmeas->tmUTC.tm_isdst=0;
						
						// Calculate GPS time of measurement 
						// FIXME why do this ? why not just convert from UTC ? and full WN is known anyway
						tgps = GPS::GPStoUnix(rmeas->gpstow,rmeas->gpswn,app->referenceTime()); // also used to resolve week rollovers
						struct tm *tmGPS = gmtime(&tgps);
						rmeas->tmGPS=*tmGPS;
						
						if (app->positioningMode)
							rmeas->tmfracs = measTOW - (int)(measTOW); 
							//if (rmeas->tmfracs > 0.5) rmeas->tmfracs -= 1.0; // place in the previous second
						else
							rmeas->tmfracs=0.0;
						
						// Need to add some logic here for determining if a measurement is selected...
						// FIXME I removed tha logic for testing, but it may not work if a datafile is mismatched
						// the configuration.
						
						//if (constellations & GNSSSystem::GPS){
							for (unsigned int sv=0;sv<svmeas.size();sv++){
								svmeas.at(sv)->dbuf3 = svmeas.at(sv)->meas; // save for debugging
								if (!app->positioningMode){ // corrections only add noise, in principle
									if (svmeas.at(sv)->code < GNSSSystem::L1C)
										svmeas.at(sv)->meas -= clockBias*1.0E-9; // evidently it is subtracted
									else{ // carrier phase, so have to convert the clock bias to cycles
										double f=1.0; // a fudge for the GNSS for which freqFromCode is not implemented
										switch (svmeas.at(sv)->constellation)
										{
											case GNSSSystem::BEIDOU:  f=beidou.codeToFreq(svmeas.at(sv)->code );break;
											case GNSSSystem::GALILEO: f=galileo.codeToFreq(svmeas.at(sv)->code );break;
											case GNSSSystem::GLONASS: f=glonass.codeToFreq(svmeas.at(sv)->code,0);break;
											case GNSSSystem::GPS:     f=gps.codeToFreq(svmeas.at(sv)->code );break;
										}
										svmeas.at(sv)->meas -= clockBias*1.0E-9*f;
									}
									// Now subtract the ms part so that ms ambiguity resolution works:
									// Need to obtain ephemeris, etc. for other GNSS to do proper ms ambiguity
									// resolution. We could only keep the ms part, but for now only do this for
									// GPS because we do get the necessary data for GPS from the ublox.
									if(svmeas.at(sv)->constellation == GNSSSystem::GPS){
										svmeas.at(sv)->meas -= 1.0E-3*floor(svmeas.at(sv)->meas/1.0E-3);
									}
								}
								svmeas.at(sv)->rm=rmeas;
							}
							rmeas->meas=svmeas;
							svmeas.clear(); // don't delete - we only made a shallow copy!
						//}
						
						
						// KEEP THIS it's useful for debugging measurement-time related problems
					//fprintf(stderr,"PC=%02d:%02d:%02d tmUTC=%02d:%02d:%02d tmGPS=%4d %02d:%02d:%02d gpstow=%d gpswn=%d measTOW=%.12lf tmfracs=%g clockbias=%g\n",
						//pchh,pcmm,pcss,UTChour,UTCmin,UTCsec, rmeas->tmGPS.tm_year+1900,rmeas->tmGPS.tm_hour, rmeas->tmGPS.tm_min,rmeas->tmGPS.tm_sec,
						//(int) rmeas->gpstow,(int) rmeas->gpswn,measTOW,rmeas->tmfracs,clockBias*1.0E-9  );
					
					//fprintf(stderr,"%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %d %d %.12lf %g %g\n",
					//pchh,pcmm,pcss,UTChour,UTCmin,UTCsec, rmeas->tmGPS.tm_hour, rmeas->tmGPS.tm_min,rmeas->tmGPS.tm_sec,
					//(int) rmeas->gpstow,(int) rmeas->gpswn,measTOW,rmeas->tmfracs,clockBias*1.0E-9  );
						
					}// if (gpsmeas.size() > 0)
				} 
				else{
					DBGMSG(debugStream,TRACE,pctime << " reqd message missing, flags = " << currentMsgs);
					deleteMeasurements(svmeas);
				}
				
				pctime=currpctime;
				currentMsgs = 0;
				
				if (msg.size()-2*2-16*2 > 0){ // don't know the expected message size yet but if we've got the header ...
					HexToBin((char *) msg.substr(11*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf);
					unsigned int nmeas=u1buf;
					if (msg.size() == (2+16+nmeas*32)*2){
						HexToBin((char *) msg.substr(0*2,2*sizeof(R8)).c_str(),sizeof(R8),(unsigned char *) &measTOW); //measurement TOW (s)
						HexToBin((char *) msg.substr(8*2,2*sizeof(U2)).c_str(),sizeof(U2),(unsigned char *) &measGPSWN); // full WN
						HexToBin((char *) msg.substr(10*2,2*sizeof(I1)).c_str(),sizeof(I1),(unsigned char *) &measLeapSecs);
						DBGMSG(debugStream,TRACE,currpctime << " meas tow=" << measTOW << std::setprecision(12) << " gps wn=" << (int) measGPSWN << " leap=" << (int) measLeapSecs);
						// Check the validity of leap seconds
						HexToBin((char *) msg.substr(12*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); // recStat
						if (!(u1buf & 0x01))
							measLeapSecs = 0;
						//DBGMSG(debugStream,TRACE,nmeas);
						for (unsigned int m=0;m<nmeas;m++){
							HexToBin((char *) msg.substr((36+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); //GNSS id
							int gnssSys = 0;
							int maxSVN=32;
							switch (u1buf){
								case 0: gnssSys=GNSSSystem::GPS; maxSVN=gps.maxSVN();break;
								case 1:case 4: case 5: break;
								case 2: gnssSys=GNSSSystem::GALILEO;  maxSVN=galileo.maxSVN();break;
								case 3: gnssSys=GNSSSystem::BEIDOU;   maxSVN=beidou.maxSVN();break;
								case 6: gnssSys=GNSSSystem::GLONASS;  maxSVN=glonass.maxSVN();break;
								default: break;
							}
							//DBGMSG(debugStream,TRACE,gnssSys);
							int sigID=0;
							if (gnssSys & constellations ){
								// Since we get all the measurements in one message (which starts each second) there's no need to check for multiple measurement messages
								// like with eg the Resolution T
                                R8 cpmeas;
								HexToBin((char *) msg.substr((16+32*m)*2,2*sizeof(R8)).c_str(),sizeof(R8),(unsigned char *) &r8buf); //pseudorange (m)
								HexToBin((char *) msg.substr((24+32*m)*2,2*sizeof(R8)).c_str(),sizeof(R8),(unsigned char *) &cpmeas); //carrier phase (cycles)
								HexToBin((char *) msg.substr((37+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); //svid
								int svID=u1buf;
								if (model == UBLOX_ZEDF9P || model == UBLOX_ZEDF9T){
									HexToBin((char *) msg.substr((38+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); //signal id
									sigID=u1buf;
									//DBGMSG(debugStream,INFO,gnssSys << " " << svID << " " << sigID);
								}
								HexToBin((char *) msg.substr((43+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf);
								int prStdDev= u1buf & 0x0f;
								HexToBin((char *) msg.substr((46+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf);
								int trkStat=u1buf;
								// When PR is reported, trkStat is always 1 but .
								if (trkStat > 0 && r8buf/CLIGHT < 1.0 && svID <= maxSVN){ // also filters out svid=255 'unknown GLONASS'
									int sig=-1;
									int cpsig=-1;
									switch (gnssSys){
										// ublox docs say BeiDou signals are
										// B1I  1561.098 MHz
										// B2I  1207.140 MHz
										case GNSSSystem::BEIDOU:
											switch (sigID){
												case 0:sig=GNSSSystem::C2I;cpsig=GNSSSystem::L2I;break; // D1
												case 1:sig=GNSSSystem::C2I;cpsig=GNSSSystem::L2I;break; // D2
												case 2:sig=GNSSSystem::C7I;cpsig=GNSSSystem::L7I;break; // D1
												case 3:sig=GNSSSystem::C7I;cpsig=GNSSSystem::L7I;break; // D2
												default: break;
											}
											break;
										// ublox docs say Galileo signals  are
										// E1-B/C 1575.42 MHz
										// E5b    1207.14
										case GNSSSystem::GALILEO:
											switch (sigID){
												case 0:sig=GNSSSystem::C1C;cpsig=GNSSSystem::L1C;break; //E1C
												case 1:sig=GNSSSystem::C1B;cpsig=GNSSSystem::L1B;break; //E1B
												case 5:sig=GNSSSystem::C7I;cpsig=GNSSSystem::L7I;break; //E5Bi
												case 6:sig=GNSSSystem::C7Q;cpsig=GNSSSystem::L7Q;break; //E5bQ
												default: break;
											}
											break;
										// ublox docs say GLONASS signals are
										// L1OF 1602 MHz + k*562.5 kHz
										// L2OF 1246 MHz + k*437.5 kHz
										case GNSSSystem::GLONASS:
											switch (sigID){
												case 0:sig=GNSSSystem::C1C;cpsig=GNSSSystem::L1C;break;
												case 2:sig=GNSSSystem::C2C;cpsig=GNSSSystem::L2C;break;
												default: break;
											}
											break;
										// ublox docs say GPS signals are
										// L1 C/A 1575.42 MHz
										// L2 CL  1227.60 MHz
										// L2 CM
										case GNSSSystem::GPS:
								 			switch (sigID){
												case 0:sig=GNSSSystem::C1C;cpsig=GNSSSystem::L1C;break;
												case 3:sig=GNSSSystem::C2L;cpsig=GNSSSystem::L2L;break;
												//case 4:sig=GNSSSystem::C2M;break;
												default: break;
											}
											break;
									}
									if (sig > 0){
										SVMeasurement *svm = new SVMeasurement(svID,gnssSys,sig,r8buf/CLIGHT,NULL);
										svm->dbuf1=0.01*pow(2.0,prStdDev); // used offset 46 instead of offset 43 above
										svmeas.push_back(svm);
										if (app->allObservations && cpsig > 0){ // FIXME until all CP used ...
											SVMeasurement *svm = new SVMeasurement(svID,gnssSys,cpsig,cpmeas,NULL);
											svmeas.push_back(svm);
										}
									}
									else{
									}
								}
								DBGMSG(debugStream,TRACE,"SYS " << gnssSys << " sig=" << sigID << " SV" << svID << " pr=" << r8buf/CLIGHT << std::setprecision(8) << " trkStat= " << (int) trkStat);
							}
						}
						currentMsgs |= MSG0215;
					}
					else{
						DBGMSG(debugStream,WARNING,"Bad 0215 message size");
					}
				}
				else{
					DBGMSG(debugStream,WARNING,"empty/malformed 0215 message");
				}
				
				continue;
				
			} // raw measurements
			
			// 0x0D01 Timepulse time data (sawtooth correction)
			if(msgid == "0d01"){
				
				if (msg.size()==(16+2)*2){
					X1 TPflags,TPrefInfo;
					U4 TPTOW;
					HexToBin((char *) msg.substr(0*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &TPTOW); // (ms)
					HexToBin((char *) msg.substr(8*2,2*sizeof(I4)).c_str(),sizeof(I4),(unsigned char *) &sawtooth); // (ps)
					HexToBin((char *) msg.substr(14*2,2*sizeof(X1)).c_str(),sizeof(X1),(unsigned char *) &TPflags);
					HexToBin((char *) msg.substr(15*2,2*sizeof(X1)).c_str(),sizeof(X1),(unsigned char *) &TPrefInfo);
					DBGMSG(debugStream,TRACE,currpctime << " tow= " << (int) TPTOW << " sawtooth=" << sawtooth << " ps" << std::hex << " flags=0x" << (unsigned int) TPflags << 
						" ref=0x" << (unsigned int) TPrefInfo << std::dec);
					currentMsgs |= MSG0D01;
				}
				else{
					DBGMSG(debugStream,WARNING,"Bad 0d01 message size");
				}
			}
			// 0x0135 UBX-NAV-SAT satellite information
			
			// 0x0121 UBX-NAV-TIME-UTC UTC time solution
			if(msgid == "0121"){
				if (msg.size()==(20+2)*2){
					HexToBin((char *) msg.substr(12*2,2*sizeof(U2)).c_str(),sizeof(U2),(unsigned char *) &UTCyear);
					HexToBin((char *) msg.substr(14*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &UTCmon);
					HexToBin((char *) msg.substr(15*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &UTCday);
					HexToBin((char *) msg.substr(16*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &UTChour);
					HexToBin((char *) msg.substr(17*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &UTCmin);
					HexToBin((char *) msg.substr(18*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &UTCsec);
					HexToBin((char *) msg.substr(19*2,2*sizeof(X1)).c_str(),sizeof(X1),(unsigned char *) &UTCvalid);
					DBGMSG(debugStream,TRACE,currpctime << " UTC:" << UTCyear << " " << (int) UTCmon << " " << (int) UTCday << " "
						<< (int) UTChour << ":" << (int) UTCmin << ":" << (int) UTCsec << std::hex << " valid=0x" << (unsigned int) UTCvalid << std::dec);
					if (UTCvalid & 0x04)
						currentMsgs |= MSG0121;
					else{
						DBGMSG(debugStream,WARNING,"UTC not valid yet");
					}
				}
				else{
					DBGMSG(debugStream,WARNING,"Bad 0121 message size");
				}
				continue;
			}
			
			// 0x0122 UBX-NAV-CLOCK clock solution  (clock bias)
			if(msgid == "0122"){
				if (msg.size()==(20+2)*2){
						HexToBin((char *) msg.substr(0*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf); // GPS tow of navigation epoch (ms)
						HexToBin((char *) msg.substr(4*2,2*sizeof(I4)).c_str(),sizeof(I4),(unsigned char *) &clockBias); // in ns
						
						DBGMSG(debugStream,TRACE,"GPS tow=" << u4buf << "ms" << " clock bias=" << clockBias << " ns");
						currentMsgs |= MSG0122;
				}
				else{
					DBGMSG(debugStream,WARNING,"Bad 0122 message size");
				}
				continue;
			}
			
			// 0x2703 UBX-SEC-UNIQID unique chip ID
			if (msgid == "2703"){ // Polled for at the beginning of each day
				if (msg.size()==(9+2)*2){
						// This is easy - the hex string is just what we want !
					  serialNumber = "0x" + msg.substr(4*2,2*5);
						DBGMSG(debugStream,INFO,"receiver serial number = " << serialNumber);
				}
				else{
					DBGMSG(debugStream,WARNING,"Bad 2703 message size");
				}
				continue;
			}
			
			
			//
			// Messages needed to contruct the RINEX navigation file
			//
			
			// Ionosphere parameters, UTC parameters 
			if (!gps.gotUTCdata){
				if(msgid == "0b02"){
					if (msg.size()==(72+2)*2){
						HexToBin((char *) msg.substr(4*2,2*sizeof(R8)).c_str(),sizeof(R8),(unsigned char *) &(gps.UTCdata.A0)); 
						HexToBin((char *) msg.substr(12*2,2*sizeof(R8)).c_str(),sizeof(R8),(unsigned char *) &r8buf);
						gps.UTCdata.A1=r8buf;
						HexToBin((char *) msg.substr(20*2,2*sizeof(I4)).c_str(),sizeof(I4),(unsigned char *) &i4buf);
						gps.UTCdata.t_ot = i4buf;
						HexToBin((char *) msg.substr(24*2,2*sizeof(I4)).c_str(),sizeof(I4),(unsigned char *) &i2buf);
						gps.UTCdata.WN_t=i2buf;
						HexToBin((char *) msg.substr(26*2,2*sizeof(I4)).c_str(),sizeof(I4),(unsigned char *) &i2buf);
						leapsecs = i2buf;
						HexToBin((char *) msg.substr(28*2,2*sizeof(I4)).c_str(),sizeof(I4),(unsigned char *) &i2buf);
						gps.UTCdata.WN_LSF=i2buf;
						HexToBin((char *) msg.substr(30*2,2*sizeof(I4)).c_str(),sizeof(I4),(unsigned char *) &i2buf);
						gps.UTCdata.DN=i2buf;
						HexToBin((char *) msg.substr(32*2,2*sizeof(I4)).c_str(),sizeof(I4),(unsigned char *) &i2buf);
						gps.UTCdata.dt_LSF=i2buf;
						
						HexToBin((char *) msg.substr(36*2,2*sizeof(R4)).c_str(),sizeof(R4),(unsigned char *) &(gps.ionoData.a0));
						HexToBin((char *) msg.substr(40*2,2*sizeof(R4)).c_str(),sizeof(R4),(unsigned char *) &(gps.ionoData.a1));
						//gps.ionoData.a1 /= ICD_PI;
						HexToBin((char *) msg.substr(44*2,2*sizeof(R4)).c_str(),sizeof(R4),(unsigned char *) &(gps.ionoData.a2));
						//gps.ionoData.a2 /= (ICD_PI*ICD_PI);
						HexToBin((char *) msg.substr(48*2,2*sizeof(R4)).c_str(),sizeof(R4),(unsigned char *) &(gps.ionoData.a3));
						//gps.ionoData.a3 /= (ICD_PI*ICD_PI*ICD_PI);
						
						HexToBin((char *) msg.substr(52*2,2*sizeof(R4)).c_str(),sizeof(R4),(unsigned char *) &(gps.ionoData.B0));
						HexToBin((char *) msg.substr(56*2,2*sizeof(R4)).c_str(),sizeof(R4),(unsigned char *) &(gps.ionoData.B1));
						//gps.ionoData.B1 /= ICD_PI;
						HexToBin((char *) msg.substr(60*2,2*sizeof(R4)).c_str(),sizeof(R4),(unsigned char *) &(gps.ionoData.B2));
						//gps.ionoData.B2 /= (ICD_PI*ICD_PI);
						HexToBin((char *) msg.substr(64*2,2*sizeof(R4)).c_str(),sizeof(R4),(unsigned char *) &(gps.ionoData.B3));
						//gps.ionoData.B3 /= (ICD_PI*ICD_PI*ICD_PI);
						
						gps.gotUTCdata=true;
						gps.gotIonoData=true;
					}
					else{
						DBGMSG(debugStream,WARNING,"Bad 0b02 message size");
					}
					continue;
				}
			}
		
			if(msgid == "0213"){
				U1 gnssID;
				U1 svID,sigID;
				U1 numWords;
				U1 freqID,chn;
				U1 ubuf[10*4]; // max number of data words is currently 10
				HexToBin((char *) msg.substr(0*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &gnssID);
				HexToBin((char *) msg.substr(1*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &svID);
				HexToBin((char *) msg.substr(2*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &sigID);
				HexToBin((char *) msg.substr(3*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &freqID);
				HexToBin((char *) msg.substr(4*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &numWords);
				HexToBin((char *) msg.substr(5*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &chn);
				HexToBin((char *) msg.substr(8*2,2*sizeof(U1)*numWords*4).c_str(),sizeof(U1)*40,(unsigned char *) &ubuf);
				//std::cerr << (int) gnssID << " " << (int) svID << " " << (int) sigID << " " << (int) numWords << std::endl;
				
				switch (gnssID)
				{
					case 0:
					{
						if (measTOW >=0) {readGPSEphemerisLNAVSubframe(svID,ubuf,(int) measTOW,(int) measGPSWN);}
						break;
					}
					case 2: 
					{
						readGALEphemerisINAVSubframe(svID,sigID,ubuf);
						break;
					}
					default:break;
				}
			}
			
			// Ephemeris
			if(msgid == "0b31"){
				if (msg.size()==(8+2)*2){
					DBGMSG(debugStream,WARNING,"Empty ephemeris");
				}
				else if (msg.size()==(104+2)*2){
					GPSEphemeris *ed = readGPSEphemeris(msg);
					int pchh,pcmm,pcss;
					if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss)))
						ed->tLogged = pchh*3600 + pcmm*60 + pcss; 
					else
						ed->tLogged = -1;
					gps.addEphemeris(ed); // FIXME memory leak
				}
				else{
					DBGMSG(debugStream,WARNING,"Bad 0b31 message size");
				}
				continue;
			} // ephemeris
		}
	} // infile is open
	else{
		app->logMessage(" unable to open " + fname);
		return false;
	}
	infile.close();
	
	timer.stop();
	
	DBGMSG(debugStream,INFO,"elapsed time: " << timer.elapsedTime(Timer::SECS) << " s");
	
	leapsecs = measLeapSecs;
	
	if (!gps.gotUTCdata){ 
		app->logMessage("failed to find ionosphere/UTC parameters");
		return false; 
	}
	
	if (measurements.size() == 0){
		app->logMessage(" no measurements available in " + fname);
		return false;
	}
	
	gps.fixWeekRollovers();
	gps.setAbsT0c(mjd); 
	
	galileo.fixWeekRollovers();
	galileo.setAbsT0c(mjd);
	
	// Pass through the data to realign the sawtooth correction.
	// This could be done in the main loop but it's more flexible this way.
	// For the ublox, the sawtooth correction applies to the next second
	// If we're missing the sawtooth correction because of eg a gap in the data
	// then we'll just use the current sawtooth. 
	
	DBGMSG(debugStream,TRACE,"Fixing sawtooth");
	double prevSawtooth=measurements.at(0)->sawtooth;
	time_t    tPrevSawtooth=mktime(&(measurements.at(0)->tmUTC));
	int nBadSawtoothCorrections = 1; // first is bad !
	// First point is untouched
	for (unsigned int i=1;i<measurements.size();i++){
		double sawTmp = measurements.at(i)->sawtooth;
		time_t tTmp= mktime(&(measurements.at(i)->tmUTC));
		if (tTmp - tPrevSawtooth == 1){
			measurements.at(i)->sawtooth = prevSawtooth;
		}
		else{
			// do nothing - the current value is the best guess
			nBadSawtoothCorrections++;
		}
		prevSawtooth=sawTmp;
		tPrevSawtooth=tTmp;
	}
	
	// The ublox sometime reports what appears to be an incorrect pseudorange after picking up an SV
	// If you wanted to filter these out, this is where you should do it
	
	// Fix 1 ms ambiguities/steps in the pseudorange
	// Do this initially and then every time a step is detected
	
	DBGMSG(debugStream,TRACE,"Fixing ms ambiguities");
	int nDropped[GNSSSystem::GALILEO+1];
	for (int i=0;i<=GNSSSystem::GALILEO;i++)
		nDropped[i]=0;
	
	for (int g=GNSSSystem::GPS;g<=GNSSSystem::GALILEO;(g <<= 1)){
		if (!(g & constellations)) continue;
		if (app->positioningMode)  continue; // measurements as reported by the receiver are untouched
		DBGMSG(debugStream,INFO,"Fixing ms ambiguities for " << g);
		GNSSSystem *gnss;
		switch (g){
			case GNSSSystem::BEIDOU:gnss=&beidou;break;
			case GNSSSystem::GALILEO:gnss=&galileo;break;
			case GNSSSystem::GLONASS:gnss=&glonass;break;
			case GNSSSystem::GPS:gnss=&gps;break;
			default:break;
		}
		for (int svn=1;svn<=gnss->maxSVN();svn++){
			unsigned int lasttow=99999999,currtow=99999999;
			double lastmeas=0,currmeas;
			double corr=0.0;
			bool first=true;
			bool ok=false;
			for (unsigned int i=0;i<measurements.size();i++){
				unsigned int m=0;
				while (m < measurements[i]->meas.size()){
					// This solves the issue of the software confuding GNSS systems (What does this mean? I am a bit confuded)
					if ((svn==measurements[i]->meas[m]->svn) && (measurements[i]->meas[m]->code == GNSSSystem::C1C) && (measurements[i]->meas[m]->constellation ==g )){
						lasttow=currtow;
						lastmeas=currmeas;
						currmeas=measurements[i]->meas[m]->meas;
						currtow=measurements[i]->gpstow;
						
						DBGMSG(debugStream,TRACE,svn << " " << currtow << " " << currmeas << " ");
						if (first){
							ok = gnss->resolveMsAmbiguity(antenna,measurements[i],measurements[i]->meas[m],&corr);
							if (ok){
								first=false;
							}
						}
						else if (currtow - lasttow > MAX_GAP){
							DBGMSG(debugStream,TRACE,"gap " << svn << " " << lasttow << "," << lastmeas << "->" << currtow << "," << currmeas);
							ok = gnss->resolveMsAmbiguity(antenna,measurements[i],measurements[i]->meas[m],&corr);
						}
						else if (currtow > lasttow){
							if (fabs(currmeas-lastmeas) > CLOCKSTEP*SLOPPINESS){
								DBGMSG(debugStream,TRACE,"first/step " << svn << " " << lasttow << "," << lastmeas << "->" << currtow << "," << currmeas);
								ok = gnss->resolveMsAmbiguity(antenna,measurements[i],measurements[i]->meas[m],&corr);
							}
						}
						if (ok){ 
							measurements[i]->meas[m]->meas += corr;
							m++;
						}
						else{ // ambiguity correction failed, so drop the measurement
							DBGMSG(debugStream,TRACE,"failed!");
							nDropped[g] += 1;
							measurements[i]->meas.erase(measurements[i]->meas.begin()+m); // FIXME memory leak
						}
						break;
					}
					else
						m++; // skip over other GNSS systems (for now)
					
				} // 
			} // for (unsigned int i=0;i<measurements.size();i++){
		}
	}
	
	DBGMSG(debugStream,INFO,"done: read " << linecount << " lines");
	DBGMSG(debugStream,INFO,measurements.size() << " measurements read");
	DBGMSG(debugStream,INFO,gps.ephemeris.size() << " GPS ephemeris entries read");
	DBGMSG(debugStream,INFO,galileo.ephemeris.size() << " GAL ephemeris entries read");
	DBGMSG(debugStream,INFO,nBadSawtoothCorrections << " bad sawtooth corrections");
	for (int g=GNSSSystem::GPS;g<=GNSSSystem::GALILEO;(g <<= 1)){
		if (!(g & constellations)) continue;
		GNSSSystem *gnss;
		switch (g){
			case GNSSSystem::BEIDOU:gnss=&beidou;break;
			case GNSSSystem::GALILEO:gnss=&galileo;break;
			case GNSSSystem::GLONASS:gnss=&glonass;break;
			case GNSSSystem::GPS:gnss=&gps;break;
			default:break;
		}
		DBGMSG(debugStream,INFO,"dropped " << nDropped[g] << " " << gnss->name() << " SV measurements (ms ambiguity failure)"); 
	}
	DBGMSG(debugStream,INFO,alertPagesCnt << " alert pages in navigation data");
	return true;
	
}

// This catches eg the situation where part of an ephemeris is transmitted
// and then a new one starts transmitting. We want to scrub the partially transmitted ephemeris.
//
bool Ublox::checkGalIODNav(GalEphemeris *ed,int IODnav)
{
	if (!(ed->subframes & 0x0f)){ // if we've got a subframe 1 - 4 then all is OK 
		ed->IODnav = IODnav;
		return true;
	}
	else if (IODnav != ed->IODnav){
		//std::cerr << "IODnav mismatch! " << IODnav << " " << ed->IODnav << " SV= " << (int) ed->SVN << std::endl;
		ed->subframes = 0x0;
		return false;
	}
	return true; // dummy
}

// To use the MID macro, LSB is bit 0, m is first bit, n is last bit
#define LAST(k,n) ((k) & ((1<<(n))-1))
#define MID(k,m,n) LAST((k)>>(m),((n)-(m)+1)) 
	
GPSEphemeris* Ublox::readGPSEphemeris(std::string msg)
{
	
	U4 u4buf;
	HexToBin((char *) msg.substr(0*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	GPSEphemeris* ed= new GPSEphemeris();
	ed->SVN=u4buf;
	DBGMSG(debugStream,INFO,"Ephemeris for SV" << (int) ed->SVN);
	
	// Data is in bits 0-23, parity bits are discarded by ublox
	// To translate from ICD numbering b24 (ICD) -> b0 (ublox)
	// subframe 1
	// word 3
	HexToBin((char *) msg.substr(8*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	
	ed->week_number = MID(u4buf,14,23);
	ed->SV_accuracy_raw=MID(u4buf,8,11); 
	ed->SV_accuracy = GPS::URA[ed->SV_accuracy_raw];
	ed->SV_health=MID(u4buf,2,7);
	// IODC b23-b24 (upper bits)
	unsigned int hibits=MID(u4buf,0,1);
	hibits = hibits << 8;
	
	//fprintf(stderr,"%i %08x %i %i %i\n",(int) ed->SVN,u4buf, (int) ed->week_number,
	//	(int) ed->SV_accuracy, ed->SV_health);
	
	// word 7 
	HexToBin((char *) msg.substr(24*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	//Tgd b17-b24 (ICD) CHECKED
	signed char tGD = MID(u4buf,0,7); // signed, scaled by 2^-31
	ed->t_GD =  (double) tGD / (double) pow(2,31);
	
	//fprintf(stderr,"%08x %.12e\n",u4buf,ed->t_GD);
	
	// word 8
	HexToBin((char *) msg.substr(28*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	// IODC b1-b8 (lower bits) // CHECKED
	unsigned int lobits = MID(u4buf,16,23);
	ed->IODC = hibits | lobits;
	// t_OC b9-b24 (ICD)
	ed->t_OC = 16*MID(u4buf,0,15);
	//fprintf(stderr,"%08x %e %i\n",u4buf,ed->t_OC,(int) ed->IODC);
	
	// word 9 a_f2 b1-b8, a_f1 b9-b24 // CHECKED a_f1
	HexToBin((char *) msg.substr(32*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	signed char af2 = MID(u4buf,16,23);
	ed->a_f2 = af2/pow(2,55);
	signed short af1= MID(u4buf,0,15);
	ed->a_f1 = (double) af1/(double) pow(2,43);
	
	//fprintf(stderr,"%08x %.12e %.12e\n",u4buf,ed->a_f1,ed->a_f2);
	if (ed->a_f2 != 0.0) fprintf(stderr,"BING!\n");

	// word 10 a_f0 b1-b22 // CHECKED
	HexToBin((char *) msg.substr(36*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	int tmp = (MID(u4buf,2,23) << 10);
	tmp = tmp >> 10;
	ed->a_f0 = (double) tmp /(double) pow(2,31); // signed, scaled by 2^-31
	//fprintf(stderr,"%08x %.12e\n",u4buf,ed->a_f0);
	
	// data frame 2
	// word 3
	// IODE b1-b8 // CHECKED
	HexToBin((char *) msg.substr(40*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	ed->IODE = MID(u4buf,16,23);
	// C_rs b9-b24 // CHECKED
	signed short Crs= MID(u4buf,0,15);
	ed->C_rs= (double) Crs/ (double) 32.0;
	// fprintf(stderr,"%08x %i %.12e\n",u4buf,ed->IODE,ed->C_rs);
	
	// word 4
	// deltaN b1-b16 // CHECKED nb this is a SINGLE so differences in 7 or 8th digit in RINEX files
	HexToBin((char *) msg.substr(44*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	signed short deltaN=MID(u4buf,8,23);
	ed->delta_N = ICD_PI*(double) deltaN/(double) pow(2,43); // GPS units are semi-circles/s, RINEX units are rad/s
	// M_0 (upper 8 bits) b17-b24
	hibits =  MID(u4buf,0,7) << 24;
	
	// word 5
	// M_0 (lower 24 bits) b1-b24 // CHECKED
	HexToBin((char *) msg.substr(48*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	lobits = MID(u4buf,0,23);
	
	ed->M_0 = ICD_PI * ((double) ((int) (hibits | lobits)))/ (double) pow(2,31);
	//fprintf(stderr,"%08x %.12e %.12e\n",u4buf,ed->delta_N, ed->M_0);
	
	// word 6
	// C_uc b1-b16 // CHECKED
	HexToBin((char *) msg.substr(52*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	signed short Cuc=MID(u4buf,8,23);
	ed->C_uc = (double) Cuc/(double) pow(2,29);
	// e b17-b24 (upper 8 bits)
	hibits =  MID(u4buf,0,7) << 24;
	
	// word 7
	// e b1-b24 (lower 24 bits) // CHECKED
	HexToBin((char *) msg.substr(56*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	lobits = MID(u4buf,0,23);
	ed->e = ((double) (unsigned int)((hibits | lobits)))/ (double) pow(2,33);
	//fprintf(stderr,"%08x %.12e %.12e \n",u4buf,ed->C_uc,ed->e);
	
	// word 8
	// C_us b1-b16 // CHECKED
	HexToBin((char *) msg.substr(60*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	signed short Cus=MID(u4buf,8,23);
	ed->C_us = (double) Cus/(double) pow(2,29);
	// sqrtA b1-b8 (upper bits)
	hibits =  MID(u4buf,0,7) << 24;
	
	// word 9
	//sqrtA b1-b24 (lower bits) // CHECKED
	HexToBin((char *) msg.substr(64*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	lobits = MID(u4buf,0,23);
	ed->sqrtA = ((double) (unsigned int)((hibits | lobits)))/ (double) pow(2,19);
	//fprintf(stderr,"%08x %.12e %.12e\n",u4buf,ed->C_us,ed->sqrtA);
	
	// word 10
	// t_OE b1-b16 // CHECKED
	HexToBin((char *) msg.substr(68*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	unsigned short toe=MID(u4buf,8,23);
	ed->t_0e = toe * 16;
	//fprintf(stderr,"%08x %.12e \n",u4buf,ed->t_oe);
	
	// data frame 3
	// word 3
	// C_ic b1-b16 // CHECKED
	HexToBin((char *) msg.substr(72*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	signed short Cic=MID(u4buf,8,23);
	ed->C_ic = (double) Cic/(double) pow(2,29);
	// OMEGA_0 b17-b24 (upper bits)
	hibits =  MID(u4buf,0,7) << 24;
	
	// word 4
	// OMEGA_0 b1-b24 lower bits // CHECKED
	HexToBin((char *) msg.substr(76*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	lobits = MID(u4buf,0,23);
	ed->OMEGA_0 = ICD_PI * ((double) (signed int)((hibits | lobits)))/ (double) pow(2,31);
	//fprintf(stderr,"%08x %.12e %.12e\n",u4buf,ed->C_ic,ed->OMEGA_0);
	
	// word 5
	// C_is b1-b16 // CHECKED
	HexToBin((char *) msg.substr(80*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	signed short Cis=MID(u4buf,8,23);
	ed->C_is= (double) Cis/(double) pow(2,29);
	// i_0 b17-b24 (upper bits)
	hibits =  MID(u4buf,0,7) << 24;
	
	// word 6
	// i_0 b1-b24 (lower bits) // CHECKED
	HexToBin((char *) msg.substr(84*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	lobits = MID(u4buf,0,23);
	ed->i_0 = ICD_PI * ((double) (signed int)((hibits | lobits)))/ (double) pow(2,31);
	//fprintf(stderr,"%08x %.12e %.12e\n",u4buf,ed->C_is,ed->i_0);
	
	// word 7
	// C_rc b1-b16 // CHECKED
	HexToBin((char *) msg.substr(88*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	signed short Crc=MID(u4buf,8,23);
	ed->C_rc= (double) Crc/32.0;
	// OMEGA b17-b24 (upper bits)
	hibits =  MID(u4buf,0,7) << 24;
	
	// word 8
	// OMEGA b1-b24 (lower bits) // CHECKED
	HexToBin((char *) msg.substr(92*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	lobits = MID(u4buf,0,23);
	ed->OMEGA = ICD_PI * ((double) (signed int)((hibits | lobits)))/ (double) pow(2,31);
	//fprintf(stderr,"%08x %.12e %.12e\n",u4buf,ed->C_rc,ed->OMEGA);
	
	
	// word 9
	// OMEGA_DOT b1-b24 // CHECKED
	HexToBin((char *) msg.substr(96*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	int odot = (MID(u4buf,0,23)) << 8;
	odot = odot >> 8;	
	ed->OMEGADOT = ICD_PI * (double) (odot)/ (double) pow(2,43);
	//fprintf(stderr,"%08x %.12e\n",u4buf,ed->OMEGADOT);

	// word 10
	// IODE b1-b8 (repeated to facilitate checking for data cutovers) 
	HexToBin((char *) msg.substr(100*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	int iode = MID(u4buf,16,23);
	// IDOT b9-b22 // CHECKED
	int idot = (MID(u4buf,2,15)) << 18;
	idot = idot >> 18;
	ed->IDOT = ICD_PI * (double) (idot)/ (double) pow(2,43);
	//fprintf(stderr,"\n%08x %d %.12e\n",u4buf,iode,ed->IDOT);
	
	// FIXME Now check consistency of IODE
	// The ICD says that the lower 8 bits of the IOCD must be the same as IODE
	return ed;
}

static unsigned int   U4x(unsigned char *p) {unsigned int   u; memcpy(&u,p,4); return u;}

void Ublox::readGALEphemerisINAVSubframe(int svID,int sigID,unsigned char *ubuf)
{
	// The ublox sends the even and odd pages together
	// There are 4 x 32 bit words in each page
	unsigned int dwords[8];
	unsigned char *p = ubuf;
	for (int i=0;i<8;i++,p+=4) {
		dwords[i] = U4x(p);
	}
	
	unsigned char evenPage =  dwords[0] >> 31;
	unsigned char pageType = (dwords[0] >> 30) & 0x01; 
	unsigned char wordType = (dwords[0] >> 24 ) & 0x3f; 
	
	if (pageType == 0x01){ // an alert page - not handled
		alertPagesCnt++;
		DBGMSG(debugStream,WARNING,svID << " alert page");
		return;
	}
	//std::cerr << svID << " " << sigID << " " << " " << (int) evenPage << " " << 
	//	(int) pageType << " " << (int) wordType << std::endl;
		
	if (wordType == 0 || wordType == 63) return; // 0 flags spare word, 63 flags no valid data is available
	
	unsigned int hibits,lobits;
	
	GalEphemeris *ed = galEph[svID];	
	ed->SVN = svID;
	
	if (wordType == 1){
		
		unsigned short IODnav = MID(dwords[0],14,23);
		if (!checkGalIODNav(ed,IODnav))
			return;
		
		// t_0e, word 1, 14 bits unsigned, scale factor 60 (ie in minutes)
		ed->t_0e = 60*MID(dwords[0],0,13);
		
		// M0 , word 2, 32 bits signed, scale factor 2^-31
		ed->M_0 = ICD_PI * (double) ((int) dwords[1]) / (double) pow(2,31);
		
		// e, word 3, 32 bits unsigned, scale factor 2^-33
		ed->e = (double) dwords[2]/ (double) pow(2,33); 
		
		 // sqrtA, 32 bits unsigned, scale factor 2^-19 
		hibits = MID(dwords[3],14,31) << 14; // word 4 top 18 bits
		lobits = MID(dwords[4],16,29); // sqrtA, odd page, word 1, 14 bits
		ed->sqrtA = (double) ((hibits | lobits))/(double ) pow(2,19);    
		
		//std::cerr << svID << " " << (int) ed->IODnav << " " << (int) ed->t_0e << " " << ed->M_0 << 
		//" " << ed->e << " " << ed->sqrtA << std::endl;
		
		//std::cerr << "W1 " << svID << " " << (int) ed->IODnav <<  std::endl;
		
		ed->subframes |= 0x01;
	}
	else if (wordType == 2){
		
		unsigned short IODnav = MID(dwords[0],14,23);
		if (!checkGalIODNav(ed,IODnav))
			return;
		
		// OMEGA_0, 32 bits signed, scale factor 2^-31
		hibits = (dwords[0] & 0x3fff) << 18; // upper 14 bits
		lobits =  dwords[1] >> 14; // lower 18 bits
		ed->OMEGA_0 = ICD_PI* (double) ((int) (hibits|lobits)) / (double) pow(2,31);
		
		// i_0 ,   32 bits signed, scale factor 2^-31
		hibits = (dwords[1] & 0x3fff) << 18; // upper 14 bits
		lobits =  dwords[2] >> 14; //lower 18 bits
		ed->i_0 = ICD_PI* (double) ((int) (hibits|lobits)) / (double) pow(2,31);
		
		// OMEGA   ,   32 bits signed, scale factor 2^-31
		hibits = (dwords[2] & 0x3fff) << 18; // upper 14 bits
		lobits =  dwords[3] >> 14; //lower 18 bits
		ed->OMEGA = ICD_PI* (double) ((int) (hibits|lobits)) / (double) pow(2,31);
		
		// odd page - 2 bits leading
		
		// idot,   14 bits signed, scale factor 2^-43		
		ed->IDOT = ICD_PI * (double) (((int) (MID(dwords[4],16,29) << 18)) >> 18)/ (double) pow(2,43);
		
		//std::cerr << svID << " " << (int) IODnav << " " << std::setprecision(14) << 
		//	ed->OMEGA_0 << " " << ed->i_0 << " " << ed->OMEGA << " " << ed->IDOT << std::endl;
		
		// std::cerr << "W2 " << svID << " " << (int) IODnav <<  std::endl;
		
		ed->subframes |= 0x02;
	}
	else if (wordType == 3){
		
		unsigned short IODnav = MID(dwords[0],14,23); 
		if (!checkGalIODNav(ed,IODnav))
			return;
		
		// nb 18 bits now consumed
		
		// OMEGA_DOT 24 bits signed scale factor 2^-43
		hibits =  MID(dwords[0],0,13) << 10; // upper 14 bits
		lobits =  MID(dwords[1],22,31);      //lower 10 bits
		ed->OMEGADOT = ICD_PI * (double) (((int) ((hibits|lobits) << 10)) >> 10) / (double) pow(2,43);
		
		// delta_N   16 bits signed scale factor 2^-43
		ed->delta_N = ICD_PI * (double) ((short) MID(dwords[1],6,21))/ (double) pow(2,43);
		
		// C_uc      16 bits signed scale factor 2^-29 (rad)
		hibits =  MID(dwords[1],0,5) << 10; // upper 6 bits
		lobits =  MID(dwords[2],22,31);      //lower 10 bits
		ed->C_uc = (double) ((short) (hibits|lobits) )/ (double) pow(2,29);
		
		// C_us      16 bits signed scale factor 2^-29 (rad)
		ed->C_us = (double) ((short) MID(dwords[2],6,21))/ (double) pow(2,29);
		
		// C_rc      16 bits signed scale factor 2^-5 (m)
		hibits =  MID(dwords[2],0,5) << 10; // upper 6 bits
		lobits =  MID(dwords[3],22,31);     //lower 10 bits
		ed->C_rc = (double) ((short) (hibits|lobits) )/ (double) 32.0;
		
		// C_rs      16 bits signed scale factor 2^-5 (m)
		hibits =  MID(dwords[3],14,21) << 8; // upper 8 bits
		// skip bits 30,31 of odd page header
		lobits =  MID(dwords[4],22,29);     //lower 8 bits
		ed->C_rs = (double) ((short) (hibits|lobits) )/ (double) 32.0;
		
		// SISA       8 bits unsigned
		ed->SISA = galileo.decodeSISA( MID(dwords[4],14,21));
		
		//std::cerr << svID << " " << (int) IODnav << " " << std::setprecision(14) << 
		//	ed->OMEGADOT << " " << ed->delta_N << " " << ed->C_uc << " " << ed->C_us << 
		//	" " << ed->C_rc << " " << ed->C_rs << " " << ed->SISA << std::endl;
		
		// std::cerr << "W3 " << svID << " " << (int) IODnav <<  std::endl;
		
		ed->subframes |= 0x04;
		
	}
	else if (wordType == 4){
		
		unsigned short IODnav = MID(dwords[0],14,23);
		if (!checkGalIODNav(ed,IODnav))
			return;
		
		// 18 bits consumed 
		
		// SVID   6 bits
		unsigned char svid = MID(dwords[0],8,13);
		if (svid != svID){ // out of sync so clear any data we might have accumulated
			ed->subframes = 0x0;
			return;
		}
		// C_ic  16 bits signed scale factor 2^-29
		hibits =  MID(dwords[0],0,7) << 8; // upper 8 bits
		lobits =  MID(dwords[1],24,31);     //lower 8 bits
		ed->C_ic = (double) ((short) (hibits|lobits) )/ (double) pow(2,29);
		
		// C_is  16 bits signed scale factor 2^-29
		ed->C_is = (double) ((short) MID(dwords[1],8,23 ))/ (double) pow(2,29);
		
		// Clock correction parameters
		
		// t_0c  14 bits unsigned scale factor 60
		hibits =  MID(dwords[1],0,7) << 6; // upper 8 bits
		lobits =  MID(dwords[2],26,31);     //lower 6 bits
		ed->t_0c = 60.0 * (double) (((unsigned short) (( hibits| lobits ) << 2)) >> 2);
		
		// a_f0  31 bits signed scale factor 2^-34
		hibits =  MID(dwords[2],0,25) << 5; // upper 26 bits
		lobits =  MID(dwords[3],27,31);     // lower 5 bits
		ed->a_f0 = (double) (((int) ((hibits|lobits) << 1)) >> 1)/ (double) pow(2,34);
		
		// a_f1  21 bits signed scale factor 2^-46
		hibits =  MID(dwords[3],14,26) << 8; // upper 13 bits
		// odd page
		lobits =  MID(dwords[4],22,29);     // lower 8 bits
		ed->a_f1 = (double) (((int) ((hibits|lobits) << 11)) >> 11)/ (double) pow(2,46);
		
		// a_f2   6 bits signed scale factor 2^-59
		ed->a_f2 = (double) (((char) (MID(dwords[4],16,21) << 2)) >> 2)/ (double) pow(2,59);
		
		// 2 bits spare - maybe for the guv' ?
		
// 		std::cerr << svID << " " << (int) IODnav << " " << (int) svid << std::setprecision(14) 
// 			<< " " << ed->C_ic << " " << ed->C_is << " " << ed->t_0c 
// 			<< " " << ed->a_f0 << " " << ed->a_f1 << " " << ed->a_f2 << std::endl;
// 		
// 		std::cerr << "W4 " << svID << " " << (int) IODnav <<  std::endl;
// 		
		ed->subframes |= 0x08;
	}
	else if (wordType == 5){ // Ionospheric correction, BGD, signal health and data validity status and GST
		// 12 24 0.33984375 0.004425048828125 0 -1.2340024113655e-08 -1.2340024113655e-08 0 1059
		
		if (!galileo.gotIonoData){
			// ai0 11 bits unsigned scale factor 2^-2
			galileo.ionoData.ai0 = (double) MID(dwords[0],13,23) / (double) 4.0;
			
			// ai1 11 bits signed   scale factor 2^-8
			galileo.ionoData.ai1 = (double) ( ((short) (MID(dwords[0],2,12) << 5)) >> 5) / (double) pow(2,8);
			
			// ai2 14 bits signed   scale factor 2^-15
			hibits = MID(dwords[0],0,1) << 12;   // upper  2 bits
			lobits = MID(dwords[1],20,31) ; // lower 12 bits
			galileo.ionoData.ai2 = (double) (((short) ((hibits|lobits) << 2)) >> 2) / (double) pow(2,15);
			
			// ionospheric disturbance flags 5 bits
			galileo.ionoData.SFflags = MID(dwords[1],15,19);
			
			galileo.gotIonoData = true;
		}
		
		// BGD E1-E5a 10 bits signed scale factor 2^-32
		ed->BGD_E1E5a = (double) ( ((short) (MID(dwords[1],5,14) << 6)) >> 6) / (double) pow(2,32);
		
		// BGD E1-E5b 10 bits signed scale factor 2^-32
		hibits = MID(dwords[1],0,4) << 5;   // upper 5 bits
		lobits = MID(dwords[2],27,31) ;     // lower 5 bits
		ed->BGD_E1E5b = (double) (((short) ((hibits|lobits) << 6)) >> 6) / (double) pow(2,32);
		
		// signal validity and health flags // 6 bits
		ed->sigFlags = MID(dwords[2],21,26); // FIXME may be wrong
		
		// GST
		
		// WN  12 bits unsigned (rolls over every 78 years)
		ed->WN = MID(dwords[2],9,20); 
		
		// TOW 20 bits unsigned
		hibits = MID(dwords[2],0,8) << 11;   // upper  9 bits
		lobits = MID(dwords[3],21,31) ;      // lower 11 bits
		ed->TOW = (double) (hibits | lobits); 
		
		ed->subframes |= 0x10;
		
// 		std::cerr << svID << " " << std::setprecision(14) 
// 			<< galileo.ionoData.ai0 << " " << galileo.ionoData.ai1 << " " << galileo.ionoData.ai2 
// 			<< " " << (int) galileo.ionoData.SFflags << " "
// 			<< ed->BGD_E1E5a << " " << ed->BGD_E1E5b << " " << (int) ed->sigFlags 
// 			<< " " << (int) ed->WN << " " << (int) ed->TOW << std::endl;
		
		// std::cerr << "W5 " << svID << " " << (int) ed->IODnav <<  std::endl;
			
	}
	else if (wordType == 6){ // GST - UTC conversion parameters
		
		if (!galileo.gotUTCdata){
			// A0    32 bits signed, scale factor 2^-30
			hibits = MID(dwords[0],0,23) << 8;   // upper  24 bits
			lobits = MID(dwords[1],24,31) ;      // lower   8 bits
			galileo.UTCdata.A0 = (double) ((int) (hibits | lobits)) / (double) pow(2,30);
			
			// A1    24 bits signed, scale factor 2^-50
			galileo.UTCdata.A1 = (double) (((int) (MID(dwords[1],0,23) << 8)) >> 8) / (double) pow(2,50);
			
			// dt_LS  8 bits signed 
			galileo.UTCdata.dt_LS = MID(dwords[2],24,31);
			leapsecs = galileo.UTCdata.dt_LS;
			
			// t_0t   8 bits unsigned, scale factor 3600
			galileo.UTCdata.t_0t  = 3600 * MID(dwords[2],16,23);

			// WN_0t  8 bits unsigned
			galileo.UTCdata.WN_0t  = MID(dwords[2],8,15);
			
			// WN_LSF 8 bits unsigned
			galileo.UTCdata.WN_LSF  = MID(dwords[2],0,7);
			
			// DN     3 bits unsigned (1 to 7 are the valid values)
			galileo.UTCdata.DN = MID(dwords[3], 29,31);
			
			// dt_LSF 8 bits signed 
			galileo.UTCdata.dt_LSF = (char) MID(dwords[3], 21,28); 
			
			// TOW   20 bits unsigned
			hibits = MID(dwords[3],14,20) << 13; // upper 7 bits
			lobits = MID(dwords[4],17,29);       // lower 13 bits
			unsigned int TOW = hibits | lobits;
			
			// 3 bits for the guv
			galileo.gotUTCdata = true;
		}
		//std::cerr << svID << " " << std::setprecision(14)  
		//<< galileo.UTCdata.A0 << " " << galileo.UTCdata.A1 << " " 
		//<< (int) galileo.UTCdata.dt_LS << " " << galileo.UTCdata.t_0t << " "
		//<< galileo.UTCdata.t_0t << " " << (int) galileo.UTCdata.WN_0t << " " << (int)  galileo.UTCdata.DN << " "
		//<< galileo.UTCdata.dt_LSF << " " << (int) TOW
		//<< std::endl;
	}
	else if (wordType == 10){ // GST-GPS parameters tucked away here
		// A_0G 16 bits signed, scale factor 2^-35
		// A_1G 12 bits signed, scale factor 2^-51
		// t_0G  8 bits unsigned, scale factor 3600
		// WN_0G 6 bits unsigned
	}
	else{ // 7-10 are almanac
	}
	
	// evenPage =  dwords[4] >> 31;
	//std::cerr << " " << (int) evenPage << std::endl;
	
	if (ed->subframes == 0x1f){ // got a complete ephemeris so process it
		// std::cerr << svID << " complete" << std::endl;
		ed->dataSource = 0x01 | 0x0100; // bit 0: I/NAV E1-B, bit 8: clock parametrs are E5A,E1
		if (!(galileo.addEphemeris(ed))){
			// don't delete it - reuse the buffer
			ed->subframes=0x0;
		}
		else{ // data was appended to the SV ephemeris list so create a new buffer for this SV
			ed = new GalEphemeris();
			galEph[svID]=ed;
			// DBGMSG(debugStream,INFO,"Added " << svID);
		}
		
	}
}

void Ublox::readGPSEphemerisLNAVSubframe(int svID,unsigned char *ubuf,int towTrans,int wnTrans)
{
	// Decode a GPS LNAV subframe
	// The (approximate) transmission time is used to resolve the week rollover
	
	// According to the manual:
	// UBX-RXM-SFRBX messages are only generated when complete subframes are detected by the
	// receiver and all appropriate parity checks have passed.
	// Where the parity checking algorithm requires data to be inverted before it is decoded (e.g. GPS
	// L1C/A), the receiver carries this out before the message output. Therefore, users can process data
	// directly and do not need to worry about repeating any parity processing.

	unsigned int dwords[10];
	unsigned char *p = ubuf;
	
	for (int i=0;i<10;i++,p+=4) {
		//fprintf(stderr,"%08x\n",U4x(p));
		dwords[i] = U4x(p) >> 6; // remove parity bits
	}
	
	int id = (dwords[1] >> 2) & 0x07; // Handover Word subframe ID bits 20-22
	
	//std::cerr << svID << " " << id << std::endl;
	
	GPSEphemeris *ed = gpsEph[svID];
	ed->t_ephem = (((dwords[1] >> 7) & 0x01ffff ) << 2)*((double) 604799 / (double) 403199.0); // CHECKME
	
	ed->SVN = svID;

	//if (id==1 && ed->subframes == 0x00 ){ 
	// The receiver does not always output subframes in sequential order
	// (Maybe signal not good and some frames fail parity check so the frames
	// come out in random order ?)
	// If you enforce strict order, then you miss some ephemerides and have fewer tracks,
	// particularly at the beginning of the day
	// If you're not fussy about order, then IODE/IODC consistency checking is done 
	// when all required subframes are received
	
	if (id==1  ){ 
		//std::cerr<< "WN " << (int) ((dwords[2] >> 14) & 1023) << std::endl; // transmission WN bits 1-10 
		// C/A or P on L2 bits 11-12
		// URA            bits 13-16
		// SV health      bits 17-22
		// IODC two MSBs  bits 23-24
		
		ed->subframes |= 0x01;
		
		ed->week_number =  ((dwords[2] >> 14) & 1023);  //  word 3 bits 1-10 
		
		ed->SV_accuracy_raw = ((dwords[2] >> 8) & 0xf);
		
		ed->SV_accuracy = GPS::URA[ed->SV_accuracy_raw];
		
		ed->SV_health = ((dwords[2] >> 2) & 63);
		
		unsigned int hibits = dwords[2] & 0x03;
		hibits = hibits << 8;
	
		signed char tGD = (dwords[6] & 0xff);// word 7 signed, scaled by 2^-31
		ed->t_GD =  (double) tGD / (double) pow(2,31);
		
		unsigned int lobits = (dwords[7] >> 16) & 0xff; // word 8
		ed->IODC = hibits | lobits;
	
		ed->t_OC = (SINGLE) (16*((dwords[7]) & 0xffff)); // word 8
		
		signed char af2 = (dwords[8] >> 16) & 0xff; // word 9
		ed->a_f2 = af2/pow(2,55);
		
		signed short af1 = dwords[8] & 0xffff;
		ed->a_f1 = (double) af1/(double) pow(2,43);
	
		int tmp = ((dwords[9] >> 2) << 10);
		tmp = tmp >> 10;
		ed->a_f0 = (double) tmp /(double) pow(2,31); // signed, scaled by 2^-31
		// fprintf(stderr,"f1 WN %d IODC %d TOC %g (%g)\n",ed->week_number,(int) ed->IODC, ed->t_OC, ed->t_OC - 86400*floor(ed->t_OC/86400)); 
			
	}
	//else if (id==2 && ed->subframes == 0x01 ){
	else if (id==2  ){
		ed->subframes |= 0x02;
		
		ed->IODE = (UINT8)  ((dwords[2] >> 16) & 0xff); // word 3
		
		// fprintf(stderr,"\nf2 %d %d\n",svID,(int)ed->IODE);
		
		signed short Crs =  dwords[2] & 0xffff; // word 3
		ed->C_rs = ((double) Crs )/((double) 32.0);
		
		signed short deltaN = (dwords[3] >> 8) & 0xffff;
		ed->delta_N = ICD_PI*(double) deltaN/(double) pow(2,43); // GPS units are semi-circles/s, RINEX units are rad/s
	
		unsigned int hibits =  (dwords[3] & 0xff) << 24; // M_0 (upper 8 bits) b17-b24
		unsigned int lobits = dwords[4] & 0xffffff; // word 5 M_0 (lower 24 bits) b1-b24
		ed->M_0 = ICD_PI * ((double) ((int) (hibits | lobits)))/ (double) pow(2,31);
	
		signed short Cuc = (dwords[5] >> 8) & 0xffff; // word 6
		ed->C_uc = (double) Cuc/(double) pow(2,29);
	
		hibits = (dwords[5] & 0xff) << 24; // word 6, e b17-b24 (upper 8 bits)
		lobits = (dwords[6] & 0xffffff);   // word 7, e b1-b23  (lower 24 bits)
		ed->e = ((double) (unsigned int)((hibits | lobits)))/ (double) pow(2,33);

		signed short Cus= (dwords[7]  >> 8) & 0xffff; // word 8, C_us b1-b16 
		ed->C_us = (double) Cus/(double) pow(2,29);
		
		hibits =  (dwords[7] & 0xff) << 24; // word 8, sqrtA b1-b8 (upper bits)
		lobits =  (dwords[8] & 0xffffff) ; //word 9, sqrtA b1-b24 (lower bits)
		ed->sqrtA = ((double) (unsigned int)((hibits | lobits)))/ (double) pow(2,19);
	
		ed->t_0e = (SINGLE) (16*((dwords[9] >> 8) & 0xffff)); // word 10
		
	}
	//else if (id==3 && ed->subframes == 0x03 ){
	else if (id==3 ){
		ed->subframes |= 0x04;
		
		signed short Cic = (dwords[2] >> 8) & 0xffff; // word 3, C_ic b1-b16 
		ed->C_ic = (double) Cic/(double) pow(2,29);
	
		unsigned int hibits =  (dwords[2] & 0xff) << 24; // word3, OMEGA_0 b17-b24 (upper bits)
		unsigned int lobits = dwords[3] & 0xffffff ; // word4, OMEGA_0 b1-b24 lower bits 
		ed->OMEGA_0 = ICD_PI * ((double) (signed int)((hibits | lobits)))/ (double) pow(2,31);
	
		signed short Cis = (dwords[4] >> 8) & 0xffff; // word 5, C_is b1-b16 
		ed->C_is= (double) Cis/(double) pow(2,29);
	
		hibits =  (dwords[4] & 0xff) << 24; // word 4
		lobits = dwords[5] & 0xffffff;  // word 6, i_0 b1-b24 (lower bits) 
		ed->i_0 = ICD_PI * ((double) (signed int)((hibits | lobits)))/ (double) pow(2,31);
	
		signed short Crc = (dwords[6] >> 8) & 0xffff; // word 7, C_rc b1-b16 
		ed->C_rc= (double) Crc/32.0;
	
		hibits =  (dwords[6] & 0xff) << 24; // word 7, OMEGA b17-b24 (upper bits)
		lobits = dwords[7] & 0xffffff ; // word 8, OMEGA b1-b24 (lower bits) 
		ed->OMEGA = ICD_PI * ((double) (signed int)((hibits | lobits)))/ (double) pow(2,31);
	
		int odot = (dwords[8] & 0xffffff) << 8; // CHECKME rounding errors ? disagreement
		odot = odot >> 8;	
		
		ed->OMEGADOT = ICD_PI * (double) (odot)/ (double) pow(2,43);
		
		// IODE b1-b8 (repeated to facilitate checking for data cutovers)
		ed->f3IODE = (UINT8)  ((dwords[9] >> 16) & 0xff);
		// fprintf(stderr,"f3 %d %d\n",svID,ed->f3IODE);
		
		int idot =  ((dwords[9] >> 2) & 0x3fff) << 18;
		idot = idot >> 18;
		ed->IDOT = ICD_PI * (double) (idot)/ (double) pow(2,43);
		
	}
	else if (id==4){ // subframe 4 contains ionosphere and UTC parameters
		// The ionosphere model changes less than once per week so no need to continually look for a new one
		// UTC data is not actually used in any of the processing but it's collected so that we can make a 
		// valid RINEX navigation file
		// Get the page ID
		
		if (gps.gotUTCdata) return; // test here, not in else if () so that we don't fall through to the catch all else
		
		unsigned int svID = (dwords[2] >> 16) & 0x3f;
		if (svID == 56){ // page 18
		
			GPS::IonosphereData &iono = gps.ionoData;
			
			// The 'a' params are all signed
			char a = (dwords[2] >> 8) & 0xff;
			iono.a0 = (double) a / (double) pow(2,30);
			
			a = (dwords[2] & 0xff);
			iono.a1 = (double) a / (double) pow(2,27);
			
			a = (dwords[3] >> 16) & 0xff;
			iono.a2 = (double) a / (double) pow(2,24);
			
			a = (dwords[3] >> 8) & 0xff;
			iono.a3 = (double) a / (double) pow(2,24);
			
			// The 'b' params are all signed
			char b = (dwords[3] & 0xff);
			iono.B0 = (double) b * (double) pow(2,11);
			
			b = (dwords[4] >> 16) & 0xff;
			iono.B1 = (double) b * (double) pow(2,14);
			
			b = (dwords[4] >> 8) & 0xff;
			iono.B2 = (double) b * (double) pow(2,16);
			
			b = (dwords[4] & 0xff);
			iono.B3 = (double) b * (double) pow(2,16);
			
			GPS::UTCData &utc = gps.UTCdata;
			int a1 = (dwords[5] & 0xffffff) << 8;
			a1 = a1 >> 8;
			utc.A1 = (double) a1 / (double) pow(2,50);
			 
			unsigned int hibits = (dwords[6] & 0xffffff) << 8;
			unsigned int lobits = (dwords[7] >> 16) & 0xff;
			utc.A0 =  (double) ((int) (hibits | lobits) ) / (double) pow(2,30);
			
			unsigned char t_ot = (dwords[7] >> 8) & 0xff;
			utc.t_ot = t_ot * 4096;
			
			utc.WN_t = dwords[7] & 0xff;
			
			char dtLs = (dwords[8] >> 16) & 0xff;
			utc.dt_LS = dtLs;
			
			utc.WN_LSF = (dwords[8] >> 8) & 0xff; // unsigned
			
			utc.DN = dwords[8] & 0xff; // word 9 unsigned, right justified ? 
			
			char dt_LSF = (dwords[9] >> 16) & 0xff; // word 10, signed
			utc.dt_LSF = dt_LSF;
			
			// Insanity check
			// GPS spec says no more than 1 microsecond offset wrt UTC. In practice it is less than 10 ns
			// Do a sloppy check (have actually seen A0 reported as 1.5 s : MJD 59100 SVN 5, not correlated with health)
			if (fabs(utc.A0) > 1.0E-6){
				//fprintf(stderr,"BAD A0 %d %g %d %02x\n",ed->SVN,utc.A0,ed->SV_health,ed->subframes);
				return;
			}
			gps.gotUTCdata = gps.gotIonoData = true;
			
			//fprintf(stderr,"%d %d\n",utc.DN, utc.dtlS);
		}
		
	}
	else if (id==5 || id == 6 || id == 7){
		// Almanac, - just ignore
	}
	else{
		return;
	}
	
	// Now test whether there is a complete ephemeris and
	// if so, if we already have it
	if (ed->subframes == 0x07){
		
		//verbosity=4;
		//DBGMSG(debugStream,INFO,"Ephemeris for SV " << svID << " IODE " << (int) ed->IODE << 
		//	" t_0e " << ed->t_0e << " t_OC " << ed->t_OC << ((ed->t_0e != ed->t_OC)?" XXX ":"") << " " << ed->week_number);
		
		// Check IODE for consistency with IODE in subframe 2 and with the lower 8 bits of IODC
		// The ICD sayeth:
		// 20.3.3.4.1 The issue of ephemeris data (IODE) term shall provide the user with a convenient means for
		// detecting any change in the ephemeris representation parameters. The IODE is provided in both
		// subframes 2 and 3 for the purpose of comparison with the 8 LSBs of the IODC term in subframe
		// 1. Whenever these three terms do not match, a data set cutover has occurred and new data must
		// be collected.
		
		// if (svID==30) fprintf(stderr,"F3 IODE %d\n",(int) ed->IODE); // REMOVE
		if ( (ed->f3IODE != ed->IODE) || ( (ed->IODC & 0x00ff)  != (UINT16) (ed->f3IODE))){
			// fprintf(stderr,"data cutover %d %d %d %d %d\n",svID, (int) ed->f3IODE , (int) ed->IODE, (int) ed->IODC, (int) (ed->IODC & 0xff ));
			ed->subframes=0x0; // retain buffer and try again
			return;
		}
		else{
			//fprintf(stderr," %d %d %d %d %d\n",svID, (int) iode , (int) ed->IODE, (int) ed->IODC, (int) (ed->IODC & 0xff ));
		}
		
		//DBGMSG(debugStream,INFO,"Ephemeris for SV " << svID << " IODE " << (int) ed->IODE << 
		//	" t_0e " << ed->t_0e << " t_OC " << ed->t_OC << ((ed->t_0e != ed->t_OC)?" XXX ":"") << " " << ed->week_number);
		
		// Fix up any week rollovers before adding the ephemeris
	
		// WN is current truncated WN whereas t_oe and T_OC may refer to previous and future weeks
		// WN in the navigation file refers to t_oe
		
		int ttrans = towTrans + wnTrans * 86400 * 7;
		int truncwnTrans = wnTrans % 1024;
		int nRollovers = wnTrans/ 1024;
		
		int wntoe = ed->week_number + 1024 * nRollovers;
		// FIXME Has WN rolled over ?
		
		// The  resultant WN must be within +/- 1 week of the full transmission week
		// This filters out the occasional ephemeris with bad WN
		if (fabs(wntoe - wnTrans) > 1){
			ed->subframes=0x0;
			return;
		}
		
		int ttoe   = ed->t_0e + wntoe * 86400 * 7;
		
		if (ttrans - ttoe > 302400.0){ // ttoe is in the next week
			ed->week_number++;
		}
		if (ttoe - ttrans > 302400.0){ // ttoe is in the previous week
			ed->week_number--;
		}
		
		if (!(gps.addEphemeris(ed))){
			// don't delete it - reuse the buffer
			ed->subframes=0x0;
			return;
		}
		else{ // data was appended to the SV ephemeris list so create a new buffer for this SV
			ed = new GPSEphemeris();
			gpsEph[svID]=ed;
			return;
		}
		//verbosity=1;
	}
	
}
