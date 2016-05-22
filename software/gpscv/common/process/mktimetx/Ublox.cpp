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

// types used by ublox receivers
typedef unsigned char  U1;
typedef signed   short I2;
typedef int            I4;
typedef unsigned int   U4;
typedef float          R4;
typedef double         R8;

// messages that are parsed

#define MSG0122 0x01
#define MSG0215 0x02
#define MSG0D01 0x04

Ublox::Ublox(Antenna *ant,string m):Receiver(ant)
{
	modelName=m;
	manufacturer="ublox";
	swversion="0.1";
	constellations=GNSSSystem::GPS;
	codes=GNSSSystem::C1;
	channels=32;
	if (modelName == "LEA8"){
		// For the future
	}
	else{
		app->logMessage("Unknown receiver model: " + modelName);
		app->logMessage("Assuming LEA8");
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
	unsigned int reqdMsgs = MSG0122 | MSG0215 | MSG0D01 ;

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
						
						rmeas->sawtooth=sawtooth*1.0E-12; // units are ps
						//rmeas->timeOffset=rxTimeOffset;
						
						int pchh,pcmm,pcss;
						if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss))){
							rmeas->pchh=pchh;
							rmeas->pcmm=pcmm;
							rmeas->pcss=pcss;
						}
						
						gpsmeas.clear(); // don't delete - we only made a shallow copy!
						
						//fprintf(stderr,"PC=%02d:%02d:%02d rx =%02d:%02d:%02d tmeasUTC=%8.6f gpstow=%d gpswn=%d\n",
						//	pchh,pcmm,pcss,msg46hh,msg46mm,msg46ss,tmeasUTC/1000.0,(int) gpstow,(int) weekNum);
					}
				} // if (gpsmeas.size() > 0)
				
				pctime=currpctime;
				currentMsgs = 0;
				
				if (msg.size()-2*2-16*2 > 0){ // don't know the expected message size yet but if we've got the header ...
					HexToBin((char *) msg.substr(11*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf);
					unsigned int nmeas=u1buf;
					if (msg.size() == (2+16+nmeas*32)*2){
						HexToBin((char *) msg.substr(0*2,2*sizeof(R8)).c_str(),sizeof(R8),(unsigned char *) &r8buf); //measurement TOW
						DBGMSG(debugStream,TRACE,"Measurement tow " << r8buf);
						for (unsigned int m=0;m<nmeas;m++){
							HexToBin((char *) msg.substr((36+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); //GNSS id
							if (u1buf == 0){
								// Since we get all the measurements in one message (which starts each second) there's no need to check for mutiple measurement messages
								// like with eg the Resolution T
								HexToBin((char *) msg.substr((16+32*m)*2,2*sizeof(R8)).c_str(),sizeof(R8),(unsigned char *) &r8buf); //pseudorange (m)
								HexToBin((char *) msg.substr((37+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); //svid
								int svID=u1buf;
								HexToBin((char *) msg.substr((46+32*m)*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &u1buf); 
								gpsmeas.push_back(new SVMeasurement(svID,r8buf/CLIGHT,NULL));// ReceiverMeasurement not known yet
								DBGMSG(debugStream,TRACE,"SV" << svID << " pr=" << r8buf/CLIGHT << " trkStat= " << (int) u1buf);
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
					HexToBin((char *) msg.substr(8*2,2*sizeof(I4)).c_str(),sizeof(I4),(unsigned char *) &sawtooth);
					DBGMSG(debugStream,TRACE,"sawtooth=" << sawtooth << " ps");
					currentMsgs |= MSG0D01;
				}
				else{
					DBGMSG(debugStream,WARNING,"Bad 0d01 message size");
				}
			}
			// 0x0135 UBX-NAV-SAT satellite information
			
			// 0x0121 UBX-NAV-TIME-UTC UTC time solution
			
			// 0x0122 UBX-NAV-CLOCK clock solution  (clock bias)
			if(msgid == "0122"){
				if (msg.size()==(20+2)*2){
						HexToBin((char *) msg.substr(0*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf); // GPS tow of navigation epoch (ms)
						HexToBin((char *) msg.substr(4*2,2*sizeof(I4)).c_str(),sizeof(I4),(unsigned char *) &clockBias); // in ns
						DBGMSG(debugStream,TRACE,"GPS tow=" << u4buf << "ms" << " clock bias=" << clockBias << " ns");
						currentMsgs |= MSG0122;
				}
				else{
					DBGMSG(debugStream,WARNING,"Bad 0b02 message size");
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
		return false;
	}
	
	
	// The Ublox sometime reports what appears to be an incorrect pseudorange after picking up an SV
	// If you wanted to filter these out, this is where you should do it
	
	//interpolateMeasurements(measurements);
	// Note that after this, tmfracs is now zero and all measurements have been interpolated to a 1 s grid
	
	DBGMSG(debugStream,INFO,"done: read " << linecount << " lines");
	DBGMSG(debugStream,INFO,measurements.size() << " measurements read");
	DBGMSG(debugStream,INFO,gps.ephemeris.size() << " GPS ephemeris entries read");
	
	exit(0);
	
	return true;
	
}
