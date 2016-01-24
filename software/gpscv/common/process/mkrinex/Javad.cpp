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
#include "Debug.h"
#include "GPS.h"
#include "HexBin.h"
#include "Javad.h"
#include "ReceiverMeasurement.h"
#include "SVMeasurement.h"

extern ostream *debugStream;

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
#define FC_MSG 0x04
#define RC_rc_MSG 0x08
#define RD_MSG 0x10
#define RT_MSG 0x20
#define SI_MSG 0x40
#define SS_MSG 0x80
#define TO_MSG 0x100
#define YA_MSG 0x200
#define ZA_MSG 0x400

Javad::Javad(Antenna *ant,string m):Receiver(ant)
{
  //model="Javad ";
	manufacturer="Javad";
	swversion="0.1";
	constellations=Receiver::GPS;
	dualFrequency=false;
}

Javad::~Javad()
{
}

bool Javad::readLog(string fname,int mjd)
{
	DBGMSG(debugStream,INFO,"reading " << fname);	
	
	
	ifstream infile (fname.c_str());
	string line;
	int linecount=0;
	
	string msgid,currpctime,pctime,msg,gpstime;
	
	U4 gpsTOD;
	F8 rxTimeOffset;
	F4 sawtooth;  
	
	vector<string> rxid;
	
	vector<SVMeasurement *> gps;
	gotIonoData = false;
	gotUTCdata=false;
	
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
	
	I4 i4bufarray[MAX_CHANNELS];
	F8 f8bufarray[MAX_CHANNELS];
	
	F8 smoothingOffset;
	int rcCnt=0,RCcnt=0;
	unsigned int errorCount=0,badMeasurements=0;
	U2 RDyyyy;
	U1 RDmm,RDdd;
	
	reqdMsgs = AZ_MSG | EL_MSG | FC_MSG | RC_rc_MSG| RT_MSG | SI_MSG | SS_MSG | TO_MSG | YA_MSG| ZA_MSG;
	
  if (infile.is_open()){
    while ( getline (infile,line) ){
			linecount++;
			
			if (line.size()==0) continue; // skip empty line
			if ('#' == line.at(0)) continue; // skip comments
			if ('%' == line.at(0)) continue;
		
			if ('@' == line.at(0)){ 
				size_t pos;
				if (string::npos != (pos = line.find("RXID",2 )) ){
					rxid.push_back(line.substr(pos+4,line.size()-pos-4));
				}
				continue;
			}
			stringstream sstr(line);
			
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
				
				if ((currMsgs == reqdMsgs) && (rcCnt <= 1) && (RCcnt <= 1)){ // Save measurements
					ReceiverMeasurement *rmeas = new ReceiverMeasurement();
					
					for (unsigned int chan=0; chan < nSats; chan++){
						
						// Check that PLLs are locked for each channel before we use the data
						if (CAlockFlags[chan*2] != 83){
							DBGMSG(debugStream,WARNING," C/A unlocked at line " << linecount << "(prn=" << (int) trackedSVs[chan] << ")");
							badMeasurements++;
							continue;
						}
						
						// Simple sanity check in case pseudoranges or the receiver time offset are wild
						if (((CApr[chan]-rxTimeOffset)<0.05) || ((CApr[chan]-rxTimeOffset)>0.10)){
							DBGMSG(debugStream,WARNING," C/A pseudorange too large at line " << linecount << "(" << CApr[chan]-rxTimeOffset << ")");
							badMeasurements++;
							continue;
						}
						SVMeasurement *svm = new SVMeasurement(trackedSVs[chan],CApr[chan]-rxTimeOffset); // pseudorange is corrected for rx offset 
						rmeas->gps.push_back(svm);
					}
					
					if (rmeas->gps.size() > 0){
						int pchh,pcmm,pcss;
						if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss))){
							rmeas->pchh=pchh;
							rmeas->pcmm=pcmm;
							rmeas->pcss=pcss;
							
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
					
							// YA and TO messages can roll over at different times - handle this case
							// Typically, the difference between the smoothed and receiver time offsets is <= 1 ns
							if ((smoothingOffset - rxTimeOffset)> 5e-4) smoothingOffset-=1e-3;
							if ((smoothingOffset - rxTimeOffset)<-5e-4) smoothingOffset+=1e-3;
			
							rmeas->sawtooth=sawtooth-(smoothingOffset-rxTimeOffset); // added to the counter measurement
							rmeas->timeOffset = rxTimeOffset;
							
							// All OK
							measurements.push_back(rmeas);
							DBGMSG(debugStream,TRACE,rmeas->gps.size() << " measurements at "  << (int) gpsTOD << " "
								<< hh << ":" << mm << ":" << (int) rmeas->tmGPS.tm_sec << " (GPS), " 
								<< pchh << ":" << pcmm << ":" << pcss << " (PC)");
						}
						else{
							errorCount++;
							DBGMSG(debugStream,WARNING,"Bad PC time " << currpctime);	
						}
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING,"No useable GPS measurements at " << currpctime);
						delete rmeas;
					}
				}
				
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
						badMeasurements++;
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
						badMeasurements++;
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
						badMeasurements++;
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
		
			//
			// Intermittent  messages - parse last
			//
			
			if (!gotIonoData){
				if (msgid=="IO"){
					if (msg.size()==39*2){
						HexToBin((char *) msg.substr(6*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &ionoData.a0);
						HexToBin((char *) msg.substr(10*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &ionoData.a1);
						HexToBin((char *) msg.substr(14*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &ionoData.a2);
						HexToBin((char *) msg.substr(18*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &ionoData.a3);
						HexToBin((char *) msg.substr(22*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &ionoData.B0);
						HexToBin((char *) msg.substr(26*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &ionoData.B1);
						HexToBin((char *) msg.substr(30*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &ionoData.B2);
						HexToBin((char *) msg.substr(34*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &ionoData.B3);
						gotIonoData=true;
						DBGMSG(debugStream,TRACE,"ionosphere parameters: a0=" << ionoData.a0);
					}
					else{
						errorCount++;
						DBGMSG(debugStream,WARNING,"Bad I0 message size");
					}
					continue;
				}
			}
	
			if (!gotUTCdata){
				if (msgid=="UO"){
					if (msg.size()==24*2){
						HexToBin((char *) msg.substr(0*2,2*sizeof(F8)).c_str(),sizeof(F8),(unsigned char *) &utcData.A0);
						HexToBin((char *) msg.substr(8*2,2*sizeof(F4)).c_str(),sizeof(F4),(unsigned char *) &utcData.A1);
						HexToBin((char *) msg.substr(12*2,2*sizeof(U4)).c_str(),sizeof(U4),(unsigned char *) &u4buf);
						utcData.t_ot=u4buf;
						HexToBin((char *) msg.substr(16*2,2*sizeof(U2)).c_str(),sizeof(U2),(unsigned char *) &utcData.WN_t);
						HexToBin((char *) msg.substr(18*2,2*sizeof(I2)).c_str(),sizeof(I2),(unsigned char *) &utcData.dtlS);
						HexToBin((char *) msg.substr(19*2,2*sizeof(U1)).c_str(),sizeof(U1),(unsigned char *) &uint8buf);
						utcData.DN=uint8buf;
						HexToBin((char *) msg.substr(20*2,2*sizeof(U2)).c_str(),sizeof(U2),(unsigned char *) &utcData.WN_LSF);
				
						HexToBin((char *) msg.substr(22*2,2*sizeof(I1)).c_str(),sizeof(I1),(unsigned char *) &sint8buf);
						utcData.dt_LSF=sint8buf;
						DBGMSG(debugStream,TRACE,"UTC parameters: dtLS=" << utcData.dtlS << ",dt_LSF=" << utcData.dt_LSF);
						gotUTCdata = setCurrentLeapSeconds(mjd,utcData);
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
					EphemerisData *ed = new EphemerisData;
 					HexToBin((char *) msg.substr(0*2,2*sizeof(UINT8)).c_str(), sizeof(UINT8),  (unsigned char *) &(ed->SVN));
					HexToBin((char *) msg.substr(1*2,2*sizeof(UINT32)).c_str(),sizeof(UINT32), (unsigned char *) &(u4buf));
 					ed->t_ephem=u4buf;
					
					HexToBin((char *) msg.substr(6*2,2*sizeof(SINT16)).c_str(),sizeof(SINT16), (unsigned char *) &(sint16buf));
						ed-> IODC=sint16buf;
					HexToBin((char *) msg.substr(8*2,2*sizeof(SINT32)).c_str(),sizeof(SINT32), (unsigned char *) &(sint32buf));
					ed->t_OC=sint32buf;
					HexToBin((char *) msg.substr(12*2,2*sizeof(SINT8)).c_str(), sizeof(SINT8),  (unsigned char *) &(sint8buf));
					ed->SV_accuracy_raw=sint8buf;
					HexToBin((char *) msg.substr(13*2,2*sizeof(UINT8)).c_str(), sizeof(UINT8),  (unsigned char *) &(ed->SV_health));
					HexToBin((char *) msg.substr(14*2,2*sizeof(SINT16)).c_str(),sizeof(SINT16), (unsigned char *) &sint16buf);
					ed->week_number=(UINT16) sint16buf;
					HexToBin((char *) msg.substr(16*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->t_GD));
						
// 					HexToBin((char *) msg.substr(37*2+2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->SV_accuracy));
					
					HexToBin((char *) msg.substr(20*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->a_f2));
					HexToBin((char *) msg.substr(24*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->a_f1));
					HexToBin((char *) msg.substr(28*2,2*sizeof(F4)).c_str(),sizeof(F4), (unsigned char *) &(ed->a_f0));
					HexToBin((char *) msg.substr(32*2,2*sizeof(SINT32)).c_str(),sizeof(SINT32), (unsigned char *) &sint32buf);
					ed->t_oe=sint32buf;
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
						
					DBGMSG(debugStream,TRACE,"Ephemeris: SVN="<< (unsigned int) ed->SVN << ",toe="<< ed->t_oe << ",IODE=" << (int) ed->IODE);
					addGPSEphemeris(ed);
					
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
		cerr << "Unable to open " << fname << endl;
		return false;
	}
	
	infile.close();

	if (!gotIonoData){
		cerr << "No IO (ionosphere) message found" << endl;
		return false;
	}
	
	if (!gotUTCdata){
		cerr << "No U0 (UTC) message found" << endl;
		return false;
	}
	
	// Post load cleanups 
	
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
			string rxinfo = rxid.at(idx) + rxid.at(idx+1) + rxid.at(idx+2) + rxid.at(idx+3);
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
	
	DBGMSG(debugStream,INFO,"done: read " << linecount << " lines");
	DBGMSG(debugStream,INFO,measurements.size() << " measurements read");
	DBGMSG(debugStream,INFO,ephemeris.size() << " ephemeris entries read");
	DBGMSG(debugStream,INFO,errorCount << " errors in input file");
	DBGMSG(debugStream,INFO,badMeasurements  << " bad measurements in input file");
	return true;
	
}
