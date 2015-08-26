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

void Receiver::sortGPSEphemeris()
{
	// Assemble a sorted, chronologically ordered hash table for quick lookup of ephemeris
	
	// Make the hash table - SVN is hashing function
	//for (unsigned int i=0;i<ephemeris.size();i++){
	//	sortedGPSEphemeris[ephemeris[i]->SVN].push_back(ephemeris[i]);
	//}
	
	// Lists are short so a bubble sort is fine
	for (unsigned int sv=1;sv<=Receiver::NGPSSATS;sv++){
		
		int swaps=1;
		while (swaps){
			swaps=0;
			for(int issue=0;issue <= (int) sortedGPSEphemeris[sv].size()-2;issue++){
				if(sortedGPSEphemeris[sv][issue]->t_oe > sortedGPSEphemeris[sv][issue+1]->t_oe){ 	//swap them
					EphemerisData *temp=sortedGPSEphemeris[sv][issue+1];
					sortedGPSEphemeris[sv][issue+1]=sortedGPSEphemeris[sv][issue];
					sortedGPSEphemeris[sv][issue]=temp;
					swaps=1;
				} 
			} 
		}
	}
	
	for (unsigned int sv=1;sv<=Receiver::NGPSSATS;sv++){
		for(int issue=0;issue < (int) sortedGPSEphemeris[sv].size();issue++){
			DBGMSG(debugStream,1,sv << " " << sortedGPSEphemeris[sv][issue]->t_oe);
		}
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

	