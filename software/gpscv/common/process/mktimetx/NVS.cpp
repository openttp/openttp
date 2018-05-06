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

#include <stdio.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <stdint.h>

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
#include "NVS.h"
#include "ReceiverMeasurement.h"
#include "SVMeasurement.h"

extern ostream *debugStream;
extern Application *app;

#define MAX_CHANNELS 16 // max channels per constellation

typedef unsigned char INT8U;
typedef char INT8S;
typedef unsigned short INT16U;
typedef short INT16S;
typedef unsigned int INT32U;
typedef int INT32S ;
typedef float FP32;
typedef double FP64;
//typedef long double FP80 ; // WARNING 80 bits on x86 but 64 bits on ARM
														 // In any case, we don't currently need it 

#define MSG46 0x01
#define MSG72 0x02
#define MSG74 0x04
#define MSGF5 0x08

// Current problems with NVS driver
// None known ...

// Utility function so that we can handle FP80 on ARM
// WARNING For some values of interest, a double has sufficient precision
// For others, best to check on x86

double FP80toFP64(uint8_t *buf)
{
	// little - endian ....
	double sign=1.0;
	if ((buf[9] & 0x80) != 0x00)
		sign=-1.0;
	uint32_t exponent = ((buf[9]& 0x7f)<<8) + buf[8];
	//printf("sign = %i raw exp = %i exp = %i \n",(int) sign, (int) exponent, (int) exponent - 16383);
	uint64_t mantissa;
	memcpy(&mantissa,buf,sizeof(uint64_t));
	// Is this a normalized number ?
	double normalizeCorrection;
	if ((mantissa & 0x8000000000000000) != 0x00)
		normalizeCorrection = 1;
	else
		normalizeCorrection = 0;
	mantissa &= 0x7FFFFFFFFFFFFFFF;
	
	return sign * pow(2,(int) exponent - 16383) * (normalizeCorrection + (double) mantissa/((uint64_t)1 << 63));
}

NVS::NVS(Antenna *ant,string m):Receiver(ant)
{
	modelName=m;
	manufacturer="NVS";
	swversion="0.1";
	constellations=GNSSSystem::GPS;
	codes=GNSSSystem::C1;
	channels=32;
	if (modelName == "NV08C-CSM"){
		// For the future
	}
	else{
		app->logMessage("Unknown receiver model: " + modelName);
		app->logMessage("Assuming NV08C-CSM");
	}
}

NVS::~NVS()
{
}

