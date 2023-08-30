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

#ifndef __RINEX_OBS_FILE_H_
#define __RINEX_OBS_FILE_H_

#include <fstream>
#include <string>
#include <vector>

#include "Measurements.h"
#include "RINEXFile.h"

class RINEXObsFile:public RINEXFile
{
	
	public:
	
		
		RINEXObsFile();
		virtual ~RINEXObsFile();
		virtual bool read(std::string,int,int);
		RINEXObsFile* merge(RINEXObsFile &);
		
		double obsInterval;
		std::vector<std::string> rnxCodes; 
		std::vector<int> rnxCols; // index into data columns that we want to extract from the RINEX file
		
		struct tm tmFirstObs,tmLastObs; // time (Unix) of first and last observations          
		double firstObsSecs,lastObsSecs; // alas, can't represent correctly in struct tm
		time_t ttFirstObs,ttLastObs;
		
		int timeSystem;
		
		Measurements gps; // .. etc
		
	protected:
		
		
		
	private:
		
		void init();
		
		bool readV3File(std::string);
	
		int readV3Obs(Measurements &, int, int,std::string);

		int code2RNXcol(std::string c);
		
};

#endif
