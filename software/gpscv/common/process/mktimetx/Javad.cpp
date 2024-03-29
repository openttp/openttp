//
//
// The MIT License (MIT)
//
// Copyright (c) 2015  Michael J. Wouters
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
//
// Credits: the nitty gritty of this is based on code by Peter Fisk and Bruce Warrington
//

#include <stdio.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cmath>

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "Antenna.h"
#include "Application.h"
#include "Debug.h"
#include "GPS.h"
#include "HexBin.h"
#include "Javad.h"
#include "ReceiverMeasurement.h"
#include "SVMeasurement.h"
#include "Timer.h"

extern std::ostream *debugStream;
extern Application *app;

// JAVAD types
#define I1 char
#define I2 short
#define I4 int
#define U1 unsigned char
#define U2 unsigned short
#define U4 unsigned int
#define F4 float
#define F8 double

#define MAX_CHANNELS 32

// Receiver message flags
#define AZ_MSG 0x01
#define EL_MSG 0x02
#define F1_MSG 0x04
#define F2_MSG 0x08
#define FC_MSG 0x10
#define R1_r1_1R_1r_MSG 0x20
#define R2_r2_2R_2r_MSG 0x40
#define RC_rc_MSG 0x80
#define RD_MSG 0x100
#define RT_MSG 0x200
#define SI_MSG 0x400
#define SS_MSG 0x800
#define TO_MSG 0x1000
#define YA_MSG 0x2000
#define ZA_MSG 0x4000

Javad::Javad(Antenna *ant,std::string m):Receiver(ant)
{
  modelName=m;
	manufacturer="Javad";
	swversion="0.1";
	constellations=GNSSSystem::GPS;
	dualFrequency=false;
	gps.codes = GNSSSystem::C1C; 
	codes=gps.codes;
	channels=32;
	if (modelName=="HE_GD"){
		dualFrequency=true;
		gps.codes = GNSSSystem::C1C | GNSSSystem::C1P | GNSSSystem::C2P;
		codes = gps.codes;
	}
	else{
		app->logMessage("Unknown receiver model: " + modelName);
		app->logMessage("Assuming generic single frequency receiver");
		modelName="generic";
	}
}

Javad::~Javad()
{
}

void Javad::addConstellation(int constellation)
{
	constellations |= constellation;
	switch (constellation)
	{
		case GNSSSystem::GPS:
			if (modelName=="HE_GD"){	
				gps.codes = GNSSSystem::C1C | GNSSSystem::C1P | GNSSSystem::C2P | GNSSSystem::L1P | GNSSSystem::L2P;
				codes |= gps.codes;
				break;
			}
			else{
				gps.codes = GNSSSystem::C1C;
				codes |= gps.codes;
			}
			break;
	}
}

