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

#include <boost/lexical_cast.hpp>

#include "Application.h"
#include "Antenna.h"
#include "CGGTTS.h"
#include "Counter.h"
#include "CounterMeasurement.h"
#include "Debug.h"
#include "GNSSSystem.h"
#include "GPS.h"
#include "MeasurementPair.h"
#include "Receiver.h"
#include "ReceiverMeasurement.h"
#include "RINEX.h"
#include "Utility.h"

extern Application *app;
extern std::ostream *debugStream;

#define NTRACKS 89
#define NTRACKPOINTS 780
#define MAXSV   37 // BDS 1-37, GALILEO 1-36 
#define OBSV1    0  
#define OBSV2    1
#define OBSV3    2

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
//	Public members
//

CGGTTS::CGGTTS(Antenna *a,Counter *c,Receiver *r)
{
	ant=a;
	cntr=c;
	rx=r;
	init();
}

unsigned int CGGTTS::strToCode(std::string str)
{
	// Convert CGGTTS code string (usually from the configuration file) to RINEX observation code(s)
	// Valid formats are 
	// (1) CGGTTS 2 letter codes
	// (2) RINEX  3 letter codes
	// but not mixed!
	unsigned int c=0;
	if (str.length()==2){
		c = str2ToCode(str);
	}
	else if (str.length()==3){ // single frequency, specified using 3 letter RINEX code
		c=GNSSSystem::strToObservationCode(str,RINEX::V3);
	}
	else{
		c= 0;
	}
	
	return c;
}

