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

#include <ostream>
#include <sstream>

#include "Debug.h"
#include "RIN2CGGTTS.h"
#include "Utility.h"

extern std::ostream *debugStream;

bool RIN2CGGTTS::read(std::string paramsFile)
{
	DBGMSG(debugStream,1,"reading" << paramsFile);
	std::ifstream fin(paramsFile.c_str());
	std::string line;
	
	if (!fin.good()){
		DBGMSG(debugStream,1,"unable to open" << paramsFile);
		return false;
	}
	bool ok =true;
	
	while (!fin.eof()){
		// This file should be strictly formatted but the keywords
		// may be in arbitrary order
		getline(fin,line);
		Utility::trim(line);
		ok = ok && getParam(fin,line,"REV DATE",revDate);
		ok = ok && getParam(fin,line,"RCVR",rcvr);
		ok = ok && getParam(fin,line,"CH",&ch);
		ok = ok && getParam(fin,line,"LAB NAME",lab);
		ok = ok && getParam(fin,line,"X COORDINATE",&antpos[0]);
		ok = ok && getParam(fin,line,"Y COORDINATE",&antpos[1]);
		ok = ok && getParam(fin,line,"Z COORDINATE",&antpos[2]);
		ok = ok && getParam(fin,line,"COMMENTS",comments);
		ok = ok && getParam(fin,line,"FRAME",frame);
		ok = ok && getParam(fin,line,"REF",ref);
		ok = ok && getParam(fin,line,"INT DELAY P1 XR+XS (in ns)",&P1delay);
		ok = ok && getParam(fin,line,"INT DELAY P1 GLO (in ns)",&P2delayGLO);
		ok = ok && getParam(fin,line,"INT DELAY C1 XR+XS (in ns)",&P1delay); // FIXME WTF
		ok = ok && getParam(fin,line,"INT DELAY P2 XR+XS (in ns)",&P2delay);
		ok = ok && getParam(fin,line,"INT DELAY P2 GLO (in ns)",&P2delayGLO);
		ok = ok && getParam(fin,line,"ANT CAB DELAY (in ns)",&cabDelay);
		ok = ok && getParam(fin,line,"LEAP SECOND",&leapSeconds);
		ok = ok && getParam(fin,line,"CLOCK CAB DELAY XP+XO (in ns)",&refDelay);
	}
	fin.close();
	return ok;
}

bool RIN2CGGTTS::getParam(std::ifstream &fin,std::string &currParam,std::string param,std::string &val)
{
	if (currParam == param)
	{
		std::string line;
		if (fin.good()){
			getline(fin,line);
			Utility::trim(line);
			val=line;
			DBGMSG(debugStream,3,currParam << " = " << val);
		}
		else{
			DBGMSG(debugStream,1,"Missing parameter for " << param);
			return false;
		}
	}
	return true;
}

bool RIN2CGGTTS::getParam(std::ifstream &fin,std::string &currParam,std::string param,double *val)
{
	if (currParam==param)
	{
		std::string line;
		if (fin.good()){
			getline(fin,line);
			std::stringstream ss(line);
			ss >> *val;
			DBGMSG(debugStream,3,currParam << " = " << std::fixed << *val);
		}
		else{
			DBGMSG(debugStream,1,"Missing parameter for " << param);
			return false;
		}
	}
	return true;
}

bool RIN2CGGTTS::getParam(std::ifstream &fin,std::string &currParam,std::string param,int *val)
{
	if (currParam==param)
	{
		std::string line;
		if (fin.good()){
			getline(fin,line);
			std::stringstream ss(line);
			ss >> *val;
			DBGMSG(debugStream,3,currParam << " = " << *val);
		}
		else{
			DBGMSG(debugStream,1,"Missing parameter for " << param);
			return false;
		}
	}
	return true;
}

