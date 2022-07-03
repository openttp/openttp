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

#ifndef __GALILEO_H_
#define __GALILEO_H_

#include <time.h>
#include <vector>

#include "GNSSSystem.h"

class Antenna;
class ReceiverMeasurement;
class SVMeasurement;

class GalEphemeris:public Ephemeris
{
	public:
		
		
		UINT8 SVN;
		SINGLE TOW; // time of ephemeris transmission
		UINT16 WN;
		UINT16 IODnav;
		SINGLE BGD_E1E5a;
		SINGLE BGD_E1E5b;
		UINT8  sigFlags; // validity and health, lower 6 bits as per ICD 
		SINGLE t_0c;
		SINGLE a_f2;
		SINGLE a_f1;
		SINGLE a_f0;
		SINGLE SISA;
		SINGLE C_rs;
		SINGLE delta_N;
		DOUBLE M_0;
		SINGLE C_uc;
		DOUBLE e;
		SINGLE C_us;
		DOUBLE sqrtA;
		SINGLE t_0e;
		SINGLE C_ic;
		DOUBLE OMEGA_0;
		SINGLE C_is;
		DOUBLE i_0;
		SINGLE C_rc;
		DOUBLE OMEGA;
		SINGLE OMEGADOT;
		SINGLE IDOT;
		
		int tLogged; // TOD the ephemeris message was logged in seconds - used for debugging
		unsigned char subframes; // used to flag receipt of each subframe
		int dataSource;  // set by the receiver capabilities
		
		GalEphemeris(){subframes=0;}
		virtual ~GalEphemeris(){};
		
		virtual double t0e(){return t_0e;}
		virtual double t0c(){return t_0c;}
		virtual int    svn(){return SVN;}
		virtual int    iod(){return IODnav;}
		
};
	
class Galileo: public GNSSSystem
{
	public:
	
	class IonosphereData
	{
		public:
			SINGLE ai0,ai1,ai2;
			unsigned char SFflags; // ionospheric disturbance
	};

	class UTCData // GSt->UTC, same as GPS
	{
		public:
			DOUBLE A0;
			SINGLE A1;
			SINT16 dt_LS;
			SINGLE t_0t;
			UINT16 WN_0t,WN_LSF,DN;
			SINT16 dt_LSF;	
	};

	class GPSData // GST -> GPS
	{
		public:
			DOUBLE A_0G;
			DOUBLE A_1G;
			UINT32 t_0G;
			UINT32 WN_0G;
	};
	
	Galileo();
	~Galileo();
	virtual double codeToFreq(int);
	
	double decodeSISA(unsigned char);
	
	virtual int maxSVN(){return NSATS;}
	
	virtual bool resolveMsAmbiguity(Antenna *,ReceiverMeasurement *,SVMeasurement *,double *);
	
	virtual void setAbsT0c(int);
	
	IonosphereData ionoData;
	UTCData UTCdata;
	GPSData GPSdata;
			
	bool currentLeapSeconds(int mjd,int *leapsecs);
	
	bool gotUTCdata,gotIonoData;
	
	private:
		
		static const int NSATS=36;
		
};

#endif