bool Javad::readLog(std::string fname,int mjd,int startTime,int stopTime,int rinexObsInterval)
{
	Timer timer;
	
	timer.start();
	
	DBGMSG(debugStream,INFO,"reading " << fname);	
	
	
	std::ifstream infile (fname.c_str());
	std::string line;
	int linecount=0;
	
	std::string msgid,currpctime,pctime,msg,gpstime;
	
	U4 gpsTOD;
	F8 rxTimeOffset;
	F4 sawtooth;  
	
	std::vector<std::string> rxid;
	
	std::vector<SVMeasurement *> gpsmeas;
	gps.gotIonoData = false;
	gps.gotUTCdata=false;
	
	U1 uint8buf;
	I1 sint8buf;
	I2 sint16buf;
	I4 sint32buf;
	U4 u4buf;
	
	unsigned int currMsgs=0,reqdMsgs;
	unsigned int nSats;
	unsigned char trackedSVs[MAX_CHANNELS];
	unsigned char navStatus[MAX_CHANNELS];
	unsigned char elevs[MAX_CHANNELS];
	unsigned char azimuths[MAX_CHANNELS];
	
	F8 CApr[MAX_CHANNELS];
	unsigned char CAlockFlags[MAX_CHANNELS*2];
	
	F8 P1pr[MAX_CHANNELS];
	//F8 relP1pr[MAX_CHANNELS];
	unsigned char P1lockFlags[MAX_CHANNELS*2];
	
	F8 P2pr[MAX_CHANNELS];
	//F8 relP2pr[MAX_CHANNELS];
	unsigned char P2lockFlags[MAX_CHANNELS*2];
	
	F8 cpP1[MAX_CHANNELS];
	
	F8 cpP2[MAX_CHANNELS];
	
	I2 i2bufarray[MAX_CHANNELS];
	I4 i4bufarray[MAX_CHANNELS];
	F4 f4bufarray[MAX_CHANNELS];
	F8 f8bufarray[MAX_CHANNELS];
	
	F8 smoothingOffset;
	int rcCnt=0,RCcnt=0; 
	int R1cnt=0,r1Cnt=0,m1RCnt=0,m1rCnt=0;
	int R2cnt=0,r2Cnt=0,m2RCnt=0,m2rCnt=0;
	unsigned int errorCount=0,badC1Measurements=0,badP1Measurements=0,badP2Measurements=0,numSVmeasurements=0;
	U2 RDyyyy;
	U1 RDmm,RDdd;
	
	// In positioning mode, RxTimeOffset is not applied to pseudorange measurements
	// because that creates clock discontinuities with the carrier phase measurements
	
	double useTimeOffset = 1.0;
	if (app->positioningMode)
		useTimeOffset=0.0;
	
	reqdMsgs = AZ_MSG | EL_MSG | FC_MSG | RC_rc_MSG| RT_MSG | SI_MSG | SS_MSG | TO_MSG | YA_MSG| ZA_MSG;
	if (dualFrequency)
		reqdMsgs |= R1_r1_1R_1r_MSG | R2_r2_2R_2r_MSG | F1_MSG | F2_MSG ; // don't require P1 and P2
	

  if (infile.is_open()){
    while ( getline (infile,line) ){
			linecount++;
			
			if (line.size()==0) continue; // skip empty line
			if ('#' == line.at(0)) continue; // skip comments
			if ('%' == line.at(0)) continue;
		
			if ('@' == line.at(0)){ 
				size_t pos;
				if (std::string::npos != (pos = line.find("RXID",2 )) ){
					rxid.push_back(line.substr(pos+4,line.size()-pos-4));
				}
				continue;
			}
			std::stringstream sstr(line);
			
			// Basic check on the format 
			if ( (line.size() < 16) || // too short
				(line.at(2) != ' ') || // missing delimiter
				(line.at(5) != ':') || // missing delimiter
				(line.at(8) != ':') ||
				(line.at(11) != ' ')){
				errorCount++;
				continue;
			}
			sstr >> msgid >> currpctime >> msg;
			if (sstr.fail()){
				DBGMSG(debugStream,WARNING," bad data at line " << linecount);
				errorCount++;
				continue;
			}
			
			int hh,mm,ss;
			if ((3==sscanf(currpctime.c_str(),"%d:%d:%d",&hh,&mm,&ss))){
				int ts = hh*3600+mm*60+ss;
				if (ts < startTime || ts > stopTime)
					continue;
			}
							
			// FIXME could improve parser by converting the msgid to a hex value?
			// Some messages we just don't want
			if (msgid == "NP"){
				continue;
			}
			
			// FIXME Valid checksum?
			// If we compute a valid checksum, then checking the message size is a bit paranoid
			//HexToBin(hexdata,count/2,rawdata);
			//sprintf(temp,"%s%03X",command,count/2);
			//for (i=0;i<count/2;i++) temp[i+5] = rawdata[i];
			//if (cs(temp,count/2 + 5 - 1 ) != rawdata[count/2 - 1]) continue;
		
			//
			// Messages that occur once per second
			//
			
			// 
			// The Receiver Date message starts each second
			//
			
			if(msgid=="RD"){ // Receiver Date (RD) message 
				
				if ((currMsgs == reqdMsgs) && (rcCnt <= 1) && (RCcnt <= 1)){ // save measurements for the current second
					
					if ((dualFrequency &&
						!((R1cnt<=1) && (r1Cnt<=1) && (m1RCnt<=1) && (m1rCnt<=1) &&
						(R2cnt<=1) && (r2Cnt<=1) && (m2RCnt<=1) && (m2rCnt<=1))))
					{
						DBGMSG(debugStream,INFO,"Too many P1/2 messages");
						pctime=currpctime;
						currMsgs=0;
						rcCnt=RCcnt=0;
						R1cnt=r1Cnt=m1RCnt=m1rCnt=0;
						R2cnt=r2Cnt=m2RCnt=m2rCnt=0;
						continue;
					}
					
					ReceiverMeasurement *rmeas = new ReceiverMeasurement();
					numSVmeasurements += nSats;
					
					// For each tracked satellite, get the pseudorange for each code
					for (unsigned int chan=0; chan < nSats; chan++){
						
						// FIXME ignore GLONASS for the present
						// GPS USID 1..37, GLONASS 38..69,70 
						if (trackedSVs[chan] > 37){
							continue;
						}
						
						if (codes & GNSSSystem::C1C){
							bool ok=true;
							// Check that PLLs are locked for each channel before we use the data
							if (CAlockFlags[chan*2] != 83){
								DBGMSG(debugStream,WARNING," C/A unlocked at line " << linecount << "(prn=" << (int) trackedSVs[chan] << ")");
								badC1Measurements++;
								ok=false;
							}
							
							// Simple sanity check in case pseudoranges or the receiver time offset are wild
							if (((CApr[chan]-rxTimeOffset)<0.05) || ((CApr[chan]-rxTimeOffset)>0.10)){
								DBGMSG(debugStream,WARNING," C/A pseudorange too large at line " << linecount << "(" << CApr[chan] - 
									rxTimeOffset << ")");
								if (ok) badC1Measurements++; // don't count it twice
								ok=false;
							}
							
							if (ok){
								SVMeasurement *svm = new SVMeasurement(trackedSVs[chan],GNSSSystem::GPS,GNSSSystem::C1C,CApr[chan] - 
									useTimeOffset*rxTimeOffset,rmeas); // pseudorange is corrected for rx offset 
								svm->dbuf3 = CApr[chan];
								rmeas->meas.push_back(svm);
							}
							
						} // if codes & C1C
						
						if (codes & GNSSSystem::C1P){
							bool ok=true;
							if (P1lockFlags[chan*2] != 83){
								DBGMSG(debugStream,WARNING," P1 unlocked at line " << linecount << "(prn=" << (int) trackedSVs[chan] << ")");
								badP1Measurements++;
								ok=false;
							}
							
							// FIXME better sanity check
							ok = ok && !std::isnan(P1pr[chan]);
							
							if (ok){
								SVMeasurement *svm = new SVMeasurement(trackedSVs[chan],GNSSSystem::GPS,GNSSSystem::C1P,P1pr[chan] -
									useTimeOffset*rxTimeOffset,rmeas); // pseudorange is corrected for rx offset 
								rmeas->meas.push_back(svm);
							}
							
						} // if codes & C1P	
							
						if (codes & GNSSSystem::C2P){
							bool ok = true;
							if (P2lockFlags[chan*2] != 83){
								DBGMSG(debugStream,WARNING," P2 unlocked at line " << linecount << "(prn=" << (int) trackedSVs[chan] << ")");
								badP2Measurements++;
								ok=false;
							}	
							
							// FIXME better sanity check
							ok = ok && !std::isnan(P2pr[chan]);
							
							if (ok){
								SVMeasurement *svm = new SVMeasurement(trackedSVs[chan],GNSSSystem::GPS,GNSSSystem::C2P,P2pr[chan] -
									useTimeOffset*rxTimeOffset,rmeas); // pseudorange is corrected for rx offset 
								rmeas->meas.push_back(svm);
							}
							

						} // if codes & C2P
						
						// Carrier phase measurements
						
						if (app->allObservations){ // don't parse if we don't need 'em
							if (codes & GNSSSystem::L1P){
								bool ok=true;
								if (P1lockFlags[chan*2] != 83)
									ok=false;
								
								// FIXME better sanity check
								ok = ok && !std::isnan(cpP1[chan]);
								
								if (ok){
									SVMeasurement *svm = new SVMeasurement(trackedSVs[chan],GNSSSystem::GPS,GNSSSystem::L1P,
										cpP1[chan] - useTimeOffset*rxTimeOffset*GPS::fL1,rmeas); 
									rmeas->meas.push_back(svm);
								}
								
							} // if codes & L1P	
								
							if (codes & GNSSSystem::L2P){
								bool ok = true;
								if (P2lockFlags[chan*2] != 83)
									ok=false;
								
								// FIXME better sanity check
								ok = ok && !std::isnan(cpP2[chan]);
								
								if (ok){
									SVMeasurement *svm = new SVMeasurement(trackedSVs[chan],GNSSSystem::GPS,GNSSSystem::L2P,
										cpP2[chan] - useTimeOffset*rxTimeOffset*GPS::fL2,rmeas); 
									rmeas->meas.push_back(svm);
								}
								
							} // if codes & L2P
						} // if app->allObservations
					}
					
					if (rmeas->meas.size() > 0){ // FIXME check other codes
						int pchh,pcmm,pcss;
						if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss))){
							rmeas->pchh=pchh;
							rmeas->pcmm=pcmm;
							rmeas->pcss=pcss;
							
							// One more check
							if ((gpsTOD/1000.0+rxTimeOffset) > -0.1 && (gpsTOD/1000.0+rxTimeOffset < 86400.1)){
								int igpsTOD = gpsTOD/1000;
								int hh = (int) (igpsTOD/3600);
								int mm = (int) (igpsTOD - hh*3600)/60;
							
								rmeas->tmGPS.tm_sec= igpsTOD - hh*3600 - mm*60;
								rmeas->tmGPS.tm_min= mm;
								rmeas->tmGPS.tm_hour=hh ;
								rmeas->tmGPS.tm_mday=RDdd;
								rmeas->tmGPS.tm_mon=RDmm-1;
								rmeas->tmGPS.tm_year=RDyyyy-1900;
								rmeas->tmGPS.tm_isdst=0;
								mktime(&(rmeas->tmGPS)); // this sets wday (note: TZ=UTC enforced in Main.cpp) so DST stays correct
								rmeas->gpstow = 86400*rmeas->tmGPS.tm_wday+igpsTOD;
								rmeas->tmfracs = 0.0; // measurements are apparently on the second ...
								
								// YA and TO messages can roll over at different times - handle this case
								// Typically, the difference between the smoothed and receiver time offsets is <= 1 ns
								if ((smoothingOffset - rxTimeOffset)> 5e-4) smoothingOffset-=1e-3;
								if ((smoothingOffset - rxTimeOffset)<-5e-4) smoothingOffset+=1e-3;
				
								rmeas->sawtooth=sawtooth-(smoothingOffset-rxTimeOffset); // added to the counter measurement
																																				 // the term (smoothingOffset-rxTimeOffset) is typically zero
								rmeas->timeOffset = rxTimeOffset; // just used for diagnostics
								
								// All OK
								measurements.push_back(rmeas);
								DBGMSG(debugStream,TRACE,rmeas->meas.size() << " measurements at "  << (int) gpsTOD << " "
									<< hh << ":" << mm << ":" << (int) rmeas->tmGPS.tm_sec << " (GPS), " 
									<< pchh << ":" << pcmm << ":" << pcss << " (PC)");
							}
							else{
								errorCount++;
								DBGMSG(debugStream,WARNING,"GPS TOD out of range at " << pchh << ":" << pcmm << ":" << pcss << " (PC)");
							}
						}
						else{
							errorCount++;
							DBGMSG(debugStream,WARNING,"Unreadable PC time " << currpctime);	
						}
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING,"No useable GPS measurements at " << currpctime);
						delete rmeas;
					}
				} // end of save measurements for the current second
				
				if (msg.size() == 6*2){
					HexToBin((char *) msg.c_str(),sizeof(U2),(unsigned char *) &RDyyyy);
					HexToBin((char *) msg.substr(2*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &RDmm);
					HexToBin((char *) msg.substr(3*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &RDdd);
					HexToBin((char *) msg.substr(4*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &uint8buf);
					DBGMSG(debugStream,TRACE," RD " << (int) RDyyyy << ":" << (int) RDmm << ":" << (int) RDdd << " rx ref time=" << (int) uint8buf);
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," RD msg wrong size at line " << linecount);
				}
				
				pctime=currpctime;
				currMsgs=0;
				rcCnt=RCcnt=0;
				R1cnt=r1Cnt=m1RCnt=m1rCnt=0;
				R2cnt=r2Cnt=m2RCnt=m2rCnt=0;
				continue;
			}
			
			if (msgid == "~~"){
				if (msg.size() == 5*2 ){
					HexToBin((char *) msg.substr(0,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &gpsTOD);
					currMsgs |= RT_MSG;
					DBGMSG(debugStream,TRACE," RT " << (int) gpsTOD);
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," bad data at line " << linecount);
				}
				continue;
			}
			
			if(msgid=="SI"){ // Satellite Indices (SI) message
				if (currMsgs & SI_MSG){
					currMsgs=0; // unexpected SI message
					rcCnt=RCcnt=0;
					R1cnt=r1Cnt=m1RCnt=m1rCnt=0;
					R2cnt=r2Cnt=m2RCnt=m2rCnt=0;
					continue;
				}
				// Can't check the message size!
				currMsgs |= SI_MSG;
				nSats=(msg.size() - 2) / 2;
				HexToBin((char *) msg.c_str(),nSats,trackedSVs);
				continue;
			}

			if(msgid=="TO"){ // Reference Time to Receiver Time Offset (TO) message 
				if (msg.size() == 9*2){
					HexToBin((char *) msg.c_str(),sizeof(F8),(unsigned char *) &rxTimeOffset);
					// Discard outliers
					if ((fabs(rxTimeOffset)>0.001) || (fabs(rxTimeOffset)<1E-10)){ 
						badC1Measurements++;
						DBGMSG(debugStream,WARNING," TO outlier at line " << linecount << ":" << rxTimeOffset);
					}
					else{
						currMsgs |= TO_MSG;
						DBGMSG(debugStream,TRACE," TO " << rxTimeOffset *1.0E9 << " ns");
					}
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," TO msg wrong size at line " << linecount);
				}
				continue;
			}
			
			if(msgid=="YA"){ // smoothing offset (YA) message - assuming we are using pps A
				if (msg.size() == 10*2){
					HexToBin((char *) msg.c_str(),sizeof(F8),(unsigned char *) &smoothingOffset);
					// Discard outliers. YA is occasionally reported as zero following a tracking glitch.
					if ((fabs(smoothingOffset)>0.001) || (smoothingOffset==0)){
						badC1Measurements++;
						DBGMSG(debugStream,WARNING," YA outlier at line " << linecount << ":" << smoothingOffset);
					}
					else{
						currMsgs |= YA_MSG;
						DBGMSG(debugStream,TRACE," YA " << smoothingOffset *1.0E9 << " ns");
					}
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," YA msg wrong size at line " << linecount << ":" << msg.size());
				}
				continue;
			}
			
			if(msgid=="ZA"){ // PPS offset (ZA) message - assuming we are using pps A
				if (msg.size() == 5*2){
					HexToBin((char *) msg.c_str(),sizeof(F4),(unsigned char *) &sawtooth); // units are ns
					// Discard outliers
					if (fabs(sawtooth)> 50.0){
						badC1Measurements++;
						DBGMSG(debugStream,WARNING," ZA outlier at line " << linecount << ":" << sawtooth);
					}
					else{
						DBGMSG(debugStream,TRACE," ZA " << sawtooth << " ns");
						sawtooth *= 1.0E-9;
						currMsgs |= ZA_MSG;
					}
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," ZA msg wrong size at line " << linecount);
				}
				continue;
			}

			// Need the SI message to parse the following messages
			if (!(currMsgs & SI_MSG)) continue;
			
			if(msgid == "SS"){ //  Navigation Status (SS) message 
				unsigned int ssnSats = (msg.size() - 4) / 2;
				if (ssnSats == nSats){
					HexToBin((char *) msg.c_str(),nSats,navStatus);
					currMsgs |= SS_MSG;
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," SS msg wrong size at line " << linecount << ":" << ssnSats << " " << nSats);	
				}
				continue;
			}

			if(msgid == "EL"){ //  Satellite Elevations (EL) message 
				unsigned int elnSats = (msg.size() - 2) / 2;
				if (elnSats == nSats){
					HexToBin((char *) msg.c_str(),nSats,elevs);
					currMsgs |= EL_MSG;
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," EL msg wrong size at line " << linecount);	
				}
				continue;
			}
			
			if(msgid == "AZ"){ //  Satellite Azimuths (AZ) message 
				unsigned int aznSats = (msg.size() - 2) / 2;
				if (aznSats == nSats){
					HexToBin((char *) msg.c_str(),nSats,azimuths);
					currMsgs |= AZ_MSG;
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," AZ msg wrong size at line " << linecount);	
				}
				continue;
			}
		
			// L1C measurements
			if(msgid == "rc"){ // Delta C/A Pseudoranges (rc) message
				if (RCcnt) continue; // full pseudoranges take precedence
				unsigned int rcnSats = (msg.size() - 2) / 8;
				if (rcnSats == nSats){
					HexToBin((char *) msg.c_str(),nSats*sizeof(I4),(unsigned char *) (i4bufarray));
					for (unsigned int i=0;i<nSats;i++) 
						CApr[i] = (double)(i4bufarray[i])*1e-11 + 0.075;
					currMsgs |= RC_rc_MSG;
					rcCnt++;
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," rc msg wrong size at line " << linecount);	
				}
				continue;
			}

			if(msgid == "RC"){ // Full C/A Pseudoranges (RC) message
				unsigned int RCnSats = (msg.size() - 2) / 16;
				if (RCnSats == nSats){
					HexToBin((char *) msg.c_str(),nSats*sizeof(F8),(unsigned char *) (f8bufarray));
					for (unsigned int i=0;i<nSats;i++) 
						CApr[i] = (double) f8bufarray[i];
					currMsgs |= RC_rc_MSG;
					RCcnt++;
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," RC msg wrong size at line " << linecount);	
				}
				continue;
			}
			
			if(msgid == "FC"){ // F/A Signal Lock Flags (FC) message
				unsigned int FCnSats = (msg.size() - 2) / 4;
				if (FCnSats == nSats){
					HexToBin((char *) msg.c_str(),nSats*sizeof(U2),(unsigned char *) (CAlockFlags));
					currMsgs |= FC_MSG;
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING," FC msg wrong size at line " << linecount);	
				}
				continue;
			}
		
			// P1 & P2 messages 
			if (dualFrequency){

				// Four pseudorange messages for each signal
				if(msgid == "R1"){ // Full P1 pseudorange (R1) message
					unsigned int msgSats = (msg.size() - 2) / (2*sizeof(F8));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(F8),(unsigned char *) (f8bufarray));
						for (unsigned int i=0;i<nSats;i++) 
							P1pr[i] = (double) f8bufarray[i];
						currMsgs |= R1_r1_1R_1r_MSG;
						R1cnt++;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," R1 msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "r1"){ // Short P1 pseudoranges (r1) message
					if (R1cnt) continue; // full pseudoranges take precedence
					unsigned int msgSats = (msg.size() - 2) / (2*sizeof(I4));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(I4),(unsigned char *) (i4bufarray));
						for (unsigned int i=0;i<nSats;i++) 
							P1pr[i] = (double)(i4bufarray[i])*1e-11 + 0.075;
						currMsgs |= R1_r1_1R_1r_MSG;
						r1Cnt++;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," r1 msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "1R"){ // Relative P1 pseudoranges (1R) message
					unsigned int msgSats = (msg.size() - 2) / (2*sizeof(F4));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(F4),(unsigned char *) (f4bufarray));
						//for (unsigned int i=0;i<nSats;i++) 
						//	relP1pr[i] = (double) f4bufarray[i];
						currMsgs |= R1_r1_1R_1r_MSG;
						m1RCnt++;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," 1R msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "1r"){ // Short relative P1 pseudoranges (1r) message
					if (m1RCnt) continue; // full relative pseudoranges take precedence
					unsigned int msgSats = (msg.size() - 2) / (2*sizeof(I2));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(I2),(unsigned char *) (i2bufarray));
						//for (unsigned int i=0;i<nSats;i++) 
						//	relP1pr[i] = (double)(i2bufarray[i])*1e-11 + 2.0e-7;
						currMsgs |= R1_r1_1R_1r_MSG;
						m1rCnt++;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," 1r msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "R2"){ // Full P2 pseudorange (R2) message
					unsigned int msgSats = (msg.size() - 2) / (2*sizeof(F8));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(F8),(unsigned char *) (f8bufarray));
						for (unsigned int i=0;i<nSats;i++) 
							P2pr[i] = (double) f8bufarray[i];
						currMsgs |= R2_r2_2R_2r_MSG;
						R2cnt++;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," R2 msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "r2"){ // Short P2 pseudoranges (r2) message
					if (R2cnt) continue; // full pseudoranges take precedence
					unsigned int msgSats = (msg.size() - 2) / (2*sizeof(I4));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(I4),(unsigned char *) (i4bufarray));
						for (unsigned int i=0;i<nSats;i++) 
							P2pr[i] = (double)(i4bufarray[i])*1e-11 + 0.075;
						currMsgs |= R2_r2_2R_2r_MSG;
						r2Cnt++;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," r2 msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "2R"){ // Relative P2 pseudoranges (2R) message
					unsigned int msgSats = (msg.size() - 2) / (2*sizeof(F4));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(F4),(unsigned char *) (f4bufarray));
						//for (unsigned int i=0;i<nSats;i++) 
						//	relP2pr[i] = (double) f4bufarray[i];
						currMsgs |= R2_r2_2R_2r_MSG;
						m2RCnt++;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," 2R msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "2r"){ // Short relative P2 pseudoranges (2r) message
					if (m2RCnt) continue; // full delta pseudoranges take precedence
					unsigned int msgSats = (msg.size() - 2) / (2*sizeof(I2));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(I2),(unsigned char *) (i2bufarray));
						//for (unsigned int i=0;i<nSats;i++) 
						//	relP2pr[i] = (double)(i2bufarray[i])*1e-11 + 2.0e-7;
						currMsgs |= R2_r2_2R_2r_MSG;
						m2rCnt++;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," 2r msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "F1"){ // P1 Lock Flags (F1) message
					unsigned int msgSats = (msg.size() - 2) /(2*sizeof(U2));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(U2),(unsigned char *) (P1lockFlags));
						currMsgs |= F1_MSG;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," F1 msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "F2"){ // P2 Lock Flags (F2) message
					unsigned int msgSats = (msg.size() - 2) /(2*sizeof(U2));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(U2),(unsigned char *) (P2lockFlags));
						currMsgs |= F2_MSG;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," F2 msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "P1"){ // L1 carrier phase message
					unsigned int msgSats = (msg.size() - 2) / (2*sizeof(F8));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(F8),(unsigned char *) (f8bufarray));
						for (unsigned int i=0;i<nSats;i++) 
							cpP1[i] = (double) f8bufarray[i];
						//currMsgs |= ;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," P1 msg wrong size at line " << linecount);	
					}
					continue;
				}
				
				if(msgid == "P2"){ // L2 carrier phase message
					unsigned int msgSats = (msg.size() - 2) / (2*sizeof(F8));
					if (msgSats == nSats){
						HexToBin((char *) msg.c_str(),nSats*sizeof(F8),(unsigned char *) (f8bufarray));
						for (unsigned int i=0;i<nSats;i++) 
							cpP2[i] = (double) f8bufarray[i];
						//currMsgs |= ;
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING," P2 msg wrong size at line " << linecount);	
					}
					continue;
				}
				
			} // if dualFrequency
		
			//
			// Intermittent  messages - parse last
			//
			
			if (!gps.gotIonoData){
				if (msgid=="IO"){
					if (msg.size()==39*2){
						HexToBin((char *) msg.substr(6*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &(gps.ionoData.a0));
						HexToBin((char *) msg.substr(10*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &(gps.ionoData.a1));
						HexToBin((char *) msg.substr(14*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &(gps.ionoData.a2));
						HexToBin((char *) msg.substr(18*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &(gps.ionoData.a3));
						HexToBin((char *) msg.substr(22*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &(gps.ionoData.B0));
						HexToBin((char *) msg.substr(26*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &(gps.ionoData.B1));
						HexToBin((char *) msg.substr(30*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &(gps.ionoData.B2));
						HexToBin((char *) msg.substr(34*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &(gps.ionoData.B3));
						gps.gotIonoData=true;
						DBGMSG(debugStream,TRACE,"ionosphere parameters: a0=" << gps.ionoData.a0);
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING,"Bad I0 message size");
					}
					continue;
				}
			}
	
			if (!gps.gotUTCdata){
				if (msgid=="UO"){
					if (msg.size()==24*2){
						HexToBin((char *) msg.substr(0*2,2*sizeof(F8)).c_str(),sizeof(F8),(unsigned char *) &(gps.UTCdata.A0));
						HexToBin((char *) msg.substr(8*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &(gps.UTCdata.A1));
						HexToBin((char *) msg.substr(12*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
						gps.UTCdata.t_ot=u4buf;
						HexToBin((char *) msg.substr(16*2,2*sizeof(U2)).c_str(),sizeof(U2),(unsigned char *) &(gps.UTCdata.WN_t));
						HexToBin((char *) msg.substr(18*2,2*sizeof(I1)).c_str(),sizeof(I1),(unsigned char *) &(sint8buf));
						gps.UTCdata.dt_LS=sint8buf;
						HexToBin((char *) msg.substr(19*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &uint8buf);
						gps.UTCdata.DN=uint8buf;
						HexToBin((char *) msg.substr(20*2,2*sizeof(U2)).c_str(),sizeof(U2),(unsigned char *) &(gps.UTCdata.WN_LSF));
						HexToBin((char *) msg.substr(22*2,2*sizeof(I1)).c_str(),sizeof(I1),(unsigned char *) &sint8buf);
						gps.UTCdata.dt_LSF=sint8buf;
						DBGMSG(debugStream,TRACE,"UTC parameters: dt_LS=" << gps.UTCdata.dt_LS << ",dt_LSF=" << gps.UTCdata.dt_LSF);
						gps.gotUTCdata = gps.currentLeapSeconds(mjd,&leapsecs);
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING,"Bad U0 message size");
					}
					continue;
				}
			}

			if(msgid == "GE"){  // GPS ephemeris
				if (msg.length() == 123*2){
					GPSEphemeris *ed = new GPSEphemeris;
 					HexToBin((char *) msg.substr(0*2,2*sizeof(UINT8)).c_str(), sizeof(UINT8),  (unsigned char *) &(ed->SVN));
					HexToBin((char *) msg.substr(1*2,2*sizeof(UINT32)).c_str(),sizeof(UINT32), (unsigned char *) &(u4buf));
 					ed->t_ephem=u4buf;
					
					HexToBin((char *) msg.substr(6*2,2*sizeof(SINT16)).c_str(),sizeof(SINT16), (unsigned char *) &(sint16buf));
						ed-> IODC=sint16buf;
					HexToBin((char *) msg.substr(8*2,2*sizeof(SINT32)).c_str(),sizeof(SINT32), (unsigned char *) &(sint32buf));
					ed->t_OC=sint32buf;
					HexToBin((char *) msg.substr(12*2,2*sizeof(SINT8)).c_str(), sizeof(SINT8),  (unsigned char *) &(sint8buf));
					ed->SV_accuracy_raw = sint8buf;
					ed->SV_accuracy = GPS::URA[ed->SV_accuracy_raw];
					HexToBin((char *) msg.substr(13*2,2*sizeof(UINT8)).c_str(), sizeof(UINT8),  (unsigned char *) &(ed->SV_health));
					HexToBin((char *) msg.substr(14*2,2*sizeof(SINT16)).c_str(),sizeof(SINT16), (unsigned char *) &sint16buf);
					ed->week_number=(UINT16) sint16buf;
					HexToBin((char *) msg.substr(16*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->t_GD));
					
					HexToBin((char *) msg.substr(20*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->a_f2));
					HexToBin((char *) msg.substr(24*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->a_f1));
					HexToBin((char *) msg.substr(28*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->a_f0));
					HexToBin((char *) msg.substr(32*2,2*sizeof(SINT32)).c_str(),sizeof(SINT32), (unsigned char *) &sint32buf);
					ed->t_0e=sint32buf;
					HexToBin((char *) msg.substr(36*2,2*sizeof(SINT16)).c_str(), sizeof(SINT16),  (unsigned char *) &sint16buf);
					ed->IODE=(UINT8) sint16buf; // WARNING! Truncated
					HexToBin((char *) msg.substr(38*2,2*sizeof(DOUBLE)).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->sqrtA));
					
					HexToBin((char *) msg.substr(46*2,2*sizeof(DOUBLE)).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->e));
					HexToBin((char *) msg.substr(54*2,2*sizeof(DOUBLE)).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->M_0));
					ed->M_0 *= M_PI;
					HexToBin((char *) msg.substr(62*2,2*sizeof(DOUBLE)).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->OMEGA_0));
					ed->OMEGA_0 *= M_PI;
					HexToBin((char *) msg.substr(70*2,2*sizeof(DOUBLE)).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->i_0));
					ed->i_0 *= M_PI;
					HexToBin((char *) msg.substr(78*2,2*sizeof(DOUBLE)).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->OMEGA));
					ed->OMEGA *= M_PI;
					HexToBin((char *) msg.substr(86*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->delta_N));
					ed->delta_N *=M_PI;
					HexToBin((char *) msg.substr(90*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->OMEGADOT));
					ed->OMEGADOT *= M_PI;
					HexToBin((char *) msg.substr(94*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->IDOT));
					ed->IDOT *= M_PI;
					HexToBin((char *) msg.substr(98*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->C_rc));
					HexToBin((char *) msg.substr(102*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->C_rs));
					HexToBin((char *) msg.substr(106*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->C_uc));
					HexToBin((char *) msg.substr(110*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->C_us));
					HexToBin((char *) msg.substr(114*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->C_ic));
					HexToBin((char *) msg.substr(118*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->C_is));
					
					int pchh,pcmm,pcss;
					if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss)))
						ed->tLogged = pchh*3600 + pcmm*60 + pcss; 
					else
						ed->tLogged = -1;
						
					DBGMSG(debugStream,TRACE,"Ephemeris: SVN="<< (unsigned int) ed->SVN << ",t0e="<< ed->t_0e << ",IODE=" << (int) ed->IODE);
					gps.addEphemeris(ed);
					
				}
				else{
					errorCount++;
					DBGMSG(debugStream,WARNING,"Bad GE message size");
				}
				continue;
			}
			
		}
	}
	else{
		app->logMessage("Unable to open "+fname);
		return false;
	}
	
	infile.close();

	if (!gps.gotIonoData){
		app->logMessage("failed to find ionosphere parameters - no IO messages");
		return false;
	}
	
	if (!gps.gotUTCdata){
		app->logMessage("failed to find UTC parameters - no U0 messages");
		return false;
	}
	
	// Post load cleanups 
	
	// Interpolation REMOVED
	
	// Calculate UTC time of measurements, now that the number of leap seconds is known
	for (unsigned int i=0;i<measurements.size();i++){
		time_t tUTC = mktime(&(measurements[i]->tmGPS));
		tUTC -= leapsecs;
		struct tm *tmUTC = gmtime(&tUTC);
		measurements[i]->tmUTC=*tmUTC;
	}
	
	// Extract the receiver id
	if (rxid.size() !=0) {
		if ((rxid.size() % 4 == 0)){
			int idx = (rxid.size() / 4) -1;
			std::string rxinfo = rxid.at(idx) + rxid.at(idx+1) + rxid.at(idx+2) + rxid.at(idx+3);
			rxinfo.erase(std::remove(rxinfo.begin(),rxinfo.end(),'{'), rxinfo.end());
			rxinfo.erase(std::remove(rxinfo.begin(),rxinfo.end(),'}'), rxinfo.end());
			rxinfo.erase(std::remove(rxinfo.begin(),rxinfo.end(),'\"'), rxinfo.end());
			std::vector<std::string> vals;
			boost::split(vals, rxinfo,boost::is_any_of(","), boost::token_compress_on);
			boost::algorithm::trim_left(vals.at(0));
			serialNumber = vals.at(0);
			modelName = vals.at(1);
			boost::algorithm::trim_left(vals.at(4));
			swversion = vals.at(4)+" "+vals.at(5)+" "+vals.at(6);
			DBGMSG(debugStream,INFO,"rx s/n " << vals.at(0) << ",model " << modelName << ",sw " << swversion);
			
		}
		else{
			DBGMSG(debugStream,INFO, "Unable to find receiver ID");
		}
	}
	
	gps.fixWeekRollovers();
	gps.setAbsT0c(mjd);  // precompute stuff for ephemeris output
	
	timer.stop();
	
	DBGMSG(debugStream,INFO,"done: read " << linecount << " lines");
	DBGMSG(debugStream,INFO,measurements.size() << " measurements read");
	DBGMSG(debugStream,INFO,gps.ephemeris.size() << " GPS ephemeris entries read");
	DBGMSG(debugStream,INFO,errorCount << " errors in input file");
	if (codes & GNSSSystem::C1C){
		DBGMSG(debugStream,INFO,badC1Measurements  << " SV C1 measurements rejected");
	}
	if (codes & GNSSSystem::C1P){
		DBGMSG(debugStream,INFO,badP1Measurements  << " SV P1 measurements rejected");
	}
	if (codes & GNSSSystem::C2P){
		DBGMSG(debugStream,INFO,badP2Measurements  << " SV P2 measurements rejected");
	}
	DBGMSG(debugStream,INFO,"elapsed time: " << timer.elapsedTime(Timer::SECS) << " s");
	return true;
	
}

