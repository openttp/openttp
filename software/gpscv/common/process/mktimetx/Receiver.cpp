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

#include <cmath>
#include <iostream>
#include <iomanip>
#include <ostream>

#include "Debug.h"
#include "Antenna.h"
#include "Receiver.h"
#include "ReceiverMeasurement.h"

extern ostream *debugStream;

static double LagrangeInterpolation(double x,double x1, double y1,double x2,double y2,double x3,double y3){
	return y1*(x-x2)*(x-x3)/((x1-x2)*(x1-x3)) + 
				 y2*(x-x1)*(x-x3)/((x2-x1)*(x2-x3)) +
				 y3*(x-x1)*(x-x2)/((x3-x1)*(x3-x2)) ;
} 

//
//	public
//	

Receiver::Receiver(Antenna *ant)
{
	modelName="undefined";
	manufacturer="undefined";
	serialNumber="undefined";
	swversion="undefined";
	version1="undefined";
	version2="undefined";
	constellations=0;
	channels=12;
	antenna = ant;
	ppsOffset=0;
	commissionYYYY=1999;
	dualFrequency=false;
	codes=GNSSSystem::C1;
	sawtoothPhase=CurrentSecond;
}

Receiver::~Receiver()
{
}

unsigned int Receiver::memoryUsage()
{
	unsigned int mem=0;
	for (unsigned int m=0;m<measurements.size();m++){
		mem += measurements.at(m)->memoryUsage();
	}
	mem += measurements.size()*sizeof(ReceiverMeasurement *);
	
	return mem;
}

//
// protected
//

//bool Receiver::setCurrentLeapSeconds(int mjd,UTCData &utcd)
//{
	
//}



void Receiver::deleteMeasurements(std::vector<SVMeasurement *> &meas)
{
	DBGMSG(debugStream,4," entries = " << meas.size());

	while(! meas.empty()){
		SVMeasurement  *tmp= meas.back();
		delete tmp;
		meas.pop_back();
	}
	DBGMSG(debugStream,4," entries left = " << meas.size());
}

void Receiver::interpolateMeasurements(std::vector<ReceiverMeasurement *> &meas)
{
	// FIXME This uses Lagrange interpolation to estimate the pseudorange at tmfracs=0
	// Possibly, this method can be removed in the future
	
	// For each SV, build up a list of all measurements for the day
	vector<SVMeasurement *> track;
	for (int prn=1;prn<=32;prn++){
		track.clear();
		for (unsigned int m=0;m<meas.size();m++){
			for (unsigned int svm=0; svm < meas.at(m)->gps.size();svm++){
				if ((prn == meas.at(m)->gps.at(svm)->svn)){
					meas.at(m)->gps.at(svm)->uibuf= mktime(&(meas.at(m)->tmGPS));
					// FIXME initialise dbuf1? That way if data are patchy, at least we put in something roughly right ..
					meas.at(m)->gps.at(svm)->dbuf2=meas.at(m)->tmfracs;
					track.push_back(meas.at(m)->gps.at(svm));
					break;
				}
			}
		}
		
		// Now interpolate the measurements
		
		if (track.size() < 3) continue;
		
		unsigned int trackStart=0;
		unsigned int trackStop=0;
		// Run through tracks, looking for contiguous tracks : if the break is more than 10 a new track is assumed
		// A quadratic is fitted so  the point either side of a point is needed.
		for (unsigned int t=1;t<track.size()-1;t++){
			// Fine a break
			if ((track.at(t+1)->uibuf - track.at(t)->uibuf > 10) || (t == track.size()-2)){ // FIXME threshold to be tweaked 
				trackStop=t;
				if ((t == track.size()-2) && (track.at(t+1)->uibuf - track.at(t)->uibuf < 10) ) trackStop++; // get the last one
						
				DBGMSG(debugStream,TRACE,"Track:" <<prn<< " " << " " << track.size() << " " << trackStart << "->" << trackStop );
				// Check that there are enough points for a quadratic fit, now that trackStop is defined
				if (trackStop-trackStart <2){
					trackStart=t+1;
					continue;
				}
				// First point
				unsigned int tgps1 = track.at(trackStart)->uibuf;
				unsigned int tgps2 = track.at(trackStart+1)->uibuf;
				unsigned int tgps3 = track.at(trackStart+2)->uibuf;
				track.at(trackStart)->dbuf1 = LagrangeInterpolation(0,
													track.at(trackStart)->dbuf2,track.at(trackStart)->meas,
						tgps2-tgps1 + track.at(trackStart+1)->dbuf2,track.at(trackStart+1)->meas,
						tgps3-tgps1 + track.at(trackStart+2)->dbuf2,track.at(trackStart+2)->meas);
				DBGMSG(debugStream,TRACE,"Track:" <<prn<< " " << track.at(trackStart)->dbuf2 << " " << track.at(trackStart)->meas << " " <<
					tgps2-tgps1 + track.at(trackStart+1)->dbuf2 << " " << track.at(trackStart+1)->meas << " " <<
					tgps3-tgps1 + track.at(trackStart+2)->dbuf2 << " " << track.at(trackStart+2)->meas);
				
				for (unsigned int i=trackStart+1;i<=trackStop-1;i++){
					tgps1 = track.at(i-1)->uibuf;
					tgps2 = track.at(i)->uibuf;
					tgps3 = track.at(i+1)->uibuf;
					track.at(i)->dbuf1 = LagrangeInterpolation(tgps2-tgps1,
													track.at(i-1)->dbuf2,track.at(i-1)->meas,
						tgps2-tgps1 + track.at(i)->dbuf2,track.at(i)->meas,
						tgps3-tgps1 + track.at(i+1)->dbuf2,track.at(i+1)->meas);
				}
				// Last point
				tgps1 = track.at(trackStop-2)->uibuf;
				tgps2 = track.at(trackStop-1)->uibuf;
				tgps3 = track.at(trackStop)->uibuf;
				track.at(trackStop)->dbuf1 = LagrangeInterpolation(tgps3-tgps1,
													track.at(trackStop-2)->dbuf2,track.at(trackStop-2)->meas,
						tgps2-tgps1  + track.at(trackStop-1)->dbuf2,track.at(trackStop-1)->meas,
						tgps3-tgps1 + track.at(trackStop)->dbuf2,track.at(trackStop)->meas);
				
				trackStart = t+1;
				
			}
		}
	
		// Update all measurements with the interpolated value
		for (unsigned int i=0;i<track.size();i++){
			double tmp=track.at(i)->meas;
			track.at(i)->meas=track.at(i)->dbuf1;
			track.at(i)->dbuf1=tmp;
			DBGMSG(debugStream,TRACE,prn << " " << i << " " << track.at(i)->uibuf << " " << track.at(i)->dbuf1 << " " << (track.at(i)->meas - track.at(i)->dbuf1)*1.0E9);
		}
		
	}
	
	track.clear();
	// Zero the fractional part of the measurement time
	for (unsigned int i=0;i<meas.size();i++){
		meas.at(i)->tmfracs=0;
	}
	
}

