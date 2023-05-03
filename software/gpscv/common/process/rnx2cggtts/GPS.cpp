//
//
// The MIT License (MIT)
//
// Copyright (c) 2023  Michael J. Wouters, Malcolm Lawn
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


#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <ostream>

#include "Application.h"
#include "Antenna.h"
#include "Debug.h"
#include "GPS.h"
#include "Troposphere.h"

#define MU 3.986005e14 // WGS 84 value of the earth's gravitational constant for GPS USER
#define OMEGA_E_DOT 7.2921151467e-5
#define F -4.442807633e-10
#define MAX_ITERATIONS 10 // for solution of the Kepler equation

extern Application *app;
extern std::ostream *debugStream;
extern std::string   debugFileName;
extern std::ofstream debugLog;
extern int verbosity;

#define CLIGHT 299792458.0

// Lookup table to convert URA index [0,15] to URA value in m for SV accuracy
static const double URAvalues[] = {2,2.8,4,5.7,8,11.3,16,32,64,128,256,512,1024,2048,4096,0.0};
const double* GPS::URA = URAvalues;
const double  GPS::fL1 = 1575420000.0; // 154*10.23 MHz
const double  GPS::fL2 = 1227620000.0; // 120*10.23 MHz


void GPSEphemeris::dump()
{
	char buf[81];
	
	std::snprintf(buf,80,"%%4s%%19.12e%%19.12e%%19.12e%%19.12e\n");
	
	std::fprintf(stdout,buf," ", // broadcast orbit 1
					(double) IODE, C_rs, delta_N, M_0);
			
	std::fprintf(stdout,buf," ", // broadcast orbit 2
		C_uc,e,C_us, sqrtA);
	
	std::fprintf(stdout,buf," ", // broadcast orbit 3
		t_0e, C_ic, OMEGA_0,C_is);
	
	std::fprintf(stdout,buf," ", // broadcast orbit 4
		i_0,C_rc,OMEGA,OMEGADOT);
	
	std::fprintf(stdout,buf," ", // broadcast orbit 5
		IDOT,1.0,(double) week_number,0.0); // FIXME this is truncated WN
	
	std::fprintf(stdout,buf," ", // broadcast orbit 6
		GPS::URA[SV_accuracy_raw],(double) SV_health,t_GD,(double) IODC);
	
	std::fprintf(stdout,"%4s%19.12e%19.12e\n"," ",t_ephem,4.0); // broadcast orbit 7

}


GPS::GPS():GNSSSystem()
{
	n="GPS";
	olc="G";
	codes = GNSSSystem::C1C;
	gotUTCdata = gotIonoData = false; // sometimes these are not in the nav file
}

GPS::~GPS()
{
}



Ephemeris* GPS::nearestEphemeris(int svn,double tow,double maxURA)
{
	Ephemeris *ed=NULL;
	double dt,tmpdt;
	
	if (sortedEphemeris[svn].size()==0)
		return ed;
	
	// This algorithm does not depend on the ephemeris being sorted
	// (and, at present, because week rollovers are not accounted for,it isn't fully sorted)
	for (unsigned int i=0;i<sortedEphemeris[svn].size();i++){
		GPSEphemeris *ephi = dynamic_cast<GPSEphemeris *>(sortedEphemeris[svn][i]);
		tmpdt= ephi->t_0e - tow;
		//DBGMSG(debugStream,INFO,ephi->t_0e << " " << tmpdt << " " << ephi->SV_accuracy << " " << maxURA)
		// handle week rollover
		if (tmpdt < -5*86400){ 
			tmpdt += 7*86400;
		}
		// algorithm as per previous software
		// Initially, we pick the first ephemeris after TOW that is close enough 
		if ((ed==NULL) && (tmpdt >=0) && (fabs(tmpdt) < 0.1*86400) && (ephi->SV_accuracy <= maxURA)){ // first time
			dt=fabs(tmpdt);
			ed=ephi;
		}
		// then we try to find a closer one
		else if ((ed!= NULL) && (fabs(tmpdt) < dt) && (tmpdt >=0 ) && (fabs(tmpdt) < 0.1*86400) && (ephi->SV_accuracy <= maxURA)){
			dt=fabs(tmpdt);
			ed=ephi;
		}
	}
	
	//DBGMSG(debugStream,INFO,"svn="<<svn << ",tow="<<tow<<",t_0e="<< ((ed!=NULL)?(int)(ed->t0e()):-1));
	
	return ed;
}

