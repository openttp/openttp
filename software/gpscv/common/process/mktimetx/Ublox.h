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


#ifndef __UBLOX_H_
#define __UBLOX_H_

#include <string>

#include "Receiver.h"


class Ublox:public Receiver
{
	public:
		
		Ublox(Antenna *,std::string);
		virtual ~Ublox();
	
		virtual bool readLog(std::string,int,int,int,int);
		
		virtual void addConstellation(int);
		
	protected:
	
	private:
		
		bool checkGalIODNav(GalEphemeris *,int);
		
		GPS::EphemerisData *readGPSEphemeris(std::string);
		void readGALEphemerisINAVSubframe(int,int,unsigned char *ubuf);
		void readGPSEphemerisLNAVSubframe(int,unsigned char *ubuf);
		
		GPS::EphemerisData     *gpsEph[32+1]; // FIXME NSATS should be used
		GalEphemeris *galEph[36+1];
};

#endif