#define OMEGA_E_DOT 7.2921151467e-5
#define CLIGHT 299792458.0

bool Receiver::resolveMsAmbiguity(ReceiverMeasurement *rxm, SVMeasurement *svm, double *corr)
{
	*corr=0.0;
	bool ok=false;
	// find closest ephemeris entry
	GPS::EphemerisData *ed = gps.nearestEphemeris(svm->svn,rxm->gpstow);
	if (ed != NULL){
		double x[3],Ek;
		
		int igpslt = rxm->gpstow; // take integer part of GPS local time (but it's integer anyway ...) 
		int gpsDayOfWeek = igpslt/86400; 
		//  if it is near the end of the day and toc->hour is < 6, then the ephemeris is from the next day.
		int tmpgpslt = igpslt % 86400;
		double toc=ed->t_OC; // toc is clock data reference time
		int tocDay=(int) toc/86400;
		toc-=86400*tocDay;
		int tocHour=(int) toc/3600;
		toc-=3600*tocHour;
		int tocMinute=(int) toc/60;
		toc-=60*tocMinute;
		int tocSecond=toc;
		if (tmpgpslt >= (86400 - 6*3600) && (tocHour < 6))
			  gpsDayOfWeek++;
		toc = gpsDayOfWeek*86400 + tocHour*3600 + tocMinute*60 + tocSecond;
	
		// Calculate clock corrections (ICD 20.3.3.3.3.1)
		double gpssvt= rxm->gpstow - svm->meas;
		double clockCorrection = ed->a_f0 + ed->a_f1*(gpssvt - toc) + ed->a_f2*(gpssvt - toc)*(gpssvt - toc); // SV PRN code phase offset
		double tk = gpssvt - clockCorrection;
		
		double range,ms,svdist,svrange,ax,ay,az;
		if (gps.satXYZ(ed,tk,&Ek,x)){
			double trel =  -4.442807633e-10*ed->e*ed->sqrtA*sin(Ek);
			range = svm->meas + clockCorrection + trel - ed->t_GD;
			// Correction for Earth rotation (Sagnac) (ICD 20.3.3.4.3.4)
			ax = antenna->x - OMEGA_E_DOT * antenna->y * range;
			ay = antenna->y + OMEGA_E_DOT * antenna->x * range;
			az = antenna->z ;
			
			svrange= (svm->meas+clockCorrection) * CLIGHT;
			svdist = sqrt( (x[0]-ax)*(x[0]-ax) + (x[1]-ay)*(x[1]-ay) + (x[2]-az)*(x[2]-az));
			double err  = (svrange - svdist);
			ms   = (int)((-err/CLIGHT *1000)+0.5)/1000.0;
			*corr = ms;
			ok=true;
		}
		else{
			DBGMSG(debugStream,1,"Failed");
			ok=false;
		}
		DBGMSG(debugStream,3, (int) svm->svn << " " << svm->meas << " " << std::fixed << std::setprecision(6) << " " << rxm->gpstow << " " << tk << " " 
		  << gpsDayOfWeek << " " << toc << " " << clockCorrection << " " << range << " " << (int) (ms*1000) << " " << svdist << " " << svrange
			<< " " << sqrt((antenna->x-ax)*(antenna->x-ax)+(antenna->y-ay)*(antenna->y-ay)+(antenna->z-az)*(antenna->z-az))
			<< " " << Ek << " " << x[0] << " " << x[1] << " " << x[2]);

	}
	return ok;
}



	
