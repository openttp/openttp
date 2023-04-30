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

#ifndef __RINEX_FILE_H_
#define __RINEX_FILE_H_

#include <fstream>
#include <string>
#include <vector>

class RINEXFile
{
	
	public:
		
		enum RINEXVERSIONS {V2=2, V3=3, V4=4};
		
		RINEXFile();
		virtual ~RINEXFile(){};
		virtual bool read(std::string){return true;};
	
		double readRINEXVersion(std::string fname);
		double version;
		
		bool leapSecsValid(){return leapsecs != -1;}
		int leapsecs;
		
	protected:
				
		bool readParam(std::string &str,int start,int len,int *val);
		bool readParam(std::string &str,int start,int len,float *val);
		bool readParam(std::string &str,int start,int len,double *val);
		bool read4DParams(std::ifstream &fin,int startCol,double *darg1,double *darg2,double *darg3,double *darg4,unsigned int *cnt);
		
	private:
		
		void init();
		
};

#endif
