//
//
// The MIT License (MIT)
//
// Copyright (c) 2015  Peter Fisk, Michael J. Wouters
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

#include <boost/regex.hpp>

#include "Utility.h"

std::string Utility::trim(std::string const& str)
{
	std::size_t first = str.find_first_not_of(' ');

	// If there is no non-whitespace character, both first and last will be std::string::npos (-1)
	// There is no point in checking both, since if either doesn't work, the
	// other won't work, either.
	if(first == std::string::npos)
		return "";

	std::size_t last  = str.find_last_not_of(' ');

	return str.substr(first, last-first+1);
}


void Utility::MJDtoDate(int mjd,int *year,int *mon, int *mday, int *yday)
{
	time_t tt = (mjd - 40587)*86400;
	struct tm *utc = gmtime(&tt);
	*year = 1900 + utc->tm_year;
	*mon  = utc->tm_mon+1;
	*mday = utc->tm_mday;
	*yday = utc->tm_yday+1;
}

bool Utility::TODStrtoTOD(std::string const& todstr,int *hh,int *mm,int *ss)
{
	// Two formats OK
	// HHMMSS or HH:MM:SS
	boost::regex re1("^(\\d{2})(\\d{2})(\\d{2})$");
	boost::smatch matches;
	if (boost::regex_search(todstr,matches,re1)){
		*hh = atoi(matches[1].str().c_str()); // regex has already checked input is valid
		*mm = atoi(matches[2].str().c_str());
		*ss = atoi(matches[3].str().c_str());
		return (*hh <= 23 && *mm <= 59 && *ss <= 59);
	}
	boost::regex re2("^(\\d{2}):(\\d{2}):(\\d{2})$");
	if (boost::regex_search(todstr,matches,re2)){
		*hh = atoi(matches[1].str().c_str()); // regex has already checked input is valid
		*mm = atoi(matches[2].str().c_str());
		*ss = atoi(matches[3].str().c_str());
		return (*hh <= 23 && *mm <= 59 && *ss <= 59);
	}
	return false;
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

bool Utility::quadFit(double x[], double y[],int n,double xinterp,double *yinterp){
	//double c,m,rmsResidual;
	//linearFit(x,y,n,xinterp,yinterp,&c,&m,&rmsResidual); // FIXME
	
	double chisq;
  gsl_matrix *X, *cov;
  gsl_vector *Y, *c;
	
	X = gsl_matrix_alloc (n, 3);
	Y = gsl_vector_alloc (n);
	c = gsl_vector_alloc (3);
	cov = gsl_matrix_alloc (3, 3);

	for (int i = 0; i < n; i++){    
		gsl_matrix_set (X, i, 0, 1.0);
		gsl_matrix_set (X, i, 1, x[i]);
		gsl_matrix_set (X, i, 2, x[i]*x[i]);
		gsl_vector_set (Y, i, y[i]);
	}
  
	gsl_multifit_linear_workspace * work = gsl_multifit_linear_alloc (n, 3);
	gsl_multifit_linear (X, Y, c, cov,&chisq, work);
	gsl_multifit_linear_free (work);
	
	#define C(i) (gsl_vector_get(c,(i)))
	
	*yinterp = C(0) + C(1)*xinterp + C(2)*xinterp*xinterp;
	
	gsl_matrix_free (X);
  gsl_vector_free (Y);
  gsl_vector_free (c);
  gsl_matrix_free (cov);
	
	return true;
}

void Utility::ECEFtoLatLonH(double X, double Y, double Z, 
	double *lat, double *lon, double *ht)
{
	// Parameters for WGS84 ellipsoid
	double a = 6378137.00; // semi-major axis
	double inverse_flattening = 298.257223563;
	double latitude,longitude;
	double p=sqrt(X*X + Y*Y);
	double r=sqrt(p*p + Z*Z);
	double f=1/inverse_flattening;
	double esq=2*f-f*f;
	double u=atan2(Z/p , 1.0/(1-f+esq*a/r));

	longitude = atan2(Y,X);
	latitude = atan2(Z*(1-f) + esq * a * pow(sin(u),3) ,
		(1-f)*(p  - esq * a * pow(cos(u),3)));
	*ht = p*cos(latitude) + Z*sin(latitude) - 
			a*sqrt(1-esq* pow(sin(latitude),2));

	// Convert to degrees
	*lat=latitude*180.0/M_PI;
	*lon=longitude*180.0/M_PI;
	
	return;
} 