bool GPS::getPseudorangeCorrections(double gpsTOW, double pRange, Antenna *ant,Ephemeris *ephd,int signal,
			double *refsyscorr,double *refsvcorr,double *iono,double *tropo,
			double *azimuth,double *elevation, int *ioe)
{
	
	GPSEphemeris *ed  = dynamic_cast<GPSEphemeris *>(ephd);
	
	*refsyscorr=*refsvcorr=0.0;
	bool ok=false;
	
	// ICD 20.3.3.3.3.2
	double freqCorr=1.0; 
	switch (signal){
		case GNSSSystem::C1C: case GNSSSystem::C1P:
			freqCorr=1.0;
			break;
		case GNSSSystem::C2P: case GNSSSystem::C2C:
			freqCorr=77.0*77.0/(60.0*60.0);
			break;
		default:
			break;
	}
	
	if (ed != NULL){
		double x[3],Ek;
		
		*ioe=ed->IODE;
		
		int igpslt = gpsTOW; // take integer part of GPS local time (but it's integer anyway ...) 
		int gpsDayOfWeek = igpslt/86400; 
		//  if it is near the end of the day and toc->hour is < 6, then the ephemeris is from the next day.
		int tmpgpslt = igpslt % 86400;
		double toc=ed->t_OC; // toc is clock data reference time
		int tocDay=(int) toc/86400;
		toc-=86400*tocDay;
		int tocHour=(int) toc/3600;
		toc-=3600*tocHour;
		int tocMinute=(int) toc/60;
		toc-=60*tocMinute;
		int tocSecond=(int) toc;
		if (tmpgpslt >= (86400 - 6*3600) && (tocHour < 6))
			  gpsDayOfWeek++;
		toc = gpsDayOfWeek*86400 + tocHour*3600 + tocMinute*60 + tocSecond;
	
		// Calculate clock corrections (ICD 20.3.3.3.3.1)
		double gpssvt= gpsTOW - pRange;
		double clockCorrection = ed->a_f0 + ed->a_f1*(gpssvt - toc) + ed->a_f2*(gpssvt - toc)*(gpssvt - toc); // SV PRN code phase offset
		double tk = gpssvt - clockCorrection;
		
		double range,svdist,svrange,ax,ay,az;
		if (satXYZ(ed,tk,&Ek,x)){
			double relativisticCorrection =  -4.442807633e-10*ed->e*ed->sqrtA*sin(Ek);
			range = pRange + clockCorrection + relativisticCorrection - freqCorr*ed->t_GD;
			// Sagnac correction (ICD 20.3.3.4.3.4)
			ax = ant->x - OMEGA_E_DOT * ant->y * range;
			ay = ant->y + OMEGA_E_DOT * ant->x * range;
			az = ant->z ;
			
			svrange= (pRange+clockCorrection) * CLIGHT;
			svdist = sqrt( (x[0]-ax)*(x[0]-ax) + (x[1]-ay)*(x[1]-ay) + (x[2]-az)*(x[2]-az));
			double err  = (svrange - svdist);
			
			// Azimuth and elevation of SV
			double R=sqrt(ant->x*ant->x+ant->y*ant->y+ant->z*ant->z); 
			double p=sqrt(ant->x*ant->x+ant->y*ant->y);
			*elevation = 57.296*asin((ant->x*(x[0] - ant->x) + ant->y*(x[1] - ant->y) + ant->z*(x[2] - ant->z))/(R*svdist));
			*azimuth = 57.296 * atan2( (-(x[0] - ant->x)*ant->y +(x[1] - ant->y)*ant->x) * R,	 
								-(x[0] - ant->x)*ant->x*ant->z -(x[1] - ant->y)*ant->y*ant->z +(x[2] - ant->z)*p*p);

			if(*azimuth < 0) *azimuth += 360;
			
			if(fabs(err/CLIGHT) < 1000.0e-9){
			
				*refsyscorr=(clockCorrection + relativisticCorrection - freqCorr*ed->t_GD - svdist/CLIGHT)*1.0E9;
				*refsvcorr =(                  relativisticCorrection - freqCorr*ed->t_GD - svdist/CLIGHT)*1.0E9;
									
				*tropo = Troposphere::delayModel(*elevation,ant->height);
				
				*iono = freqCorr*ionoDelay(*azimuth, *elevation, ant->latitude, ant->longitude,gpsTOW,
					ionoData.a0,ionoData.a1,ionoData.a2,ionoData.a3,
					ionoData.B0,ionoData.B1,ionoData.B2,ionoData.B3);
				
				ok=true;
			}
			else{
				DBGMSG(debugStream,WARNING,"Error too big : " << 1.0E9*fabs(err/CLIGHT) << "ns");
			}
		}
		else{
			DBGMSG(debugStream,WARNING,"Failed");
			ok=false;
		}	
	}
	else{
		ok=false;
	}
		
	return ok;
}

