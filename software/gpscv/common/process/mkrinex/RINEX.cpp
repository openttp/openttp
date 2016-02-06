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

#include <cmath>
#include <cstdio>

#include "Antenna.h"
#include "Counter.h"
#include "Debug.h"
#include "MakeTimeTransferFile.h"
#include "MeasurementPair.h"
#include "Receiver.h"
#include "ReceiverMeasurement.h"
#include "RINEX.h"

extern MakeTimeTransferFile *app;
extern ostream *debugStream;

const char * RINEXVersionName[]= {"2.11","3.02"};

// Lookup table to convert URA index to URA value in m for SV accuracy
//      0   1 2   3 4    5  6  7  8   9  10  11   12   13   14  15
double URA[]={2,2.8,4,5.7,8,11.3,16,32,64,128,256,512,1024,2048,4096,0.0};

//
// Public methods
//
		
RINEX::RINEX(Antenna *a,Counter *c,Receiver *r)
{
	ant=a;
	cntr=c;
	rx=r;
	
	init();
}

bool RINEX::writeObservationFile(int ver,string fname,int mjd,int interval, MeasurementPair **mpairs)
{
	char buf[81];
	FILE *fout;
	if (!(fout = fopen(fname.c_str(),"w"))){
		return false;
	}
	
	char obs;
	switch (rx->constellations){
		case Receiver::GPS: obs='G';break;
		case Receiver::GLONASS: obs='R';break;
		case Receiver::GALILEO: obs='E';break;
		case Receiver::BEIDOU:obs='X';break; // not defined yet
		default: obs= 'M';break;
		
	}
	fprintf(fout,"%9s%11s%-20s%c%-19s%-20s\n",RINEXVersionName[ver],"","O",obs,"","RINEX VERSION / TYPE");
	
	time_t tnow = time(NULL);
	struct tm *tgmt = gmtime(&tnow);
	
	switch (ver){
		case V2:
		{
			strftime(buf,80,"%d-%b-%y %T",tgmt);
			fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			break;
		}
		case V3:
		{
			snprintf(buf,80,"%04d%02d%02d %02d%02d%02d UTC",tgmt->tm_year+1900,tgmt->tm_mon+1,tgmt->tm_mday,
					 tgmt->tm_hour,tgmt->tm_min,tgmt->tm_sec);
			fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			break;
		}
		default:break;
	}
	
	fprintf(fout,"%-60s%-20s\n",ant->markerName.c_str(),"MARKER NAME");
	fprintf(fout,"%-20s%40s%-20s\n",ant->markerNumber.c_str(),"","MARKER NUMBER");
	fprintf(fout,"%-20s%-40s%-20s\n",observer.c_str(),agency.c_str(),"OBSERVER / AGENCY");
	fprintf(fout,"%-20s%-20s%-20s%-20s\n",
		rx->serialNumber.c_str(),rx->modelName.c_str(),rx->version1.c_str(),"REC # / TYPE / VERS");
	fprintf(fout,"%-20s%-20s%-20s%-20s\n",ant->antennaNumber.c_str(),ant->antennaType.c_str()," ","ANT # / TYPE");
	fprintf(fout,"%14.4lf%14.4lf%14.4lf%-18s%-20s\n",ant->x,ant->y,ant->z," ","APPROX POSITION XYZ");
	fprintf(fout,"%14.4lf%14.4lf%14.4lf%-18s%-20s\n",ant->deltaH,ant->deltaE,ant->deltaN," ","ANTENNA: DELTA H/E/N");
	
	int nobs = 0;
	string obsTypes="";
	if (rx->constellations & Receiver::GPS) {
		nobs++;
		obsTypes = "    C1";
	}
	if (rx->constellations & Receiver::GLONASS) {
		nobs++;
		obsTypes = "    C1";
	}
	if (rx->constellations & Receiver::GALILEO) {
		nobs++;
		obsTypes += "    C5"; 
	}
	if (rx->constellations & Receiver::BEIDOU) {
		// FIXME observation types undefined ...
		nobs++;
	}
	
	switch (ver){
		case V2:
		{
			fprintf(fout,"%6d%-54s%-20s\n",nobs,obsTypes.c_str(),"# / TYPES OF OBSERV");
			break;
		}
		case V3:
		{
			fprintf(fout,"%1s  %3d %3s%50s%-20s\n","G",1,"C1C"," ","SYS / # / OBS TYPES"); // FIXME
			break;
		}
		default:break;
	}
	
	// Find the first observation
	
	int obsTime=0;
	int currMeas=0;
	while (currMeas < 86400 && obsTime <= 86400){
		if (mpairs[currMeas]->flags==0x03){
			ReceiverMeasurement *rm = mpairs[currMeas]->rm;
			// Round the measurement time to the nearest second, accounting for any fractional part of the second)
			int tMeas=(int) rint(rm->tmGPS.tm_hour*3600+rm->tmGPS.tm_min*60+rm->tmGPS.tm_sec + rm->tmfracs);
			if (tMeas==obsTime){
				fprintf(fout,"%6d%6d%6d%6d%6d%13.7lf%-5s%3s%-9s%-20s\n",
					rm->tmGPS.tm_year+1900,rm->tmGPS.tm_mon+1,rm->tmGPS.tm_mday,rm->tmGPS.tm_hour,rm->tmGPS.tm_min,
					(double) (rm->tmGPS.tm_sec+rm->tmfracs),
					" ", "GPS"," ","TIME OF FIRST OBS");
				break;
			}
			else if (tMeas < obsTime)
				currMeas++;
			else if (tMeas > obsTime)
				obsTime += interval;
		}
		else
			currMeas++;
	}
	fprintf(fout,"%6d%54s%-20s\n",rx->leapsecs," ","LEAP SECONDS");
	fprintf(fout,"%60s%-20s\n","","END OF HEADER");
	
	obsTime=0;
	currMeas=0;
	while (currMeas < 86400 && obsTime <= 86400){
		if (mpairs[currMeas]->flags==0x03){
			ReceiverMeasurement *rm = mpairs[currMeas]->rm;
			// Round the measurement time to the nearest second, accounting for any fractional part of the second)
			int tMeas=(int) rint(rm->tmGPS.tm_hour*3600+rm->tmGPS.tm_min*60+rm->tmGPS.tm_sec + rm->tmfracs);
			if (tMeas==obsTime){
				switch (ver){
					case V2:
					{
						int yy = rm->tmGPS.tm_year - 100*(rm->tmGPS.tm_year/100);
						fprintf(fout," %02d %2d %2d %2d %2d%11.7lf  %1d%3d",
							yy,rm->tmGPS.tm_mon+1,rm->tmGPS.tm_mday,rm->tmGPS.tm_hour,rm->tmGPS.tm_min,
							(double) (rm->tmGPS.tm_sec+rm->tmfracs),
							rm->epochFlag,(int) rm->gps.size());
						
						int svcount=0;
						int nsv = rm->gps.size();
						for (unsigned int sv=0;sv<rm->gps.size();sv++){
							svcount++;
							fprintf(fout,"G%02d",rm->gps[sv]->svn);
							if ((nsv > 12) && ((svcount % 12)==1)){ // more to do so start a new line
								fprintf(fout,"\n%32s","");
							}
						}
						fprintf(fout,"\n"); // CHECK does this work OK when there are no observations
						
						for (unsigned int sv=0;sv<rm->gps.size();sv++)
							fprintf(fout,"%14.3lf%1i%1i\n",rm->gps[sv]->meas*CVACUUM,rm->gps[sv]->lli,rm->gps[sv]->signal);
						break;
					}
					case V3:
					{
						fprintf(fout,"> %4d %2.2d %2.2d %2.2d %2.2d%11.7f %1d%3d%6s%15.12lf\n",
							rm->tmGPS.tm_year+1900,rm->tmGPS.tm_mon+1,rm->tmGPS.tm_mday,rm->tmGPS.tm_hour,rm->tmGPS.tm_min,(double) rm->tmGPS.tm_sec,
							rm->epochFlag,(int) rm->gps.size()," ",0.0);
						for (unsigned int i=0;i<rm->gps.size();i++)
							fprintf(fout,"G%2.2d%14.3lf%1i%1i\n",rm->gps[i]->svn,rm->gps[i]->meas*CVACUUM,rm->gps[i]->lli,rm->gps[i]->signal);
					} // case V3
				} // switch (RINEXversion)
				
				obsTime+=interval;
				currMeas++;
			}
			else if (tMeas < obsTime){
				currMeas++;
			}
			else if (tMeas > obsTime){
				obsTime += interval;
			}
		}
		else{
			currMeas++;
		}
	}
	fclose(fout);

	return true;
}

