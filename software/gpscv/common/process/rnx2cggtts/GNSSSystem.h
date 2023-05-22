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

#ifndef __GNSS_SYSTEM_H_
#define __GNSS_SYSTEM_H_

#include <ctime>
#include <string>
#include <vector>


typedef double DOUBLE;
typedef float  SINGLE;
typedef unsigned char UINT8;
typedef signed char SINT8;
typedef unsigned short UINT16;
typedef short SINT16;
typedef int SINT32;
typedef unsigned int UINT32;

class Antenna;

class Ephemeris
{
	public:
		
		virtual double t0e(){return 0.0;}
		virtual double t0c(){return 0.0;}
		virtual int    svn(){return 0;}
		virtual int    iod(){return 0;}
		virtual double tgd(){return 0;}
		virtual int healthy(){return 1;}
		
		Ephemeris(){};
		virtual ~Ephemeris(){};
		virtual void dump(){};
		
		// this is kinda messy but it's handy to have t0c corrected for week rollover,
		// converted to Unix time
		//time_t t0cAbs;
		//int correctedWeek;
		
		
};

class GNSSSystem
{
	
	public:
		
		enum Constellation {GPS=0x01,GLONASS=0x02,BEIDOU=0x04,GALILEO=0x08};
		
		GNSSSystem();
		~GNSSSystem();
		
		virtual int maxSVN(){return -1;}
		virtual int strToCode(std::string){return 0;}
		
		std::vector<Ephemeris *> ephemeris;
		std::vector<Ephemeris *> sortedEphemeris[37+1]; // FIXME this is the maximum number of SVNs (BDS currently)
		virtual bool addEphemeris(Ephemeris *);
		
		virtual Ephemeris *nearestEphemeris(int,double,double);
		
		virtual bool getPseudorangeCorrections(double gpsTOW, double pRange, Antenna *ant,Ephemeris *ephd,std::string obsCode,
			double *refsyscorr,double *refsvcorr,double *iono,double *tropo,
			double *azimuth,double *elevation, int *ioe);
		
	protected:
		std::string n; // system name
		std::string olc; // one letter code for the system
};

#endif
