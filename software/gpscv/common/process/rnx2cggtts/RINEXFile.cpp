//
//
// The MIT License (MIT)
//
// Copyright (c) 2023  Michael J. Wouters
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

#include "Application.h"
#include "Debug.h"
#include "RINEXFile.h"

extern Application *app;
extern std::ostream *debugStream;

#define SBUFSIZE 160

//
// Public methods
//
		
RINEXFile::RINEXFile()
{
	init();
}

double RINEXFile::readRINEXVersion(std::string fname)
{
	unsigned int lineCount=0;
	char line[SBUFSIZE];
	
	FILE *fin = std::fopen(fname.c_str(),"r");
	// First, determine the version
	double RINEXver = 0.0;
	
	while (!std::feof(fin)){
		std::fgets(line,SBUFSIZE,fin);
		lineCount++;
		if (NULL != strstr(line,"RINEX VERSION")){
			//parseParam(line,1,12,&RINEXver);
			break;
		}
	}

	if (std::feof(fin)){
		//app->logMessage("Unable to determine the RINEX version in " + fname);
		return 0;
	}
	
	DBGMSG(debugStream,TRACE,"RINEX version is " << RINEXver);
	
	std::fclose(fin);
	
	return RINEXver;
}

//
// Protected methods
//

// Note: these subtract one from the index !
void RINEXFile::parseParam(char *str,int start,int len,int *val)
{
	char sbuf[SBUFSIZE];
	memset(sbuf,0,SBUFSIZE);
	*val=0;
	strncpy(sbuf,&(str[start-1]),len);
	*val = strtol(sbuf,NULL,10);
}

void RINEXFile::parseParam(char *str,int start,int len,float *val)
{
	//std::replace(str.begin(), str.end(), 'D', 'E'); // filthy FORTRAN
	char *pch;
	if ((pch=strstr(str,"D"))){
		(*pch)='E';
	}
	char sbuf[SBUFSIZE];
	memset(sbuf,0,SBUFSIZE);
	*val=0.0;
	strncpy(sbuf,&(str[start-1]),len);
	*val = strtof(sbuf,NULL);
	
}

void RINEXFile::parseParam(char *str,int start,int len,double *val)
{
	char *pch;
	if ((pch=strstr(str,"D"))){
		(*pch)='E';
	}
	char sbuf[SBUFSIZE];
	memset(sbuf,0,SBUFSIZE);
	*val=0.0;
	strncpy(sbuf,&(str[start-1]),len);
	*val = strtod(sbuf,NULL);
}

//
// Private methods
//
		
void RINEXFile::init()
{
	
}
