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

#ifndef __SV_MEASUREMENT_H_
#define __SV_MEASUREMENT_H_

class ReceiverMeasurement;

class SVMeasurement
{
	public:
		SVMeasurement(unsigned char n,double m,ReceiverMeasurement *rxm)
		{
			svn=n;
			meas=m;
			lli=0;
			signal=0;
			rm=rxm;
		}
		unsigned char svn;
		double meas; // units are seconds (s)
		unsigned char lli;
		unsigned char signal;

		double dbuf1,dbuf2; // FIXME used as temporaries when interpolating. Remove eventually.
		unsigned int uibuf;
		
		ReceiverMeasurement *rm; // allows us to get at data common to each SV
		
		unsigned int memoryUsage(){return sizeof(*this);}
};

#endif