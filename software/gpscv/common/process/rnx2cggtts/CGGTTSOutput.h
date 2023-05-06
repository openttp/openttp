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

#ifndef __CGGTTS_OUTPUT_H_
#define __CGGTTS_OUTPUT_H_

#include <string>

class CGGTTSOutput{
	public:
		
		CGGTTSOutput();
		CGGTTSOutput(int constellation,std::string rnxcode1,std::string rnxcode2,bool isP3,std::string frc,std::string path,std::string calID,
			double internalDelay,double internalDelay2,int delayKind,
			std::string ephemerisPath,std::string ephemerisFile
			);
		
		int constellation;
		std::string rnxcode1; // the code for single frequency output
		std::string rnxcode2; // the extra code for dual frequency output
		std::string rnxcode3; // extra extra code for single frequency output, but with MSIO reported
		bool isP3;    
		std::string FRC;
		std::string path;
		std::string calID;
		double internalDelay,internalDelay2;
		int delayKind;
		std::string ephemerisPath;
		std::string ephemerisFile;
		std::string reportMSIO; // report MSIO in single frequency output, if MSIO can be calculated, ehich cna only be determined after the RINEX files are loaded
		
};

#endif

