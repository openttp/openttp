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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
  
#include <iostream>
#include <fstream>
#include <boost/lexical_cast.hpp>

#include "Application.h"
#include "Counter.h"
#include "CounterMeasurement.h"
#include "Debug.h"
#include "Timer.h"

#define MAXSIZE 90000

extern std::ostream *debugStream;
extern Application *app;

//
//	public methods
//		

Counter::Counter()
{
	flipSign=false;
}

Counter::~Counter()
{
}
	
bool Counter::readLog(std::string fname,int startTime,int stopTime)
{
	// Expected format is
	// (optional) comments prefaced with
	// and then successive measurements like
	// HH:MM:SS reading_in_seconds
	
	Timer timer;
	timer.start();
	
	double mSign=1.0;
	if (flipSign) mSign = -1.0;
	
	DBGMSG(debugStream,INFO,"reading " << fname);
	
	std::ifstream infile (fname.c_str());
	std::string line;
  if (infile.is_open()){
    while ( getline (infile,line) ){
			int hh,mm,ss,t;
			double rdg;
			if (4==sscanf(line.c_str(),"%d:%d:%d %lf",&hh,&mm,&ss,&rdg)){
				t = hh*3600+mm*60+ss;
				if (t>= startTime && t<= stopTime)
					measurements.push_back(new CounterMeasurement(hh,mm,ss,mSign*rdg));
			}
			// check how much data there is - shouldn't be more than 86400 readings
			if (measurements.size() > MAXSIZE){ //something is really wrong
				std::cerr << " data file is too large" << std::endl;
				return false;
			}
		}
	}
	else{
		std::cerr << " unable to open " << fname << std::endl;
		return false;
	}
	infile.close();
	timer.stop();
	
	app->logMessage("read " + boost::lexical_cast<std::string>(measurements.size()) + " counter measurements");
	DBGMSG(debugStream,INFO,"elapsed time: " << timer.elapsedTime(Timer::SECS) << " s");
	return true;
}

unsigned int Counter::memoryUsage()
{
	unsigned int mem=0;
	mem+= measurements.size()*(sizeof(CounterMeasurement *) + sizeof(CounterMeasurement));
	return mem+sizeof(*this);
}

