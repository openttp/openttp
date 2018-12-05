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

extern ostream *debugStream;
extern Application *app;

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


Ublox::Ublox(Antenna *ant,string m):Receiver(ant)
{
	modelName=m;
	manufacturer="ublox";
	swversion="0.1";
	constellations=GNSSSystem::GPS;
	codes=GNSSSystem::C1;
	channels=32;
	if (modelName == "LEA8MT"){
		// For the future
	}
	else if (modelName == "NEO8MT"){
	}
	else{
		app->logMessage("Unknown receiver model: " + modelName);
		app->logMessage("Assuming NEO8MT");
	}
}

Ublox::~Ublox()
{
}

bool Ublox::readLog(string fname,int mjd,int startTime,int stopTime,int rinexObsInterval)
{
	DBGMSG(debugStream,INFO,"reading " << fname);	
	
	ifstream infile (fname.c_str());
	string line;
	int linecount=0;
	
	string msgid,currpctime,pctime,msg,gpstime;
	
	I4 sawtooth;
	I4 clockBias;
	R8 measTOW;
	U2 measGPSWN;
	I1 measLeapSecs;
	
	U2 UTCyear;
	U1 UTCmon,UTCday,UTChour,UTCmin,UTCsec,UTCvalid;
	
	U1 u1buf;
	I2 i2buf;
	I4 i4buf;
	U4 u4buf;
	R4 r4buf;
	R8 r8buf;
	
	float rxTimeOffset; // single
	
	vector<SVMeasurement *> svmeas;
	
	gotUTCdata=false;
	gotIonoData=false;
	
	unsigned int currentMsgs=0;
	unsigned int reqdMsgs =  MSG0121 | MSG0122 | MSG0215 | MSG0D01 ;

  if (infile.is_open()){
    while ( getline (infile,line) ){
			linecount++;
			
			if (line.size()==0) continue; // skip empty line
			if ('#' == line.at(0)) continue; // skip comments
			if ('%' == line.at(0)) continue;
			if ('@' == line.at(0)) continue;
			
			stringstream sstr(line);
			sstr >> msgid >> currpctime >> msg;
			if (sstr.fail()){
				DBGMSG(debugStream,WARNING," bad data at line " << linecount);
				currentMsgs=0;
				deleteMeasurements(svmeas);
				continue;
			}
			
			// The 0x0215 message starts each second
			if(msgid == "0215"){ // raw measurements 
				
				if (currentMsgs == reqdMsgs){ // save the measurements from the previous second
					if (svmeas.size() > 0){
						ReceiverMeasurement *rmeas = new ReceiverMeasurement();
						measurements.push_back(rmeas);
						
						rmeas->sawtooth=sawtooth*1.0E-12; // units are ps, must be added to TIC measurement
						rmeas->timeOffset=clockBias*1.0E-9; // units are ns WARNING no sign convention defined yet ...
						
						int pchh,pcmm,pcss;
						if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss))){
							rmeas->pchh=pchh;
							rmeas->pcmm=pcmm;
							rmeas->pcss=pcss;
						}
						
						// GPSTOW is used for pseudorange estimations
						// note: this is rounded because measurements are interpolated on a 1s grid
						rmeas->gpstow=rint(measTOW);  
						rmeas->gpswn=measGPSWN % 1024; // Converted to truncated WN. Not currently used 
						
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
						time_t tgps = GPS::GPStoUnix(rmeas->gpstow,rmeas->gpswn,app->referenceTime());
						struct tm *tmGPS = gmtime(&tgps);
						rmeas->tmGPS=*tmGPS;
						
						//rmeas->tmfracs = measTOW - (int)(measTOW); 
						//if (rmeas->tmfracs > 0.5) rmeas->tmfracs -= 1.0; // place in the previous second
						
						rmeas->tmfracs=0.0;
						
						if (constellations & GNSSSystem::GPS){
							for (unsigned int sv=0;sv<svmeas.size();sv++){
								svmeas.at(sv)->dbuf3 = svmeas.at(sv)->meas; // save for debugging
								svmeas.at(sv)->meas -= clockBias*1.0E-9; // evidently it is subtracted
								// Now subtract the ms part so that ms ambiguity resolution works
								svmeas.at(sv)->meas -= 1.0E-3*floor(svmeas.at(sv)->meas/1.0E-3);
								svmeas.at(sv)->rm=rmeas;
							}
							rmeas->meas=svmeas;
							svmeas.clear(); // don't delete - we only made a shallow copy!
						}
						
						
						// KEEP THIS it's useful for debugging measurement-time related problems
					//fprintf(stderr,"PC=%02d:%02d:%02d tmUTC=%02d:%02d:%02d tmGPS=%4d %02d:%02d:%02d gpstow=%d gpswn=%d measTOW=%.12lf tmfracs=%g clockbias=%g\n",
					//	pchh,pcmm,pcss,UTChour,UTCmin,UTCsec, rmeas->tmGPS.tm_year+1900,rmeas->tmGPS.tm_hour, rmeas->tmGPS.tm_min,rmeas->tmGPS.tm_sec,
					//	(int) rmeas->gpstow,(int) rmeas->gpswn,measTOW,rmeas->tmfracs,clockBias*1.0E-9  );
					
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
						DBGMSG(debugStream,TRACE,currpctime << " meas tow=" << measTOW << setprecision(12) << " gps wn=" << (int) measGPSWN << " leap=" << (int) measLeapSecs);
						//DBGMSG(debugStream,TRACE,nmeas);
						for (unsigned int m=0;m<nmeas;m++){
							HexToBin((char *) msg.substr((36+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); //GNSS id
							int gnssSys = 0;
							switch (u1buf){
								case 0: gnssSys=GNSSSystem::GPS; break;
								case 1:case 4: case 5: break;
								case 2: gnssSys=GNSSSystem::GALILEO; break;
								case 3: gnssSys=GNSSSystem::BEIDOU; break;
								case 6: gnssSys=GNSSSystem::GLONASS; break;
								default: break;
							}
							//DBGMSG(debugStream,TRACE,gnssSys);
							if (gnssSys & constellations ){
								// Since we get all the measurements in one message (which starts each second) there's no need to check for multiple measurement messages
								// like with eg the Resolution T
								HexToBin((char *) msg.substr((16+32*m)*2,2*sizeof(R8)).c_str(),sizeof(R8),(unsigned char *) &r8buf); //pseudorange (m)
								HexToBin((char *) msg.substr((37+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); //svid
								int svID=u1buf;
								HexToBin((char *) msg.substr((46+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf);
								int prStdDev= u1buf & 0x0f;
								HexToBin((char *) msg.substr((46+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf);
								int trkStat=u1buf;
								// When PR is reported, trkStat is always 1 but .
								if (trkStat > 0 && r8buf/CLIGHT < 1.0){
									SVMeasurement *svm = new SVMeasurement(svID,gnssSys,GNSSSystem::C1,r8buf/CLIGHT,NULL);
									//svm->dbuf1=0.01*pow(2.0,prStdDev); 
									svmeas.push_back(svm);
								}
								DBGMSG(debugStream,TRACE,"SYS " <<gnssSys << " SV" << svID << " pr=" << r8buf/CLIGHT << setprecision(8) << " trkStat= " << (int) trkStat);
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
						<< (int) UTChour << ":" << (int) UTCmin << ":" << (int) UTCsec << " valid=" << (unsigned int) UTCvalid);
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
			
			//
			// Messages needed to contruct the RINEX navigation file
			//
			
			// Ionosphere parameters, UTC parameters 
			if (!gotUTCdata){
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
						
						gotUTCdata=true;
						gotIonoData=true;
					}
					else{
						DBGMSG(debugStream,WARNING,"Bad 0b02 message size");
					}
					continue;
				}
			}
		
			// Ephemeris
			if(msgid == "0b31"){
				if (msg.size()==(8+2)*2){
					DBGMSG(debugStream,WARNING,"Empty ephemeris");
				}
				else if (msg.size()==(104+2)*2){
					GPS::EphemerisData *ed = decodeGPSEphemeris(msg);
					int pchh,pcmm,pcss;
						if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss)))
							ed->tLogged = pchh*3600 + pcmm*60 + pcss; 
						else
							ed->tLogged = -1;
					gps.addEphemeris(ed);
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

	
	if (!gotUTCdata){
		app->logMessage("failed to find ionosphere/UTC parameters - no 0b02 messages");
		//return false; // FIXME temporary
	}
	
	if (measurements.size() == 0){
		app->logMessage(" no measurements available in " + fname);
		return false;
	}
	
	// Pass through the data to realign the sawtooth correction.
	// This could be done in the main loop but it's more flexible this way.
	// For the ublox, the sawtooth correction applies to the next second
	// If we're missing the sawtooth correction because of eg a gap in the data
	// then we'll just use the current sawtooth. 
	
	DBGMSG(debugStream,TRACE,"Fixing sawtooth");
	double prevSawtooth=measurements.at(0)->sawtooth;
	time_t    tPrevSawtooth=mktime(&(measurements.at(0)->tmUTC));
	int nBadSawtoothCorrections =1; // first is bad !
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
	
	// The Ublox sometime reports what appears to be an incorrect pseudorange after picking up an SV
	// If you wanted to filter these out, this is where you should do it
	
	// Fix 1 ms ambiguities/steps in the pseudorange
	// Do this initially and then every time a step is detected
	
	DBGMSG(debugStream,TRACE,"Fixing ms ambiguities");
	int nDropped;
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
		for (int svn=1;svn<=gnss->nsats();svn++){
			unsigned int lasttow=99999999,currtow=99999999;
			double lastmeas=0,currmeas;
			double corr=0.0;
			bool first=true;
			bool ok=false;
			for (unsigned int i=0;i<measurements.size();i++){
				unsigned int m=0;
				while (m < measurements[i]->meas.size()){
					if ((svn==measurements[i]->meas[m]->svn) && (measurements[i]->meas[m]->code == GNSSSystem::C1)){
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
							nDropped++;
							measurements[i]->meas.erase(measurements[i]->meas.begin()+m); // FIXME memory leak
						}
						break;
					}
					else
						m++;
					
				} // 
			} // for (unsigned int i=0;i<measurements.size();i++){
		}
	}
	
	DBGMSG(debugStream,INFO,"done: read " << linecount << " lines");
	DBGMSG(debugStream,INFO,measurements.size() << " measurements read");
	DBGMSG(debugStream,INFO,gps.ephemeris.size() << " GPS ephemeris entries read");
	DBGMSG(debugStream,INFO,nBadSawtoothCorrections << " bad sawtooth corrections");
	DBGMSG(debugStream,INFO,"dropped " << nDropped << " SV measurements (ms ambiguity failure)"); 
	
	return true;
	
}

GPS::EphemerisData* Ublox::decodeGPSEphemeris(string msg)
{
	U4 u4buf;
	HexToBin((char *) msg.substr(0*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	GPS::EphemerisData* ed= new GPS::EphemerisData();
	ed->SVN=u4buf;
	DBGMSG(debugStream,TRACE,"Ephemeris for SV" << (int) ed->SVN);
	
	// cerr << "EPH" << endl;

	// To use the MID macro, LSB is bit 0, m is first bit, n is last bit
	#define LAST(k,n) ((k) & ((1<<(n))-1))
	#define MID(k,m,n) LAST((k)>>(m),((n)-(m)+1)) 
	
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
	//fprintf(stderr,"%08x %i %.12e\n",u4buf,ed->IODE,ed->C_rs);
	
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
	ed->t_oe = toe * 16;
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
	// IODE b1-b8 (repeated to facilitate checking for data cutovers) ... but which one should I use ???
	// IDOT b9-b22 // CHECKED
	HexToBin((char *) msg.substr(100*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
	int idot = (MID(u4buf,2,15)) << 18;
	idot = idot >> 18;
	ed->IDOT = ICD_PI * (double) (idot)/ (double) pow(2,43);
	//fprintf(stderr,"%08x %.12e\n",u4buf,ed->IDOT);
	
	return ed;
}
