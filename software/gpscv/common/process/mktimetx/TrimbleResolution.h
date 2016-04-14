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


#ifndef __TRIMBLE_RESOLUTION_H_
#define __TRIMBLE_RESOLUTION_H_

#include <string>

#include "Receiver.h"

using namespace std;

class TrimbleResolution:public Receiver
{
	public:
		
		enum Models {ResolutionT,ResolutionSMT,Resolution360};
		
		TrimbleResolution(Antenna *,string);
		virtual ~TrimbleResolution();
	
		virtual bool readLog(string,int);
		
	protected:
	
	private:
		bool resolveMsAmbiguity(ReceiverMeasurement *,SVMeasurement *,double *);
		
		int appvermajor,appverminor,appmonth,appday,appyear;
		int corevermajor,coreverminor,coremonth,coreday,coreyear;
	
		int model;
		
};

#endif