bool RINEX::writeNavigationFile(int ver,string fname,int mjd)
{
	char buf[81];
	FILE *fout;
	if (!(fout = fopen(fname.c_str(),"w"))){
		return false;
	}
	
	switch (ver)
	{
		case V2:
			fprintf(fout,"%9s%11s%1s%-39s%-20s\n",RINEXVersionName[ver],"","N","","RINEX VERSION / TYPE");
			break;
		case V3:
			fprintf(fout,"%9s%11s%-20s%-20s%-20s\n",RINEXVersionName[ver],"","N: GNSS NAV DATA","G: GPS","RINEX VERSION / TYPE");
			break;
		default:
			break;
	}
	
	time_t tnow = time(NULL);
	struct tm *tgmt = gmtime(&tnow);
	
	// Determine the GPS week number
	// GPS week 0 begins midnight 5/6 Jan 1980, MJD 44244
	int gpsWeek=int ((mjd-44244)/7);	
	switch (ver)
	{
		case V2:
		{
			strftime(buf,80,"%d-%b-%y %T",tgmt);
			fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			fprintf(fout,"%2s%12.4e%12.4e%12.4e%12.4e%10s%-20s\n","",
				rx->ionoData.a0,rx->ionoData.a1,rx->ionoData.a2,rx->ionoData.a3,"","ION ALPHA");
			fprintf(fout,"%2s%12.4e%12.4e%12.4e%12.4e%10s%-20s\n","",
				rx->ionoData.B0,rx->ionoData.B1,rx->ionoData.B2,rx->ionoData.B3,"","ION BETA");
			fprintf(fout,"%3s%19.12e%19.12e%9d%9d %-20s\n","",
				rx->utcData.A0,rx->utcData.A1,(int) rx->utcData.t_ot, gpsWeek,"DELTA-UTC: A0,A1,T,W");
			break;
		}
		case V3:
		{
			snprintf(buf,80,"%04d%02d%02d %02d%02d%02d UTC",tgmt->tm_year+1900,tgmt->tm_mon+1,tgmt->tm_mday,
					 tgmt->tm_hour,tgmt->tm_min,tgmt->tm_sec);
			fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			fprintf(fout,"GPSA %12.4e%12.4e%12.4e%12.4e%7s%-20s\n",
					rx->ionoData.a0,rx->ionoData.a1,rx->ionoData.a2,rx->ionoData.a3,"","IONOSPHERIC CORR");
			fprintf(fout,"GPSB %12.4e%12.4e%12.4e%12.4e%7s%-20s\n",
					rx->ionoData.B0,rx->ionoData.B1,rx->ionoData.B2,rx->ionoData.B3,"","IONOSPHERIC CORR");
			fprintf(fout,"GPUT %17.10e%16.9e%7d%5d %5s %2d %-20s\n",rx->utcData.A0,rx->utcData.A1,(int) rx->utcData.t_ot,gpsWeek," ", 0,"TIME SYSTEM CORR");
			break;
		}
	}
	
	fprintf(fout,"%6d%54s%-20s\n",rx->leapsecs," ","LEAP SECONDS");
	fprintf(fout,"%60s%-20s\n"," ","END OF HEADER");
	
	// Write out the ephemeris entries
	int lastGPSWeek=-1;
	int lastToc=-1;
	int weekRollovers=0;
	struct tm tmGPS0;
	tmGPS0.tm_sec=tmGPS0.tm_min=tmGPS0.tm_hour=0;
	tmGPS0.tm_mday=6;tmGPS0.tm_mon=0;tmGPS0.tm_year=1980-1900,tmGPS0.tm_isdst=0;
	time_t tGPS0=mktime(&tmGPS0);
	for (unsigned int i=0;i<rx->ephemeris.size();i++){
			
		// Account for GPS rollover:
		// GPS week 0 begins midnight 5/6 Jan 1980, MJD 44244
		// GPS week 1024 begins midnight 21/22 Aug 1999, MJD 51412
		// GPS week 2048 begins midnight 6/7 Apr 2019, MJD 58580
		int tmjd=mjd;
		int GPSWeek=rx->ephemeris[i]->week_number;
		
		while (tmjd>=51412) {
			GPSWeek+=1024;
			tmjd-=(7*1024);
		}
		if (-1==lastGPSWeek){lastGPSWeek=GPSWeek;}
		// Convert GPS week + $Toc to epoch as year, month, day, hour, min, sec
		// Note that the epoch should be specified in GPS time
		double Toc=rx->ephemeris[i]->t_OC;
		if (-1==lastToc) {lastToc = Toc;}
		// If GPS week is unchanged and Toc has gone backwards by more than 2 days, increment GPS week
		if ((GPSWeek == lastGPSWeek) && (Toc-lastToc < -2*86400)){
			weekRollovers=1;
		}
		else if (GPSWeek == lastGPSWeek+1){//OK now 
			weekRollovers=0; 	
		}
		
		lastGPSWeek=GPSWeek;
		lastToc=Toc;
		
		GPSWeek = GPSWeek + weekRollovers;
		
		double t=Toc;
		int day=(int) t/86400;
		t-=86400*day;
		int hour=(int) t/3600;
		t-=3600*hour;
		int minute=(int) t/60;
		t-=60*minute;
		int second=t;
	
		time_t tgps = tGPS0+GPSWeek*86400*7+Toc;
		struct tm *tmGPS = gmtime(&tgps);
		
		switch (ver)
		{
			case V2:
			{
				int yy = tmGPS->tm_year-100*(tmGPS->tm_year/100);
				fprintf(fout,"%02d %02d %02d %02d %02d %02d%5.1f%19.12e%19.12e%19.12e\n",rx->ephemeris[i]->SVN,
					yy,tmGPS->tm_mon+1,tmGPS->tm_mday,hour,minute,(float) second,
					rx->ephemeris[i]->a_f0,rx->ephemeris[i]->a_f1,rx->ephemeris[i]->a_f2);
				
				snprintf(buf,80,"%%3s%%19.12e%%19.12e%%19.12e%%19.12e\n");
			
				break;
			}
			case V3:
			{
				fprintf(fout,"G%02d %4d %02d %02d %02d %02d %02d%19.12e%19.12e%19.12e\n",rx->ephemeris[i]->SVN,
					tmGPS->tm_year+1900,tmGPS->tm_mon+1,tmGPS->tm_mday,hour,minute,second,
					rx->ephemeris[i]->a_f0,rx->ephemeris[i]->a_f1,rx->ephemeris[i]->a_f2);
				
				snprintf(buf,80,"%%4s%%19.12e%%19.12e%%19.12e%%19.12e\n");
				
				break;
			}
		}
		
		fprintf(fout,buf," ", // broadcast orbit 1
					(double) rx->ephemeris[i]->IODE,rx->ephemeris[i]->C_rs,rx->ephemeris[i]->delta_N,rx->ephemeris[i]->M_0);
				
		fprintf(fout,buf," ", // broadcast orbit 2
			rx->ephemeris[i]->C_uc,rx->ephemeris[i]->e,rx->ephemeris[i]->C_us,rx->ephemeris[i]->sqrtA);
		
		fprintf(fout,buf," ", // broadcast orbit 3
			rx->ephemeris[i]->t_oe,rx->ephemeris[i]->C_ic,rx->ephemeris[i]->OMEGA_0,rx->ephemeris[i]->C_is);
		
		fprintf(fout,buf," ", // broadcast orbit 4
			rx->ephemeris[i]->i_0,rx->ephemeris[i]->C_rc,rx->ephemeris[i]->OMEGA,rx->ephemeris[i]->OMEGADOT);
		
		fprintf(fout,buf," ", // broadcast orbit 5
			rx->ephemeris[i]->IDOT,1.0,(double) GPSWeek,0.0);
	
		fprintf(fout,buf," ", // broadcast orbit 6
			URA[rx->ephemeris[i]->SV_accuracy_raw],(double) rx->ephemeris[i]->SV_health,rx->ephemeris[i]->t_GD,(double) rx->ephemeris[i]->IODC);
		
		fprintf(fout,buf," ", // broadcast orbit 7
			rx->ephemeris[i]->t_ephem,0.0,0.0,0.0);
	}
	
	fclose(fout);
	
	return true;
}


//
// private members
//

void RINEX::init()
{
	agency = "KAOS";
	observer = "Siegfried";
}
