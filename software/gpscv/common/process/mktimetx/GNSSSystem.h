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
		enum Code { // use RINEX V3 designations for the observation names
				C1C=0x01, // Galileo E1C, GLONASS, GPS
				C1P=0x02, // GLONASS,GPS
				C1B=0x04, // Galileo E1B
				C2C=0x08, // GLONASS,GPS
				C2P=0x10, // GLONASS,GPS   
				C2L=0x20, // GPS
				C2M=0x40, // GPS
				C2I=0x80, // BeiDou
				C7I=0x0100, // Galileo E5bI,BeiDou
				C7Q=0x0200, // Galileo E5bQ
				L1C=0x10000, // Boundary for carrier phase obervation
				L1P=0x20000,
				L2P=0x40000,
				L2C=0x80000,
                L2L=0x100000, 
				L2I=0x200000,
				L7I=0x400000,
				NONE=0x800000
		}; 
		
		GNSSSystem(){};
		~GNSSSystem(){};
		
		int codes; // observation codes
		static std::string observationCodeToStr(int c,int RINEXmajorVersion,int RINEXminorVersion=-1);
		static unsigned int strToObservationCode(std::string, int RINEXversion);
		
		virtual int maxSVN(){return -1;}
		std::string oneLetterCode(){return olc;}
	
		virtual void deleteEphemeris(){};
		
		std::string name(){return n;}
		
		virtual bool resolveMsAmbiguity(Antenna* antenna,ReceiverMeasurement *,SVMeasurement *,double *){return true;}
		
	protected:
		std::string n; // system name
		std::string olc; // one letter code for the system
		
};

#endif
