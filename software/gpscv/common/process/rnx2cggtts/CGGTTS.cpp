//
//
// The MIT License (MIT)
//
// Copyright (c) 2023  Michael J. Wouters
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

#include <cmath>
#include <cstring>

#include <algorithm>
#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>


#include "Antenna.h"
#include "Application.h"
#include "Debug.h"
#include "CGGTTS.h"
#include "GNSSSystem.h"
#include "Measurements.h"
#include "Receiver.h"
#include "RINEXFile.h"
#include "Utility.h"

extern Application *app;
extern std::ostream *debugStream;

#define NTRACKS 89      // maximum number of tracks in a day
#define NTRACKPOINTS 26 // 30 s sampling
#define OBSINTERVAL 30  // 30 s

#define MAXSV   37 // GPS 1-32, BDS 1-37, GALILEO 1-36 
// indices into svtrk
#define INDX_OBSV1    0  
#define INDX_OBSV2    1
#define INDX_OBSV3    2
#define INDX_TOD      3 // decimal INDX_TOD, need this in case timestamps are not on the second
#define INDX_MJD      4

#define CLIGHT 299792458.0

static unsigned int str2ToCode(std::string s)
{
	int c=0;
	if (s== "C1")
		c=GNSSSystem::C1C;
	else if (s == "P1")
		c=GNSSSystem::C1P;
	else if (s == "E1")
		c=GNSSSystem::C1C; // FIXME what about C1B?
	else if (s == "B1")
		c=GNSSSystem::C2I;
	else if (s == "C2") // C2L ?? C2M
		c=GNSSSystem::C2C;
	else if (s == "P2")
		c=GNSSSystem::C2P;
	//else if (c1 == "E5")
	//	c=E5a;
	else if (s== "B2")
		c=GNSSSystem::C7I;
	return c;
}

//
//	public
//



CGGTTS::CGGTTS()
{
	init();
}
	
