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


#include "CGGTTSOutput.h"
//
// public members
//

CGGTTSOutput::CGGTTSOutput()
{
}
		
CGGTTSOutput::CGGTTSOutput(int constellation,std::string rnxcode1,std::string rnxcode2,std::string rnxcode3,bool isP3,bool reportMSIO,std::string frc,std::string path,
		std::string ephemerisPath,std::string ephemerisFile
		):
		constellation(constellation),rnxcode1(rnxcode1),rnxcode2(rnxcode2),rnxcode3(rnxcode3),isP3(isP3),FRC(frc),path(path),
		ephemerisPath(ephemerisPath),ephemerisFile(ephemerisFile),reportMSIO(reportMSIO)
{
}
		




