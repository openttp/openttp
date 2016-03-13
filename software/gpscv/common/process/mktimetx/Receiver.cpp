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
	codes=C1;
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

bool Receiver::setCurrentLeapSeconds(int mjd,UTCData &utcd)
{
	if (!(utcData.dtlS == 0 && utcData.dt_LSF == 0)){ 
		// Figure out when the leap second is/was scheduled; we only have the low
		// 8 bits of the week number in WN_LSF, but we know that "the
		// absolute value of the difference between the untruncated WN and Wlsf
		// values shall not exceed 127" (ICD 20.3.3.5.2.4, p122)
		int gpsWeek=int ((mjd-44244)/7);
		int gpsSchedWeek=(gpsWeek & ~0xFF) | utcData.WN_LSF;
		while ((gpsWeek-gpsSchedWeek)> 127) {gpsSchedWeek+=256;}
		while ((gpsWeek-gpsSchedWeek)<-127) {gpsSchedWeek-=256;}
		int gpsSchedMJD=44244+7*gpsSchedWeek+utcData.DN;
		// leap seconds is either tls or tlsf depending on past/future schedule
		leapsecs=(mjd>=gpsSchedMJD? utcData.dt_LSF : utcData.dtlS);
		return true;
	}
	return false;
}

void Receiver::addGPSEphemeris(EphemerisData *ed)
{
	// Check whether this is a duplicate
	int issue;
	for (issue=0;issue < (int) sortedGPSEphemeris[ed->SVN].size();issue++){
		if (sortedGPSEphemeris[ed->SVN][issue]->t_oe == ed->t_oe){
			DBGMSG(debugStream,4,"ephemeris: duplicate SVN= "<< (unsigned int) ed->SVN << " toe= " << ed->t_oe);
			return;
		}
	}
	
	if (ephemeris.size()>0){

		// Update the ephemeris list - this is time-ordered
		std::vector<EphemerisData *>::iterator it;
		for (it=ephemeris.begin(); it<ephemeris.end(); it++){
			if (ed->t_OC < (*it)->t_OC){ // RINEX uses TOC
				DBGMSG(debugStream,4,"list inserting " << ed->t_OC << " " << (*it)->t_OC);
				ephemeris.insert(it,ed);
				break;
			}
		}
		
		if (it == ephemeris.end()){ // got to end, so append
			DBGMSG(debugStream,4,"appending " << ed->t_OC);
			ephemeris.push_back(ed);
		}
		
		// Update the ephemeris hash - 
		if (sortedGPSEphemeris[ed->SVN].size() > 0){
			std::vector<EphemerisData *>::iterator it;
			for (it=sortedGPSEphemeris[ed->SVN].begin(); it<sortedGPSEphemeris[ed->SVN].end(); it++){
				if (ed->t_OC < (*it)->t_OC){ 
					DBGMSG(debugStream,4,"hash inserting " << ed->t_OC << " " << (*it)->t_OC);
					sortedGPSEphemeris[ed->SVN].insert(it,ed);
					break;
				}
			}
			if (it == sortedGPSEphemeris[ed->SVN].end()){ // got to end, so append
				DBGMSG(debugStream,4,"hash appending " << ed->t_OC);
				sortedGPSEphemeris[ed->SVN].push_back(ed);
			}
		}
		else{ // first one for this SVN
			DBGMSG(debugStream,4,"first for svn " << (int) ed->SVN);
			sortedGPSEphemeris[ed->SVN].push_back(ed);
		}
	}
	else{ //first one
		DBGMSG(debugStream,4,"first eph ");
		ephemeris.push_back(ed);
		sortedGPSEphemeris[ed->SVN].push_back(ed);
		return;
	}
	
}

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


EphemerisData *Receiver::nearestEphemeris(int constellation,int svn,int tow)
{
	EphemerisData *ed = NULL;
	double dt,tmpdt;
	switch (constellation){
		case Receiver::GPS:
			{
				if (sortedGPSEphemeris[svn].size()==0)
					return ed;
				ed=sortedGPSEphemeris[svn][0];
				dt=fabs(ed->t_oe - tow);
				for (unsigned int i=1;i<sortedGPSEphemeris[svn].size();i++){
					tmpdt=fabs(sortedGPSEphemeris[svn][i]->t_oe - tow);
					if (tmpdt < dt){
						ed=sortedGPSEphemeris[svn][i];
						dt=fabs(ed->t_oe - tow);
					}
				}
			}
			break;
		default:
			break;
	}
	DBGMSG(debugStream,4,"svn="<<svn << ",tow="<<tow<<",t_oe="<< ((ed!=NULL)?(int)(ed->t_oe):-1));
	return ed;
}

	