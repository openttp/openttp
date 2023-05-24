//
//
// The MIT License (MIT)
//
// Copyright (c) 2023  Michael J. Wouters
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

#ifndef __CGGTTS_H_
#define __CGGTTS_H_

#include <string>
#include <boost/concept_check.hpp>

class Antenna;
class Receiver;
class Measurements;
class GNSSDelay;
class GNSSSystem;

class CGGTTS
{
	
	public:
		
		enum CGGTTSVERSIONS {V1=0, V2E=2}; // used as array indices too ..
		
		
		CGGTTS();
	
		bool write(Measurements *meas,GNSSSystem *gnss,GNSSDelay *dly,int leapsecs1, std::string fname,int mjd,int startTime,int stopTime);

		
		Antenna *antenna;
		Receiver *receiver;
		
		std::string ref;
		std::string lab;
		std::string comment;
	
		int ver;
		int constellation; 	
		std::string rnxcode1;
		std::string rnxcode2;
		std::string rnxcode3;
		std::string FRC;
		bool reportMSIO;
		bool isP3;
		
		int revDateYYYY,revDateMM,revDateDD; // last date CGGTTS header was updated
		
		int minTrackLength; // in seconds
		double minElevation; // in degrees
		double maxDSG; // in ns
		double maxURA; // in m
		
	private:
		
		void init();
		void writeHeader(FILE *fout,GNSSSystem *gnss,GNSSDelay *dly);
		int checkSum(char *);
		
		//  a bit lazy - these are to avoid passing parameters to writeHeader
		
		double  dly1,dly2,dly3;
		
};

#endif

