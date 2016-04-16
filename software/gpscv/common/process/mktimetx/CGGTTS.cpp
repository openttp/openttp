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

#include <cmath>
#include <cstdio>
#include <cstring>

#include <iostream>
#include <algorithm>

#include "Antenna.h"
#include "CGGTTS.h"
#include "Counter.h"
#include "CounterMeasurement.h"
#include "Debug.h"
#include "GPS.h"
#include "MakeTimeTransferFile.h"
#include "MeasurementPair.h"
#include "Receiver.h"
#include "ReceiverMeasurement.h"
#include "Utility.h"

extern MakeTimeTransferFile *app;
extern ostream *debugStream;

#define NTRACKS 89
#define MAXSV   32 // per constellation 

//
//	Public members
//

CGGTTS::CGGTTS(Antenna *a,Counter *c,Receiver *r)
{
	ant=a;
	cntr=c;
	rx=r;
	init();
}

bool CGGTTS::writeObservationFile(string fname,int mjd,MeasurementPair **mpairs)
{
	FILE *fout;
	if (!(fout = fopen(fname.c_str(),"w"))){
		cerr << "Unable to open " << fname << endl;
		return false;
	}
	
	double measDelay = rx->ppsOffset + intDly + cabDly - refDly; // the measurement system delay to be subtracted from REFSV and REFSYS
	
	writeHeader(fout);
	
	// Generate the observation schedule as per DefraignePetit2015 pg3

	int schedule[NTRACKS+1];
	int ntracks=NTRACKS;
	// There will be a 28 minute gap between two observations (32-4 mins)
	// which means that you can't just find the first and then add n*16 minutes
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

	// Constellation/code identifiers as per V2E
	
	string GNSSconst;
	string GNSScode;
	
	switch (constellation){
		case Receiver::GPS:
			GNSSconst="G";
			switch (code){
				case Receiver::C1:GNSScode="L1C";break;
				case Receiver::P1:GNSScode="L1P";break;
				case Receiver::P2:GNSScode="L2P";break;
				default:break;
			}
			break;// FIXME GPS only
		//case Receiver::GLONASS:GNSScode="R";break;
		default:break;
	}
	
	// Use a fixed array of vectors so that we can use the index as a hash for the SVN. Memory is cheap
	vector<SVMeasurement *> svtrk[MAXSV+1];
	
	int lowElevationCnt=0; // a few diagnostics
	int highDSGCnt=0;
	int shortTrackCnt=0;
	
	for (int i=0;i<ntracks;i++){
		int trackStart = schedule[i]*60;
		int trackStop =  schedule[i]*60+780-1;
		if (trackStop >= 86400) trackStop=86400-1;
		// Matched measurement pairs can be looked up without a search since the index is TOD
		// FIXME Sanity check post-match would be prudent
		for (int m=trackStart;m<=trackStop;m++){
			
			if ((mpairs[m]->flags==0x03)){
				ReceiverMeasurement *rm = mpairs[m]->rm;
				switch (constellation){
					case Receiver::GPS:
						switch (code){
							case Receiver::C1:{
								for (unsigned int sv=0;sv<rm->gps.size();sv++) 
									svtrk[rm->gps.at(sv)->svn].push_back(rm->gps.at(sv));
								break;
							}
							case Receiver::P1:{
								for (unsigned int sv=0;sv<rm->gpsP1.size();sv++) 
									svtrk[rm->gpsP1.at(sv)->svn].push_back(rm->gpsP1.at(sv));
								break;
							}
							case Receiver::P2:{
								for (unsigned int sv=0;sv<rm->gpsP2.size();sv++) 
									svtrk[rm->gpsP2.at(sv)->svn].push_back(rm->gpsP2.at(sv));
								break;
							}
							default:break;
						}// switch (code) 
					break;
					
				}// switch constellation
			} // if
		}
		
		
		int hh = schedule[i] / 60;
		int mm = schedule[i] % 60;
		
		double refsv[52]; //use arrays which can store the quadratic fits as well
		double refsys[52];
		double mdtr[52];
		double mdio[52];
		double tutc[52];
		double svaz[52];
		double svel[52];
		
		int fitInterval=30; // length of fitting interval 
		if (quadFits) fitInterval=15;
		
		for (unsigned int sv=1;sv<=MAXSV;sv++){
			if (svtrk[sv].size() > 0){
				
				if (quadFits){
					// FIXME if I ever implement this, the sawtooth-corrected pps measurements get folded in here to maximize smoothing?
				}
				
				int npts=0;
				int tsearch=trackStart;
				int t=0;
				int ioe;
			  while (t<svtrk[sv].size()){
					ReceiverMeasurement *rxmt = svtrk[sv].at(t)->rm;
					int tmeas=rint(rxmt->tmUTC.tm_sec + rxmt->tmUTC.tm_min*60+ rxmt->tmUTC.tm_hour*3600+rxmt->tmfracs);
					if (tmeas==tsearch){
						double refsyscorr,refsvcorr,iono,tropo,az,el,refpps;
						// FIXME MDIO needs to change for L2
						if (GPS::getPseudorangeCorrections(rx,rxmt,svtrk[sv].at(t),ant,&refsyscorr,&refsvcorr,&iono,&tropo,&az,&el,&ioe)){
							tutc[npts]=tmeas;
							svaz[npts]=az;
							svel[npts]=el;
							mdtr[npts]=tropo;
							mdio[npts]=iono;
							refpps=(rxmt->cm->rdg + rxmt->sawtooth)*1.0E9;
							refsv[npts]  = svtrk[sv].at(t)->meas*1.0E9 + refsvcorr  - iono - tropo + refpps;
							refsys[npts] = svtrk[sv].at(t)->meas*1.0E9 + refsyscorr - iono - tropo + refpps;
							npts++;
						}
						tsearch += 30;
						t++;
					}
					else if (tmeas > tsearch){
						tsearch += 30;
						// don't increment t because this measurement must be re-tested	
					}
					else{
						t++;
					}
				}
				
				if (npts*fitInterval >= minTrackLength){
					double tc=(trackStart+trackStop)/2.0; // FIXME may need to add MJD to allow rollovers
					
					double aztc,azc,azm,azresid;
					Utility::linearFit(tutc,svaz,npts,tc,&aztc,&azc,&azm,&azresid);
					aztc=rint(aztc*10);
					
					double eltc,elc,elm,elresid;
					Utility::linearFit(tutc,svel,npts,tc,&eltc,&elc,&elm,&elresid);
					eltc=rint(eltc*10);
					
					double mdtrtc,mdtrc,mdtrm,mdtrresid;
					Utility::linearFit(tutc,mdtr,npts,tc,&mdtrtc,&mdtrc,&mdtrm,&mdtrresid);
					mdtrtc=rint(mdtrtc*10);
					mdtrm=rint(mdtrm*10000);
					
					double refsvtc,refsvc,refsvm,refsvresid;
					Utility::linearFit(tutc,refsv,npts,tc,&refsvtc,&refsvc,&refsvm,&refsvresid);
					refsvtc=rint((refsvtc-measDelay)*10); // apply total measurement system delay
					refsvm=rint(refsvm*10000);
					
					double refsystc,refsysc,refsysm,refsysresid;
					Utility::linearFit(tutc,refsys,npts,tc,&refsystc,&refsysc,&refsysm,&refsysresid);
					refsystc=rint((refsystc-measDelay)*10); // apply total measurement system delay
					refsysm=rint(refsysm*10000);
					refsysresid=rint(refsysresid*10);
					
					double mdiotc,mdioc,mdiom,mdioresid;
					Utility::linearFit(tutc,mdio,npts,tc,&mdiotc,&mdtrc,&mdiom,&mdioresid);
					mdiotc=rint(mdiotc*10);
					mdiom=rint(mdiom*10000);
					
					// Ready to output
					if (eltc >= minElevation*10 && refsysresid <= maxDSG*10){ 
						char sout[155]; // V2E
						switch (ver){
							case V1:
								snprintf(sout,128," %02i %2s %5i %02i%02i00 %4i %3i %4i %11i %6i %11i %6i %4i %3i %4i %4i %4i %4i ",sv,"FF",mjd,hh,mm,
												npts*fitInterval,(int) eltc,(int) aztc, (int) refsvtc ,(int) refsvm,(int)refsystc,(int) refsysm,(int) refsysresid,
												ioe,(int) mdtrtc, (int) mdtrm, (int) mdiotc, (int) mdiom);
								fprintf(fout,"%s%02X\n",sout,checkSum(sout) % 256);
								break;
							case V2E:
								snprintf(sout,154,"%s%02i %2s %5i %02i%02i00 %4i %3i %4i %11i %6i %11i %6i %4i %3i %4i %4i %4i %4i %2i %2i %3s ",GNSSconst.c_str(),sv,"FF",mjd,hh,mm,
												npts*fitInterval,(int) eltc,(int) aztc, (int) refsvtc,(int) refsvm,(int)refsystc,(int) refsysm,(int) refsysresid,
												ioe,(int) mdtrtc, (int) mdtrm, (int) mdiotc, (int) mdiom,0,0,GNSScode.c_str());
								fprintf(fout,"%s%02X\n",sout,checkSum(sout) % 256); // FIXME
								break;
						} // switch
					} // if (eltc >= minElevation*10 && refsysresid <= maxDSG*10)
					else{
						if (eltc < minElevation*10) lowElevationCnt++;
						if (refsysresid > maxDSG*10) highDSGCnt++;
					}
				} // if (npts*fitInterval >= minTrackLength)
				else{
					shortTrackCnt++;
				}
			}
		}
		
		for (unsigned int sv=1;sv<=MAXSV;sv++)
			svtrk[sv].clear();
	}
	
	DBGMSG(debugStream,INFO,lowElevationCnt << " low elevation tracks");
	DBGMSG(debugStream,INFO,highDSGCnt <<  " high DSG tracks");
	DBGMSG(debugStream,INFO,shortTrackCnt << " short tracks");
	fclose(fout);
	
	return true;
}
 