bool GPS::satXYZ(GPSEphemeris *ed,double t,double *Ek,double x[3])
{
	// t is GPS system time at time of transmission
	
	double tk; // time from ephemeris reference epoch
	int nit;
	double Mk,Ekold=0.0;
	double A=ed->sqrtA*ed->sqrtA;
	double e=ed->e;
	
	// as per the ICD 20.3.3.4.3.1, account for beginning/end of week crossovers
	if ( (tk = t - ed->t_0e) > 302400) tk -= 604800;
	else if (tk < -302400) tk += 604800; // make (-302400 <= tk <= 302400)
	
	// solve Kepler's Equation for the Eccentric Anomaly by iteration
	*Ek = Mk = ed->M_0 + (sqrt(MU/(A*A*A)) + ed->delta_N)*tk;
	for (nit=0; nit != MAX_ITERATIONS; nit++){
		*Ek = Mk + e*sin(Ekold = *Ek);
		if (fabs(*Ek-Ekold) < 1e-8) break;
	}
	if (nit == MAX_ITERATIONS){
		//DBGMSG(debugStream,WARNING,"No convergence");
		return false;
	}
	
	double phik= atan2(sqrt(1-e*e)*sin(*Ek),cos(*Ek) - e) + ed->OMEGA;
	
	double uk = phik            + ed->C_us*sin(2*phik) + ed->C_uc*cos(2*phik) ;
	double rk = A*(1-e*cos(*Ek)) + ed->C_rc*cos(2*phik) + ed->C_rs*sin(2*phik);
	double ik = ed->i_0 + ed->IDOT*tk + ed->C_ic*cos(2*phik) + ed->C_is*sin(2*phik);
	double xkprime = rk*cos(uk);
	double ykprime = rk*sin(uk);
	double omegak = ed->OMEGA_0 + (ed->OMEGADOT - OMEGA_E_DOT)*tk - OMEGA_E_DOT*ed->t_0e;
	
	x[0] = xkprime*cos(omegak) - ykprime*cos(ik)*sin(omegak);
	x[1] = xkprime*sin(omegak) + ykprime*cos(ik)*cos(omegak);
	x[2] = ykprime*sin(ik);

	return true;
}

#undef F

double GPS::ionoDelay(double az, double elev, double lat, double longitude, double GPSt,
	float alpha0,float alpha1,float alpha2,float alpha3,
	float beta0,float beta1,float beta2,float beta3)
{
	// Model as per IS-GPS-200H pg 126 (Klobuchar model)
	// nb GPSt is forced into the range [0,86400]
	double psi, phi_i,lambda_i, t, phi_m, PER, x, F, Tiono, phi_u;
	double lambda_u, AMP;
	double pi=3.141592654;
	
	az = az/180.0; // satellite azimuth in semi-circles
	elev = elev/180.0; // satellite elevation in semi-circles

	phi_u = lat/180.0; // phi-u user geodetic latitude (semi-circles) 
	lambda_u = longitude/180.0; // lambda-u user geodetic longitude (semi-circles)

	psi = 0.0137/(elev + 0.11) - 0.022;

	phi_i = phi_u + psi*cos(az*pi);

	if(phi_i > 0.416){phi_i = 0.416;}
	if(phi_i < -0.416){phi_i = -0.416;}
	
	lambda_i = lambda_u + (psi*sin(az*pi)/cos(phi_i*pi));

	t = 4.32e4 * lambda_i + GPSt;

	while (t >= 86400) {t -=86400;} // FIXME
	while (t < 0) {t +=86400;}      // FIXME

	phi_m = phi_i + 0.064*cos((lambda_i - 1.617)*pi); // units of lambda_i are semicircles, hence factor of pi

	PER = beta0 + beta1*phi_m + beta2*pow(phi_m,2) + beta3*pow(phi_m,3);
	if(PER < 72000){PER = 72000;}

	x = 2*pi*(t - 50400)/PER ;

	AMP = alpha0 + alpha1*phi_m + alpha2*pow(phi_m,2) + alpha3*pow(phi_m,3);
	if(AMP < 0){AMP = 0;}

	F = 1+16*pow((0.53 - elev),3);

	if(fabs(x) < 1.57)
		Tiono = F*(5e-9 + AMP*(1 - pow(x,2)/2 + pow(x,4)/24));
	else
		Tiono = F*5e-9;

	return(Tiono*1e9);

} // ionnodelay

double GPS::codeToFreq(int c)
{
	double f=1.0;
	switch(c){
		case GNSSSystem::C1C: case GNSSSystem::C1P:case GNSSSystem::L1C: case GNSSSystem::L1P:
			f = GPS::fL1;break;
		case GNSSSystem::C2C: case GNSSSystem::C2P:case GNSSSystem::C2M:case GNSSSystem::L2C:case GNSSSystem::L2P:case GNSSSystem::L2L:
			f = GPS::fL2;break;
	}
	return f;
}

#undef CLIGHT

