//
//
// The MIT License (MIT)
//
// Copyright (c) 2023  Peter Fisk, Michael J. Wouters
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
#include <time.h>
#include <gsl/gsl_multifit.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/regex.hpp>

#include "Application.h"
#include "Utility.h"

extern Application *app;
extern std::ostream *debugStream;

#define GPS_EPOCH 44244 // MJD of the beginning of GPS time

void Utility::MJDtoDate(int mjd,int *year,int *mon, int *mday, int *yday)
{
	time_t tt = (mjd - 40587)*86400;
	struct tm *utc = gmtime(&tt);
	*year = 1900 + utc->tm_year;
	*mon  = utc->tm_mon+1;
	*mday = utc->tm_mday;
	*yday = utc->tm_yday+1;
}


int Utility::DateToMJD(int year, int month, int day)
{
	// from TVB at leapsecond.com
	long Y = year, M = month, D = day;
	long mjd =
		367 * Y
		- 7 * (Y + (M + 9) / 12) / 4
		- 3 * ((Y + (M - 9) / 7) / 100 + 1) / 4
		+ 275 * M / 9
		+ D + 1721029 - 2400001;
	return mjd;
}

void Utility::GPSDateTimeToGPSTime(int year, int month, int day,int hh,int mm,double ss,int *fullWN,double *TOW)
{
	int mjd = DateToMJD(year,month,day);
	*fullWN = (mjd - GPS_EPOCH)/7; 
	*TOW = ((mjd - GPS_EPOCH)%7)*86400 + hh*3600 + mm*60 + ss;
}

void Utility::MJDToGPSTime(int mjd,double tods,int *fullWN,double *TOW)
{
	*fullWN = (mjd - GPS_EPOCH)/7; 
	*TOW = ((mjd - GPS_EPOCH)%7)*86400 + tods;
}

bool Utility::linearFit(double x[], double y[],int n,double xinterp,double *yinterp,double *c,double *m,double *rmsResidual)
{
	double sx = 0, sy = 0, sr2 = 0, xbar, ybar, sxxbary=0,sxxbarsq=0;
	
	for (int i=0;i<n;i++){
		sx += x[i];
		sy += y[i];
	}
	
	xbar = sx/n;
	ybar = sy/n;

	for (int i=0;i<n;i++){
		sxxbary += (x[i] - xbar) * y[i];
		sxxbarsq += (x[i] - xbar)*(x[i] - xbar);
	}

	// Slope and intercept
	*m = sxxbary/sxxbarsq;
	*c = ybar - *m * xbar;

	// Sum of residuals^2

	for (int i=0;i<n;i++)
		sr2 += pow(y[i] - *m * x[i] - *c,2);

	// RMS residuals
	*rmsResidual = sqrt(sr2/n);
	
	*yinterp = *m * xinterp + *c;
	
	return true;
}