bool CGGTTS::writeObservationFile(std::string fname,int mjd,int startTime,int stopTime,MeasurementPair **mpairs,bool TICenabled)
{
	FILE *fout;
	if (!(fout = std::fopen(fname.c_str(),"w"))){
		std::cerr << "Unable to open " << fname << std::endl;
		return false;
	}
	
	app->logMessage("generating CGGTTS file " + fname);
	
	// Correction for any programmed delay in the receiver PPS output and the reference delay
	// This is treated separately from SYSDLY because it does not affect the raw pseudorange
	double ppsDelay  = rx->ppsOffset - refDly;   
	
	int useTIC = (TICenabled?1:0);
	DBGMSG(debugStream,1,"Using TIC = " << (TICenabled? "yes":"no"));
	
	int lowElevationCnt=0; // a few diagnostics
	int highDSGCnt=0;
	int shortTrackCnt=0;
	int goodTrackCnt=0;
	int ephemerisMisses=0;
	int badHealth=0;
	int pseudoRangeFailures=0;
	int badMeasurementCnt=0;
	
	// Constellation/code identifiers as per V2E
	int freqBand=0;
	
	std::string GNSSsys;
	std::string FRCcode="";
	unsigned int code1=code,code2=0;
	switch (code){ 
		case GNSSSystem::C1C:
			if (constellation == GNSSSystem::GALILEO){
				FRCcode=" E1";
				code1Str="E1";
			}
			else{
				FRCcode="L1C";
				code1Str="C1";
				freqBand = 1;
			}
			break;
		case GNSSSystem::C1B:
			FRCcode="E1";
			code1Str="E1";
			break;
		case GNSSSystem::C1P:
			FRCcode="L1P";
			code1Str="P1";
			freqBand = 1;
			break;
		case GNSSSystem::C2P:
			FRCcode="L2P";
			code1Str="P2";
			freqBand = 2;
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
			FRCcode="L3P";
			freqBand=0;break;
		case GNSSSystem::C1C | GNSSSystem::C2P:
			code1 = GNSSSystem::C1C;
			code2 = GNSSSystem::C2P;
			code1Str = "C1";
			code2Str = "P2";
			FRCcode="L3P";
			freqBand=0;break;
		case GNSSSystem::C1C | GNSSSystem::C2C:
			code1 = GNSSSystem::C1C;
			code2 = GNSSSystem::C2C;
			code1Str = "C1";
			code2Str = "C2";
			FRCcode="L3P";
			freqBand=0;break;
		default:break;
	}
	
	double aij=0.0; // frequency weight for pseudoranges, as per CGGTTS v2E
	
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
	
	// System delays - these should be subtracted from the raw pseudoranges
	double sysdly1 = 1.0E-9*(intDly1  + cabDly)*CLIGHT; // Note: in m!
	double sysdly2 = 1.0E-9*(intDly2  + cabDly)*CLIGHT;
	
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

	switch (ver){
		case V1: quadFits=true;break;
		case V2E:quadFits=false;break;
		default:quadFits=false;
	}
	
	// Use a fixed array of vectors so that we can use the index as a hash for the SVN. Memory is cheap
	// and svtrk is only 780 points long anyway
	SVMeasurement * svtrk[MAXSV+1][NTRACKPOINTS][3]; 
	int svObsCount[MAXSV+1];
	
	for (int i=0;i<ntracks;i++){
	
		int trackStart = schedule[i]*60;
		int trackStop =  schedule[i]*60+NTRACKPOINTS-1;
		if (trackStop >= 86400) trackStop=86400-1;
		// Now window it
		if (trackStart < startTime || trackStart > stopTime) continue; // svtrk empty so no need to clear
	
		// Matched measurement pairs can be looked up without a search since the index is TOD
		
		for (int s=1;s<=MAXSV;s++){
			svObsCount[s]=0;
			for (int t=0;t<NTRACKPOINTS;t++)
				for (int o=0;o<2;o++)
					svtrk[s][t][o]=NULL;
		}
		
		if (reportMSIO){
		}
		else {
			for (int m=trackStart;m<=trackStop;m++){
				if (mpairs[m]->flags==0x03){
					ReceiverMeasurement *rm = mpairs[m]->rm;
					for (unsigned int sv=0;sv<rm->meas.size();sv++){
						SVMeasurement * svm = rm->meas.at(sv);
						if (svm->constellation == constellation && svm->code == code1){
							svtrk[svm->svn][m-trackStart][OBSV1]=svm;
							svObsCount[svm->svn] += 1;
						}
					}
				} 
			}
		}
		
		int hh = schedule[i] / 60;
		int mm = schedule[i] % 60;
		
		 //use arrays which can store the 15s quadratic fits and 30s decimated data
		double refsv[52],refsys[52],mdtr[52],mdio[52],msio[52],tutc[52],svaz[52],svel[52];
		
		int linFitInterval=30; // length of fitting interval 
		if (quadFits) linFitInterval=15;
		
		for (unsigned int sv=1;sv<=MAXSV;sv++){
			
			if (0 == svObsCount[sv]) continue;
			
			int npts=0;
			GPSEphemeris *ed=NULL;
			
			if (quadFits){
				double qprange[15],qtutc[15],qrefpps[15]; // for the 15s fits
				double pr[52], refpps[52]; // for the results of the 15s fits
				unsigned int nqfitpts=0,nqfits=0,gpsTOW[52];
				int t=trackStart;
				ReceiverMeasurement *rxm;
				while (t<=trackStop){
					SVMeasurement *svm1 = svtrk[sv][t-trackStart][OBSV1];
					if (NULL != svm1){
						rxm = svm1->rm;
						int tmeas=rint(rxm->tmUTC.tm_sec + rxm->tmUTC.tm_min*60 + rxm->tmUTC.tm_hour*3600 + rxm->tmfracs); // tmfracs is set to zero by interpolateMeasurements()
						
						// FIXME MDIO needs to change for L2
						if (nqfitpts > 14){ // shouldn't happen
							std::cerr << "Error in CGGTTS::writeObservationFile() - nqfitpts too big" << std::endl;
							exit(EXIT_FAILURE);
						}
						// smooth the counter measurements - this helps clean up any residual sawtooth error
						// FIXME should just fold into the pseudorange ...
						qrefpps[nqfitpts]= useTIC*(rxm->cm->rdg + rxm->sawtooth)*1.0E9;
						qprange[nqfitpts]=svm1->meas*CLIGHT - sysdly1; // back to m!
						qtutc[nqfitpts]=tmeas;
						nqfitpts++;
					}
					
					t++;
				
					if (((t-trackStart) % 15 == 0) || ((t - trackStart)== NTRACKPOINTS)){ // have got a full set of points for a quadratic fit
						//DBGMSG(debugStream,1,sv << " " << trackStart << " " << nqfitpts << " " << nqfits);
						// Sanity checks
						if (nqfits > 51){// shouldn't happen
							std::cerr << "Error in CGGTTS::writeObservationFile() - nqfits too big" << std::endl;
							exit(EXIT_FAILURE);
						}
						
						if (nqfitpts > 7){ // demand at least half a track - then we are not extrapolating
							double tc=(t-1)-7; // subtract 1 because we've gone one too far
							tutc[nqfits] = tc;
							// Compute and save GPS TOW so that we have it available for computing the pseudorange corrections
							// FIXME This does not handle the week rollover 
							unsigned int gpsDay = (rxm->gpstow / 86400); // use the last receiver measurement for day number - shouldn't be NULL because nqfitpts >0
							unsigned int TOD = tc+rx->leapsecs;
							if (TOD >= 86400){
								TOD -= 86400;
								gpsDay++;
								if (gpsDay == 7){
									gpsDay=0;
								}
							}
							// FIXME as a kludge could just drop points at the week rollover
							gpsTOW[nqfits] =  tc + rx->leapsecs + gpsDay*86400;
							Utility::quadFit(qtutc,qprange,nqfitpts,tc,&(pr[nqfits]) );
							Utility::quadFit(qtutc,qrefpps,nqfitpts,tc,&(refpps[nqfits]) );
							nqfits++;
						}
						nqfitpts=0;
					} // 
					
					//if ((t - trackStart) == NTRACKPOINTS) break;  // no more measurements available FIXME off by 1?
				} // while (t<=trackStart)
				
				// Now we can compute the pr corrections etc for the fitted prs

				npts=0;
				for ( unsigned int q=0;q<nqfits;q++){
					if (ed==NULL) // use only one ephemeris for each track
							ed = dynamic_cast<GPSEphemeris *>(rx->gps.nearestEphemeris(sv,gpsTOW[q],maxURA));
					if (NULL == ed){
						ephemerisMisses++;
					}
					else{
						if (ed->SV_health > 0){
							badHealth++;
							continue;
						}
					}
					
					double corrRange,clockCorr,modIonoCorr,tropoCorr,gdCorr,relCorr,az,el;
					// FIXME MDIO needs to change for L2
					// getPseudorangeCorrections will check for NULL ephemeris
					//if (rx->gps.getPseudorangeCorrections(gpsTOW[q],uncorrprange[q],ant,ed,code,
					//		&refsyscorr,&refsvcorr,&iono,&tropo,&az,&el,&ioe)){
					if (rx->gps.getPseudorangeCorrections(gpsTOW[q],pr[q],ant,ed,freqBand,&corrRange,&clockCorr,&modIonoCorr,&tropoCorr,&gdCorr,&relCorr,&az,&el)){
						tutc[npts]=tutc[q]; // ok to overwrite, because npts <= q
						svaz[npts]=az;
						svel[npts]=el;
						mdtr[npts]=tropoCorr * 1.0E9;
						mdio[npts]=modIonoCorr * 1.0E9;
						
						double refsvcorr =  relCorr - gdCorr - corrRange - tropoCorr;
						double refsyscorr=  refsvcorr + clockCorr;
						
						refsv[npts]  = 1.0E9*(pr[q]/CLIGHT + refsvcorr  - modIonoCorr ) + refpps[q]; // units are ns, for CGGTTS
						refsys[npts] = 1.0E9*(pr[q]/CLIGHT + refsyscorr - modIonoCorr ) + refpps[q];
						
						//refsv[npts]  = uncorrprange[q]*1.0E9 + refsvcorr  - iono - tropo + refpps[q];
						//refsys[npts] = uncorrprange[q]*1.0E9 + refsyscorr - iono - tropo + refpps[q];
						npts++;
					}
					else{
						pseudoRangeFailures++;
					}
				}
			}                                 
			else{ // v2E specifies 30s sampled values 
				int tsearch=trackStart;
				int t=0;
				
				
				while (t< NTRACKPOINTS){
					SVMeasurement *svm1  = svtrk[sv][t][OBSV1];
					SVMeasurement *svm2  = NULL;
					if (NULL == svm1){
						t++;
						continue;
					}
			
					svm1->dbuf2=0.0;
					ReceiverMeasurement *rxmt = svm1->rm;
					int tmeas=rint(rxmt->tmUTC.tm_sec + rxmt->tmUTC.tm_min*60+ rxmt->tmUTC.tm_hour*3600+rxmt->tmfracs);
					
					if (tmeas==tsearch){
					
						if (ed==NULL) // use only one ephemeris for each track
							ed = dynamic_cast<GPSEphemeris *>(rx->gps.nearestEphemeris(sv,rxmt->gpstow,maxURA));
						
						if (NULL == ed){
							ephemerisMisses++;
						}
						else{
							if (ed->SV_health > 0){
								badHealth++;
								tsearch += 30;
								t++;
								continue;
							}
						}
					
						double pr,corrRange,clockCorr,modIonoCorr,tropoCorr,gdCorr,relCorr,az,el;
						
						// FIXME MDIO needs to change for L2
						// getPseudorangeCorrections will check for NULL ephemeris
						pr = svm1->meas*CLIGHT - sysdly1; // back to metres !
						
						//if (rx->gps.getPseudorangeCorrections(rxmt->gpstow,pr,ant,ed,code1,&refsyscorr,&refsvcorr,&iono,&tropo,&az,&el,&ioe)){
						if (rx->gps.getPseudorangeCorrections(rxmt->gpstow,pr,ant,ed,freqBand,&corrRange,&clockCorr,&modIonoCorr,&tropoCorr,&gdCorr,&relCorr,&az,&el)){
							tutc[npts]=tmeas;
							svaz[npts]=az;
							svel[npts]=el;
							mdtr[npts]=tropoCorr*1.0E9;
							mdio[npts]=modIonoCorr*1.0E9;
							if (reportMSIO){
								msio[npts]=1.0E9*rx->gps.measIonoDelay(code1,code2,svm1->meas,svm2->meas,0,0,ed);// FIXME calibrated delays ....
								//DBGMSG(debugStream,INFO,tmeas << " " <<"G"<<(int) svm1->svn << "G" << (int)svm2->svn <<  " " << (svm2->meas - svm1->meas)*1.0E9 << " " << msio[npts])
							}
							
							double refpps= useTIC*(rxmt->cm->rdg + rxmt->sawtooth)*1.0E9;
							
							double refsvcorr =  relCorr - gdCorr - corrRange - tropoCorr;
							double refsyscorr=  refsvcorr + clockCorr;
						
							refsv[npts]  = 1.0E9*(pr/CLIGHT + refsvcorr  - modIonoCorr ) + refpps; // units are ns, for CGGTTS
							refsys[npts] = 1.0E9*(pr/CLIGHT + refsyscorr - modIonoCorr ) + refpps;
							
							//DBGMSG(debugStream,INFO,sv << " " << relCorr*1.0E9 << " " << gdCorr*1.0E9 << " " << corrRange * 1.0E9 <<  " " << tropoCorr *1.0E9 << " " <<  modIonoCorr*1.0E9 << " " << refpps);
							svm1->dbuf2 = refsv[npts]/1.0E9; // back to seconds !
							npts++;
						}
						else{
							
							pseudoRangeFailures++;
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
			} // else quadfits
			
			if (npts*linFitInterval >= minTrackLength){
				
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
				refsvtc=rint((refsvtc-ppsDelay)*10); // apply total measurement system delay
				refsvm=rint(refsvm*10000);
				
				double refsystc,refsysc,refsysm,refsysresid;
				Utility::linearFit(tutc,refsys,npts,tc,&refsystc,&refsysc,&refsysm,&refsysresid);
				refsystc=rint((refsystc-ppsDelay)*10); // apply total measurement system delay
				refsysm=rint(refsysm*10000);
				refsysresid=rint(refsysresid*10);
				
				double mdiotc,mdioc,mdiom,mdioresid;
				Utility::linearFit(tutc,mdio,npts,tc,&mdiotc,&mdioc,&mdiom,&mdioresid);
				mdiotc=rint(mdiotc*10);
				mdiom=rint(mdiom*10000);
				
				double msiotc=0.0,msioc=0.0,msiom=0.0,msioresid=0.0;
				if (reportMSIO){
					Utility::linearFit(tutc,msio,npts,tc,&msiotc,&msioc,&msiom,&msioresid);
					msiotc=rint(msiotc*10);
					if (msiotc < -999)
						msiotc=-999;
					else if (msiotc > 9999)
						msiotc=9999;
					msiom=rint(msiom*10000); // 4 digits
					if (msiom < -999) // clamp out of range
						msiom=-999;
					else if (msiom > 9999)
						msiom=9999;
					msioresid=rint(msioresid*10); // 3 digits
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
					switch (ver){
						case V1:
							std::snprintf(sout,128," %02i %2s %5i %02i%02i00 %4i %3i %4i %11li %6i %11li %6i %4i %3i %4i %4i %4i %4i ",sv,"FF",mjd,hh,mm,
											npts*linFitInterval,(int) eltc,(int) aztc, (long int) refsvtc ,(int) refsvm,(long int)refsystc,(int) refsysm,(int) refsysresid,
											ed->iod(),(int) mdtrtc, (int) mdtrm, (int) mdiotc, (int) mdiom);
							std::fprintf(fout,"%s%02X\n",sout,checkSum(sout) % 256);
							break;
						case V2E:
							
							std::snprintf(sout,154,"%s%02i %2s %5i %02i%02i00 %4i %3i %4i %11li %6i %11li %6i %4i %3i %4i %4i %4i %4i %2i %2i %3s ",GNSSsys.c_str(),sv,"FF",mjd,hh,mm,
											npts*linFitInterval,(int) eltc,(int) aztc, (long int) refsvtc,(int) refsvm,(long int)refsystc,(int) refsysm,(int) refsysresid,
											ed->iod(),(int) mdtrtc, (int) mdtrm, (int) mdiotc, (int) mdiom,0,0,FRCcode.c_str());
							std::fprintf(fout,"%s%02X\n",sout,checkSum(sout) % 256); // FIXME
							break;
					} // switch
				} // if (eltc >= minElevation*10 && refsysresid <= maxDSG*10)
				else{
					if (eltc < minElevation*10) lowElevationCnt++;
					if (refsysresid > maxDSG*10) highDSGCnt++;
				}
			} // if (npts*linFitInterval >= minTrackLength)
			else{
				shortTrackCnt++;
			}
		}
	
		
	} // for (int i=0;i<ntracks;i++){
	
	app->logMessage("Ephemeris search misses: " + boost::lexical_cast<std::string>(ephemerisMisses));
	app->logMessage("Bad health: " + boost::lexical_cast<std::string>(badHealth) );
	app->logMessage("Pseudorange calculation failures: " + boost::lexical_cast<std::string>(pseudoRangeFailures-ephemerisMisses) ); // PR calculation skipped if unhealthy
	app->logMessage("Bad measurements: " + boost::lexical_cast<std::string>(badMeasurementCnt) );
	
	app->logMessage(boost::lexical_cast<std::string>(goodTrackCnt) + " good tracks");
	app->logMessage(boost::lexical_cast<std::string>(lowElevationCnt) + " low elevation tracks");
	app->logMessage(boost::lexical_cast<std::string>(highDSGCnt) + " high DSG tracks");
	app->logMessage(boost::lexical_cast<std::string>(shortTrackCnt) + " short tracks");
	
	std::fclose(fout);
	
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
	intDly1=intDly2=cabDly=refDly=0.0;
	delayKind=INTDLY;
	quadFits=false;
	minTrackLength=390;
	minElevation=10.0;
	maxDSG=100.0;
	maxURA=3.0; // as reported by receivers, typically 2.0 m, with a few at 2.8 m
	reportMSIO=false;
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
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"REV DATE = %4d-%02d-%02d",revDateYYYY,revDateMM,revDateDD); 
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"RCVR = %s %s %s %4d %s,v%s",rx->manufacturer.c_str(),rx->modelName.c_str(),rx->serialNumber.c_str(),
		rx->commissionYYYY,APP_NAME, APP_VERSION);
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"CH = %02d",rx->channels); 
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	if (reportMSIO)
		std::snprintf(buf,MAXCHARS,"IMS =%s %s %s %4d %s,v%s",rx->manufacturer.c_str(),rx->modelName.c_str(),rx->serialNumber.c_str(),
			rx->commissionYYYY,APP_NAME, APP_VERSION);
	else
		std::snprintf(buf,MAXCHARS,"IMS = 99999");
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"LAB = %s",lab.c_str()); 
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"X = %+.3f m",ant->x);
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"Y = %+.3f m",ant->y);
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"Z = %+.3f m",ant->z);
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	std::snprintf(buf,MAXCHARS,"FRAME = %s",ant->frame.c_str());
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	if (comment == "") 
		comment="NO COMMENT";
	
	std::snprintf(buf,MAXCHARS,"COMMENTS = %s",comment.c_str());
	cksum += checkSum(buf);
	std::fprintf(fout,"%s\n",buf);
	
	switch (ver){
		case V1:
		{
			std::snprintf(buf,MAXCHARS,"INT DLY = %.1f ns",intDly1);
			break;
		}
		case V2E:
		{
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
			std::snprintf(buf,MAXCHARS,"%s DLY = %.1f ns (%s %s)     CAL_ID = %s",dly.c_str(),intDly1,cons.c_str(),code1Str.c_str(),calID.c_str());
			break;
		}
	}
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
	
	switch (ver){
		case V1:
			if (reportMSIO){
				std::fprintf(fout,"PRN CL  MJD  STTIME TRKL ELV AZTH   REFSV      SRSV     REFGPS    SRGPS  DSG IOE MDTR SMDT MDIO SMDI MSIO SMSI ISG CK\n");
				std::fprintf(fout,"             hhmmss  s  .1dg .1dg    .1ns     .1ps/s     .1ns    .1ps/s .1ns     .1ns.1ps/s.1ns.1ps/s.1ns.1ps/s.1ns  \n");
			}
			else{
				std::fprintf(fout,"PRN CL  MJD  STTIME TRKL ELV AZTH   REFSV      SRSV     REFGPS    SRGPS  DSG IOE MDTR SMDT MDIO SMDI CK\n");
				std::fprintf(fout,"             hhmmss  s  .1dg .1dg    .1ns     .1ps/s     .1ns    .1ps/s .1ns     .1ns.1ps/s.1ns.1ps/s  \n");
			}
			break;
		case V2E:
			if (reportMSIO){
				std::fprintf(fout,"SAT CL  MJD  STTIME TRKL ELV AZTH   REFSV      SRSV     REFSYS    SRSYS  DSG IOE MDTR SMDT MDIO SMDI MSIO SMSI ISG FR HC FRC CK\n");
				std::fprintf(fout,"             hhmmss  s  .1dg .1dg    .1ns     .1ps/s     .1ns    .1ps/s .1ns     .1ns.1ps/s.1ns.1ps/s.1ns.1ps/s.1ns            \n");
			}
			else{
				std::fprintf(fout,"SAT CL  MJD  STTIME TRKL ELV AZTH   REFSV      SRSV     REFSYS    SRSYS  DSG IOE MDTR SMDT MDIO SMDI FR HC FRC CK\n");
				std::fprintf(fout,"             hhmmss  s  .1dg .1dg    .1ns     .1ps/s     .1ns    .1ps/s .1ns     .1ns.1ps/s.1ns.1ps/s            \n");
			}
			break;
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

#undef NTRACKS

