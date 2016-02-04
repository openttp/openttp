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

#ifndef __RECEIVER_H_
#define __RECEIVER_H_

#include <time.h>

#include <string>
#include <vector>
#include <boost/iterator/iterator_concepts.hpp>

#include "SVMeasurement.h"

using namespace std;

typedef double DOUBLE;
typedef float  SINGLE;
typedef unsigned char UINT8;
typedef signed char SINT8;
typedef unsigned short UINT16;
typedef short SINT16;
typedef int SINT32;
typedef unsigned int UINT32;

class ReceiverMeasurement;

class IonosphereData
{
	public:
		SINGLE a0,a1,a2,a3;
		SINGLE B0,B1,B2,B3;
};

class UTCData
{
	public:
		DOUBLE A0;
		SINGLE A1;
		SINT16 dtlS;
		SINGLE t_ot;
		UINT16 WN_t,WN_LSF,DN;
		SINT16 dt_LSF;
};

class EphemerisData
{
	public:
		UINT8 SVN;
		SINGLE t_ephem;
		UINT16 week_number;
		UINT8 SV_accuracy_raw;
		UINT8 SV_health;
		UINT16 IODC;
		SINGLE t_GD;
		SINGLE t_OC;
		SINGLE a_f2;
		SINGLE a_f1;
		SINGLE a_f0;
		SINGLE SV_accuracy;
		UINT8 IODE;
		SINGLE C_rs;
		SINGLE delta_N;
		DOUBLE M_0;
		SINGLE C_uc;
		DOUBLE e;
		SINGLE C_us;
		DOUBLE sqrtA;
		SINGLE t_oe;
		SINGLE C_ic;
		DOUBLE OMEGA_0;
		SINGLE C_is;
		DOUBLE i_0;
		SINGLE C_rc;
		DOUBLE OMEGA;
		SINGLE OMEGADOT;
		SINGLE IDOT;
		DOUBLE Axis;
		DOUBLE n;
		DOUBLE r1me2;
		DOUBLE OMEGA_N;
		DOUBLE ODOT_n;
};

class Antenna;

class Receiver
{
	public:
		
		enum Constellation {GPS=0x01,GLONASS=0x02,BEIDOU=0x04,GALILEO=0x08};
		enum {NGPSSATS=32};
		
		Receiver(Antenna *);
		virtual ~Receiver();
		
		//void setProcessingInterval(int,int);
		
		string modelName;
		string manufacturer;
		string version1;
		string version2;
		string serialNumber;
		
		string swversion;
		
		int constellations;
		int channels;
		int commissionYYYY; // yaer of commissioning, used in CGGTTS files
		int leapsecs;
		
		int ppsOffset; // 1 pps offset, in nanoseconds
		
		virtual bool readLog(string,int){return true;} // must be reimplemented
		
		IonosphereData ionoData;
		UTCData        utcData;
		vector<ReceiverMeasurement *> measurements;
		vector<EphemerisData *> ephemeris;
		
		vector<EphemerisData *> sortedGPSEphemeris[NGPSSATS+1];
		EphemerisData *nearestEphemeris(int,int,int);
		
		Antenna *antenna;
		
	protected:
	
		bool setCurrentLeapSeconds(int,UTCData &);
		void addGPSEphemeris(EphemerisData *);
		
		void deleteMeasurements(std::vector<SVMeasurement *> &);
		void interpolateMeasurements(std::vector<ReceiverMeasurement *> &);
		
		bool gotUTCdata,gotIonoData;
		
	private:
	
};
#endif