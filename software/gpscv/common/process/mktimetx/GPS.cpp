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
#include <cstring>
#include <iostream>
#include <iomanip>
#include <ostream>

#include "Application.h"
#include "Antenna.h"
#include "Debug.h"
#include "GPS.h"
#include "ReceiverMeasurement.h"
#include "SVMeasurement.h"
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

GPS::GPS():GNSSSystem()
{
	n="GPS";
	olc="G";
	codes = GNSSSystem::C1C;
	for (int i=0;i<=NSATS;i++)
		memset((void *)(&L1lastunlock[i]),0,sizeof(time_t)); // all good
}

GPS::~GPS()
{
}

double GPS::codeToFreq(int c)
{
	double f=0.0;
	switch(c){
		case GNSSSystem::C1C: case GNSSSystem::C1P:case GNSSSystem::L1C: case GNSSSystem::L1P:
			f = GPS::fL1;break;
		case GNSSSystem::C2C: case GNSSSystem::C2P:case GNSSSystem::C2M:case GNSSSystem::L2C:case GNSSSystem::L2P:
			f = GPS::fL2;break;
	}
	return f;
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


// The ephemeris is sorted so that the RINEX navigation file is written correctly
// A hash table is also built for quick ephemeris lookup
// Note that when the ephemeris is completely read in, another fixup must be done for week rollovers

// t_0e and t_OC are usually the same 
// Some disagreements at the end of the GPS week but at other times too (when satellite picked up?)
// No particular relationship - sometimes t_oe is before t_OC, sometimes after
// Sometimes IODE will be the same ( and data the same too)
// eg SV 17 IODE 66 t_oe 597600 t_OC 597600
//    SV 17 IODE 66 t_oe 597600 t_OC 0
// eg SV 25 IODE 4 t_oe 532800 t_OC 532800
//    SV 25 IODE 4 t_oe 532800 t_OC 540000
// eg SV 24 IODE 13 t_oe 547200 t_OC 532800
//    SV 24 IODE 13 t_oe 547200 t_OC 547200
//
bool GPS::addEphemeris(EphemerisData *ed)
{
	// Check whether this is a duplicate
	int issue;
	for (issue=0;issue < (int) sortedEphemeris[ed->SVN].size();issue++){
		if (sortedEphemeris[ed->SVN][issue]->t_0e == ed->t_0e){
			DBGMSG(debugStream,4,"ephemeris: duplicate SVN= "<< (unsigned int) ed->SVN << " toe= " << ed->t_0e);
			return false;
		}
	}
	
	if (ephemeris.size()>0){

		// Update the ephemeris list - this is time-ordered
		
		std::vector<EphemerisData *>::iterator it;
		for (it=ephemeris.begin(); it<ephemeris.end(); it++){
			if (ed->t_OC < (*it)->t_OC){ // RINEX uses t_OC
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
		return true;
	}
	return true;
}

GPS::EphemerisData* GPS::nearestEphemeris(int svn,int tow,double maxURA)
{
	EphemerisData *ed = NULL;
	double dt,tmpdt;
	
	if (sortedEphemeris[svn].size()==0)
		return ed;
	
	// This algorithm does not depend on the ephemeris being sorted
	// (and, at present, because week rollovers are not accounted for,it isn't fully sorted)
	for (unsigned int i=0;i<sortedEphemeris[svn].size();i++){
		tmpdt=sortedEphemeris[svn][i]->t_0e - tow;
		// handle week rollover
		if (tmpdt < -5*86400){ 
			tmpdt += 7*86400;
		}
		// algorithm as per previous software
		// Initially, we pick the first ephemeris after TOW that is close enough 
		if ((ed==NULL) && (tmpdt >=0) && (fabs(tmpdt) < 0.1*86400) && (sortedEphemeris[svn][i]->SV_accuracy <= maxURA)){ // first time
			dt=fabs(tmpdt);
			ed=sortedEphemeris[svn][i];
		}
		// then we try to find a closer one
		else if ((ed!= NULL) && (fabs(tmpdt) < dt) && (tmpdt >=0 ) && (fabs(tmpdt) < 0.1*86400) && (sortedEphemeris[svn][i]->SV_accuracy <= maxURA)){
			dt=fabs(tmpdt);
			ed=sortedEphemeris[svn][i];
		}
	}
				
	DBGMSG(debugStream,4,"svn="<<svn << ",tow="<<tow<<",t_0e="<< ((ed!=NULL)?(int)(ed->t_0e):-1));
	
	return ed;
}

bool GPS::fixWeekRollovers()
{
	// There are two cases:
	// (1) We are at the end of the week and get an ephemeris for the next day. Week number can't be used to discriminate.
	//      In this case, the ephemeris needs to be moved to the end
	// (2) We are at the beginning of the week and get an ephemeris for the previous day. 
	//      This ephemeris must move to the beginning of the day
	// Note that further disambiguation information is available from the time the message was logged
	
	if (ephemeris.size() <= 1) return false;
	
	std::vector<EphemerisData *>::iterator it;
	for (it=ephemeris.begin(); it<ephemeris.end(); it++){
		GPS::EphemerisData *ed = (*it);
		//cout << (int) ed->SVN << " " << ed->t_oe << " " << ed->t_OC << " " << (int) ed->IODE << " " <<  (int) ed->tLogged << endl;
	}
	
	// Because the ephemeris has been ordered by t_OC, the misplaced ephemerides can be moved as a block
	
	// This handles case (1)
	int tOClast = ephemeris[0]->t_OC;
	for (unsigned i=1; i < ephemeris.size(); i++){
		GPS::EphemerisData *ed = ephemeris[i];
		if (ed->t_OC - tOClast > 5*86400){ // Detect the position of the break 
			DBGMSG(debugStream,INFO,"GPS week rollover detected in ephemeris");
			// Have to copy the first "i" entries to the end
			for (unsigned int j=0;j<i;j++)
				ephemeris.push_back(ephemeris[j]);
			// and then remove the first i entries
			ephemeris.erase(ephemeris.begin(),ephemeris.begin()+i);
			std::vector<EphemerisData *>::iterator it;
			for (it=ephemeris.begin(); it<ephemeris.end(); it++){
				GPS::EphemerisData *ed = (*it);
			//	cout << (int) ed->SVN << " " << ed->t_oe << " " << ed->t_OC << " " << (int) ed->IODE << " " <<  (int) ed->tLogged << endl;
			}
			return true;
		}
		else
			tOClast = ed->t_OC;
	}
	
	return false;
}

bool GPS::resolveMsAmbiguity(Antenna* antenna,ReceiverMeasurement *rxm,SVMeasurement *svm,double *corr)
{
	*corr=0.0;
	bool ok=false;
	// find closest ephemeris entry
	GPS::EphemerisData *ed = nearestEphemeris(svm->svn,rxm->gpstow,3.0); // max URA probably not important
	if (ed != NULL){
		double x[3],Ek;
		
		int igpslt = rxm->gpstow; // take integer part of GPS local time (but it's integer anyway ...) 
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
		int tocSecond=toc;
		if (tmpgpslt >= (86400 - 6*3600) && (tocHour < 6))
			  gpsDayOfWeek++;
		toc = gpsDayOfWeek*86400 + tocHour*3600 + tocMinute*60 + tocSecond;
	
		// Calculate clock corrections (ICD 20.3.3.3.3.1)
		double gpssvt= rxm->gpstow - svm->meas;
		double clockCorrection = ed->a_f0 + ed->a_f1*(gpssvt - toc) + ed->a_f2*(gpssvt - toc)*(gpssvt - toc); // SV PRN code phase offset
		double tk = gpssvt - clockCorrection;
		
		double range,ms,svdist,svrange,ax,ay,az;
		if (satXYZ(ed,tk,&Ek,x)){
			double trel =  -4.442807633e-10*ed->e*ed->sqrtA*sin(Ek);
			range = svm->meas + clockCorrection + trel - ed->t_GD;
			// Correction for Earth rotation (Sagnac) (ICD 20.3.3.4.3.4)
			ax = antenna->x - OMEGA_E_DOT * antenna->y * range;
			ay = antenna->y + OMEGA_E_DOT * antenna->x * range;
			az = antenna->z ;
			
			svrange= (svm->meas+clockCorrection) * CLIGHT;
			svdist = sqrt( (x[0]-ax)*(x[0]-ax) + (x[1]-ay)*(x[1]-ay) + (x[2]-az)*(x[2]-az));
			double err  = (svrange - svdist);
			ms   = (int)((-err/CLIGHT *1000)+0.5)/1000.0;
			*corr = ms;
			ok=true;
		}
		else{
			DBGMSG(debugStream,1,"Failed");
			ok=false;
		}
		DBGMSG(debugStream,3, (int) svm->svn << " " << svm->meas << " " << std::fixed << std::setprecision(6) << " " << rxm->gpstow << " " << tk << " " 
		  << gpsDayOfWeek << " " << toc << " " << clockCorrection << " " << range << " " << (int) (ms*1000) << " " << svdist << " " << svrange
			<< " " << sqrt((antenna->x-ax)*(antenna->x-ax)+(antenna->y-ay)*(antenna->y-ay)+(antenna->z-az)*(antenna->z-az))
			<< " " << Ek << " " << x[0] << " " << x[1] << " " << x[2]);

	}
	return ok;
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
	if ( (tk = t - ed->t_0e) > 302400) tk -= 604800;
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
	double omegak = ed->OMEGA_0 + (ed->OMEGADOT - OMEGA_E_DOT)*tk - OMEGA_E_DOT*ed->t_0e;
	
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

double GPS::measIonoDelay(unsigned int code1,unsigned int code2,double tpr1,double tpr2,double calDelay1,double calDelay2,EphemerisData *ed)
{
	// code1 is assumed to be the higher frequency
	// This returns the measured ionospheric delay for the higher of the two frequencies as per the CGGTTS V2E specification
	// For GPS, this will typically be the L1 frequency
	// Pseudoranges must be in seconds
	
	double f1,f2,GD=0.0;
	
    // FIXME GD is zero when the signal combination matches the broadcast clock ..
    GD=ed->t_GD;
	
	
	switch (code1){
		case GNSSSystem::C1C: case GNSSSystem::C1P:
			f1 = 77.0;
			break;
		default:
			break;
	}
	
	switch (code2){
		case GNSSSystem::C2P: case GNSSSystem::C2C:
			f2 = 60.0;
			break;
		default:
			break;
	}
    
	return (1.0 - f1*f1/(f1*f1 - f2*f2))*((tpr1-calDelay1) - (tpr2-calDelay2)) - GD; 
}


bool GPS::getPseudorangeCorrections(double gpsTOW, double pRange, Antenna *ant,
	EphemerisData *ed,int signal,
	double *refsyscorr,double *refsvcorr,double *iono,double *tropo,
	double *azimuth,double *elevation,int *ioe){
	
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
		if (GPS::satXYZ(ed,tk,&Ek,x)){
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
	struct tm *tmUTC,long refTime)
{
	
	if (truncatedWN > 1023){
		std::cerr << "GPS::GPStoUTC() truncated WN > 1023" << std::endl;
		exit(EXIT_FAILURE);
	}
	time_t tUTC = 315964800 + tow + truncatedWN*7*86400 - nLeapSeconds; 
	
	// Now fix the truncated week number.
	// tUTC - ref time must be greater than zero
	// If not, add  rollovers
	int nRollovers = 0;
	while (tUTC - refTime < 0){
	  tUTC += 1024*7*86400;
	  nRollovers++;
	}
	
	gmtime_r(&tUTC,tmUTC);
}

 time_t GPS::GPStoUnix(unsigned int tow, unsigned int truncatedWN,long refTime){
	 
	// See above ...
	if (truncatedWN > 1023){
		std::cerr << "GPS::GPStoUnix() truncated WN > 1023" << std::endl;
		exit(EXIT_FAILURE);
	}
	time_t tGPS = 315964800 + tow + truncatedWN*7*86400;
	while (tGPS - refTime < 0){
	  tGPS += 1024*7*86400;
	}
	return tGPS;
}

#undef SECSPERWEEK

#undef CLIGHT

