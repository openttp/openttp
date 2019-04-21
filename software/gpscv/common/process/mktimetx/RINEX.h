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

#ifndef __RINEX_H_
#define __RINEX_H_

#include <fstream>
#include <string>
#include <vector>

#include "BeiDou.h"
#include "GPS.h"

class Antenna;
class Counter;
class EphemerisData;
class MeasurementPair;
class Receiver;

class RINEX
{
	
	public:
		
		enum RINEXVERSIONS {V2=0, V3=1}; // used as array indices too ..
		
		RINEX();
		bool writeObservationFile(Antenna *ant, Counter *cntr, Receiver *rx,int ver,std::string fname,int mjd,int interval,MeasurementPair **mpairs,bool TICenabled);
		bool writeNavigationFile(Receiver *rx,int constellation,int ver,std::string fname,int mjd);
		
		bool readNavigationFile(Receiver *rx,int constellation,std::string fname);
		
		std::string makeFileName(std::string pattern,int mjd);
		
		std::string observer;
		std::string agency;
	
		bool allObservations;
		std::string observationCode(int);
		
	private:
		
		void init();
		bool readV2NavigationFile(Receiver* rx, int constellation,std::string fname);
		bool readV3NavigationFile(Receiver *rx,int constellation,std::string fname);
		
		GPS::EphemerisData* getGPSEphemeris(int ver,FILE *fin,unsigned int *lineCount);
		BeiDou::EphemerisData* getBeiDouEphemeris(FILE *fin,unsigned int *lineCount);
		
		bool writeGPSNavigationFile(Receiver *rx,int ver,std::string fname,int mjd);
		bool writeBeiDouNavigationFile(Receiver *rx,int ver,std::string fname,int mjd);
		
		char * formatFlags(int,int);
		
		void parseParam(char *str,int start,int len,int *val);
		void parseParam(char *str,int start,int len,float *val);
		void parseParam(char *,int start,int len,double *val);
		bool get4DParams(FILE *fin,int startCol,double *darg1,double *darg2,double *darg3,double *darg4,unsigned int *cnt);
		
};

#endif
