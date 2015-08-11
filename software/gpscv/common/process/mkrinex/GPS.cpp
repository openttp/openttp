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

#include "Debug.h"
#include "GPS.h"
#include "Receiver.h"

#define MU 3.986005e14 // WGS 84 value of the earth's gravitational constant for GPS USER
#define OMEGA_E_DOT 7.2921151467e-5
#define F -4.442807633e-10
#define MAX_ITERATIONS 10 // for solution of the Kepler equation


bool GPS::satxyz(EphemerisData *ed,double t,double *Ek,double x[3])
{
	// t is GPS system time at time of transmission
	
	double tk; // time from ephemeris reference epoch
	int nit;
	double Mk,Ekold=0.0;
	double A=ed->sqrtA*ed->sqrtA;
	double e=ed->e;
	
	// as per the ICD 20.3.3.4.3.1, account for beginning/end of week crossovers
	if ( (tk = t - ed->t_oe) > 302400) tk -= 604800;
	else if (tk < -302400) tk += 604800; // make (-302400 <= tk <= 302400)
	
	// solve Kepler's Equation for the Eccentric Anomaly by iteration
	*Ek = Mk = ed->M_0 + (sqrt(MU/(A*A*A)) + ed->delta_N)*tk;
	for (nit=0; nit != MAX_ITERATIONS; nit++){
		*Ek = Mk + e*sin(Ekold = *Ek);
		if (fabs(*Ek-Ekold) < 1e-8) break;
	}
	if (nit == MAX_ITERATIONS){
		//fprintf(stderr,"no convergence for E\n");
		return false;
	}
	
	double phik= atan2(sqrt(1-e*e)*sin(*Ek),cos(*Ek) - e) + ed->OMEGA;
	
	double uk = phik            + ed->C_us*sin(2*phik) + ed->C_uc*cos(2*phik) ;
	double rk = A*(1-e*cos(*Ek)) + ed->C_rc*cos(2*phik) + ed->C_rs*sin(2*phik);
	double ik = ed->i_0 + ed->IDOT*tk + ed->C_ic*cos(2*phik) + ed->C_is*sin(2*phik);
	double xkprime = rk*cos(uk);
	double ykprime = rk*sin(uk);
	double omegak = ed->OMEGA_0 + (ed->OMEGADOT - OMEGA_E_DOT)*tk - OMEGA_E_DOT*ed->t_oe;
	
	x[0] = xkprime*cos(omegak) - ykprime*cos(ik)*sin(omegak);
	x[1] = xkprime*sin(omegak) + ykprime*cos(ik)*cos(omegak);
	x[2] = ykprime*sin(ik);
	
	return true;
}

double GPS::sattime(EphemerisData *ed,double Ek,double tsv,double toc)
{
	// SV clock correction as per ICD 20.3.3.3.3.1
	double trel= F*ed->e*ed->sqrtA*sin(Ek);
	return tsv+ed->a_f0 + ed->a_f1*(tsv - toc) + ed->a_f2*(tsv - toc)*(tsv - toc)+trel;
}