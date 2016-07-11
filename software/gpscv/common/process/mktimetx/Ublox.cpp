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

#define MAX_CHANNELS 16 // max channels per constellation
#define SLOPPINESS 0.99
#define CLOCKSTEP  0.001

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
		app->logMessage("Assuming LEA8MT");
	}
}

Ublox::~Ublox()
{
}

bool Ublox::readLog(string fname,int mjd)
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
	I4 i4buf;
	U4 u4buf;
	R4 r4buf;
	R8 r8buf;
	
	float rxTimeOffset; // single
	
	vector<SVMeasurement *> gpsmeas;
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
				deleteMeasurements(gpsmeas);
				continue;
			}
			
			// The 0x0215 message starts each second
			if(msgid == "0215"){ // raw measurements 
				
				if (currentMsgs == reqdMsgs){ // save the measurements from the previous second
					if (gpsmeas.size() > 0){
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
						rmeas->gpswn=measGPSWN % 1024; // Converted to truncaed WN. Not currently used 
						
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
						time_t tgps = GPS::GPStoUnix(rmeas->gpstow,rmeas->gpswn);
						struct tm *tmGPS = gmtime(&tgps);
						rmeas->tmGPS=*tmGPS;
						
						rmeas->tmfracs = measTOW - (int)(measTOW); 
						if (rmeas->tmfracs > 0.5) rmeas->tmfracs -= 1.0; // place in the previous second
						
						for (unsigned int sv=0;sv<gpsmeas.size();sv++){
							gpsmeas.at(sv)->dbuf3 = gpsmeas.at(sv)->meas; // save for debugging
							gpsmeas.at(sv)->meas -= clockBias*1.0E-9; // evidently it is subtracted
							// Now subtract the ms part so that ms ambiguity resolution works
							gpsmeas.at(sv)->meas -= 1.0E-3*floor(gpsmeas.at(sv)->meas/1.0E-3);
							gpsmeas.at(sv)->rm=rmeas;
						}
						rmeas->gps=gpsmeas;
						gpsmeas.clear(); // don't delete - we only made a shallow copy!
						
						// KEEP THIS it's useful for debugging measurement-time related problems
					fprintf(stderr,"PC=%02d:%02d:%02d tmUTC=%02d:%02d:%02d tmGPS=%02d:%02d:%02d gpstow=%d gpswn=%d measTOW=%.12lf tmfracs=%g clockbias=%g\n",
						pchh,pcmm,pcss,UTChour,UTCmin,UTCsec, rmeas->tmGPS.tm_hour, rmeas->tmGPS.tm_min,rmeas->tmGPS.tm_sec,
						(int) rmeas->gpstow,(int) rmeas->gpswn,measTOW,rmeas->tmfracs,clockBias*1.0E-9  );
						
					}// if (gpsmeas.size() > 0)
				} 
				else{
					DBGMSG(debugStream,1,pctime << " reqd message missing, flags = " << currentMsgs);
					deleteMeasurements(gpsmeas);
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
						for (unsigned int m=0;m<nmeas;m++){
							HexToBin((char *) msg.substr((36+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); //GNSS id
							if (u1buf == 0){
								// Since we get all the measurements in one message (which starts each second) there's no need to check for mutiple measurement messages
								// like with eg the Resolution T
								HexToBin((char *) msg.substr((16+32*m)*2,2*sizeof(R8)).c_str(),sizeof(R8),(unsigned char *) &r8buf); //pseudorange (m)
								HexToBin((char *) msg.substr((37+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); //svid
								int svID=u1buf;
								HexToBin((char *) msg.substr((46+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); 
								gpsmeas.push_back(new SVMeasurement(svID,r8buf/CLIGHT+0.016,NULL));// ReceiverMeasurement not known yet
								DBGMSG(debugStream,TRACE,"SV" << svID << " pr=" << r8buf/CLIGHT << setprecision(8) << " trkStat= " << (int) u1buf);
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
						gotUTCdata=true;
						gotIonoData=true;
					}
					else{
						DBGMSG(debugStream,WARNING,"Bad 0b02 message size");
					}
				}
				continue;
			}
			
			// Ephemeris
			if(msgid == "0b31"){
				if (msg.size()==(8+2)*2){
					DBGMSG(debugStream,WARNING,"Empty ephemeris");
				}
				else if (msg.size()==(104+2)*2){
					HexToBin((char *) msg.substr(0*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
					DBGMSG(debugStream,TRACE,"Ephemeris for SV" << u4buf);
				}
				else{
					DBGMSG(debugStream,WARNING,"Bad 0b31 message size");
				}
				continue;
			}
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
	
	
	// Pass through the data to realign the sawtooth correction.
	// This could be done in the main loop but it's more flexible this way.
	// For the ublox, the sawtooth correction applies to the next second
	// If we're missing the sawtooth correction because of eg a gap in the data
	// then we'll just use the current sawtooth. 
	
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
	
	for (int prn=1;prn<=32;prn++){
		unsigned int lasttow=99999999,currtow=99999999;
		double lastmeas,currmeas;
		double corr=0.0;
		bool first=true;
		bool ok=false;
		for (unsigned int i=0;i<measurements.size();i++){
			for (unsigned int m=0;m < measurements[i]->gps.size();m++){
				if (prn==measurements[i]->gps[m]->svn){
					lasttow=currtow;
					lastmeas=currmeas;
					currmeas=measurements[i]->gps[m]->meas;
					currtow=measurements[i]->gpstow;
					
					DBGMSG(debugStream,TRACE,prn << " " << currtow << " " << currmeas << " ");
					if (first){
						first=false;
						ok = resolveMsAmbiguity(measurements[i],measurements[i]->gps[m],&corr);
					}
					else if (currtow > lasttow){ // FIXME better test of gaps
						if (fabs(currmeas-lastmeas) > CLOCKSTEP*SLOPPINESS){
							DBGMSG(debugStream,TRACE,"first/step " << prn << " " << lasttow << "," << lastmeas << "->" << currtow << "," << currmeas);
							ok = resolveMsAmbiguity(measurements[i],measurements[i]->gps[m],&corr);
						}
					}
					if (ok) measurements[i]->gps[m]->meas += corr;
					break;
				}
				
			}
		}
	}
	
	interpolateMeasurements(measurements);
	// Note that after this, tmfracs is now zero and all measurements have been interpolated to a 1 s grid
	
	DBGMSG(debugStream,INFO,"done: read " << linecount << " lines");
	DBGMSG(debugStream,INFO,measurements.size() << " measurements read");
	DBGMSG(debugStream,INFO,gps.ephemeris.size() << " GPS ephemeris entries read");
	DBGMSG(debugStream,INFO,nBadSawtoothCorrections << " bad sawtooth corrections");
	
	return true;
	
}
