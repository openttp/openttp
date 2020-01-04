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
