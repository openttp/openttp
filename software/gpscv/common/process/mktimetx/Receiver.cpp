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

extern std::ostream *debugStream;

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
	codes=GNSSSystem::C1C;
	sawtoothPhase=CurrentSecond;
	sawtooth=0.0;
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

void Receiver::setVersion(std::string v)
{
	version_=v;
}

std::string Receiver::version()
{
	return version_;
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

void Receiver::interpolateMeasurements()
{
	// FIXME This uses Lagrange interpolation to estimate the pseudorange at tmfracs=0
	// Possibly, this method can be removed in the future
	
	DBGMSG(debugStream,1,"starting " << constellations );
	
	std::vector<SVMeasurement *> track;
	
	// For each SV, build up a list of all measurements for the day
	// Loop over all constellation+signal combinations
	
	for (int g = GNSSSystem::GPS; g<= GNSSSystem::GALILEO; (g<<= 1)){ 
		
		
		if (!(constellations & g)) continue;
		
		
		GNSSSystem *gnss;
		switch (g){
			case GNSSSystem::BEIDOU:gnss = &beidou;break;
			case GNSSSystem::GALILEO:gnss = &galileo ;break;
			case GNSSSystem::GLONASS:gnss = &glonass ;break;
			case GNSSSystem::GPS:gnss = &gps ;break;
		}
		DBGMSG(debugStream,1,"processing " << gnss->name());
		
		for (int code = GNSSSystem::C1C;code <=GNSSSystem::L7I; (code <<= 1)){
			
			if (!(codes & code)) continue;
			
			DBGMSG(debugStream,1,"GNSS code " << code);
			
			for (int svn=1;svn<=gnss->nsats();svn++){ // loop over all svn for constellation+code combination
				
				track.clear();
				for (unsigned int m=0;m<measurements.size();m++){
					for (unsigned int svm=0; svm < measurements.at(m)->meas.size();svm++){
						SVMeasurement *sv=measurements.at(m)->meas.at(svm);
						if ((svn == sv->svn) && (g==sv->constellation) && (code==sv->code)){
							sv->uibuf= mktime(&(measurements.at(m)->tmGPS));
							// FIXME initialise dbuf1? That way if data are patchy, at least we put in something roughly right ..
							sv->dbuf2=measurements.at(m)->tmfracs;
							track.push_back(sv);
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
								
						DBGMSG(debugStream,TRACE,"Track:" <<svn<< " " << " " << track.size() << " " << trackStart << "->" << trackStop );
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
						DBGMSG(debugStream,TRACE,"Track:" <<svn<< " " << track.at(trackStart)->dbuf2 << " " << track.at(trackStart)->meas << " " <<
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
					DBGMSG(debugStream,TRACE,svn << " " << i << " " << track.at(i)->uibuf << " " << track.at(i)->dbuf1 << " " << (track.at(i)->meas - track.at(i)->dbuf1)*1.0E9);
				}
				
			}

			track.clear();
			
		} // for (int code =
	}
	
	// Zero the fractional part of the measurement time
	for (unsigned int i=0;i<measurements.size();i++){
		measurements.at(i)->tmfracs=0;
	} 
			
	DBGMSG(debugStream,1,"done");
}




	
