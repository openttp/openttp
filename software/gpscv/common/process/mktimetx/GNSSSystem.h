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

#ifndef __GNSS_SYSTEM_H_
#define __GNSS_SYSTEM_H_

#include <string>

class Antenna;
class ReceiverMeasurement;
class SVMeasurement;

class GNSSSystem
{
	
	public:
		
		enum Constellation {GPS=0x01,GLONASS=0x02,BEIDOU=0x04,GALILEO=0x08};
		enum Code {C1C=0x01,C1P=0x02,C2P=0x04,L1=0x08,L2=0x10,
				C2C=0x20,C2L=0x40,C2M=0x80,
				C2I=0x0100,C7I=0x0200,
				L2I=0x4000,L7I=0x8000}; // FIXME signal names are a dog's breakfast: use RINEXV3.03 from now on
		
		GNSSSystem(){};
		~GNSSSystem(){};
		
		virtual int nsats(){return -1;}
		std::string oneLetterCode(){return olc;}
		
		virtual void deleteEphemeris(){};
		
		std::string name(){return n;}
		
		virtual bool resolveMsAmbiguity(Antenna* antenna,ReceiverMeasurement *,SVMeasurement *,double *){return true;}
		
	protected:
		std::string n; // system name
		std::string olc; // one letter code for the system
};

#endif