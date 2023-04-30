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


#ifndef __ANTENNA_H_
#define __ANTENNA_H_

#include <string>


class Antenna
{
	public:
	
		Antenna()
		{
			markerName="MNAME";
			markerNumber="MNUM";
			markerType="NON_GEODETIC"; // required for V3 RINEX
			antennaNumber="ANUM";
			antennaType="ATYPE";
			x=y=z=0.0;
			deltaH=deltaE=deltaN=0.0;
			frame="REFFRAME";
		}
		
		std::string markerName;
		std::string markerNumber;
		std::string markerType;
		std::string antennaNumber;
		std::string antennaType;
		double x,y,z;
		double deltaH,deltaE,deltaN;
		std::string frame;
		
		double latitude,longitude,height; // these are calculated from (x,y,z)
		
};

#endif

