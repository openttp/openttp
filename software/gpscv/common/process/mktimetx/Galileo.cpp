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


#include <ctime>

#include "Galileo.h"


Galileo::Galileo():GNSSSystem()
{
	n="Galileo";
	olc="E";
	gotUTCdata = gotIonoData=false;
}

Galileo::~Galileo()
{
}

double Galileo::codeToFreq(int c)
{
	double f=1.0;
	switch (c)
	{
		case GNSSSystem::C1C: case GNSSSystem::C1B:case GNSSSystem::L1C:case GNSSSystem::L1B:
			f = 1575420000.0; break; // E1C and E1B (same as GPS C1)
		case GNSSSystem::C7I: case GNSSSystem::C7Q:case GNSSSystem::L7I:case GNSSSystem::L7Q:
			f = 1207140000.0; break; // E5b
		default:
			break;
	}
	return f;
}

double Galileo::decodeSISA(unsigned char sisa)
{
	
	if (sisa <= 49){
		return sisa/100.0;
	}
	else if (sisa <= 74){
		return 0.5 + (sisa-50) * 0.02;
	}
	else if (sisa <= 99){
		return 1.0 + (sisa-75) * 0.04;
	}
	else if (sisa <= 125){
		return 2.0 + (sisa - 100) * 0.16;
	}
	else if (sisa <= 254){ // shouldn't happen ...
		return 6.0; 
	}
	else
		return -1.0;
}

bool Galileo::resolveMsAmbiguity(Antenna*,ReceiverMeasurement *,SVMeasurement *,double *)
{
	return true;
}

// This sets ephemeris t0c, corrected for week rollovers 
void Galileo::setAbsT0c(int mjd)
{
	// GAL week 0 begins 23:59:47 22nd August 1999, MJD 51412 ie 1024 weeks before GPS epoch. Johnny come lately.
	// int galWeek=int ((mjd-51412)/7);
	// GPS epoch is used as reference week in RINEX
	int GALweek=int ((mjd-44244)/7);
	
	int lastGALweek=-1;
	int lastToc=-1;
	int weekRollovers=0;
	struct tm tmGAL0;
	tmGAL0.tm_sec=tmGAL0.tm_min=tmGAL0.tm_hour=0;
	tmGAL0.tm_mday=6;tmGAL0.tm_mon=0;tmGAL0.tm_year=1980-1900,tmGAL0.tm_isdst=0;
	time_t tGAL0 = std::mktime(&tmGAL0);
	
	for (unsigned int i=0;i<ephemeris.size();i++){
		GalEphemeris *ed = dynamic_cast<GalEphemeris *>(ephemeris.at(i));
		
		//std::fprintf(fout,"%d %d %d %d\n",(int) eph->t_0c,(int) eph->t_0e,(int) eph->SVN,(int) eph->IODnav);
		if (-1==lastGALweek){lastGALweek=GALweek;}
		double Toc=ed->t_0c;
		if (-1==lastToc) {lastToc = Toc;}
		if ((GALweek == lastGALweek) && (Toc-lastToc < -2*86400)){
			weekRollovers=1;
		}
		else if (GALweek == lastGALweek+1){//OK now 
			weekRollovers=0; 	
		}
		
		lastGALweek=GALweek;
		lastToc=Toc;
		
		GALweek += weekRollovers;
		
		ed->t0cAbs = tGAL0+GALweek*86400*7+Toc;
		ed->correctedWeek = GALweek;
	}
	
}