bool NVS::readLog(string fname,int mjd,int startTime,int stopTime)
{
	DBGMSG(debugStream,INFO,"reading " << fname);	
	
	ifstream infile (fname.c_str());
	string line;
	int linecount=0;
	
	string msgid,currpctime,pctime="",msg,gpstime;
	
	float rxTimeOffset; // single
	FP64 sawtooth;     // units are ns
	FP64  fp64buf;
	INT16U int16ubuf;
	INT32U int32ubuf;
	INT8S int8sbuf;
	INT8U int8ubuf;
	
	vector<SVMeasurement *> gpsmeas;
	gotIonoData = false;
	gotUTCdata=false;
	
	INT8U msg46ss,msg46mm,msg46hh,msg46mday,msg46mon;
	INT16U msg46yyyy;
	FP64 tmeasUTC,dGPSUTC;
	INT16U weekNum;
	FP64 msg72TOW; // this is in ms
	
	unsigned int currentMsgs=0;
	unsigned int reqdMsgs = MSG46 | MSG72 | MSG74 | MSGF5;
	bool duplicateMessages=false;
	double gpsUTCOffset;
	
  if (infile.is_open()){
    while ( getline (infile,line) ){
			linecount++;
			
			if (line.size()==0) continue; // skip empty line
			if ('#' == line.at(0)) continue; // skip comments
			if ('%' == line.at(0)) continue;
			if ('@' == line.at(0)) continue;
			
			stringstream sstr(line);
			sstr >> msgid >> currpctime >> msg;
			if (sstr.fail()){ // throw away whatever we have, invalidating the rest of the second's data too
				DBGMSG(debugStream,WARNING," bad data at line " << linecount);
				currentMsgs=0;
				deleteMeasurements(gpsmeas);
				duplicateMessages=false;
				continue;
			}
			
			// Message order can vary so the messages are grouped by the PC time stamp
			// When this changes, we check whether all the required messages were received
			
			if (currpctime != pctime){
				if (currentMsgs == reqdMsgs && !duplicateMessages){ // save the measurements from the previous second
					if (gpsmeas.size() > 0){
						ReceiverMeasurement *rmeas = new ReceiverMeasurement();
						measurements.push_back(rmeas);
						
						rmeas->sawtooth=sawtooth; 
						rmeas->timeOffset=rxTimeOffset;
						if (rxTimeOffset !=0){ // FIXME this is here until we get sufficient experience !
							// Occur at ms rollovers
							cerr << "FIXME " << pctime << " rxTimeoffset = " << rxTimeOffset << "\n";
						}
						int pchh,pcmm,pcss;
						if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss))){
							rmeas->pchh=pchh;
							rmeas->pcmm=pcmm;
							rmeas->pcss=pcss;
						}
						
						// Calculate the time of measurement in various systems
						// The NVS reports the measurement time as X.Y, in the second after X
						// Some fudgery is used in the various time assignments below to make this fit into 
						// various assumptions that are made in the processing
						
						// GPSTOW is used for pseudorange estimations
						// note: this is rounded because measurements are interpolated on a 1s grid
						rmeas->gpstow=rint((tmeasUTC+dGPSUTC)/1000);  
						rmeas->gpswn=weekNum; // note: this is truncated. Not currently used FIXME UTC or GPS???
						
						// UTC time of measurement
						GPS::GPStoUTC(rmeas->gpstow,rmeas->gpswn,(int) rint(dGPSUTC/1000.0),&(rmeas->tmUTC));
						
						// Calculate GPS time of measurement 
						time_t tgps = GPS::GPStoUnix(rmeas->gpstow,rmeas->gpswn);
						struct tm *tmGPS = gmtime(&tgps);
						rmeas->tmGPS=*tmGPS;
						
						// This may seem obscure.
						// What we're doing here is calculating the offset of the measurement time
						// from the nominal measurement time. Note that this is how the measurement time is
						// given by the Javad receiver, as a +/- offset from the nominal. This is all necessary 
						// so that the interpolation function works correctly
						// FIXME tmfracs is poorly named. 
						rmeas->tmfracs = (tmeasUTC/1000.0 - int(tmeasUTC/1000.0)); 
						if (rmeas->tmfracs > 0.5) rmeas->tmfracs -= 1.0; // place in the previous second
						
						for (unsigned int i=0;i<gpsmeas.size();i++)
							gpsmeas.at(i)->rm=rmeas; // code measurements are not reported with ms ambiguities
						rmeas->meas=gpsmeas;
						gpsmeas.clear(); // don't delete - we only made a shallow copy!
				
						currentMsgs = 0;
						duplicateMessages=false;
						
						// KEEP THIS it's useful for debugging measurement-time related problems
						// fprintf(stderr,"PC=%02d:%02d:%02d msg46=%02d:%02d:%02d msg72TOW=%8.6f tmeasUTC=%8.6f gpstow=%d gpswn=%d tmfracs=%g\n",
						// pchh,pcmm,pcss,msg46hh,msg46mm,msg46ss,msg72TOW,tmeasUTC/1000.0,(int) rmeas->gpstow,(int) weekNum,rmeas->tmfracs );
					} // if (gps.size() > 0)
				}
				else if (pctime != ""){ // throw away the whole second
					deleteMeasurements(gpsmeas);
					currentMsgs = 0;
					duplicateMessages=false;
					DBGMSG(debugStream,INFO," Duplicate/missing messages at " << pctime);
				}
			}
			
			pctime = currpctime;
			
			if(msgid == "F5"){ // Raw measurements 
				
				if (currentMsgs & MSGF5){
					duplicateMessages = true;
					continue;
				}
				
				if (((msg.size()-27*2) % 30*2) == 0){
					
					HexToBin((char *) msg.substr(0,2*sizeof(FP64)).c_str(),sizeof(FP64),(unsigned char *) &tmeasUTC); // in ms, since beginning of week
					HexToBin((char *) msg.substr(8*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U),(unsigned char *) &weekNum); // truncated
					HexToBin((char *) msg.substr(10*2,2*sizeof(FP64)).c_str(),sizeof(FP64),(unsigned char *) &dGPSUTC); // in ms - current number of leap secs
					HexToBin((char *) msg.substr(26*2,2*sizeof(INT8S)).c_str(),sizeof(INT8S),(unsigned char *) &int8sbuf); // in ms
		
					rxTimeOffset = int8sbuf * 1.0E-3; // mostly zero 
					
					int nsats=(msg.size()-27*2) / (30*2);
					INT8U svn,signal,flags;
					
					for (int s=0;s<nsats;s++){
						HexToBin((char *) msg.substr((27+s*30)*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),&signal); 
						if (signal &0x02){ // GPS
							HexToBin((char *) msg.substr((28+s*30)*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),&svn); 
							HexToBin((char *) msg.substr((39+s*30)*2,2*sizeof(FP64)).c_str(),sizeof(FP64),(unsigned char *) &fp64buf);
							HexToBin((char *) msg.substr((55+s*30)*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),&flags);
							// FIXME use flags to filter measurements 
							DBGMSG(debugStream,TRACE,pctime << " svn "<< (int) svn << " pr " << fp64buf*1.0E-3 << " flags " << (int) flags);
							if (flags & (0x01 | 0x02 | 0x04 | 0x10)){ // FIXME determine optimal set of flags
								double svmeas = fp64buf*1.0E-3 + (rint(gpsUTCOffset)-gpsUTCOffset)*1.0E-3; // correct for GPS-UTC offset, which steps each day
								SVMeasurement *svm = new SVMeasurement(svn,GNSSSystem::GPS,GNSSSystem::C1,svmeas,NULL);
								svm->dbuf3=svmeas;
								gpsmeas.push_back(svm); 
							}
							else{
							}
						}
					}
					
					if (gpsmeas.size() >= MAX_CHANNELS){ // too much data - something is wrong
						DBGMSG(debugStream,WARNING,"Too many F5 (raw data) messages at line " << linecount  << " " << currpctime << "(got " << gpsmeas.size() << ")");
						deleteMeasurements(gpsmeas);
						continue;
					}
					
					currentMsgs |= MSGF5; // All OK
				}
				
				else{
					DBGMSG(debugStream,WARNING,"0xf5 msg wrong size at " << linecount << " " << msg.size());
				}
				continue;
				
			} // raw data (F5)
			
			if(msgid=="72"){ // Time and frequency parameters (sawtooth correction in particular)
				
				if (currentMsgs & MSG72){
					duplicateMessages = true;
					continue;
				}
				
				if (msg.size()==34*2){
					
					unsigned char fp80buf[10];
					
					HexToBin((char *) msg.substr(0*2,2*10).c_str(),10,fp80buf);
					msg72TOW = FP80toFP64(fp80buf);
					
					// Check the time scale - if this is not GPS then quit
					// Checked repeatedly in case of receiver restarts
					//HexToBin((char *) msg.substr(12*2,sizeof(INT8U)*2).c_str(),sizeof(INT8U),(unsigned char *) &int8ubuf);
					//if (!(int8ubuf == 0x01)){
					//	app->logMessage("reference time scale is not GPS");
					//	return false;
					//}
					HexToBin((char *) msg.substr(21*2,sizeof(FP64)*2).c_str(),sizeof(FP64),(unsigned char *) &sawtooth);
					sawtooth = - sawtooth * 1.0E-9; // convert from ns to seconds and fix sign
					currentMsgs |= MSG72;
				}
				else{
					DBGMSG(debugStream,WARNING,"0x72h msg wrong size at line "<<linecount);
				}
				
				continue;
			}
			
			if(msgid=="46"){ // Time message
				
				if (currentMsgs & MSG46){
					duplicateMessages = true;
					continue;
				}
				
				if (msg.size()==10*2){
					INT32U tow;
					HexToBin((char *) msg.substr(0*2,2*sizeof(INT32U)).c_str(),sizeof(INT32U),(unsigned char *) &tow);
					tow = tow-(tow/86400)*86400;
					msg46hh = tow/3600;
					msg46mm = (tow - 3600*msg46hh)/60;
					msg46ss = tow - msg46hh*3600 - msg46mm*60;
					HexToBin((char *) msg.substr(4*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),(unsigned char *) &msg46mday);
					HexToBin((char *) msg.substr(5*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),(unsigned char *) &msg46mon);
					HexToBin((char *) msg.substr(6*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U),(unsigned char *) &msg46yyyy);
					currentMsgs |= MSG46;
				}
				else{
					DBGMSG(debugStream,WARNING,"0x46 msg wrong size at line "<<linecount);
				}
				continue;
			}
			
			if(msgid=="74"){ // Time scale parameters (validity of time scales) 
				
				if (currentMsgs & MSG74){
					duplicateMessages = true;
					continue;
				}
				
				if (msg.size()==51*2){
					unsigned char fp80buf[10];
					
					HexToBin((char *) msg.substr(0*2,2*10).c_str(),10,fp80buf);
					double gpsRxOffset = FP80toFP64(fp80buf);
					
					HexToBin((char *) msg.substr(20*2,2*10).c_str(),10,fp80buf);
					gpsUTCOffset = FP80toFP64(fp80buf);
					
					INT8U validity;
					HexToBin((char *) msg.substr(50*2,sizeof(INT8U)*2).c_str(),sizeof(INT8U),(unsigned char *) &validity);
					currentMsgs |= MSG74;
					DBGMSG(debugStream,TRACE,"0x74 GPS-Rx = " << setprecision(16) << gpsRxOffset << " GPS-UTC = " <<  gpsUTCOffset);
				}
				else{
					DBGMSG(debugStream,WARNING,"0x74 msg wrong size at line "<<linecount);
				}
				continue;
			}
			
			//
			// Messages needed to contruct the RINEX navigation file
			//
			
			if (msgid == "4A"){ // Ionosphere parameters
				if (msg.size()==33*2){
					INT8U reliability;
					HexToBin((char *) msg.substr(32*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),&reliability); 
					if (reliability == 255){
						HexToBin((char *) msg.substr(0*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(gps.ionoData.a0)); 
						HexToBin((char *) msg.substr(4*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(gps.ionoData.a1));
						HexToBin((char *) msg.substr(8*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(gps.ionoData.a2));
						HexToBin((char *) msg.substr(12*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(gps.ionoData.a3));
						HexToBin((char *) msg.substr(16*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(gps.ionoData.B0)); 
						HexToBin((char *) msg.substr(20*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(gps.ionoData.B1));
						HexToBin((char *) msg.substr(24*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(gps.ionoData.B2));
						HexToBin((char *) msg.substr(28*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(gps.ionoData.B3));
						gotIonoData = true;
					}
				}
				else{
					DBGMSG(debugStream,WARNING,"0x4A msg wrong size at line "<<linecount);
				}
				continue;
			}
			
			if (msgid == "4B"){ // GPS, GLONASS and UTC parameters
				if (msg.size()==42*2){
					INT8U reliability;
					HexToBin((char *) msg.substr(30*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),&reliability); // GPS reliability only
					if (reliability == 255){
						HexToBin((char *) msg.substr(0*2,2*sizeof(FP64)).c_str(),sizeof(FP64),(unsigned char *) &fp64buf); 
						gps.UTCdata.A1=fp64buf;
						HexToBin((char *) msg.substr(8*2,2*sizeof(FP64)).c_str(),sizeof(FP64),(unsigned char *) &fp64buf); 
						gps.UTCdata.A0=fp64buf;
						HexToBin((char *) msg.substr(16*2,2*sizeof(INT32U)).c_str(),sizeof(INT32U),(unsigned char *) &int32ubuf);
						gps.UTCdata.t_ot = int32ubuf;
						HexToBin((char *) msg.substr(20*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U),(unsigned char *) &(gps.UTCdata.WN_t));
						HexToBin((char *) msg.substr(22*2,2*sizeof(INT16S)).c_str(),sizeof(INT16S),(unsigned char *) &(gps.UTCdata.dtlS));
						HexToBin((char *) msg.substr(24*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U),(unsigned char *) &(gps.UTCdata.WN_LSF));
						HexToBin((char *) msg.substr(26*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U),(unsigned char *) &(gps.UTCdata.DN));
						HexToBin((char *) msg.substr(28*2,2*sizeof(INT16S)).c_str(),sizeof(INT16S),(unsigned char *) &(gps.UTCdata.dt_LSF));
						gotUTCdata = gps.currentLeapSeconds(mjd,&leapsecs);
					}
				}
				else{
					DBGMSG(debugStream,WARNING,"0x4B msg wrong size at line "<<linecount);
				}
				continue;
			}
			
			if (msgid=="F7"){ // Extended Ephemeris
				if (msg.size()==138*2){
					INT8U eph;
					HexToBin((char *) msg.substr(0,sizeof(INT8U)*2).c_str(),sizeof(INT8U),(unsigned char *) &eph);
					if (eph == 0x01){
						
						GPS::EphemerisData *ed = new GPS::EphemerisData;
						HexToBin((char *) msg.substr(1*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),(unsigned char *) &(ed->SVN));
						HexToBin((char *) msg.substr(2*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->C_rs));
						HexToBin((char *) msg.substr(6*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->delta_N));
						ed->delta_N *=1000.0;
						HexToBin((char *) msg.substr(10*2,2*sizeof(FP64)).c_str(),sizeof(FP64), (unsigned char *) &(ed->M_0));
						HexToBin((char *) msg.substr(18*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->C_uc));
						HexToBin((char *) msg.substr(22*2,2*sizeof(FP64)).c_str(),sizeof(FP64), (unsigned char *) &(ed->e));
						HexToBin((char *) msg.substr(30*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->C_us));
						HexToBin((char *) msg.substr(34*2,2*sizeof(FP64)).c_str(),sizeof(FP64), (unsigned char *) &(ed->sqrtA));
						HexToBin((char *) msg.substr(42*2,2*sizeof(FP64)).c_str(),sizeof(FP64), (unsigned char *) &fp64buf);
						ed->t_oe = fp64buf*1.0E-3;
						HexToBin((char *) msg.substr(50*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->C_ic));
						HexToBin((char *) msg.substr(54*2,2*sizeof(FP64)).c_str(),sizeof(FP64), (unsigned char *) &(ed->OMEGA_0));
						HexToBin((char *) msg.substr(62*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->C_is));
						HexToBin((char *) msg.substr(66*2,2*sizeof(FP64)).c_str(),sizeof(FP64), (unsigned char *) &(ed->i_0));
						HexToBin((char *) msg.substr(74*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->C_rc));
						HexToBin((char *) msg.substr(78*2,2*sizeof(FP64)).c_str(),sizeof(FP64), (unsigned char *) &(ed->OMEGA));
						HexToBin((char *) msg.substr(86*2,2*sizeof(FP64)).c_str(),sizeof(FP64), (unsigned char *) &fp64buf);
						ed->OMEGADOT=fp64buf*1000.0;
						HexToBin((char *) msg.substr(94*2,2*sizeof(FP64)).c_str(),sizeof(FP64), (unsigned char *) &(fp64buf));
						ed->IDOT=fp64buf*1000.0;
						HexToBin((char *) msg.substr(102*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->t_GD));
						ed->t_GD *= 1.0E-3;
						HexToBin((char *) msg.substr(106*2,2*sizeof(FP64)).c_str(),sizeof(FP64), (unsigned char *) &fp64buf);
						ed->t_OC=fp64buf*1.0E-3; // note that this is reported as a UTC time
						HexToBin((char *) msg.substr(114*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->a_f2));
						ed->a_f2 *= 1000.0;
						HexToBin((char *) msg.substr(118*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->a_f1));
						HexToBin((char *) msg.substr(122*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->a_f0));
						ed->a_f0 *= 1.0E-3;
						HexToBin((char *) msg.substr(126*2,2*sizeof(INT16U)).c_str(), sizeof(INT16U),  (unsigned char *) &int16ubuf);
						ed->SV_accuracy_raw = int16ubuf;
						ed->SV_accuracy = GPS::URA[ed->SV_accuracy_raw];
						HexToBin((char *) msg.substr(128*2,2*sizeof(INT16U)).c_str(), sizeof(INT16U), (unsigned char *) &int16ubuf);
						ed->IODE=int16ubuf;
						HexToBin((char *) msg.substr(130*2,2*sizeof(INT16U)).c_str(), sizeof(INT16U), (unsigned char *) &int16ubuf);
						ed->IODC=int16ubuf;
						
						ed->SV_health=0.;
						HexToBin((char *) msg.substr(136*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U), (unsigned char *) &(ed->week_number));
						
						ed->t_ephem=0.0; // FIXME unknown - how to flag ?
						int pchh,pcmm,pcss;
						if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss)))
							ed->tLogged = pchh*3600 + pcmm*60 + pcss; 
						else
							ed->tLogged = -1;
						DBGMSG(debugStream,TRACE,"GPS eph  "<< (int) ed->SVN << " " << ed->t_oe << " " << ed->t_OC << " " << (int) ed->SV_accuracy_raw);
						gps.addEphemeris(ed);
					
					}
					else{
						DBGMSG(debugStream,WARNING,"0xF7 msg (GPS) wrong size at line "<<linecount);
					}
					continue;
				}
				else if (msg.size()==93*2){
				}
				else{
					DBGMSG(debugStream,WARNING,"0xF7 msg wrong size at line "<<linecount);
				}
			}
			
		}
	}
	else{
		app->logMessage(" unable to open " + fname);
		return false;
	}
	infile.close();

	if (!gotIonoData){
		app->logMessage("failed to find ionosphere parameters - no 4A messages");
		return false;
	}
	
	if (!gotUTCdata){
		app->logMessage("failed to find UTC parameters - no 4B messages");
		return false;
	}
	
	// Pass through the data to realign the sawtooth correction.
	// This could be done in the main loop but it's more flexible this way.
	// For the NVS, the sawtooth correction applies to the next second
	// If we're missing the sawtooth correction because of eg a gap in the data
	// then we'll just use the current sawtooth. 
	
	int nBadSawtoothCorrections=0;
	
	if (sawtoothPhase == Receiver::NextSecond){
		double prevSawtooth=measurements.at(0)->sawtooth;
		time_t    tPrevSawtooth=mktime(&(measurements.at(0)->tmUTC));
		nBadSawtoothCorrections =1; // first is bad !
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
	}
	
	// The NVS sometime reports what appears to be an incorrect pseudorange after picking up an SV
	// If you wanted to filter these out, this is where you should do it
	
	interpolateMeasurements();
	// Note that after this, tmfracs is now zero and all measurements have been interpolated to a 1 s grid
	
	DBGMSG(debugStream,INFO,"done: read " << linecount << " lines");
	DBGMSG(debugStream,INFO,measurements.size() << " measurements read");
	DBGMSG(debugStream,INFO,gps.ephemeris.size() << " GPS ephemeris entries read");
	DBGMSG(debugStream,INFO,nBadSawtoothCorrections << " bad sawtooth corrections");
	return true;
	
}
