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

#include "Application.h"
#include "Antenna.h"
#include "Debug.h"
#include "GPS.h"
#include "HexBin.h"
#include "ReceiverMeasurement.h"
#include "SVMeasurement.h"
#include "TrimbleResolution.h"

extern ostream *debugStream;
extern Application *app;

#define SLOPPINESS 0.99
#define CLOCKSTEP  0.001
#define MAX_CHANNELS 12 // max channels per constellation


// reverse string by twos
string 
reversestr (
	string instring)
{
	string outstr;
	int len=instring.size();

	for (int i=0;i<len;i+=2){
		outstr[i]=instring[len-i-2];
		outstr[i+1]=instring[len-i-1];
	}

	return outstr;
}

//
//	public
//		

TrimbleResolution::TrimbleResolution(Antenna *ant,string m):Receiver(ant)
{
	modelName = m;
	if (modelName=="Resolution T"){
		model=ResolutionT;
		channels = 12; // pg.39 of manual
	}
	//else if (modelName=="Resolution SMT"){
	//model=ResolutionSMT;
	//channels =12;
	//}
	else if (modelName=="Resolution SMT 360"){
		model=Resolution360;
		channels=32;
	}
	else{
		cerr << "Unknown receiver model: " << modelName << endl;
		cerr << "Assuming Resolution SMT 360 " << endl;
		model=Resolution360;
	}
	manufacturer="Trimble";
	swversion="0.1";
	constellations=GNSSSystem::GPS;
	codes=GNSSSystem::C1;
	// Since we only have 2 systems with old firmware which report the sawtooth
	// correction in units of seconds
	// we'll make new firmware the default
	sawtoothMultiplier=1.0E-9;
}

TrimbleResolution::~TrimbleResolution()
{
}