bool CGGTTS::write(Measurements *meas1,GNSSSystem *gnss1,int leapsecs1, std::string fname,int mjd,int startTime,int stopTime)
{
	FILE *fout;
	if (!(fout = std::fopen(fname.c_str(),"w"))){
		std::cerr << "Unable to open " << fname << std::endl;
		return false;
	}
	
	app->logMessage("generating CGGTTS file " + fname);
	
	double measDelay = intDly + cabDly - refDly; // the measurement system delay to be subtracted from REFSV and REFSYS
	//DBGMSG(debugStream,1,"P3 = " << (isP3 ? "yes":"no"));
	
	int lowElevationCnt=0; // a few diagnostics
	int highDSGCnt=0;
	int shortTrackCnt=0;
	int goodTrackCnt=0;
	int ephemerisMisses=0;
	int badHealth=0;
	int pseudoRangeFailures=0;
	int badMeasurementCnt=0;
	
	// Constellation/code identifiers as per V2E
	
	double aij=0.0; // frequency weight for pseudoranges, as per CGGTTS v2E
	
	std::string  GNSSsys;
	
	switch (constellation){
		case GNSSSystem::BEIDOU:
			GNSSsys="C"; break;
		case GNSSSystem::GALILEO:
			GNSSsys="E"; break;
		case GNSSSystem::GLONASS:
			GNSSsys="R"; break;
		case GNSSSystem::GPS:
			GNSSsys="G"; 
			aij = 77.0*77.0/(77.0*77.0-60*60);break;
			
		default:break;
	}
	
	
	std::string FRCcode="";
	unsigned int code1=code,code2=0;
	int indxm1c1=-1;
	
	switch (code){ 
		case GNSSSystem::C1C:
			if (constellation == GNSSSystem::GALILEO){
				FRCcode=" E1";
				code1Str="E1";
			}
			else{
				FRCcode="L1C";
				code1Str="C1"; // identifies the delay
			}
			indxm1c1 = meas1->colIndexFromCode("C1C");
			break;
		case GNSSSystem::C1B:
			FRCcode="E1";
			code1Str="E1";
			break;
		case GNSSSystem::C1P:
			FRCcode="L1P";
			code1Str="P1";
			break;
		case GNSSSystem::C2P:
			FRCcode="L2P";
			code1Str="P2";
			break;
		case GNSSSystem::C2I:
			FRCcode="B1i";
			code1Str="B1";
			break; // RINEX 3.02
		// dual frequency combinations
		case GNSSSystem::C1P | GNSSSystem::C2P:
			code1 = GNSSSystem::C1P;
			code2 = GNSSSystem::C2P;
			code1Str = "P1";
			code2Str = "P2";
			FRCcode="L3P";break;
		case GNSSSystem::C1C | GNSSSystem::C2P:
			code1 = GNSSSystem::C1C;
			code2 = GNSSSystem::C2P;
			code1Str = "C1";
			code2Str = "P2";
			FRCcode="L3P";break;
		case GNSSSystem::C1C | GNSSSystem::C2C:
			code1 = GNSSSystem::C1C;
			code2 = GNSSSystem::C2C;
			code1Str = "C1";
			code2Str = "C2";
			FRCcode="L3P";break;
		default:break;
	}
	
	writeHeader(fout);
	
	// Generate the observation schedule as per DefraignePetit2015 pg3

	int schedule[NTRACKS+1];
	int ntracks=NTRACKS;
	// There will be a 28 minute gap between two observations (32-4 mins)
	// which means that you can't just find the first and then add n*16 minutes
	// Track start times are all UTC, of course
	for (int i=0,mins=2; i<NTRACKS; i++,mins+=16){
		schedule[i]=mins-4*(mjd-50722);
		if (schedule[i] < 0){ // always negative in practice anyway 
			int ndays = abs(schedule[i]/1436) + 1;
			schedule[i] += ndays*1436;
		}
	}
	
	// The schedule is not in ascending order so fix this 
	std::sort(schedule,schedule+NTRACKS); // don't include the last element, which may or may not be used
	
	// Fixup - one more track possibly at the end of the day
	// Will need the next day's data to use this properly though
	if ((schedule[NTRACKS-1]%60) < 43){
		schedule[NTRACKS]=schedule[NTRACKS-1]+16;
		ntracks++;
	}
	
	// Data structures for accumulating data at each track time
	// Use a fixed array so that we can use the index as a hash for the SVN. Memory is cheap
	// and svtrk is only 780 points long anyway
	double svtrk[MAXSV+1][NTRACKPOINTS][INDX_MJD+1]; 
	unsigned int svObsCount[MAXSV+1];
	
	int leapOffset1 = leapsecs1/30.0; // measurements are assumed to be in GPS time so we'll need to shift the lookup index back by this
	int indxMJD = meas1->colMJD();
	int indxTOD = meas1->colTOD();
	
	//ntracks = 2; // FIXME
	int prcount = 0; // count of pseudorange estimations 
	
	for (int i=0;i<ntracks;i++){
	
		int trackStart = schedule[i]*60; // in seconds since start of UTC day
		int trackStop =  schedule[i]*60 + (NTRACKPOINTS-1)*OBSINTERVAL; // note the time of the last point 
		if (trackStop >= 86400) trackStop=86400-1; // FIXME we can get more from the next day ...
		DBGMSG(debugStream,INFO,"Track " << i << " start " << trackStart << " stop " << trackStop);
		
		// Now window it
		if (trackStart < startTime || trackStart > stopTime) continue; // svtrk empty so no need to clear
		
		for (int s=1;s<=MAXSV;s++){
			svObsCount[s]=0;
			for (int t=0;t<NTRACKPOINTS;t++)
				for (int o=INDX_OBSV1;o<= INDX_MJD;o++)
					svtrk[s][t][o] = NAN; 
		}
		
		// CASE 1: single code + MDIO
		int iTrackStart = trackStart/30;
		int iTrackStop  = trackStop/30;
		for (int m=iTrackStart;m<=iTrackStop;m++){
			int mGPS = m - leapOffset1; // at worst, we miss one measurement since we compensate when leapSecs > 30 (which may not happen for a very long time)
			for (int sv = 1; sv <= meas1->maxSVN;sv++){
				if (!isnan(meas1->meas[mGPS][sv][indxm1c1])){ 
					//DBGMSG(debugStream,INFO,sv << " " << meas1->meas[mGPS][sv][indxMJD] << " " << meas1->meas[mGPS][sv][indxTOD] << " " << meas1->meas[mGPS][sv][indxm1c1]);
					svtrk[sv][svObsCount[sv]][INDX_OBSV1]= meas1->meas[mGPS][sv][indxm1c1];
					svtrk[sv][svObsCount[sv]][INDX_TOD]= meas1->meas[mGPS][sv][indxTOD]; 
					svtrk[sv][svObsCount[sv]][INDX_MJD] = meas1->meas[mGPS][sv][indxMJD];
					svObsCount[sv] +=1; // since the value of this is bounded by iTrackStop - iTrackStart, no need to test it
				}
			}
		}
		
		
		//for (unsigned int sv=1;sv<=MAXSV;sv++){
		for (unsigned int sv=1;sv<=1;sv++){
			if (0 == svObsCount[sv]) continue;
			
			int npts=0; // count of number of points for the linear fit
			int ioe;    // issue of ephemeris
			
			//DBGMSG(debugStream,INFO,sv << " " << svObsCount[sv]);
			
			int hh = schedule[i] / 60; // schedule hour   (UTC)
			int mm = schedule[i] % 60; // schedule minute (UTC)
			
			double refsv[26],refsys[26],mdtr[26],mdio[26],msio[26],tutc[26],svaz[26],svel[26]; //buffers for the data for the linear fits
			
			Ephemeris *ed=NULL;
			
			for (unsigned int tt=0;tt<svObsCount[sv];tt++){
				int fwn;
				double tow;
				Utility::MJDToGPSTime(svtrk[sv][tt][INDX_MJD],svtrk[sv][tt][INDX_TOD],&fwn,&tow);
				//DBGMSG(debugStream,INFO,svtrk[sv][tt][INDX_MJD] << " " << svtrk[sv][tt][INDX_TOD] << " " << fwn << " " << tow);
		
				// Now window it
				
				if (ed==NULL) // use only one ephemeris for each track
					ed = gnss1->nearestEphemeris(sv,tow,maxURA);
				
				if (NULL == ed){
					ephemerisMisses++;
				}
				else{
					if (!(ed->healthy())){
						badHealth++;
						//DBGMSG(debugStream,INFO,"Unhealthy SV = " << sv);
						continue;
					}
					//ed->dump();
				}
				
				double refsyscorr,refsvcorr,iono,tropo,az,el,pr;
				
				pr = svtrk[sv][tt][INDX_OBSV1]/CLIGHT;
				//DBGMSG(debugStream,INFO,tow << " " << pr << " " << antenna->x << " " << antenna->y  << " " << antenna->z << " " << ed->iod());
				prcount++;
				if (gnss1->getPseudorangeCorrections(tow,pr,antenna,ed,code1,&refsyscorr,&refsvcorr,&iono,&tropo,&az,&el,&ioe)){
					DBGMSG(debugStream,INFO,tow << " " << refsyscorr);
				}
				else{
					pseudoRangeFailures++;
				}
			} // for (unsigned int tt=0;tt<svObsCount[sv];tt++)
		} // for (unsigned int sv=1;sv<=MAXSV;sv++)
		
	} // for (int i=0;i<ntracks;i++)
	
	std::fclose(fout);
	
	app->logMessage("Ephemeris search misses: " + boost::lexical_cast<std::string>(ephemerisMisses));
	app->logMessage("Bad health: " + boost::lexical_cast<std::string>(badHealth) );
	app->logMessage("Pseudorange calculation failures: " + boost::lexical_cast<std::string>(pseudoRangeFailures) +
		"/" + boost::lexical_cast<std::string>(prcount)); 
	app->logMessage("Bad measurements: " + boost::lexical_cast<std::string>(badMeasurementCnt) );
	
	app->logMessage(boost::lexical_cast<std::string>(goodTrackCnt) + " good tracks");
	app->logMessage(boost::lexical_cast<std::string>(lowElevationCnt) + " low elevation tracks");
	app->logMessage(boost::lexical_cast<std::string>(highDSGCnt) + " high DSG tracks");
	app->logMessage(boost::lexical_cast<std::string>(shortTrackCnt) + " short tracks");
	
	return true;
}

