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

#include "BeiDou.h"
#include "Galileo.h"
#include "GLONASS.h"
#include "GPS.h"

#include "SVMeasurement.h"

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
		
		virtual void setVersion(std::string);
		std::string version(); // RX version information used to control processing - set in config NOT by information in RX files
			    
		std::string modelName;
		std::string manufacturer;
		std::string version1;
		std::string version2;
		std::string serialNumber;
		
		std::string swversion;
		
		bool dualFrequency;
		int constellations;
		int codes;
		int channels;
		int commissionYYYY; // year of commissioning, used in CGGTTS files
		int leapsecs;
		
		int ppsOffset; // 1 pps offset, in nanoseconds
		
		double sawtooth; // sawtooth, peak to peak, in ns;
		
		virtual bool readLog(std::string,int,int startTime=0,int stopTime=86399,int rinexObsInterval=30){return true;} // must be reimplemented
		
		std::vector<ReceiverMeasurement *> measurements;
		
		int sawtoothPhase; // pps to apply sawtooth correction to
		
		GPS gps;
		Galileo galileo;
		GLONASS glonass;
		BeiDou beidou;
		
		Antenna *antenna;
		
	protected:
	
		//bool setCurrentLeapSeconds(int,UTCData &);
		
		void deleteMeasurements(std::vector<SVMeasurement *> &);
		void interpolateMeasurements();
		bool gotUTCdata,gotIonoData;
		
	private:
	
		std::string version_;
		
};
#endif

