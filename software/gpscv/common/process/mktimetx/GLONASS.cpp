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


#include <cstring>

#include "GLONASS.h"


GLONASS::GLONASS():GNSSSystem()
{
	n="GLONASS";
	olc="R";
	for (int i=0;i<=NSATS;i++)
		memset((void *)(&L1lastunlock[i]),0,sizeof(time_t)); // all good
}

GLONASS::~GLONASS()
{
}

double GLONASS::codeToFreq(int c,int k)
{
	double f=1.0;
		// L1OF 1602 MHz + k*562.5 kHz
										// L2OF 1246 MHz + k*437.5 kHz
	switch (c)
	{
		case GNSSSystem::C1C:case GNSSSystem::L1C:
			f = 1602000000.0 + k * 562500.0; break; 
		case GNSSSystem::C2C:case GNSSSystem::L2C:
			f = 1246000000.0 + k * 437500.0; break; 
		default:
			break;
	}
	return f;
}

void GLONASS::deleteEphemeris()
{
}

bool GLONASS::resolveMsAmbiguity(Antenna* antenna,ReceiverMeasurement *,SVMeasurement *,double *){
	return true;
}