//
// private
//		

void CGGTTS::init()
{
	antenna = NULL;
	receiver = NULL;
	
	revDateYYYY = 2023;
	revDateMM = 1;
	revDateDD = 1;
	ref="UTC(XLAB)";
	lab="XLAB";
	comment="";
	calID="UNCALIBRATED";
	intDly=intDly2=cabDly=refDly=0.0;
	delayKind = INTDLY;
	minTrackLength = 390;
	minElevation = 10.0;
	maxDSG = 100.0;
	maxURA = 3.0; // as reported by receivers, typically 2.0 m, with a few at 2.8 m
	
	useMSIO = false;
}


unsigned int CGGTTS::strToCode(std::string str, bool *isP3)
{
	// Convert CGGTTS code string (usually from the configuration file) to RINEX observation code(s)
	// Dual frequency combinations are of the form 'code1+code2'
	// Valid formats are 
	// (1) CGGTTS 2 letter codes
	// (2) RINEX  3 letter codes
	// but not mixed!
	// so valid string lengths are 2 and 5 (CGGTTS)  or 3 and 7 (RINEX)
	unsigned int c=0;
	if (str.length()==2){
		c = str2ToCode(str);
	}
	else if (str.length()==5){ // dual frequency, specified using 2 letter codes
		unsigned int c1=str2ToCode(str.substr(0,2));
		unsigned int c2=str2ToCode(str.substr(3,2));
		c = c1 | c2;
		*isP3=true;
	} 
	else if (str.length()==3){ // single frequency, specified using 3 letter RINEX code
		c=GNSSSystem::strToObservationCode(str,RINEXFile::V3);
	}
	else if (str.length()==7){ // dual frequency, specified using 3 letter RINEX codes
		unsigned int c1=GNSSSystem::strToObservationCode(str.substr(0,3),RINEXFile::V3);
		unsigned int c2=GNSSSystem::strToObservationCode(str.substr(4,3),RINEXFile::V3);
		c = c1 | c2;
		*isP3=true;
	}
	else{
		c= 0;
		*isP3=false;
	}
	
	return c;
}