//
//	Private members
//		

void CGGTTS::init()
{
	revDateYYYY=2016;
	revDateMM=1;
	revDateDD=1;
	ref="";
	lab="";
	comment="";
	calID="";
	intDly=cabDly=refDly=0.0;
	quadFits=false;
	minTrackLength=390;
	minElevation=10.0;
	maxDSG=100.0;
}
		
void CGGTTS::writeHeader(FILE *fout)
{
#define MAXCHARS 128
	int cksum=0;
	char buf[MAXCHARS+1];
	
	// NB maximum line length is 128 columns
	switch (ver){
		case V1:
			strncpy(buf,"GGTTS GPS DATA FORMAT VERSION = 01",MAXCHARS);
			break;
		case V2E:
			strncpy(buf,"CGGTTS     GENERIC DATA FORMAT VERSION = 2E",MAXCHARS);
			break;
	}
	
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"REV DATE = %4d-%02d-%02d",revDateYYYY,revDateMM,revDateDD); 
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"RCVR = %s %s %s %4d %s,v%s",rx->manufacturer.c_str(),rx->modelName.c_str(),rx->serialNumber.c_str(),
		rx->commissionYYYY,APP_NAME, APP_VERSION);
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"CH = %02d",rx->channels); 
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"IMS = 99999"); // FIXME dual frequency 
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"LAB = %s",lab.c_str()); 
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"X = %+.3f m",ant->x);
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"Y = %+.3f m",ant->y);
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"Z = %+.3f m",ant->z);
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"FRAME = %s",ant->frame.c_str());
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	if (comment == "") 
		comment="NO COMMENT";
	
	snprintf(buf,MAXCHARS,"COMMENTS = %s",comment.c_str());
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	switch (ver){
		case V1:
		{
			snprintf(buf,MAXCHARS,"INT DLY = %.1f ns",intDly);
			break;
		}
		case V2E:
		{
			string cons;
			switch (constellation){
				case Receiver::BEIDOU:cons="BDS";break;
				case Receiver::GALILEO:cons="GAL";break;
				case Receiver::GLONASS:cons="GLO";break;
				case Receiver::GPS:cons="GPS";break;
				case Receiver::QZSS:cons="QZSS";break;
			}
			string code1;
			switch (code){
				case Receiver::B1:code1="B1";break;
				case Receiver::C1:code1="C1";break;
				case Receiver::E1:code1="E1";break;
				case Receiver::P1:code1="P1";break;
				case Receiver::P2:code1="P2";break;
			}
			snprintf(buf,MAXCHARS,"INT DLY = %.1f ns %s %s     CAL_ID = %s",intDly,cons.c_str(),code1.c_str(),calID.c_str());
			break;
		}
	}
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"CAB DLY = %.1f ns",cabDly);
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"REF DLY = %.1f ns",refDly);
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"REF = %s",ref.c_str());
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"CKSUM = ");
	cksum += checkSum(buf);
	fprintf(fout,"%s%02X\n",buf,cksum % 256);
	
	fprintf(fout,"\n");
	switch (ver){
		case V1:
			fprintf(fout,"PRN CL  MJD  STTIME TRKL ELV AZTH   REFSV      SRSV     REFGPS    SRGPS  DSG IOE MDTR SMDT MDIO SMDI CK\n");
			fprintf(fout,"             hhmmss  s  .1dg .1dg    .1ns     .1ps/s     .1ns    .1ps/s .1ns     .1ns.1ps/s.1ns.1ps/s  \n");
			break;
		case V2E:
			fprintf(fout,"SAT CL  MJD  STTIME TRKL ELV AZTH   REFSV      SRSV     REFSYS    SRSYS  DSG IOE MDTR SMDT MDIO SMDI FR HC FRC CK\n");
			fprintf(fout,"             hhmmss  s  .1dg .1dg    .1ns     .1ps/s     .1ns    .1ps/s .1ns     .1ns.1ps/s.1ns.1ps/s            \n");
			break;
	}
	
	fflush(fout);
	
#undef MAXCHARS	
}
	

int CGGTTS::checkSum(char *l)
{
	int cksum =0;
	for (unsigned int i=0;i<strlen(l);i++)
		cksum += (int) l[i];
	return cksum;
}

#undef NTRACKS

