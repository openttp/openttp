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

#include <cmath>
#include <ostream>

#include "Debug.h"
#include "Antenna.h"
#include "Receiver.h"

extern ostream *debugStream;

//
//	public
//	

Receiver::Receiver(Antenna *ant)
{
	modelName="undefined";
	manufacturer="undefined";
	serialNumber="undefined";
	swversion="undefined";
	version1="undefined";
	version2="undefined";
	constellations=0;
	antenna = ant;
}

Receiver::~Receiver()
{
}

//
// protected
//

bool Receiver::setCurrentLeapSeconds(int mjd,UTCData &utcd)
{
	if (!(utcData.dtlS == 0 && utcData.dt_LSF == 0)){ 
		// Figure out when the leap second is/was scheduled; we only have the low
		// 8 bits of the week number in WN_LSF, but we know that "the
		// absolute value of the difference between the untruncated WN and Wlsf
		// values shall not exceed 127" (ICD 20.3.3.5.2.4, p122)
		int gpsWeek=int ((mjd-44244)/7);
		int gpsSchedWeek=(gpsWeek & ~0xFF) | utcData.WN_LSF;
		while ((gpsWeek-gpsSchedWeek)> 127) {gpsSchedWeek+=256;}
		while ((gpsWeek-gpsSchedWeek)<-127) {gpsSchedWeek-=256;}
		int gpsSchedMJD=44244+7*gpsSchedWeek+utcData.DN;
		// leap seconds is either tls or tlsf depending on past/future schedule
		leapsecs=(mjd>=gpsSchedMJD? utcData.dt_LSF : utcData.dtlS);
		return true;
	}
	return false;
}

void Receiver::addGPSEphemeris(EphemerisData *ed)
{
	// Check whether this is a duplicate
	int issue;
	for (issue=0;issue < (int) sortedGPSEphemeris[ed->SVN].size();issue++){
		if (sortedGPSEphemeris[ed->SVN][issue]->t_oe == ed->t_oe){
			DBGMSG(debugStream,1,"ephemeris: duplicate SVN= "<< (unsigned int) ed->SVN << " toe= " << ed->t_oe);
			return;
		}
	}
	
	if (ephemeris.size()>0){

		// Update the ephemeris list - this is time-ordered
		std::vector<EphemerisData *>::iterator it;
		for (it=ephemeris.begin(); it<ephemeris.end(); it++){
			if (ed->t_OC < (*it)->t_OC){ // RINEX uses TOC
				DBGMSG(debugStream,1,"list inserting " << ed->t_OC << " " << (*it)->t_OC);
				ephemeris.insert(it,ed);
				break;
			}
		}
		
		if (it == ephemeris.end()){ // got to end, so append
			DBGMSG(debugStream,1,"appending " << ed->t_OC);
			ephemeris.push_back(ed);
		}
		
		// Update the ephemeris hash - 
		if (sortedGPSEphemeris[ed->SVN].size() > 0){
			std::vector<EphemerisData *>::iterator it;
			for (it=sortedGPSEphemeris[ed->SVN].begin(); it<sortedGPSEphemeris[ed->SVN].end(); it++){
				if (ed->t_OC < (*it)->t_OC){ 
					DBGMSG(debugStream,1,"hash inserting " << ed->t_OC << " " << (*it)->t_OC);
					sortedGPSEphemeris[ed->SVN].insert(it,ed);
					break;
				}
			}
			if (it == sortedGPSEphemeris[ed->SVN].end()){ // got to end, so append
				DBGMSG(debugStream,1,"hash appending " << ed->t_OC);
				sortedGPSEphemeris[ed->SVN].push_back(ed);
			}
		}
		else{ // first one for this SVN
			DBGMSG(debugStream,1,"first for svn " << (int) ed->SVN);
			sortedGPSEphemeris[ed->SVN].push_back(ed);
		}
	}
	else{ //first one
		DBGMSG(debugStream,1,"first eph ");
		ephemeris.push_back(ed);
		sortedGPSEphemeris[ed->SVN].push_back(ed);
		return;
	}
	
}


EphemerisData *Receiver::nearestEphemeris(int constellation,int svn,int wn,int tow)
{
	EphemerisData *ed = NULL;
	double dt,tmpdt;
	switch (constellation){
		case Receiver::GPS:
			{
				if (sortedGPSEphemeris[svn].size()==0)
					return ed;
				ed=sortedGPSEphemeris[svn][0];
				dt=fabs(ed->t_oe - tow);
				for (unsigned int i=1;i<sortedGPSEphemeris[svn].size();i++){
					tmpdt=fabs(sortedGPSEphemeris[svn][i]->t_oe - tow);
					if (tmpdt < dt){
						ed=sortedGPSEphemeris[svn][i];
						dt=fabs(ed->t_oe - tow);
					}
				}
			}
			break;
		default:
			break;
	}
	DBGMSG(debugStream,1,"svn="<<svn << ",tow="<<tow<<",t_oe="<< ((ed!=NULL)?(int)(ed->t_oe):-1));
	return ed;
}

	