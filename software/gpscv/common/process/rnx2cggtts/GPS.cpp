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

#include "Debug.h"
#include "GPS.h"

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
	gotUTCdata = gotIonoData = false; // sometimes these are not in the nav file
}

GPS::~GPS()
{
}

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

