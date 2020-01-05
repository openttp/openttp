//
//
// The MIT License (MIT)
//
// Copyright (c) 2019  Michael J. Wouters
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

#include "Application.h"
#include "Debug.h"
#include "GNSSSystem.h"
#include "RINEX.h"

extern Application *app;
extern std::ostream *debugStream;
extern std::string   debugFileName;
extern std::ofstream debugLog;
extern int verbosity;

std::string GNSSSystem::observationCodeToStr(int c,int RINEXmajorVersion,int RINEXminorVersion){
	switch (c)
	{
		case C1C:
			switch (RINEXmajorVersion)
			{
				case RINEX::V2:return "C1";break;
				case RINEX::V3:return "C1C";break;
			}
			break;
		case C1P:
			switch (RINEXmajorVersion)
			{
				case RINEX::V2:return "P1";break;
				case RINEX::V3:return "C1P";break;
			}
			break;
		case C1B:return "C1B";break;
		case C2C:return "C2C";break;
		case C2P:
			switch (RINEXmajorVersion)
			{
				case RINEX::V2:return "P2";break;
				case RINEX::V3:return "C2P";break;
			}
			break;
		case C2L:
			switch (RINEXmajorVersion)
			{
				case RINEX::V2:return "C2";break; // FIXME L2C signal in V2.12 is CC
				case RINEX::V3:return "C2L";break;
			}
			break;
		case C2M:return "C2M";break;
		case C2I:return "C2I";break;
		case C7I:return "C7I";break;
		case C7Q:return "C7Q";break;
		case L1C:
			switch (RINEXmajorVersion)
			{
				case RINEX::V2:return "L1";break;
				case RINEX::V3:return "L1C";break;
			}
			break;
		case L1P:
			switch (RINEXmajorVersion)
			{
				case RINEX::V2:return "L1";break;
				case RINEX::V3:return "L1P";break;
			}
			break;
		case L2P:
			switch (RINEXmajorVersion)
			{
				case RINEX::V2:return "L2";break;
				case RINEX::V3:return "L2P";break;
			}
			break;
		case L2C:return "L2C";break;
        case L2L:
            switch (RINEXmajorVersion)
			{
				case RINEX::V2:return "L2";break;
				case RINEX::V3:return "L2L";break;
			}
            break;
		case L2I:return "L2I";break;
		case L7I:return "L7I";break;
		default:return "";break;
	}
	return "";
}

unsigned int GNSSSystem::strToObservationCode(std::string s, int RINEXversion)
{
	unsigned int c=0;
	switch (RINEXversion)
	{
		case RINEX::V2:
			{
				if (s=="C1"){c=C1C;}
				else if (s=="P1"){c=C1P;}
				else if (s=="P2"){c=C2P;}
				else if (s=="C2"){c=C2L;}
				//else if (s=="L1"){c=L1C;} // FIXME ambiguity
				else if (s=="L1"){c=L1P;}   // this is the more common case 
				else if (s=="L2"){c=L2P;}   // ditto (Javad is L2P, ublox L2 is L2L) 
			}
			break;
		case RINEX::V3:
			{
				if (s=="C1C"){c=C1C;}
				else if (s=="C1P"){c=C1P;}
				else if (s=="C2P"){c=C2P;}
				else if (s=="C2L"){c=C2L;}
				else if (s=="C1B"){c=C1B;}
				else if (s=="C2C"){c=C2C;}
				else if (s=="C2M"){c=C2M;}
				else if (s=="C2I"){c=C2I;}
				else if (s=="C7I"){c=C7I;}
				else if (s=="C7Q"){c=C7Q;}
				else if (s=="L1C"){c=L1C;}
				else if (s=="L1P"){c=L1P;}
				else if (s=="L2P"){c=L2P;}
				else if (s=="L2C"){c=L2C;}
				else if (s=="L2L"){c=L2L;}
				else if (s=="L2I"){c=L2I;}
				else if (s=="L7I"){c=L7I;}
			}
			break;
	}
	
	return c;
}

void GNSSSystem::deleteEphemerides()
{
	DBGMSG(debugStream,TRACE,"deleting rx ephemeris");
	
	while(! ephemeris.empty()){
		Ephemeris  *tmp= ephemeris.back();
		delete tmp;
		ephemeris.pop_back();
	}
	
	for (int s=0;s<=maxSVN();s++){
		sortedEphemeris[s].clear(); // nothing left to delete 
	}
}

// The ephemeris is sorted so that the RINEX navigation file is written correctly
// A hash table is also built for quick ephemeris lookup
// Note that when the ephemeris is completely read in, another fixup must be done for week rollovers

