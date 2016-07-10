//
//
// The MIT License (MIT)
//
// Copyright (c) 2015  Michael J. Wouters, Malcolm Lawn
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

#include <iostream>

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
extern ostream *debugStream;
extern string   debugFileName;
extern ofstream debugLog;
extern int verbosity;

#define CLIGHT 299792458.0

GPS::GPS():GNSSSystem()
{
	n="GPS";
}

GPS::~GPS()
{
}


void GPS::deleteEphemeris()
{
	DBGMSG(debugStream,TRACE,"deleting rx ephemeris");
	
	while(! ephemeris.empty()){
		EphemerisData  *tmp= ephemeris.back();
		delete tmp;
		ephemeris.pop_back();
	}
	
	for (unsigned int s=0;s<=NSATS;s++){
		sortedEphemeris[s].clear(); // nothing left to delete 
	}
}


// Adding to the ephemeris this way builds the sorted ephemeris
void GPS::addEphemeris(EphemerisData *ed)
{
	// Check whether this is a duplicate
	int issue;
	for (issue=0;issue < (int) sortedEphemeris[ed->SVN].size();issue++){
		if (sortedEphemeris[ed->SVN][issue]->t_oe == ed->t_oe){
			DBGMSG(debugStream,4,"ephemeris: duplicate SVN= "<< (unsigned int) ed->SVN << " toe= " << ed->t_oe);
			return;
		}
	}
	
	if (ephemeris.size()>0){

		// Update the ephemeris list - this is time-ordered
		std::vector<EphemerisData *>::iterator it;
		for (it=ephemeris.begin(); it<ephemeris.end(); it++){
			if (ed->t_OC < (*it)->t_OC){ // RINEX uses TOC
				DBGMSG(debugStream,4,"list inserting " << ed->t_OC << " " << (*it)->t_OC);
				ephemeris.insert(it,ed);
				break;
			}
		}
		
		if (it == ephemeris.end()){ // got to end, so append
			DBGMSG(debugStream,4,"appending " << ed->t_OC);
			ephemeris.push_back(ed);
		}
		
		// Update the ephemeris hash - 
		if (sortedEphemeris[ed->SVN].size() > 0){
			std::vector<EphemerisData *>::iterator it;
			for (it=sortedEphemeris[ed->SVN].begin(); it<sortedEphemeris[ed->SVN].end(); it++){
				if (ed->t_OC < (*it)->t_OC){ 
					DBGMSG(debugStream,4,"hash inserting " << ed->t_OC << " " << (*it)->t_OC);
					sortedEphemeris[ed->SVN].insert(it,ed);
					break;
				}
			}
			if (it == sortedEphemeris[ed->SVN].end()){ // got to end, so append
				DBGMSG(debugStream,4,"hash appending " << ed->t_OC);
				sortedEphemeris[ed->SVN].push_back(ed);
			}
		}
		else{ // first one for this SVN
			DBGMSG(debugStream,4,"first for svn " << (int) ed->SVN);
			sortedEphemeris[ed->SVN].push_back(ed);
		}
	}
	else{ //first one
		DBGMSG(debugStream,4,"first eph ");
		ephemeris.push_back(ed);
		sortedEphemeris[ed->SVN].push_back(ed);
		return;
	}
}

GPS::EphemerisData* GPS::nearestEphemeris(int svn,int tow)
{
	EphemerisData *ed = NULL;
	double dt,tmpdt;
	
	if (sortedEphemeris[svn].size()==0)
		return ed;
				
	for (unsigned int i=0;i<sortedEphemeris[svn].size();i++){
		tmpdt=sortedEphemeris[svn][i]->t_oe - tow;
		// handle week rollover
		if (tmpdt < -5*86400){ 
			tmpdt += 7*86400;
		}
		// algorithm as per previous software
		if (ed==NULL && tmpdt >=0 && fabs(tmpdt) < 0.1*86400){ // first time
			dt=fabs(tmpdt);
			ed=sortedEphemeris[svn][i];
		}
		else if ((ed!= NULL) && (fabs(tmpdt) < dt) && (tmpdt >=0 ) && fabs(tmpdt) < 0.1*86400){
			dt=fabs(tmpdt);
			ed=sortedEphemeris[svn][i];
		}
	}
				
	DBGMSG(debugStream,4,"svn="<<svn << ",tow="<<tow<<",t_oe="<< ((ed!=NULL)?(int)(ed->t_oe):-1));
	return ed;
}

