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
#include <iomanip>
#include <string>
#include <boost/lexical_cast.hpp>


#include "Antenna.h"
#include "Application.h"
#include "Debug.h"
#include "CGGTTS.h"
#include "GNSSDelay.h"
#include "GNSSSystem.h"
#include "Measurements.h"
#include "Receiver.h"
#include "RINEXFile.h"
#include "Utility.h"

extern Application *app;
extern std::ostream *debugStream;

using boost::lexical_cast;

#define NTRACKS 89      // maximum number of tracks in a day
#define NTRACKPOINTS 26 // 30 s sampling
#define OBSINTERVAL 30  // 30 s

#define MAXSV   37 // GPS 1-32, BDS 1-37, GALILEO 1-36 
// indices into svtrk
#define INDX_OBSV1    0  
#define INDX_OBSV2    1
#define INDX_TOD      2 // decimal INDX_TOD, need this in case timestamps are not on the second
#define INDX_MJD      3

#define CLIGHT 299792458.0

//
//	public
//

CGGTTS::CGGTTS()
{
	init();
}
	
bool CGGTTS::write(Measurements *meas,GNSSSystem *gnss,GNSSDelay *dly,int leapsecs1, std::string fname,int mjd,int startTime,int stopTime)
{
	FILE *fout;
	if (!(fout = std::fopen(fname.c_str(),"w"))){
		std::cerr << "Unable to open " << fname << std::endl;
		return false;
	}
	
	app->logMessage("generating CGGTTS file " + fname);
	
	DBGMSG(debugStream,INFO,"Using CAB DLY = " << dly->cabDelay << " " << " REF DLY = " << dly->refDelay);
	
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
			aij = 77.0*77.0/(77.0*77.0-60*60); // aij == 2.54573
			break;
			
		default:break;
	}
	
	// Set indices into measurement arrays of RINEX observation codes
	int obs1indx=-1,obs2indx=-1,obs3indx=-1;
	double totdly1=0.0,totdly2=0.0,totdly3=0.0;
	
	obs1indx=meas->colIndexFromCode(rnxcode1);
	
	totdly1 = 1.0E-9*(dly->getDelay(rnxcode1) + dly->cabDelay - dly->refDelay)*CLIGHT; // Note: in m! cabDly and refDly wll be zero ig eg the delay "rnxcode1" is a total delay
	
	if (!rnxcode2.empty()){
		obs2indx = meas->colIndexFromCode(rnxcode2);
		totdly2 = 1.0E-9*(dly->getDelay(rnxcode2) + dly->cabDelay - dly->refDelay)*CLIGHT;
	}
	if (!rnxcode3.empty()){
		obs3indx = meas->colIndexFromCode(rnxcode3);
		totdly3 = 1.0E-9*(dly->getDelay(rnxcode3) + dly->cabDelay - dly->refDelay)*CLIGHT;
	}
	
	int freqBand;
	if (isP3){
		freqBand = 0; // unfortunately '3' is a Beidou band so use 0 to flag a P3 combination
	}
	else{
		freqBand = lexical_cast<int>(rnxcode1[1]);
	}
	// a bit of checking
	reportMSIO = (obs2indx != -1)  && (obs3indx != -1);
	writeHeader(fout,gnss,dly);
	
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
	// Use a fixed array so that we can use the index for the SVN. Memory is cheap
	// and svtrk is only 36 points long anyway
	double svtrk[MAXSV+1][NTRACKPOINTS][INDX_MJD+1]; 
	
	int leapOffset1 = leapsecs1/30.0; // measurements are assumed to be in GPS time so we'll need to shift the lookup index back by this
	int indxMJD = meas->colMJD();
	int indxTOD = meas->colTOD();
	
	//ntracks = 2; // FIXME
	int prcount = 0; // count of pseudorange estimations 
	
	for (int i=0;i<ntracks;i++){
	
		int trackStart = schedule[i]*60; // in seconds since start of UTC day
		int trackStop =  schedule[i]*60 + (NTRACKPOINTS-1)*OBSINTERVAL; // note the time of the last point 
		if (trackStop >= 86400) trackStop=86400-1; // FIXME we can get more from the next day ...
		//DBGMSG(debugStream,INFO,"Track " << i << " start " << trackStart << " stop " << trackStop);
		
		// Now window it
		if (trackStart < startTime || trackStart > stopTime) continue; // svtrk empty so no need to clear
		
		for (int s=1;s<=MAXSV;s++){
			for (int t=0;t<NTRACKPOINTS;t++)
				for (int o=INDX_OBSV1;o<= INDX_MJD;o++)
					svtrk[s][t][o] = NAN; // as usual, NAN flags no data
		}
		
		
		int iTrackStart = trackStart/OBSINTERVAL;
		int iTrackStop  = trackStop/OBSINTERVAL;
		
		// CASE 1: single code 
		// Observation in svtrk[][][INDX_OBSV1]
		// MSIO in svtrk[][][INDX_OBSV2], if available
		if (obs1indx >=0 and !isP3){
			int measIndx = 0;
			for (int m=iTrackStart;m<=iTrackStop;m++){
				measIndx = m - iTrackStart;
				int mGPSt = m - leapOffset1 + 1; // at worst, we miss one measurement since we compensate when leapSecs > 30 (which may not happen for a very long time)
																				 // Add one so that the first point is AFTER the start of the track
				for (int sv = 1; sv <= meas->maxSVN;sv++){
					if (!isnan(meas->meas[mGPSt][sv][obs1indx])){ 
						//DBGMSG(debugStream,INFO,sv << " " << meas->meas[mGPS][sv][indxMJD] << " " << meas->meas[mGPS][sv][indxTOD] << " " << meas->meas[mGPS][sv][obs1indx]);
						svtrk[sv][measIndx][INDX_OBSV1]= meas->meas[mGPSt][sv][obs1indx] - totdly1; // delays here!
						svtrk[sv][measIndx][INDX_TOD]= meas->meas[mGPSt][sv][indxTOD]; 
						svtrk[sv][measIndx][INDX_MJD] = meas->meas[mGPSt][sv][indxMJD];
					}
					
					if (reportMSIO){
						if (!isnan(meas->meas[mGPSt][sv][obs1indx]) && !isnan(meas->meas[mGPSt][sv][obs2indx]) && !isnan(meas->meas[mGPSt][sv][obs3indx])){ // got the lot so OK
							//DBGMSG(debugStream,INFO,sv << " " << meas->meas[mGPS][sv][indxMJD] << " " << meas->meas[mGPS][sv][indxTOD] << " msio pair");
							// Measured ionosphere for the frequency we are reporting is just PR - PR_IF
							svtrk[sv][measIndx][INDX_OBSV2] = ( meas->meas[mGPSt][sv][obs1indx] - totdly1 
							  - (aij*(meas->meas[mGPSt][sv][obs2indx] - totdly2)
								 + (1.0-aij)*(meas->meas[mGPSt][sv][obs3indx] - totdly3)))/CLIGHT;
							//DBGMSG(debugStream,INFO,sv << " " << meas->meas[mGPSt][sv][indxMJD] << " " << meas->meas[mGPSt][sv][indxTOD] << " " << svtrk[sv][measIndx][INDX_OBSV2] 
							//	<< std::setprecision(14) << " " << meas->meas[mGPSt][sv][obs1indx] << " " << meas->meas[mGPSt][sv][obs2indx] << " " << meas->meas[mGPSt][sv][obs3indx]);
						
							svtrk[sv][measIndx][INDX_TOD] = meas->meas[mGPSt][sv][indxTOD];  // in case these haven't been set yet
							svtrk[sv][measIndx][INDX_MJD] = meas->meas[mGPSt][sv][indxMJD];
						}
					}
				}
			}
		}
		
		// CASE 2: ionosphere free combination
		// Observation in svtrk[][][INDX_OBSV1]
		// MSIO in svtrk[][][INDX_OBSV2]
		if (isP3){
			int measIndx = 0;
			for (int m=iTrackStart;m<=iTrackStop;m++){
				measIndx = m - iTrackStart;
				int mGPSt = m - leapOffset1 + 1; // at worst, we miss one measurement since we compensate when leapSecs > 30 (which may not happen for a very long time)
				for (int sv = 1; sv <= meas->maxSVN;sv++){
					if (!isnan(meas->meas[mGPSt][sv][obs1indx]) && !isnan(meas->meas[mGPSt][sv][obs2indx])){
					
						svtrk[sv][measIndx][INDX_OBSV1] = (aij*(meas->meas[mGPSt][sv][obs1indx] - totdly1) // Ionosphere free code
								 + (1.0-aij)*(meas->meas[mGPSt][sv][obs2indx] - totdly2));
						
						svtrk[sv][measIndx][INDX_OBSV2] = (1.0 -aij)*(( meas->meas[mGPSt][sv][obs1indx] - totdly1) - (meas->meas[mGPSt][sv][obs2indx] - totdly2) )/CLIGHT;  // MSIO sans GD
						
						svtrk[sv][measIndx][INDX_TOD]= meas->meas[mGPSt][sv][indxTOD]; 
						svtrk[sv][measIndx][INDX_MJD] = meas->meas[mGPSt][sv][indxMJD];
					}
				}
			}
		}
		
		for (unsigned int sv=1;sv<=MAXSV;sv++){
			
			
			
			int nfitpts=0; // count of number of points for the linear fit
			int msiofitpts=0; // separately tracked for single code+ MSIO
			
			//DBGMSG(debugStream,INFO,sv);
			
			int hh = schedule[i] / 60; // schedule hour   (UTC)
			int mm = schedule[i] % 60; // schedule minute (UTC)
			
			double refsv[26],refsys[26],mdtr[26],mdio[26],msio[26],tutc[26],svaz[26],svel[26],msiotutc[26]; //buffers for the data used for the linear fits
			
			Ephemeris *ed=NULL;
			int nSVObs = 0; // keep track of SV observations in track so we can dsistinguish misisng data from pseudorange failures etc ..
			for (unsigned int tt=0;tt<NTRACKPOINTS;tt++){
				int fwn;
				double tow;
				
				// Don't mask out MSIO if there is no corresponding code measurement
				// Even if you did, you would still have to separately store timestamps (unless you dropped code measurements) 
				if (reportMSIO || isP3){
					if (!isnan(svtrk[sv][tt][INDX_OBSV2])){
						msio[msiofitpts] = svtrk[sv][tt][INDX_OBSV2];
						msiotutc[msiofitpts] = (svtrk[sv][tt][INDX_MJD] - mjd)*86400 + svtrk[sv][tt][INDX_TOD] - leapsecs1; // be careful about data which runs into the next day 
						msiofitpts++;
						//DBGMSG(debugStream,INFO,sv << " " << msio[msiofitpts-1] << " " << msiotutc[msiofitpts -1]);
					}
				}
						
				if (isnan(svtrk[sv][tt][INDX_OBSV1])) continue; // no OBSV1 data, so move along
				nSVObs++;
				
				//std::cerr << "G" << sv << " " << svtrk[sv][tt][INDX_TOD] << " ";
				
				Utility::MJDToGPSTime(svtrk[sv][tt][INDX_MJD],svtrk[sv][tt][INDX_TOD],&fwn,&tow);
				//DBGMSG(debugStream,INFO,svtrk[sv][tt][INDX_MJD] << " " << svtrk[sv][tt][INDX_TOD] << " " << fwn << " " << tow);
		
				// Now window it
				
				if (NULL == ed) // use only one ephemeris for each track
					ed = gnss->nearestEphemeris(sv,tow,maxURA);
				
				if (NULL == ed){ //tried to get ephemeris and failed :-(
					ephemerisMisses++;
					//pseudoRangeFailures++;
					continue; // fixme - probably should 'break'
				}
				else{
					if (!(ed->healthy())){
						badHealth++;
						//DBGMSG(debugStream,INFO,"Unhealthy SV = " << sv);
						continue; // WARNING didn't do this in previous code 
					}
					//ed->dump();
				}
				
				double corrRange,clockCorr,modIonoCorr,tropoCorr,gdCorr,relCorr,az,el;
				
				double pr = svtrk[sv][tt][INDX_OBSV1]; 
		
				prcount++;
				if (gnss->getPseudorangeCorrections(tow,pr,antenna,ed,freqBand,&corrRange,&clockCorr,&modIonoCorr,&tropoCorr,&gdCorr,&relCorr,&az,&el)){
					tutc[nfitpts] = (svtrk[sv][tt][INDX_MJD] - mjd)*86400 + svtrk[sv][tt][INDX_TOD] - leapsecs1; // be careful about data which runs into the next day 
					svaz[nfitpts] = az;
					svel[nfitpts] = el;
					mdtr[nfitpts] = tropoCorr * 1.0E9;
					mdio[nfitpts] = modIonoCorr * 1.0E9;
					
					double refsvcorr =  relCorr - gdCorr - corrRange - tropoCorr;
					double refsyscorr=  refsvcorr + clockCorr;
					
					if (isP3){ // ionosphere has been removed
						refsv[nfitpts]  = 1.0E9*(pr/CLIGHT  + refsvcorr ); // units are ns, for CGGTTS
						refsys[nfitpts] = 1.0E9*(pr/CLIGHT  + refsyscorr);
					}
					else{ // use modelled ionosphere
						refsv[nfitpts]  = 1.0E9*(pr/CLIGHT + refsvcorr  - modIonoCorr ); // units are ns, for CGGTTS
						refsys[nfitpts] = 1.0E9*(pr/CLIGHT + refsyscorr - modIonoCorr );
					
					}
					
					nfitpts++;
					DBGMSG(debugStream,TRACE,svtrk[sv][tt][INDX_TOD] << " " << sv << std::setprecision(16) << " PR: " << corrRange << " C: " << clockCorr*1.0E10 << " T: " << tropoCorr*1.0E10 << " R: " << relCorr*1.0E10  << " G: " << gdCorr );
	
				}
				else{
					pseudoRangeFailures++;
				}
			} // for (unsigned int tt=0;tt<svObsCount[sv];tt++)
			
			if (nSVObs == 0) continue;
			
			if (nfitpts*OBSINTERVAL >= minTrackLength){
				
				//double tc=(trackStart+trackStop)/2.0; // FIXME may need to add MJD to allow rollovers
				double tc=trackStart + 390;
				//std::cerr << "** " << trackStart << " " << trackStop << " " << tc << std::endl;
				
				double aztc,azc,azm,azresid;
				Utility::linearFit(tutc,svaz,nfitpts,tc,&aztc,&azc,&azm,&azresid);
				aztc=rint(aztc*10); // CGGTTS liketh integers nb r2cggtts also rounds to the nearest integer using nint()
				
				double eltc,elc,elm,elresid;
				Utility::linearFit(tutc,svel,nfitpts,tc,&eltc,&elc,&elm,&elresid);
				eltc=rint(eltc*10);
			
				double mdtrtc,mdtrc,mdtrm,mdtrresid;
				Utility::linearFit(tutc,mdtr,nfitpts,tc,&mdtrtc,&mdtrc,&mdtrm,&mdtrresid);
				mdtrtc=rint(mdtrtc*10);
				mdtrm=rint(mdtrm*10000);
				
				double refsvtc,refsvc,refsvm,refsvresid;
				Utility::linearFit(tutc,refsv,nfitpts,tc,&refsvtc,&refsvc,&refsvm,&refsvresid);
				refsvtc=rint(refsvtc*10); 
				refsvm=rint(refsvm*10000);
				
				double refsystc,refsysc,refsysm,refsysresid;
				Utility::linearFit(tutc,refsys,nfitpts,tc,&refsystc,&refsysc,&refsysm,&refsysresid);
				refsystc=rint(refsystc*10); 
				refsysm=rint(refsysm*10000);
				refsysresid=rint(refsysresid*10);
				
				double mdiotc,mdioc,mdiom,mdioresid;
				Utility::linearFit(tutc,mdio,nfitpts,tc,&mdiotc,&mdioc,&mdiom,&mdioresid);
				mdiotc=rint(mdiotc*10);
				mdiom=rint(mdiom*10000);
				
				double msiotc=0.0,msioc=0.0,msiom=0.0,msioresid=0.0;
				if (reportMSIO || isP3){
					Utility::linearFit(msiotutc,msio,msiofitpts,tc,&msiotc,&msioc,&msiom,&msioresid);
					//DBGMSG(debugStream,INFO,"MSIO " << rint(msiotc*1.0E10) << " " << rint(ed->tgd()*1.0E10));
					msiotc=rint((msiotc - ed->tgd())*10*1.0E9); // FIXME frequency correction
					if (msiotc < -999)
						msiotc=-999;
					else if (msiotc > 9999)
						msiotc=9999;
					msiom=rint(msiom*10000*1.0E9); // 4 digits
					if (msiom < -999) // clamp out of range
						msiom=-999;
					else if (msiom > 9999)
						msiom=9999;
					msioresid=rint(msioresid*10*1.0E9); // 3 digits
					if (msioresid > 999)
						msioresid=999;
					
				}
				
				// Some range checks on the data - flag bad measurements
				if (refsvm >  99999) refsvm=99999;
				if (refsvm < -99999) refsvm=-99999;
				
				if (refsysm >  99999) refsysm=99999;
				if (refsysm < -99999) refsysm=-99999;
				
				if (refsysresid > 999.9) refsysresid = 999.9;
				
				if (fabs(refsvm)==99999 || fabs(refsysm) == 99999 || refsysresid == 999.9 || msioresid==999) // so that we only count a single track
					badMeasurementCnt++;
				
				// Ready to output
				if (eltc >= minElevation*10 && refsysresid <= maxDSG*10){ 
					char sout[155]; // V2E
					goodTrackCnt++;
						
					if (isP3 || reportMSIO)
						std::snprintf(sout,154,"%s%02i %2s %5i %02i%02i00 %4i %3i %4i %11li %6i %11li %6i %4i %3i %4i %4i %4i %4i %4i %4i %3i %2i %2i %3s ",GNSSsys.c_str(),sv,"FF",mjd,hh,mm,
									nfitpts*OBSINTERVAL,(int) eltc,(int) aztc, (long int) refsvtc,(int) refsvm,(long int)refsystc,(int) refsysm,(int) refsysresid,
									ed->iod(),(int) mdtrtc, (int) mdtrm, (int) mdiotc, (int) mdiom,(int) msiotc,(int) msiom,(int) msioresid,0,0,FRC.c_str());
					else 
						std::snprintf(sout,154,"%s%02i %2s %5i %02i%02i00 %4i %3i %4i %11li %6i %11li %6i %4i %3i %4i %4i %4i %4i %2i %2i %3s ",GNSSsys.c_str(),sv,"FF",mjd,hh,mm,
									nfitpts*OBSINTERVAL,(int) eltc,(int) aztc, (long int) refsvtc,(int) refsvm,(long int)refsystc,(int) refsysm,(int) refsysresid,
									ed->iod(),(int) mdtrtc, (int) mdtrm, (int) mdiotc, (int) mdiom,0,0,FRC.c_str());
					std::fprintf(fout,"%s%02X\n",sout,checkSum(sout) % 256); // FIXME
						
				} // if (eltc >= minElevation*10 && refsysresid <= maxDSG*10)
				else{
					if (eltc < minElevation*10) lowElevationCnt++;
					if (refsysresid > maxDSG*10) highDSGCnt++;
				}
			} // if (npts*linFitInterval >= minTrackLength)
			else{
				shortTrackCnt++;
			}
			
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
	minTrackLength = 390;
	minElevation = 10.0;
	maxDSG = 100.0;
	maxURA = 3.0; // as reported by receivers, typically 2.0 m, with a few at 2.8 m
	
	reportMSIO = false; // MSIO can be reported for for C1C output
	isP3 = false;
	FRC="";
	
}


void CGGTTS::writeHeader(FILE *fout,GNSSSystem *gnss,GNSSDelay *dly)
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
	if (reportMSIO)
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
	
	std::string dlyName;
	switch (dly->kind){
		case GNSSDelay::INTDLY:dlyName="INT";break;
		case GNSSDelay::SYSDLY:dlyName="SYS";break;
		case GNSSDelay::TOTDLY:dlyName="TOT";break;
	}
	
	std::string dly1Code,dly2Code,dly3Code; // these are for identifying the delays
	dly1Code = gnss->rnxCodeToCGGTTSCode(rnxcode1);
	dly2Code = gnss->rnxCodeToCGGTTSCode(rnxcode2);
	dly3Code = gnss->rnxCodeToCGGTTSCode(rnxcode3);
	
	double dly1 = dly->getDelay(rnxcode1);
	double dly2 = dly->getDelay(rnxcode2);
	//double dly3    = dly->getDelay(rnxcode3);
	
	if (isP3) // FIXME presuming that the logical thing happens here ...
		std::snprintf(buf,MAXCHARS,"%s DLY = %.1f ns (%s %s),%.1f ns (%s %s)     CAL_ID = %s",dlyName.c_str(),
			dly1,cons.c_str(),dly1Code.c_str(), // FIXME
			dly2,cons.c_str(),dly2Code.c_str(),dly->calID.c_str()); // FIXME
	else{
		if (reportMSIO){
// 			std::snprintf(buf,MAXCHARS,"%s DLY = %.1f ns (%s %s), %.1f ns (%s %s), %.1f ns (%s %s)     CAL_ID = %s",dlyName.c_str(), // spec says only the measured code
// 				dly1,cons.c_str(),dly1Code.c_str(),
// 				dly2,cons.c_str(),dly2Code.c_str(),
// 				dly3,cons.c_str(),dly3Code.c_str(),
// 				dly->calID.c_str());
			std::snprintf(buf,MAXCHARS,"%s DLY = %.1f ns (%s %s)     CAL_ID = %s",dlyName.c_str(),dly1,cons.c_str(),dly1Code.c_str(),dly->calID.c_str()); 
		}
		else{
			std::snprintf(buf,MAXCHARS,"%s DLY = %.1f ns (%s %s)     CAL_ID = %s",dlyName.c_str(),dly1,cons.c_str(),dly1Code.c_str(),dly->calID.c_str()); 
		}// FIXME
	}	
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	if (dly->kind == GNSSDelay::INTDLY){
		std::snprintf(buf,MAXCHARS,"CAB DLY = %.1f ns",dly->cabDelay);
		cksum += checkSum(buf);
		std::fprintf(fout,"%s\n",buf);
	}
	
	if (dly->kind != GNSSDelay::TOTDLY){
		std::snprintf(buf,MAXCHARS,"REF DLY = %.1f ns",dly->refDelay);
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
	
	if (isP3 || reportMSIO){
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