// Notes on GPS
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

bool GNSSSystem::addEphemeris(Ephemeris *ed)
{
	// Check whether this is a duplicate
	int issue;
	for (issue=0;issue < (int) sortedEphemeris[ed->svn()].size();issue++){
		if (sortedEphemeris[ed->svn()][issue]->t0e() == ed->t0e()){
			DBGMSG(debugStream,4,"ephemeris: duplicate SVN= "<< (unsigned int) ed->svn() << " toe= " << ed->t0e());
			return false;
		}
	}
	
	if (ephemeris.size()>0){

		// Update the ephemeris list - this is time-ordered
		
		std::vector<Ephemeris *>::iterator it;
		for (it=ephemeris.begin(); it<ephemeris.end(); it++){
			if (ed->t0c() < (*it)->t0c()){ // RINEX uses t0c()
				DBGMSG(debugStream,4,"list inserting " << ed->t0c() << " " << (*it)->t0c());
				ephemeris.insert(it,ed);
				break;
			}
		}
		
		if (it == ephemeris.end()){ // got to end, so append
			DBGMSG(debugStream,4,"appending " << ed->t0c());
			ephemeris.push_back(ed);
		}
		
		// Update the ephemeris hash - 
		if (sortedEphemeris[ed->svn()].size() > 0){
			std::vector<Ephemeris *>::iterator it;
			for (it=sortedEphemeris[ed->svn()].begin(); it<sortedEphemeris[ed->svn()].end(); it++){
				if (ed->t0c() < (*it)->t0c()){ 
					DBGMSG(debugStream,4,"hash inserting " << ed->t0c() << " " << (*it)->t0c());
					sortedEphemeris[ed->svn()].insert(it,ed);
					break;
				}
			}
			if (it == sortedEphemeris[ed->svn()].end()){ // got to end, so append
				DBGMSG(debugStream,4,"hash appending " << ed->t0c());
				sortedEphemeris[ed->svn()].push_back(ed);
			}
		}
		else{ // first one for this svn()
			DBGMSG(debugStream,4,"first for svn " << (int) ed->svn());
			sortedEphemeris[ed->svn()].push_back(ed);
		}
	}
	else{ //first one
		DBGMSG(debugStream,4,"first eph ");
		ephemeris.push_back(ed);
		sortedEphemeris[ed->svn()].push_back(ed);
		return true;
	}
	return true;
}

bool GNSSSystem::fixWeekRollovers()
{
	// There are two cases:
	// (1) We are at the end of the week and get an ephemeris for the next day. Week number can't be used to discriminate.
	//      In this case, the ephemeris needs to be moved to the end
	// (2) We are at the beginning of the week and get an ephemeris for the previous day. 
	//      This ephemeris must move to the beginning of the day
	// Note that further disambiguation information is available from the time the message was logged
	
	if (ephemeris.size() <= 1) return false;
	
	//std::vector<Ephemeris *>::iterator it;
	//for (it=ephemeris.begin(); it<ephemeris.end(); it++){
	//	GPSEphemeris *ed = dynamic_cast<GPSEphemeris *>(*it);
		//cout << (int) ed->SVN << " " << ed->t_oe << " " << ed->t_OC << " " << (int) ed->IODE << " " <<  (int) ed->tLogged << endl;
	//}
	
	// Because the ephemeris has been ordered by t_OC, the misplaced ephemerides can be moved as a block
	
	// This handles case (1)
	int tOClast = ephemeris[0]->t0c();
	for (unsigned i=1; i < ephemeris.size(); i++){
		Ephemeris *ed = ephemeris[i];
		if (ed->t0c() - tOClast > 5*86400){ // Detect the position of the break 
			DBGMSG(debugStream,INFO,"Week rollover detected in ephemeris");
			// Have to copy the first "i" entries to the end
			for (unsigned int j=0;j<i;j++)
				ephemeris.push_back(ephemeris[j]);
			// and then remove the first i entries
			ephemeris.erase(ephemeris.begin(),ephemeris.begin()+i);
			
			//std::vector<Ephemeris *>::iterator it;
			//for (it=ephemeris.begin(); it<ephemeris.end(); it++){
			//	GPSEphemeris *ed = dynamic_cast<GPSEphemeris *>(*it);
			//	cout << (int) ed->SVN << " " << ed->t_oe << " " << ed->t_OC << " " << (int) ed->IODE << " " <<  (int) ed->tLogged << endl;
			//}
			return true;
		}
		else
			tOClast = ed->t0c();
	}
	
	return false;
}
