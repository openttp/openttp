//
// The MIT License (MIT)
//
// Copyright (c) 2016 Michael J. Wouters
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

#include <cstdio>
#include <cmath>
#include "Timer.h"

Timer::Timer()
{
}

void Timer::start()
{
	gettimeofday(&tvstart,NULL);
}

void Timer::stop()
{
	gettimeofday(&tvstop,NULL);
}

double Timer::elapsedTime(int unit)
{
	double t = (tvstop.tv_sec-tvstart.tv_sec) + (tvstop.tv_usec-tvstart.tv_usec)/1.0E6;
	switch (unit){
		case USECS: t*= 1000000;break;
		case MSECS: t*= 1000;break;
		case SECS: break;
	}
	return t;
}