bool GPS::currentLeapSeconds(int mjd,int *leapsecs)
{
	if (!(UTCdata.dtlS == 0 && UTCdata.dt_LSF == 0)){ 
		// Figure out when the leap second is/was scheduled; we only have the low
		// 8 bits of the week number in WN_LSF, but we know that "the
		// absolute value of the difference between the untruncated WN and Wlsf
		// values shall not exceed 127" (ICD 20.3.3.5.2.4, p122)
		int gpsWeek=int ((mjd-44244)/7);
		int gpsSchedWeek=(gpsWeek & ~0xFF) | UTCdata.WN_LSF;
		while ((gpsWeek-gpsSchedWeek)> 127) {gpsSchedWeek+=256;}
		while ((gpsWeek-gpsSchedWeek)<-127) {gpsSchedWeek-=256;}
		int gpsSchedMJD=44244+7*gpsSchedWeek+UTCdata.DN;
		// leap seconds is either tls or tlsf depending on past/future schedule
		(*leapsecs)=(mjd>=gpsSchedMJD? UTCdata.dt_LSF : UTCdata.dtlS);
		return true;
	}
	return false;
}

bool GPS::satXYZ(GPS::EphemerisData *ed,double t,double *Ek,double x[3])
{
	// t is GPS system time at time of transmission
	
	double tk; // time from ephemeris reference epoch
	int nit;
	double Mk,Ekold=0.0;
	double A=ed->sqrtA*ed->sqrtA;
	double e=ed->e;
	
	// as per the ICD 20.3.3.4.3.1, account for beginning/end of week crossovers
	if ( (tk = t - ed->t_oe) > 302400) tk -= 604800;
	else if (tk < -302400) tk += 604800; // make (-302400 <= tk <= 302400)
	
	// solve Kepler's Equation for the Eccentric Anomaly by iteration
	*Ek = Mk = ed->M_0 + (sqrt(MU/(A*A*A)) + ed->delta_N)*tk;
	for (nit=0; nit != MAX_ITERATIONS; nit++){
		*Ek = Mk + e*sin(Ekold = *Ek);
		if (fabs(*Ek-Ekold) < 1e-8) break;
	}
	if (nit == MAX_ITERATIONS){
		//fprintf(stderr,"no convergence for E\n");
		return false;
	}
	
	double phik= atan2(sqrt(1-e*e)*sin(*Ek),cos(*Ek) - e) + ed->OMEGA;
	
	double uk = phik            + ed->C_us*sin(2*phik) + ed->C_uc*cos(2*phik) ;
	double rk = A*(1-e*cos(*Ek)) + ed->C_rc*cos(2*phik) + ed->C_rs*sin(2*phik);
	double ik = ed->i_0 + ed->IDOT*tk + ed->C_ic*cos(2*phik) + ed->C_is*sin(2*phik);
	double xkprime = rk*cos(uk);
	double ykprime = rk*sin(uk);
	double omegak = ed->OMEGA_0 + (ed->OMEGADOT - OMEGA_E_DOT)*tk - OMEGA_E_DOT*ed->t_oe;
	
	x[0] = xkprime*cos(omegak) - ykprime*cos(ik)*sin(omegak);
	x[1] = xkprime*sin(omegak) + ykprime*cos(ik)*cos(omegak);
	x[2] = ykprime*sin(ik);

	return true;
}