bool TrimbleResolution::readLog(string fname,int mjd)
{
	DBGMSG(debugStream,1,"reading " << fname);	
	struct stat statbuf;
	
	if ((0 != stat(fname.c_str(),&statbuf))){
		DBGMSG(debugStream,1," unable to stat " << fname);
		return false;
	}
	
	ifstream infile (fname.c_str());
	string line;
	int linecount=0;
	bool useData=true;
	bool got8FAC=false;
	bool gotrxid=false;
	bool gotSWVersion=false;
	
	string msgid,currpctime,pctime,msg,gpstime;
	
	unsigned int gpstow;
	UINT16 gpswn;
	float rxtimeoffset; // single
	float sawtooth;     // single
	unsigned char cbuf;
	
	vector<SVMeasurement *> gpsmeas;
	gotIonoData = false;
	gotUTCdata=false;
	UINT8 fabss,fabmm,fabhh,fabmday,fabmon;
	UINT16 fabyyyy;
	
	int yearOffset; // for version dates
	
	switch (model)
	{
		case ResolutionT:
			yearOffset=1900;
			if (version == "old") sawtoothMultiplier = 1.0;
			break;
		case ResolutionSMT:
			yearOffset=2000;
			break;
		case Resolution360:
			yearOffset=2000;
			break;
	}
	
  if (infile.is_open()){
    while ( getline (infile,line) ){
			linecount++;
			
			if (line.size()==0) continue; // skip empty line
			if ('#' == line.at(0)) continue; // skip comments
			if ('%' == line.at(0)) continue;
			if ('@' == line.at(0)) continue;
			// Format is 
			// message_id time_stamp message
			
			stringstream sstr(line);
			sstr >> msgid >> currpctime >> msg;
			if (sstr.fail()){
				DBGMSG(debugStream,1," bad data at line " << linecount);
				// no need to reset things - not so bad if we miss a message
				continue;
			}
			
			// NB In the documentation for the Resolution 360, the Packet ID is now included as byte 0 so the indexing
			// in the documentation now corresponds to what we were doing anyway (offsetting by one byte)
		
			// The primary time message 8FAB is the first message of interest output each second 
			if(strncmp(msg.c_str(),"8fab",4)==0){
				if (got8FAC && gpsmeas.size()>0 && useData){ // complete data for the current second has been processed
					ReceiverMeasurement *rmeas = new ReceiverMeasurement();
					measurements.push_back(rmeas);
					rmeas->gpstow=gpstow;
					rmeas->gpswn=gpswn;
					rmeas->sawtooth=-sawtooth; // the sawtooth correction is subtracted and our convention is that it will be added
					rmeas->timeOffset=rxtimeoffset;
					
					// 8fab packet is configured for UTC date
					// so save this so that we can calculate GPS date later when the number of leap seconds is known
					
					rmeas->tmUTC.tm_sec=fabss;
					rmeas->tmUTC.tm_min=fabmm;
					rmeas->tmUTC.tm_hour=fabhh;
					rmeas->tmUTC.tm_mday=fabmday;
					rmeas->tmUTC.tm_mon=fabmon-1;
					rmeas->tmUTC.tm_year=fabyyyy-1900;
					
					int pchh,pcmm,pcss;
					if ((3==sscanf(pctime.c_str(),"%d:%d:%d",&pchh,&pcmm,&pcss))){
						rmeas->pchh=pchh;
						rmeas->pcmm=pcmm;
						rmeas->pcss=pcss;
					}
					for (unsigned int i=0;i<gpsmeas.size();i++)
						gpsmeas.at(i)->rm=rmeas;
					rmeas->meas=gpsmeas;
					// correct all code measurements for the receiver time offset here
					for (unsigned int sv=0;sv < gpsmeas.size(); sv++){ // FIXME GPS only
						rmeas->meas[sv]->dbuf3 = rmeas->meas[sv]->meas; 
						rmeas->meas[sv]->meas += rxtimeoffset*1.0E-9;// reported units are ns
						DBGMSG(debugStream,4,(int) fabhh << ":" << (int) fabmm << ":" << (int) fabss << " " <<
							(int) rmeas->meas[sv]->svn << " " <<rmeas->meas[sv]->meas);
					}	
					got8FAC=false;
					gpsmeas.clear();
					DBGMSG(debugStream,3,rmeas->meas.size() << " GPS measurements " << "[PC " << pchh << " " << pcmm << " " << pcss << "]" <<
						"[RX UTC " << (int) fabhh << " " << (int) fabmm << " " << (int) fabss << "]");
				}
				
				//  Check GPS time - it may not be valid yet
				HexToBin((char *) msg.substr(2*9+2,2).c_str(),1,&cbuf);
				if (cbuf & 0x04){ //  discard data for the second if GPS time is not set
					useData=false;
					gpsmeas.clear();
					DBGMSG(debugStream,2,"GPS time is not set yet");
					continue;
				}
				
				// Starting a new second with valid GPS time so get started
				gpsmeas.clear();
				useData=true;
				got8FAC=false;
				pctime=currpctime;
				
				HexToBin((char *) reversestr(msg.substr(1*2+2,2*sizeof(int))).c_str(),sizeof(int),(unsigned char *) &gpstow);
				HexToBin((char *) reversestr(msg.substr(5*2+2,2*sizeof(UINT16))).c_str(),sizeof(UINT16),(unsigned char *) &gpswn);
				HexToBin((char *) reversestr(msg.substr(10*2+2,2*sizeof(UINT8))).c_str(),sizeof(UINT8),(unsigned char *) &fabss);
				HexToBin((char *) reversestr(msg.substr(11*2+2,2*sizeof(UINT8))).c_str(),sizeof(UINT8),(unsigned char *) &fabmm);
				HexToBin((char *) reversestr(msg.substr(12*2+2,2*sizeof(UINT8))).c_str(),sizeof(UINT8),(unsigned char *) &fabhh);
				HexToBin((char *) reversestr(msg.substr(13*2+2,2*sizeof(UINT8))).c_str(),sizeof(UINT8),(unsigned char *) &fabmday);
				HexToBin((char *) reversestr(msg.substr(14*2+2,2*sizeof(UINT8))).c_str(),sizeof(UINT8),(unsigned char *) &fabmon);
				HexToBin((char *) reversestr(msg.substr(15*2+2,2*sizeof(UINT16))).c_str(),sizeof(UINT16),(unsigned char *) &fabyyyy);
			}
			
			if(strncmp(msg.c_str(),"5a",2)==0){ // look for Raw Measurement Report (5A) 
				if (gpsmeas.size() >= MAX_CHANNELS){ // too much data - something is missing 
					useData=false; // flag bad data   
					DBGMSG(debugStream,1,"Too many 5A messages at line " << linecount);
				}
				HexToBin((char *) msg.substr(0+2,2).c_str(),1,&cbuf); // Get SVN
				if (cbuf <= 32){  // FIXME GPS only
					// Check whether we already have data for this SV. If we do
					// something is wrong and we should abort data collection for the 
					// current second 
					unsigned int ichan;
					for (ichan=0;ichan<gpsmeas.size();ichan++){
						if (gpsmeas[ichan]->svn == cbuf)
							break;
					}
					
					if (ichan == gpsmeas.size()){
						float fbuf;
						HexToBin((char *) reversestr(msg.substr(18+2,4*2)).c_str(),4,(unsigned char *) &fbuf);
						gpsmeas.push_back(new SVMeasurement(cbuf,GNSSSystem::GPS,GNSSSystem::C1,fbuf*61.0948*1.0E-9,NULL));// ReceiverMeasurement not known yet
					}
					else{
						useData=false; 
						gpsmeas.clear();
						// typically get unexpected messages because of loss of data caused by polling the receiver
						DBGMSG(debugStream,2," duplicate/unexpected SV at line "<<linecount);
					}
				}
			}
		
			if(strncmp(msg.c_str(),"8fac",4)==0){ // Secondary time message (8FAC) 
				
				HexToBin((char *) reversestr(msg.substr(2*16+2,2*4)).c_str(),4,(unsigned char *) &rxtimeoffset);
				HexToBin((char *) reversestr(msg.substr(2*60+2,2*4)).c_str(),4,(unsigned char *) &sawtooth);
				sawtooth = sawtooth*sawtoothMultiplier; // nb this in seconds/ns according to firmware 
				DBGMSG(debugStream,3," 8FAC bias= " << rxtimeoffset << ",sawtooth= " << sawtooth);
				got8FAC=true;
				continue;
			}
			
			if (!gotrxid){
				if(strncmp(msg.c_str(),"8f41",4)==0){ // hardware version
					if (msg.size() == 18*2){
						unsigned int sn;
						SINT16 snprefix;
						HexToBin((char *) reversestr(msg.substr(1*2+2,2*sizeof(SINT16))).c_str(),sizeof(SINT16),(unsigned char *) &snprefix);
						HexToBin((char *) reversestr(msg.substr(3*2+2,2*sizeof(unsigned int))).c_str(),sizeof(unsigned int),(unsigned char *) &sn);
						gotrxid=true;
						stringstream ss;
						ss << snprefix << "-" << sn;
						serialNumber = ss.str();
						DBGMSG(debugStream,1,"serial number " << serialNumber);
					}
					else{
						DBGMSG(debugStream,1,"Bad 8f41 message size");
					}
					continue;
				}
			}
			
			if (!gotIonoData){
				if(strncmp(msg.c_str(),"580204",6)==0){ // ionosphere
					DBGMSG(debugStream,1,"ionosphere parameters");
					if (msg.size()==45*2){
						
						HexToBin((char *) reversestr(msg.substr(12*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE),(unsigned char *) &(gps.ionoData.a0));
						HexToBin((char *) reversestr(msg.substr(16*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE),(unsigned char *) &(gps.ionoData.a1));
						HexToBin((char *) reversestr(msg.substr(20*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE),(unsigned char *) &(gps.ionoData.a2));
						HexToBin((char *) reversestr(msg.substr(24*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE),(unsigned char *) &(gps.ionoData.a3));
						HexToBin((char *) reversestr(msg.substr(28*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE),(unsigned char *) &(gps.ionoData.B0));
						HexToBin((char *) reversestr(msg.substr(32*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE),(unsigned char *) &(gps.ionoData.B1));
						HexToBin((char *) reversestr(msg.substr(36*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE),(unsigned char *) &(gps.ionoData.B2));
						HexToBin((char *) reversestr(msg.substr(40*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE),(unsigned char *) &(gps.ionoData.B3));
						gotIonoData=true;
						DBGMSG(debugStream,1,"ionosphere parameters: a0=" << gps.ionoData.a0);
						continue;
					}
					else{
						DBGMSG(debugStream,1,"Bad 580204 message size");
					}
				}
			}
			
			if (!gotUTCdata){
				if(strncmp(msg.c_str(),"580205",6)==0){ // UTC
					DBGMSG(debugStream,1,"UTC parameters");
					if (msg.size()==44*2){
							HexToBin((char *) reversestr(msg.substr(17*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE),(unsigned char *) &(gps.UTCdata.A0));
							HexToBin((char *) reversestr(msg.substr(25*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE),(unsigned char *) &(gps.UTCdata.A1));
							HexToBin((char *) reversestr(msg.substr(29*2+2,2*sizeof(SINT16))).c_str(),sizeof(SINT16),(unsigned char *) &(gps.UTCdata.dtlS));
							HexToBin((char *) reversestr(msg.substr(31*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE),(unsigned char *) &(gps.UTCdata.t_ot));
							HexToBin((char *) reversestr(msg.substr(35*2+2,2*sizeof(UINT16))).c_str(),sizeof(UINT16),(unsigned char *) &(gps.UTCdata.WN_t));
							HexToBin((char *) reversestr(msg.substr(37*2+2,2*sizeof(UINT16))).c_str(),sizeof(UINT16),(unsigned char *) &(gps.UTCdata.WN_LSF));
							HexToBin((char *) reversestr(msg.substr(39*2+2,2*sizeof(UINT16))).c_str(),sizeof(UINT16),(unsigned char *) &(gps.UTCdata.DN));
							HexToBin((char *) reversestr(msg.substr(41*2+2,2*sizeof(SINT16))).c_str(),sizeof(SINT16),(unsigned char *) &(gps.UTCdata.dt_LSF));
							DBGMSG(debugStream,1,"UTC parameters: dtLS=" << gps.UTCdata.dtlS << ",dt_LSF=" << gps.UTCdata.dt_LSF);
							gotUTCdata = gps.currentLeapSeconds(mjd,&leapsecs);
							continue;
					}
					else{
						DBGMSG(debugStream,1,"Bad 580205 message size");
					}
				}
			}
			
			if (!gotSWVersion){
				if(strncmp(msg.c_str(),"45",2)==0){  // software version information report packet 
					HexToBin((char *) msg.substr(0+2,2).c_str(),1,&cbuf);//offset by 2 for message id
					appvermajor=cbuf;
					HexToBin((char *) msg.substr(2+2,2).c_str(),1,&cbuf);
					appverminor=cbuf;
					HexToBin((char *) msg.substr(4+2,2).c_str(),1,&cbuf);
					appmonth=cbuf;
					HexToBin((char *) msg.substr(6+2,2).c_str(),1,&cbuf);
					appday=cbuf;
					HexToBin((char *) msg.substr(8+2,2).c_str(),1,&cbuf);
					appyear=cbuf+1900;
					HexToBin((char *) msg.substr(10+2,2).c_str(),1,&cbuf);
					corevermajor=cbuf;
					HexToBin((char *) msg.substr(12+2,2).c_str(),1,&cbuf);
					coreverminor=cbuf;
					HexToBin((char *) msg.substr(14+2,2).c_str(),1,&cbuf);
					coremonth=cbuf;
					HexToBin((char *) msg.substr(16+2,2).c_str(),1,&cbuf);
					coreday=cbuf;
					HexToBin((char *) msg.substr(18+2,2).c_str(),1,&cbuf);
					coreyear=cbuf+yearOffset;
					stringstream ss;
					ss << appvermajor << "." << appverminor;
					ss << " " << appyear << "-" << appmonth << "-" << appday;
					version2=ss.str();
					ss.str("");ss.clear();
					ss << corevermajor << "." << coreverminor;
					ss << " " << coreyear << "-" << coremonth<< "-" << coreday;
					version1=ss.str(); // report this as principal version info
					DBGMSG(debugStream,1,version1);
					DBGMSG(debugStream,1,version2);
					continue;
				}
			}
			
			if(strncmp(msg.c_str(),"580206",6)==0){ // ephemeris
				if (msg.size()==172*2){
					GPS::EphemerisData *ed = new GPS::EphemerisData;
					HexToBin((char *) reversestr(msg.substr(4*2+2,2*sizeof(UINT8))).c_str(), sizeof(UINT8),  (unsigned char *) &(ed->SVN));
					HexToBin((char *) reversestr(msg.substr(5*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->t_ephem));
					HexToBin((char *) reversestr(msg.substr(9*2+2,2*sizeof(UINT16))).c_str(),sizeof(UINT16), (unsigned char *) &(ed->week_number));
					HexToBin((char *) reversestr(msg.substr(13*2+2,2*sizeof(UINT8))).c_str(), sizeof(UINT8),  (unsigned char *) &(ed->SV_accuracy_raw));
					HexToBin((char *) reversestr(msg.substr(14*2+2,2*sizeof(UINT8))).c_str(), sizeof(UINT8),  (unsigned char *) &(ed->SV_health));
					HexToBin((char *) reversestr(msg.substr(15*2+2,2*sizeof(UINT16))).c_str(),sizeof(UINT16), (unsigned char *) &(ed-> IODC));
					HexToBin((char *) reversestr(msg.substr(17*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->t_GD));
					HexToBin((char *) reversestr(msg.substr(21*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->t_OC));
					HexToBin((char *) reversestr(msg.substr(25*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->a_f2));
					HexToBin((char *) reversestr(msg.substr(29*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->a_f1));
					HexToBin((char *) reversestr(msg.substr(33*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->a_f0));
					HexToBin((char *) reversestr(msg.substr(37*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->SV_accuracy));
					HexToBin((char *) reversestr(msg.substr(41*2+2,2*sizeof(UINT8))).c_str(), sizeof(UINT8),  (unsigned char *) &(ed->IODE));
					HexToBin((char *) reversestr(msg.substr(43*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->C_rs));
					HexToBin((char *) reversestr(msg.substr(47*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->delta_N));
					HexToBin((char *) reversestr(msg.substr(51*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->M_0));
					HexToBin((char *) reversestr(msg.substr(59*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->C_uc));
					HexToBin((char *) reversestr(msg.substr(63*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->e));
					HexToBin((char *) reversestr(msg.substr(71*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->C_us));
					HexToBin((char *) reversestr(msg.substr(75*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->sqrtA));
					HexToBin((char *) reversestr(msg.substr(83*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->t_oe));
					HexToBin((char *) reversestr(msg.substr(87*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->C_ic));
					HexToBin((char *) reversestr(msg.substr(91*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->OMEGA_0));
					HexToBin((char *) reversestr(msg.substr(99*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->C_is));
					HexToBin((char *) reversestr(msg.substr(103*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->i_0));
					HexToBin((char *) reversestr(msg.substr(111*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->C_rc));
					HexToBin((char *) reversestr(msg.substr(115*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->OMEGA));
					HexToBin((char *) reversestr(msg.substr(123*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->OMEGADOT));
					HexToBin((char *) reversestr(msg.substr(127*2+2,2*sizeof(SINGLE))).c_str(),sizeof(SINGLE), (unsigned char *) &(ed->IDOT));
					HexToBin((char *) reversestr(msg.substr(131*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->Axis));
					HexToBin((char *) reversestr(msg.substr(139*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->n));
					HexToBin((char *) reversestr(msg.substr(147*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->r1me2));
					HexToBin((char *) reversestr(msg.substr(155*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->OMEGA_N));
					HexToBin((char *) reversestr(msg.substr(163*2+2,2*sizeof(DOUBLE))).c_str(),sizeof(DOUBLE), (unsigned char *) &(ed->ODOT_n));
					DBGMSG(debugStream,3,"Ephemeris: SVN="<< (unsigned int) ed->SVN);
					gps.addEphemeris(ed);
					continue;
				}
				else{
					DBGMSG(debugStream,1,"Bad 580206 message size");
				}
			} // ephemeris
			
		}
	}
	else{
		DBGMSG(debugStream,1," unable to open " << fname);
		return false;
	}
	infile.close();
	
	if (!gotIonoData){
		app->logMessage("failed to find ionosphere parameters - no 580204 messages");
		return false;
	}
	
	if (!gotUTCdata){
		app->logMessage("failed to find UTC parameters - no 580205 messages");
		return false;
	}
	
	// Post-load cleanups
	// Calculate GPS time of measurements, now that the number of leap seconds is known
	for (unsigned int i=0;i<measurements.size();i++){
		
		time_t tgps = mktime(&(measurements[i]->tmUTC));
		tgps += leapsecs;
		struct tm *tmGPS = gmtime(&tgps);
		measurements[i]->tmGPS=*tmGPS;
		//printf("%02d:%02d:%02d\n",measurements[i]->tmGPS.tm_hour,measurements[i]->tmGPS.tm_min,measurements[i]->tmGPS.tm_sec);
	}
	
	// Fix 1 ms ambiguities/steps in the pseudo range
	// Do this initially and then every time a step is detected
	
	for (int prn=1;prn<=32;prn++){
		unsigned int lasttow=99999999,currtow=99999999;
		double lastmeas,currmeas;
		double corr=0.0;
		bool first=true;
		bool ok=false;
		for (unsigned int i=0;i<measurements.size();i++){
			for (unsigned int m=0;m < measurements[i]->meas.size();m++){
				if (prn==measurements[i]->meas[m]->svn){
					lasttow=currtow;
					lastmeas=currmeas;
					currmeas=measurements[i]->meas[m]->meas;
					currtow=measurements[i]->gpstow;
					
					DBGMSG(debugStream,4,prn << " " << currtow << " " << currmeas << " ");
					if (first){
						first=false;
						ok = resolveMsAmbiguity(measurements[i],measurements[i]->meas[m],&corr);
					}
					else if (currtow > lasttow){ // FIXME better test of gaps
						if (fabs(currmeas-lastmeas) > CLOCKSTEP*SLOPPINESS){
							DBGMSG(debugStream,3,"first/step " << prn << " " << lasttow << "," << lastmeas << "->" << currtow << "," << currmeas);
							ok = resolveMsAmbiguity(measurements[i],measurements[i]->meas[m],&corr);
						}
					}
					if (ok) measurements[i]->meas[m]->meas += corr;
					break;
				}
				
			}
		}
	}

	interpolateMeasurements();
	
	ostringstream ss;
	ss << measurements.size() << " receiver measurements read";
	app->logMessage(ss.str());
	DBGMSG(debugStream,1,gps.ephemeris.size() << " GPS ephemeris entries read");
	
	return true;
}
	



		
		
