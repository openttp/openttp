//
//
// The MIT License (MIT)
//
// Copyright (c) 2017  Michael J. Wouters
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

#ifndef __GLONASS_H_
#define __GLONASS_H_

#include <time.h>
#include <vector>

#include "GNSSSystem.h"

class Antenna;
class ReceiverMeasurement;
class SVMeasurement;


class GLONASS: public GNSSSystem
{
	private:
		
		static const int NSATS=32;
		
	public:
	
	class IonosphereData
	{
		public:
			
	};

	class UTCData
	{
		public:
			
	};

	class EphemerisData
	{
		public:
			
	};
	
	GLONASS();
	~GLONASS();
	virtual double codeToFreq(int,int);
	virtual int maxSVN(){return NSATS;}
	virtual void deleteEphemeris();
	virtual bool resolveMsAmbiguity(Antenna* antenna,ReceiverMeasurement *,SVMeasurement *,double *);
	
	IonosphereData ionoData;
	UTCData UTCdata;
	std::vector<EphemerisData *> ephemeris;
			
	bool currentLeapSeconds(int mjd,int *leapsecs);
	
	time_t L1lastunlock[NSATS+1]; // used for tracking loss of carrier-phase lock
	
	
};

#endif



