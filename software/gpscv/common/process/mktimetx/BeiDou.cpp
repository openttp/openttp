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

#include <cmath>
#include <iostream>
#include <iomanip>
#include <ostream>

#include "Antenna.h"
#include "Application.h"
#include "BeiDou.h"
#include "Debug.h"
#include "ReceiverMeasurement.h"
#include "SVMeasurement.h"

extern Application *app;
extern std::ostream *debugStream;
extern std::string   debugFileName;
extern std::ofstream debugLog;
extern int verbosity;

BeiDou::BeiDou():GNSSSystem()
{
	n="BeiDou";
	olc="C";
}

BeiDou::~BeiDou()
{
}


// The ephemeris is sorted on t_OC but searched on t_OE ...
// Sorting on t_OC gives time-ordered output in the navigation file
// For BeiDou it appears that these are the same, but this needs to be checked against data from a receiver
// rather than a RINEX file
void BeiDou::addEphemeris(EphemerisData *ed)
{
	// Check whether this is a duplicate
	for (int issue=0;issue < (int) sortedEphemeris[ed->SVN].size();issue++){
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

void BeiDou::deleteEphemeris()
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

bool BeiDou::resolveMsAmbiguity(Antenna*,ReceiverMeasurement *,SVMeasurement *,double *)
{
	return true;
}