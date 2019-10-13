//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Michael J. Wouters
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
class Counter;
class MeasurementPair;
class Receiver;

class CGGTTS
{
	
	public:
		
		enum CGGTTSVERSIONS {V1=0, V2E=2}; // used as array indices too ..
		enum DELAYS {INTDLY=0,SYSDLY=2,TOTDLY=3};
		
		CGGTTS(Antenna *,Counter *,Receiver *);
		bool writeObservationFile(std::string fname,int mjd,int startTime,int stopTime,MeasurementPair **mpairs,bool TICenabled);
	
		static unsigned int strToCode(std::string,bool *);
		
		std::string ref;
		std::string lab;
		std::string comment;
		std::string calID; // for V2E only
		
		int ver;
		int constellation; 		// V2E
		unsigned int code;     // V2E
		int delayKind;         // V2E
		bool useMSIO;
		bool isP3;
		
		int revDateYYYY,revDateMM,revDateDD; // last date CGGTTS header was updated
		double intDly,intDly2,cabDly,refDly; // intDly is also SYSDLY or TOTDLY in v2E
		bool quadFits;
		
		int minTrackLength; // in seconds
		double minElevation; // in degrees
		double maxDSG; // in ns
		double maxURA; // in m
		
	private:
		
		void init();
		
		void writeHeader(FILE *fout);
		int checkSum(char *);
		
		Antenna *ant;
		Counter *cntr;
		Receiver *rx;
		
		std::string code1Str,code2Str;
};

#endif

