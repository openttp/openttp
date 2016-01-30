//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Michael J. Wouters
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
#include <cstdio>
#include <cstring>

#include <iostream>
#include "Debug.h"

#include "MakeRINEX.h"

#include "Antenna.h"
#include "CGGTTS.h"
#include "Counter.h"
#include "MeasurementPair.h"
#include "Receiver.h"
#include "ReceiverMeasurement.h"


extern MakeRINEX *app;
extern ostream *debugStream;
extern string   debugFileName;
extern ofstream debugLog;
extern int verbosity;

//
//	Public members
//

CGGTTS::CGGTTS(Antenna *a,Counter *c,Receiver *r)
{
	ant=a;
	cntr=c;
	rx=r;
	init();
}

bool CGGTTS::writeObservationFile(int ver,string fname,int mjd,MeasurementPair **mpairs)
{
	FILE *fout;
	if (!(fout = fopen(fname.c_str(),"w"))){
		cerr << "Unable to open " << fname << endl;
		return false;
	}
	
	switch (ver)
	{
		case V1:
			writeV1(fout);
			break;
		case V2E:
			break;
	}
	
	fclose(fout);
	
	return true;
}

	

//
//	Private members
//		

void CGGTTS::init()
{
	revDateYYYY=2016;
	revDateMM=1;
	revDateDD=1;
	ref="";
	lab="";
	comment="";
}
		
void CGGTTS::writeV1(FILE *fout)
{
#define MAXCHARS 128
	int cksum=0;
	char buf[MAXCHARS+1];
	
	// NB maximum line length is 128 columns
	
	strncpy(buf,"GGTTS GPS DATA FORMAT VERSION = 01",MAXCHARS);
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"REV DATE = %4d-%02d-%02d",revDateYYYY,revDateMM,revDateDD); 
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"RCVR = %s %s %s %4d %s",rx->manufacturer.c_str(),rx->modelName.c_str(),rx->serialNumber.c_str(),
		rx->commissionYYYY,rx->swversion.c_str());
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"CH = %02d",rx->channels); 
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"IMS = 99999"); // FIXME dual frequency 
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"LAB = %s",lab.c_str()); 
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"X =  m");
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"Y =  m");
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"Z =  m");
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"FRAME = %s",ant->frame.c_str());
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"COMMENTS = %s",comment.c_str());
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"INT DLY =  ns");
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"CAB DLY =  ns");
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"REF DLY =  ns");
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"REF = %s",ref.c_str());
	cksum += checkSum(buf);
	fprintf(fout,"%s\n",buf);
	
	snprintf(buf,MAXCHARS,"CKSUM = ");
	cksum += checkSum(buf);
	fprintf(fout,"%s%02X\n",buf,cksum % 256);
	
	fprintf(fout,"\n");
	
	fprintf(fout,"PRN CL  MJD  STTIME TRKL ELV AZTH   REFSV      SRSV     REFGPS    SRGPS  DSG IOE MDTR SMDT MDIO SMDI CK\n");
	
#undef MAXCHARS	
}
	
int CGGTTS::checkSum(char *l)
{
	int cksum =0;
	for (unsigned int i=0;i<strlen(l);i++)
		cksum += (int) l[i];
	return cksum;
}