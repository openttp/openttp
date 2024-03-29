//
//
// The MIT License (MIT)
//
// Copyright (c) 2014  Michael J. Wouters
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

#ifndef __GPS_H_
#define __GPS_H_

#include <time.h>
#include <vector>
#include <boost/concept_check.hpp>

#include "GNSSSystem.h"

class Antenna;
class ReceiverMeasurement;
class SVMeasurement;

class GPSEphemeris:public Ephemeris
{
	
	public:
		
		UINT8 SVN;
		SINGLE t_ephem;
		UINT16 week_number;
		UINT8 SV_accuracy_raw;  // this is URA index
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
		SINGLE t_0e;
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
		
		int tLogged; // TOD the ephemeris message was logged in seconds - used for debugging
		unsigned char subframes; // used to flag receipt of each subframe
		UINT8 f3IODE; // IODE from frame 3
		
		GPSEphemeris(){subframes=0;IODE=0;IODC=0;f3IODE=0;}
		~GPSEphemeris(){};
		
		virtual double t0e(){return t_0e;}
		virtual double t0c(){return t_OC;}
		virtual int    svn(){return SVN;}
		virtual int    iod(){return IODE;}
		
};
	
class GPS: public GNSSSystem
{
	private:
		
		static const int NSATS=32;
		
	public:
	
		static const double *URA; // User Range Accuracy table
		static const double fL1; // L1 frequency
		static const double fL2; // L2 frequency
		
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
			SINT16 dt_LS;
			SINGLE t_ot;
			UINT16 WN_t,WN_LSF,DN;
			SINT16 dt_LSF;
	};

	
	
	GPS();
	~GPS();
	
	virtual double codeToFreq(int);
	virtual int maxSVN(){return NSATS;}

	IonosphereData ionoData;
	UTCData UTCdata;
	
	bool resolveMsAmbiguity(Antenna *,ReceiverMeasurement *,SVMeasurement *,double *);
	GPSEphemeris *nearestEphemeris(int,int,double);
	virtual void setAbsT0c(int);
	
	bool satXYZ(GPSEphemeris *ed,double t,double *Ek,double x[3]);
	
	double sattime(GPSEphemeris *ed,double Ek,double tsv,double toc);
	
	double ionoDelay(double az, double elev, double lat, double longitude, double GPSt,
		float alpha0,float alpha1,float alpha2,float alpha3,
		float beta0,float beta1,float beta2,float beta3);
	
	double measIonoDelay(unsigned int code1,unsigned int code2,double tpr1, double tpr2,double calDelay1,double calDelay2,GPSEphemeris *ed);
	
	bool getPseudorangeCorrections(double gpsTOW, double pRange, Antenna *ant,GPSEphemeris *ed,int signal,
		double *refsyscorr,double *refsvcorr,double *iono,double *tropo,
		double *azimuth,double *elevation, int *ioe);
	
	static void UTCtoGPS(struct tm *tmUTC, unsigned int nLeapSeconds,
		unsigned int *tow,unsigned int *truncatedWN=NULL,unsigned int*fullWN=NULL);
	
	static void GPStoUTC(unsigned int tow, unsigned int truncatedWN, unsigned int nLeapSeconds,
		struct tm *tmUTC,long refTime);
	static time_t GPStoUnix(unsigned int tow, unsigned int truncatedWN,long refTime);
	
	bool currentLeapSeconds(int mjd,int *leapsecs);
	
	time_t L1lastunlock[NSATS+1]; // used for tracking loss of carrier-phase lock
	
	bool gotUTCdata,gotIonoData;
	
};

#endif