void CGGTTS::writeHeader(FILE *fout)
{
#define MAXCHARS 128
	int cksum=0;
	char buf[MAXCHARS+1];

	std::strncpy(buf,"CGGTTS     GENERIC DATA FORMAT VERSION = 2E",MAXCHARS);
	
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"REV DATE = %4d-%02d-%02d",revDateYYYY,revDateMM,revDateDD); 
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"RCVR = %s %s %s %4d %s,v%s",receiver->manufacturer.c_str(),receiver->model.c_str(),receiver->serialNumber.c_str(),
		receiver->commissionYYYY,APP_NAME, APP_VERSION);
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"CH = %02d",receiver->nChannels); 
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	if (useMSIO)
		std::snprintf(buf,MAXCHARS,"IMS =%s %s %s %4d %s,v%s",receiver->manufacturer.c_str(),receiver->model.c_str(),receiver->serialNumber.c_str(),
			receiver->commissionYYYY,APP_NAME, APP_VERSION);
	else
		std::snprintf(buf,MAXCHARS,"IMS = 99999");
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"LAB = %s",lab.c_str()); 
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"X = %+.3f m",antenna->x);
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"Y = %+.3f m",antenna->y);
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"Z = %+.3f m",antenna->z);
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"FRAME = %s",antenna->frame.c_str());
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	if (comment == "") 
		comment="NO COMMENT";
	
	std::snprintf(buf,MAXCHARS,"COMMENTS = %s",comment.c_str());
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::string cons;
	switch (constellation){
		case GNSSSystem::BEIDOU:cons="BDS";break;
		case GNSSSystem::GALILEO:cons="GAL";break;
		case GNSSSystem::GLONASS:cons="GLO";break;
		case GNSSSystem::GPS:cons="GPS";break;
	}
	std::string dly;
	switch (delayKind){
		case INTDLY:dly="INT";break;
		case SYSDLY:dly="SYS";break;
		case TOTDLY:dly="TOT";break;
	}
	if (isP3) // FIXME presuming that the logical thing happens here ...
		std::snprintf(buf,MAXCHARS,"%s DLY = %.1f ns (%s %s),%.1f ns (%s %s)      CAL_ID = %s",dly.c_str(),
			intDly,cons.c_str(),code1Str.c_str(),
			intDly2,cons.c_str(),code2Str.c_str(),calID.c_str());
	else
		std::snprintf(buf,MAXCHARS,"%s DLY = %.1f ns (%s %s)     CAL_ID = %s",dly.c_str(),intDly,cons.c_str(),code1Str.c_str(),calID.c_str());

	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	if (delayKind == INTDLY){
		std::snprintf(buf,MAXCHARS,"CAB DLY = %.1f ns",cabDly);
		cksum += checkSum(buf);
		std::fprintf(fout,"%s\n",buf);
	}
	
	if (delayKind != TOTDLY){
		std::snprintf(buf,MAXCHARS,"REF DLY = %.1f ns",refDly);
		cksum += checkSum(buf);
		std::fprintf(fout,"%s\n",buf);
	}
	
	std::snprintf(buf,MAXCHARS,"REF = %s",ref.c_str());
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"CKSUM = ");
	cksum += checkSum(buf);
	std::fprintf(fout,"%s%02X\n",buf,cksum % 256);
	
	std::fprintf(fout,"\n");
	
	if (useMSIO){
		std::fprintf(fout,"SAT CL  MJD  STTIME TRKL ELV AZTH   REFSV      SRSV     REFSYS    SRSYS  DSG IOE MDTR SMDT MDIO SMDI MSIO SMSI ISG FR HC FRC CK\n");
		std::fprintf(fout,"             hhmmss  s  .1dg .1dg    .1ns     .1ps/s     .1ns    .1ps/s .1ns     .1ns.1ps/s.1ns.1ps/s.1ns.1ps/s.1ns            \n");
	}
	else{
		std::fprintf(fout,"SAT CL  MJD  STTIME TRKL ELV AZTH   REFSV      SRSV     REFSYS    SRSYS  DSG IOE MDTR SMDT MDIO SMDI FR HC FRC CK\n");
		std::fprintf(fout,"             hhmmss  s  .1dg .1dg    .1ns     .1ps/s     .1ns    .1ps/s .1ns     .1ns.1ps/s.1ns.1ps/s            \n");
	}

	std::fflush(fout);
	
#undef MAXCHARS	
}
	

int CGGTTS::checkSum(char *l)
{
	int cksum =0;
	for (unsigned int i=0;i<strlen(l);i++)
		cksum += (int) l[i];
	return cksum;
}




