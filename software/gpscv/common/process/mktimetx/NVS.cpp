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
typedef long double FP80   ; // WARNING 80 bits on x86 but 64 bits on ARM

#define MSG46 0x01
#define MSG72 0x02
#define MSG74 0x04
#define MSGF5 0x08

NVS::NVS(Antenna *ant,string m):Receiver(ant)
{
	modelName=m;
	manufacturer="NVS";
	swversion="0.1";
	constellations=Receiver::GPS;
	codes=C1;
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

bool NVS::readLog(string fname,int mjd)
{
	DBGMSG(debugStream,INFO,"reading " << fname);	
	
	ifstream infile (fname.c_str());
	string line;
	int linecount=0;
	
	string msgid,currpctime,pctime,msg,gpstime;
	
	float rxtimeoffset; // single
	FP64 sawtooth;     // units are ns
	FP64  fp64buf;
	INT16U int16ubuf;
	INT32U int32ubuf;
	INT8S int8sbuf;
	
	vector<SVMeasurement *> gps;
	gotIonoData = false;
	gotUTCdata=false;
	INT8U msg46ss,msg46mm,msg46hh,msg46mday,msg46mon;
	INT16U msg46yyyy;
	FP64 tmeasUTC,dGPSUTC;
	INT16U weekNum;
	
	unsigned int currentMsgs=0;
	unsigned int reqdMsgs = MSG46 | MSG72 | MSG74;
	
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
				deleteMeasurements(gps);
				continue;
			}
			
			// The 0xF5 message starts each second
			if(msgid == "F5"){ // Raw measurements 
				
				if (currentMsgs == reqdMsgs){ // save the measurements from the previous second
					if (gps.size() > 0){
						ReceiverMeasurement *rmeas = new ReceiverMeasurement();
						measurements.push_back(rmeas);
						
						//rmeas->gpstow=gpstow; // only needed for Trimble
						//rmeas->gpswn=gpswn;   // ditto
						rmeas->sawtooth=sawtooth; 
						rmeas->timeOffset=rxtimeoffset;
						
						int pchh,pcmm,pcss;
						if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss))){
							rmeas->pchh=pchh;
							rmeas->pcmm=pcmm;
							rmeas->pcss=pcss;
						}
						
						// Save UTC time of measurement
						
						rmeas->tmUTC.tm_sec=msg46ss;
						rmeas->tmUTC.tm_min=msg46mm;
						rmeas->tmUTC.tm_hour=msg46hh;
						rmeas->tmUTC.tm_mday=msg46mday;
						rmeas->tmUTC.tm_mon=msg46mon-1;
						rmeas->tmUTC.tm_year=msg46yyyy-1900;
						rmeas->tmUTC.tm_isdst=0;
						
						// Calculate GPS time of measurement
						time_t tgps = mktime(&(rmeas->tmUTC));
						tgps += (int) rint(dGPSUTC/1000.0); // add leap seconds
						struct tm *tmGPS = gmtime(&tgps);
						rmeas->tmGPS=*tmGPS;
						
						rmeas->tmfracs = tmeasUTC/1000.0 - int(tmeasUTC/1000.0);
						for (unsigned int i=0;i<gps.size();i++)
							gps.at(i)->rm=rmeas;
						rmeas->gps=gps;
						gps.clear(); // don't delete - we only made a shallow copy!
					}
				} // if (gps.size() > 0)
				
				if (((msg.size()-27*2) % 30*2) != 0){
					DBGMSG(debugStream,WARNING,"0xf5 msg wrong size at " << linecount << " " << msg.size());
					currentMsgs=0;
					deleteMeasurements(gps);
					continue;
				}
				
				HexToBin((char *) msg.substr(0,2*sizeof(FP64)).c_str(),sizeof(FP64),(unsigned char *) &tmeasUTC);
				HexToBin((char *) msg.substr(8*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U),(unsigned char *) &weekNum);
				HexToBin((char *) msg.substr(10*2,2*sizeof(FP64)).c_str(),sizeof(FP64),(unsigned char *) &dGPSUTC);
				HexToBin((char *) msg.substr(26*2,2*sizeof(INT8S)).c_str(),sizeof(INT8S),(unsigned char *) &int8sbuf);
				rxtimeoffset = int8sbuf * 1.0E-3;
				
				int nsats=(msg.size()-27*2) / (30*2);
				INT8U svn,signal,flags;
				
				for (int s=0;s<nsats;s++){
					HexToBin((char *) msg.substr((27+s*30)*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),&signal); 
					if (signal &0x02){ // GPS
						HexToBin((char *) msg.substr((28+s*30)*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),&svn); 
						HexToBin((char *) msg.substr((39+s*30)*2,2*sizeof(FP64)).c_str(),sizeof(FP64),(unsigned char *) &fp64buf);
						HexToBin((char *) msg.substr((55+s*30)*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),&flags);
						// FIXME use flags to filter measurements 
						//DBGMSG(debugStream,2,"svn "<< (int) svn << " pr " << dbuf << " flags " << (int) flags);
						gps.push_back(new SVMeasurement(svn,fp64buf*1.0E-3,NULL)); // ReceiverMeasurement not known yet; convert from ms
					}
				}
				
				if (gps.size() >= MAX_CHANNELS){ // too much data - something is wrong
					DBGMSG(debugStream,WARNING,"Too many F5 (raw data) messages at line " << linecount  << " " << currpctime << "(got " << gps.size() << ")");
					currentMsgs=0;
					deleteMeasurements(gps);
					continue;
				}
				
				pctime=currpctime;
				currentMsgs = 0;
				continue;
				
			} // raw data (F5)
			
			if(msgid=="72"){ // Time and frequency parameters (sawtooth correction)
				if (msg.size()==34*2){
					HexToBin((char *) msg.substr(21*2,sizeof(FP64)*2).c_str(),sizeof(FP64),(unsigned char *) &sawtooth);
					sawtooth = sawtooth * 1.0E-9; // convert from ns to seconds
					currentMsgs |= MSG72;
					continue;
				}
				else{
					DBGMSG(debugStream,WARNING,"0x72h msg wrong size at line "<<linecount);
					continue;
				}
			}
			
			if(msgid=="46"){ // Time message
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
				if (msg.size()==51*2){
					INT8U validity;
					HexToBin((char *) msg.substr(50*2,sizeof(INT8U)*2).c_str(),sizeof(INT8U),(unsigned char *) &validity);
					currentMsgs |= MSG74;
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
						HexToBin((char *) msg.substr(0*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(ionoData.a0)); 
						HexToBin((char *) msg.substr(4*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(ionoData.a1));
						HexToBin((char *) msg.substr(8*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(ionoData.a2));
						HexToBin((char *) msg.substr(12*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(ionoData.a3));
						HexToBin((char *) msg.substr(16*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(ionoData.B0)); 
						HexToBin((char *) msg.substr(20*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(ionoData.B1));
						HexToBin((char *) msg.substr(24*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(ionoData.B2));
						HexToBin((char *) msg.substr(28*2,2*sizeof(FP32)).c_str(),sizeof(FP32),(unsigned char *) &(ionoData.B3));
						gotIonoData = true;
					}
				}
				else{
					DBGMSG(debugStream,WARNING,"0x4A msg wrong size at line "<<linecount);
					continue;
				}
			}
			
			if (msgid == "4B"){ // GPS, GLONASS and UTC parameters
				if (msg.size()==42*2){
					INT8U reliability;
					HexToBin((char *) msg.substr(30*2,2*sizeof(INT8U)).c_str(),sizeof(INT8U),&reliability); // GPS reliability only
					if (reliability == 255){
						HexToBin((char *) msg.substr(0*2,2*sizeof(FP64)).c_str(),sizeof(FP64),(unsigned char *) &fp64buf); 
						utcData.A1=fp64buf;
						HexToBin((char *) msg.substr(8*2,2*sizeof(FP64)).c_str(),sizeof(FP64),(unsigned char *) &fp64buf); 
						utcData.A0=fp64buf;
						HexToBin((char *) msg.substr(16*2,2*sizeof(INT32U)).c_str(),sizeof(INT32U),(unsigned char *) &int32ubuf);
						utcData.t_ot = int32ubuf;
						HexToBin((char *) msg.substr(20*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U),(unsigned char *) &(utcData.WN_t));
						HexToBin((char *) msg.substr(22*2,2*sizeof(INT16S)).c_str(),sizeof(INT16S),(unsigned char *) &(utcData.dtlS));
						HexToBin((char *) msg.substr(24*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U),(unsigned char *) &(utcData.WN_LSF));
						HexToBin((char *) msg.substr(26*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U),(unsigned char *) &(utcData.DN));
						HexToBin((char *) msg.substr(28*2,2*sizeof(INT16S)).c_str(),sizeof(INT16S),(unsigned char *) &(utcData.dt_LSF));
						gotUTCdata = setCurrentLeapSeconds(mjd,utcData);
					}
				}
				else{
					DBGMSG(debugStream,WARNING,"0x4B msg wrong size at line "<<linecount);
					continue;
				}
			}
			
			if (msgid=="F7"){ // Extended Ephemeris
				if (msg.size()==138*2){
					INT8U eph;
					HexToBin((char *) msg.substr(0,sizeof(INT8U)*2).c_str(),sizeof(INT8U),(unsigned char *) &eph);
					if (eph == 0x01){
						
						EphemerisData *ed = new EphemerisData;
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
						ed->t_OC=fp64buf*1.0E-3;
						HexToBin((char *) msg.substr(114*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->a_f2));
						ed->a_f2 *= 1000.0;
						HexToBin((char *) msg.substr(118*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->a_f1));
						HexToBin((char *) msg.substr(122*2,2*sizeof(FP32)).c_str(),sizeof(FP32), (unsigned char *) &(ed->a_f0));
						ed->a_f0 *= 1.0E-3;
						HexToBin((char *) msg.substr(126*2,2*sizeof(INT16U)).c_str(), sizeof(INT16U),  (unsigned char *) &int16ubuf);
						ed->SV_accuracy_raw=int16ubuf;
						HexToBin((char *) msg.substr(128*2,2*sizeof(INT16U)).c_str(), sizeof(INT16U), (unsigned char *) &int16ubuf);
						ed->IODE=int16ubuf;
						HexToBin((char *) msg.substr(130*2,2*sizeof(INT16U)).c_str(), sizeof(INT16U), (unsigned char *) &int16ubuf);
						ed->IODC=int16ubuf;
						
						ed->SV_health=0.;
						HexToBin((char *) msg.substr(136*2,2*sizeof(INT16U)).c_str(),sizeof(INT16U), (unsigned char *) &(ed->week_number));
						
						ed->t_ephem=0.0; // FIXME
						
						DBGMSG(debugStream,TRACE,"GPS eph  "<< (int) ed->SVN << " " << ed->t_oe << " " << ed->t_OC);
						addGPSEphemeris(ed);
						continue;
					
					}
					else{
						DBGMSG(debugStream,WARNING,"0xF7 msg (GPS) wrong size at line "<<linecount);
					}
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
	
	interpolateMeasurements(measurements);

	DBGMSG(debugStream,INFO,"done: read " << linecount << " lines");
	DBGMSG(debugStream,INFO,measurements.size() << " measurements read");
	DBGMSG(debugStream,INFO,ephemeris.size() << " ephemeris entries read");
	
	return true;
	
}
