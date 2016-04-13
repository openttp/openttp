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

#include "CounterMeasurement.h"
#include "ReceiverMeasurement.h"
#include "SVMeasurement.h"

ReceiverMeasurement::ReceiverMeasurement()
{
	sawtooth=0.0;
	timeOffset=0.0;
	epochFlag=0;
	signalLevel=0.0;
	tmfracs=0.0;
	cm=NULL;
}

ReceiverMeasurement::~ReceiverMeasurement()
{
	
	while(! gps.empty()){
		SVMeasurement  *tmp= gps.back();
		delete tmp;
		gps.pop_back();
	}
	
	while(! glonass.empty()){
		SVMeasurement  *tmp= glonass.back();
		delete tmp;
		glonass.pop_back();
	}
	
}

unsigned int ReceiverMeasurement::memoryUsage(){
	unsigned int mem=0; 
	for (unsigned int i=0;i<gps.size();i++){
		mem+=gps.at(i)->memoryUsage();
	}
	for (unsigned int i=0;i<gpsP1.size();i++){
		mem+=gpsP1.at(i)->memoryUsage();
	}
	for (unsigned int i=0;i<gpsP2.size();i++){
		mem+=gpsP2.at(i)->memoryUsage();
	}
	for (unsigned int i=0;i<glonass.size();i++){
		mem+=glonass.at(i)->memoryUsage();
	}
	return mem+sizeof(*this);
}
