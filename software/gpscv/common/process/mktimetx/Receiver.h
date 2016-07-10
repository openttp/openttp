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

#ifndef __RECEIVER_H_
#define __RECEIVER_H_

#include <time.h>

#include <string>
#include <vector>
#include <boost/iterator/iterator_concepts.hpp>

#include "GPS.h"
#include "SVMeasurement.h"

using namespace std;

class ReceiverMeasurement;
class Antenna;

class Receiver
{
	public:
	
		enum SawtoothPhase {CurrentSecond,NextSecond,ReceiverSpecified};
		
		Receiver(Antenna *);
		virtual ~Receiver();
		
		//void setProcessingInterval(int,int);
		
		virtual unsigned int memoryUsage();
		
		string version; // RX version information used to control processing - set in config NOT by information in RX files
			    
		string modelName;
		string manufacturer;
		string version1;
		string version2;
		string serialNumber;
		
		string swversion;
		
		bool dualFrequency;
		int constellations;
		int codes;
		int channels;
		int commissionYYYY; // year of commissioning, used in CGGTTS files
		int leapsecs;
		
		int ppsOffset; // 1 pps offset, in nanoseconds
		
		virtual bool readLog(string,int){return true;} // must be reimplemented
		
		vector<ReceiverMeasurement *> measurements;
		
		int sawtoothPhase; // pps to apply sawtooth correction to
		
		GPS gps;
		
		Antenna *antenna;
		
	protected:
	
		//bool setCurrentLeapSeconds(int,UTCData &);
		
		void deleteMeasurements(std::vector<SVMeasurement *> &);
		void interpolateMeasurements(std::vector<ReceiverMeasurement *> &);
		bool resolveMsAmbiguity(ReceiverMeasurement *,SVMeasurement *,double *);
		
		bool gotUTCdata,gotIonoData;
		
	private:
	
};
#endif