double GPS::sattime(GPS::EphemerisData *ed,double Ek,double tsv,double toc)
{
	// SV clock correction as per ICD 20.3.3.3.3.1
	double trel= F*ed->e*ed->sqrtA*sin(Ek);
	return tsv+ed->a_f0 + ed->a_f1*(tsv - toc) + ed->a_f2*(tsv - toc)*(tsv - toc)+trel;
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

bool GPS::getPseudorangeCorrections(double gpsTOW, double pRange, Antenna *ant,
	EphemerisData *ed,int signal,
	double *refsyscorr,double *refsvcorr,double *iono,double *tropo,
	double *azimuth,double *elevation,int *ioe){
	
	*refsyscorr=*refsvcorr=0.0;
	bool ok=false;
	
	// ICD 20.3.3.3.3.2
	double tGDcorr=1.0;
	switch (signal){
		case GNSSSystem::C1: case GNSSSystem::P1:
			tGDcorr=1.0;
			break;
		case GNSSSystem::P2:
			tGDcorr=77.0*77.0/(60.0*60.0);
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
		if (GPS::satXYZ(ed,tk,&Ek,x)){
			double relativisticCorrection =  -4.442807633e-10*ed->e*ed->sqrtA*sin(Ek);
			range = pRange + clockCorrection + relativisticCorrection - tGDcorr*ed->t_GD;
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
			
				*refsyscorr=(clockCorrection + relativisticCorrection - tGDcorr*ed->t_GD - svdist/CLIGHT)*1.0E9;
				*refsvcorr =(                  relativisticCorrection - tGDcorr*ed->t_GD - svdist/CLIGHT)*1.0E9;
									
				*tropo = Troposphere::delayModel(*elevation,ant->height);
				
				*iono = ionoDelay(*azimuth, *elevation, ant->latitude, ant->longitude,gpsTOW,
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

// Convert UTC time to GPS time of week
// mktime is used - for this to work, the time zone must be UTC (set in main())

#define SECSPERWEEK 604800

void GPS::UTCtoGPS(struct tm *tmUTC, unsigned int nLeapSeconds,
	unsigned int *tow,unsigned int *truncatedWN,unsigned int *fullWN)
{
  // 315964800 is origin of GPS time, 00:00:00 Jan 6 1980 UTC, reckoned in Unix time
	time_t tGPS = mktime(tmUTC) + nLeapSeconds - 315964800;
	unsigned int GPSWeekNumber = (int) (tGPS/SECSPERWEEK);
	(*tow) = tGPS - GPSWeekNumber*SECSPERWEEK;
	if (truncatedWN) (*truncatedWN) = GPSWeekNumber % 1024;
	if (fullWN)      (*fullWN) = GPSWeekNumber;
}

// Convert GPS time to UTC time
// 
void GPS::GPStoUTC(unsigned int tow, unsigned int truncatedWN, unsigned int nLeapSeconds,
	struct tm *tmUTC)
{
	time_t tUTC = 315964800 + tow + truncatedWN*7*86400 - nLeapSeconds;
	
	// Now fix the truncated week number.
	// We'll require that it be later than
	// 2016-01-01 00:00:00 UTC == 1451606400 Unix time
	// which means it will bomb after I retire, and will then be Somebody Else's Problem
	
	int nRollovers = (1451606400 - tUTC)/(86400*7*1024); // mus be +ve since at least one rollover has taken place
	if (nRollovers < 0){
		cerr << "GPS time earlier than reference time - unable to resolve WN" << endl;
		exit(EXIT_FAILURE);
	}
	tUTC += nRollovers * 1024*7*86400;
	gmtime_r(&tUTC,tmUTC);
}

 time_t GPS::GPStoUnix(unsigned int tow, unsigned int truncatedWN){
	time_t tGPS = 315964800 + tow + truncatedWN*7*86400;
	
	// Now fix the truncated week number.
	// We'll require that it be later than
	// 2016-01-01 00:00:00 UTC == 1451606400 Unix time
	// which means it will bomb after I retire, and will then be Somebody Else's Problem
	
	int nRollovers = (1451606400 - tGPS)/(86400*7*1024); // mus be +ve since at least one rollover has taken place
	if (nRollovers < 0){
		cerr << "GPS time earlier than reference time - unable to resolve WN" << endl;
		exit(EXIT_FAILURE);
	}
	tGPS += nRollovers * 1024*7*86400;
	return tGPS;
}

#undef SECSPERWEEK

#undef CLIGHT

