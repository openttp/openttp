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

#ifndef __COUNTER_MEASUREMENT_H_
#define __COUNTER_MEASUREMENT_H_

#include <cstdio>
#include <string>

class CounterMeasurement
{
	public:
		CounterMeasurement(int h,int m,int s,double r)
		{
			hh=(unsigned char)h;mm=(unsigned char)m;ss=(unsigned char)s;
			rdg=r;
		}
		std::string timestamp()
		{
			char sbuf[9];
			sprintf(sbuf,"%02d:%02d:%02d",(int) hh, int (mm), int (ss));
			return std::string(sbuf);
		}
		
		unsigned char hh,mm,ss;
		double rdg; // units are seconds (s)

};

#endif

