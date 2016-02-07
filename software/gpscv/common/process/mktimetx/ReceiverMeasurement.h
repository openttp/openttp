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

#ifndef __RECEIVER_MEASUREMENT_H_
#define __RECEIVER_MEASUREMENT_H_

#include <time.h>
#include <string>
#include <vector>
#include <boost/concept_check.hpp>

using namespace std;

class SVMeasurement;

class ReceiverMeasurement
{
	public:
		ReceiverMeasurement();
		~ReceiverMeasurement();
		
		unsigned int gpstow;
		unsigned int gpswn;
		double sawtooth; // units are seconds, ADDED to the counter measurement
		double timeOffset; // units are seconds, stored for timing diagnostics
		double signalLevel;
		int epochFlag;
		unsigned char pchh,pcmm,pcss; // time of measurement, as determined from the log time stamp
		struct tm tmGPS,tmUTC; // time of measurement, according to the receiver. tmGPS is mostly used but sometimes we get UTC for the receiver TOM
													 // and have to convert to GPS later when the number of leap seconds are known
		double tmfracs; // fractional part of time of measurement, units are seconds (s) - must be +ve
		
		vector<SVMeasurement*> gps;
		vector<SVMeasurement*> glonass;
	
};

#endif