//
//
// The MIT License (MIT)
//
// Copyright (c) 2014  Michael J. Wouters
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

#ifndef __GPS_H_
#define __GPS_H_

#include <time.h>

	class Antenna;
	class EphemerisData;
	class Receiver;
	class ReceiverMeasurement;
	class SVMeasurement;
	
	namespace GPS {
		bool satXYZ(EphemerisData *ed,double t,double *Ek,double x[3]);
		double sattime(EphemerisData *ed,double Ek,double tsv,double toc);
		double ionoDelay(double az, double elev, double lat, double longitude, double GPSt,
			float alpha0,float alpha1,float alpha2,float alpha3,
			float beta0,float beta1,float beta2,float beta3);
		bool getPseudorangeCorrections(Receiver *rx,double gpsTOW, double pRange, Antenna *ant,EphemerisData *ed,
			double *refsyscorr,double *refsvcorr,double *iono,double *tropo,
			double *azimuth,double *elevation, int *ioe);
		unsigned int UTCtoTOW(struct tm *tmUTC, unsigned int nLeapSeconds);
	}

#endif

