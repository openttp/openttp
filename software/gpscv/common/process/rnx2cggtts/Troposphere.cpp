
//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Malcolm Lawn, Michael J. Wouters
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
#include "Troposphere.h"

double Troposphere::delayModel(double elev, double height)
{
	
	// elev   = satellite elevation angle in degrees
	// height = height of GPS antenna above sea level in metres
	// result in ns
	
	float Ns = 324.8; // surface refractivity at mean sea level
	float el,deltaN,deltaR, R, f;
	double c = 299792458; 
	
	// Convert elev to radians
	el = elev * M_PI/180; //satellite elevation in radians 

	height = height/1000; // convert height in metres to kilometres

	if(elev < 90) {
		f = 1/(sin(el) + 0.00143/(tan(el) + 0.0455));}
	else {f=1.0;}

	deltaN =-7.32*exp(0.005577*Ns);
	
	if (height < 1.0){
		deltaR = (2162.0 + Ns*(1.0 - height) + 0.5*deltaN*(1.0 - height*height))*0.001;
	}
	else{
		deltaR = (Ns + 0.5*deltaN - Ns*height - 
				0.5*deltaN*pow(height,2) + 1430 + 732 )*0.001;
	}
	
	R = f*deltaR; // range error caused by troposphere
	
	return(R/c * 1e9); 

}
