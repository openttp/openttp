//
//
// The MIT License (MIT)
//
// Copyright (c) 2017  Michael J. Wouters
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

#ifndef __BEIDOU_H_
#define __BEIDOU_H_

#include <time.h>
#include <vector>
#include <boost/concept_check.hpp>

#include "GNSSSystem.h"

class Antenna;
class ReceiverMeasurement;
class SVMeasurement;

typedef double DOUBLE;
typedef float  SINGLE;
typedef unsigned char UINT8;
typedef signed char SINT8;
typedef unsigned short UINT16;
typedef short SINT16;
typedef int SINT32;
typedef unsigned int UINT32;

class BeiDou: public GNSSSystem
{
	private:
		static const int NSATS=35;
		
	public:
	
	class IonosphereData
	{
		public:
			SINGLE a0,a1,a2,a3;
			SINGLE b0,b1,b2,b3;
	};

	class UTCData
	{
		public:
			DOUBLE A_0UTC,A_1UTC,A_2UTC;
			SINGLE t_ot;
			UINT16 WN_t;
			UINT16 dT_LS;
			UINT16 dT_LSF;
			UINT16 WN_LSF;
			UINT16 DN;
	};

	class EphemerisData
	{
		public:
			UINT8 SVN;
			UINT16 WN;
			UINT8 URAI;
			UINT8 SatH1;
			SINGLE t_GD1;
			SINGLE t_GD2;
			SINGLE AODC;
			SINGLE t_OC;
			SINGLE  a_0,a_1,a_2;
			UINT8  AODE;
			SINGLE t_oe;
			DOUBLE sqrtA;
			DOUBLE e;
			DOUBLE OMEGA;
			SINGLE delta_N;
			DOUBLE M_0;
			DOUBLE OMEGA_0;
			SINGLE OMEGADOT;
			DOUBLE i_0;
			SINGLE IDOT;
			SINGLE C_uc;
			SINGLE C_us;
			SINGLE C_rc;
			SINGLE C_rs;
			SINGLE C_ic;
			SINGLE C_is;
			
			DOUBLE tx_e; // transmission time of message - in RINEX
			int year,mon,mday,hour,mins,secs; // when read in a navigation file, we need to store this
	};
	
	BeiDou();
	~BeiDou();
	
	
	
	virtual int nsats(){return NSATS;}
	virtual void deleteEphemeris();
	
	virtual bool resolveMsAmbiguity(Antenna *,ReceiverMeasurement *,SVMeasurement *,double *);

	IonosphereData ionoData;
	UTCData UTCdata;
	std::vector<EphemerisData *> ephemeris;
	std::vector<EphemerisData *> sortedEphemeris[NSATS+1];
	
	void addEphemeris(EphemerisData *ed);
		
	bool currentLeapSeconds(int mjd,int *leapsecs);
	
	
};

#endif



