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

#include <fstream>
#include <sstream>

#include <boost/algorithm/string.hpp>

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
	std::ifstream fin;
	
	fin.open(fname.c_str()); // already tested 'good'
	
	std::string line,str;
	while (!fin.eof()){
		std::getline(fin,line);
		lineCount++;
		// don't test the line length (eg <80), some lines may be short
		if (std::string::npos != line.find("RINEX VERSION")){
			readParam(line,1,12,&version);
			break;
		}
		
		if (std::string::npos != line.find("END OF HEADER")){
			break;
		}
	}
			
	DBGMSG(debugStream,TRACE,"RINEX version is " << version);
	
	fin.close();
	
	return version;
}

//
// Protected methods
//

bool RINEXFile::readParam(std::string &str,int start,int len,int *val)
{
	std::stringstream ss(str.substr(start-1,len));
	ss >> *val;
	return true;
}

bool RINEXFile::readParam(std::string &str,int start,int len,float *val)
{
	boost::algorithm::replace_all(str, "D", "E"); // filthy FORTRAN
	std::stringstream ss(str.substr(start-1,len));
	ss >> *val;
	return true;
}
	
bool RINEXFile::readParam(std::string &str,int start,int len,double *val)
{
	boost::algorithm::replace_all(str, "D", "E");
	std::stringstream ss(str.substr(start-1,len));
	ss >> *val;
	return !ss.fail();
}

bool RINEXFile::read4DParams(std::ifstream &fin,int startCol,
	double *darg1,double *darg2,double *darg3,double *darg4,
	unsigned int *lineCount)
{

	std::string line;
	if (!fin.eof()){
		std::getline(fin,line);
		(*lineCount)++;
	}
	else{
		return false;
	}
	
	int slen = line.length();
	*darg1 = *darg2= *darg3 = *darg4 = 0.0;
	if (slen >= startCol + 19 -1)
		readParam(line,startCol,19,darg1);
	if (slen >= startCol + 38 -1)
		readParam(line,startCol+19,19,darg2);
	if (slen >= startCol + 57 -1)
		readParam(line,startCol+2*19,19,darg3);
	if (slen >= startCol + 76 -1)
		readParam(line,startCol+3*19,19,darg4);
	
	return true;
	
}



//
// Private methods
//
		
void RINEXFile::init()
{
	version = 0.0; // unknown
	leapsecs = -1; // 
}
