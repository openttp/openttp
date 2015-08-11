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

#include "Counter.h"
#include "CounterMeasurement.h"
#include "Debug.h"


#define MAXSIZE 90000

extern ostream *debugStream;
//
//	public methods
//		

Counter::Counter()
{
}

Counter::~Counter()
{
}
	
bool Counter::readLog(string fname)
{
	
	DBGMSG(debugStream,1,"reading " << fname);
	// Expected format is
	// (optional) comments prefaced with
	// and then successive measurements like
	// HH:MM:SS reading_in_seconds
	
	struct stat statbuf;
	
	if ((0 != stat(fname.c_str(),&statbuf))){
		DBGMSG(debugStream,1," unable to stat " << fname);
		return false;
	}
	
	ifstream infile (fname.c_str());
	string line;
  if (infile.is_open()){
    while ( getline (infile,line) ){
			int hh,mm,ss;
			double rdg;
			if (4==sscanf(line.c_str(),"%d:%d:%d %lf",&hh,&mm,&ss,&rdg)){
				measurements.push_back(new CounterMeasurement(hh,mm,ss,rdg));
			}
			// check how much data there is - shouldn't be more than 86400 readings
			if (measurements.size() > MAXSIZE){ //something is really wrong
				DBGMSG(debugStream,1," data file is too large");
				return false;
			}
		}
	}
	else{
		DBGMSG(debugStream,1," unable to open " << fname);
		return false;
	}
	infile.close();
	DBGMSG(debugStream,1,"done: read " << measurements.size());
	DBGMSG(debugStream,1,"first " << measurements.front()->timestamp());
	DBGMSG(debugStream,1,"last " << measurements.back()->timestamp());
	return true;
}