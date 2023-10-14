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


#include <ostream>

#include "Debug.h"
#include "GNSSDelay.h"

extern std::ostream *debugStream;

//
//	Public
//

GNSSDelay::GNSSDelay()
{
	// default values maketh no sense
}

double GNSSDelay::getDelay(std::string c)
{
	
	for (unsigned int i=0;i<code.size();i++){
		if (code[i] == c){
			DBGMSG(debugStream,INFO,"Got delay " << c);
			return delay[i];
		}
	}
	DBGMSG(debugStream,INFO,"Failed to get delay " << c);
	return 0.0;
}

		
void GNSSDelay::addDelay(std::string c, double v)
{
	for (unsigned int i=0;i<code.size();i++){ // is it already there ?
		if (code[i] == c){
			delay.at(i) = v;
			DBGMSG(debugStream,INFO,"Set delay " << c << " " << v);
			return;
		}
	}
	// not there so add it
	code.push_back(c);
	delay.push_back(v);
}

