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


#include <string>
#include <boost/concept_check.hpp>

#include "CGGTTS.h"
#include "GNSSSystem.h"
#include "RINEXFile.h"

//
//	public
//

static unsigned int str2ToCode(std::string s)
{
	int c=0;
	if (s== "C1")
		c=GNSSSystem::C1C;
	else if (s == "P1")
		c=GNSSSystem::C1P;
	else if (s == "E1")
		c=GNSSSystem::C1C; // FIXME what about C1B?
	else if (s == "B1")
		c=GNSSSystem::C2I;
	else if (s == "C2") // C2L ?? C2M
		c=GNSSSystem::C2C;
	else if (s == "P2")
		c=GNSSSystem::C2P;
	//else if (c1 == "E5")
	//	c=E5a;
	else if (s== "B2")
		c=GNSSSystem::C7I;
	return c;
}

CGGTTS::CGGTTS()
{
	init();
}
	
//
// private
//		

void CGGTTS::init()
{
}


unsigned int CGGTTS::strToCode(std::string str, bool *isP3)
{
	// Convert CGGTTS code string (usually from the configuration file) to RINEX observation code(s)
	// Dual frequency combinations are of the form 'code1+code2'
	// Valid formats are 
	// (1) CGGTTS 2 letter codes
	// (2) RINEX  3 letter codes
	// but not mixed!
	// so valid string lengths are 2 and 5 (CGGTTS)  or 3 and 7 (RINEX)
	unsigned int c=0;
	if (str.length()==2){
		c = str2ToCode(str);
	}
	else if (str.length()==5){ // dual frequency, specified using 2 letter codes
		unsigned int c1=str2ToCode(str.substr(0,2));
		unsigned int c2=str2ToCode(str.substr(3,2));
		c = c1 | c2;
		*isP3=true;
	} 
	else if (str.length()==3){ // single frequency, specified using 3 letter RINEX code
		c=GNSSSystem::strToObservationCode(str,RINEXFile::V3);
	}
	else if (str.length()==7){ // dual frequency, specified using 3 letter RINEX codes
		unsigned int c1=GNSSSystem::strToObservationCode(str.substr(0,3),RINEXFile::V3);
		unsigned int c2=GNSSSystem::strToObservationCode(str.substr(4,3),RINEXFile::V3);
		c = c1 | c2;
		*isP3=true;
	}
	else{
		c= 0;
		*isP3=false;
	}
	
	return c;
}


	


