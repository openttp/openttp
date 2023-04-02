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


#include "GNSSSystem.h"
#include "RINEXFile.h"

GNSSSystem::GNSSSystem()
{
}

GNSSSystem::~GNSSSystem()
{
}
		

unsigned int GNSSSystem::strToObservationCode(std::string s, int RINEXversion)
{
	unsigned int c=0;
	switch (RINEXversion)
	{
		case RINEXFile::V2:
			{
				if (s=="C1"){c=C1C;}
				else if (s=="P1"){c=C1P;}
				else if (s=="P2"){c=C2P;}
				else if (s=="C2"){c=C2L;}
				//else if (s=="L1"){c=L1C;} // FIXME ambiguity
				else if (s=="L1"){c=L1P;}   // this is the more common case 
				else if (s=="L2"){c=L2P;}   // ditto (Javad is L2P, ublox L2 is L2L) 
			}
			break;
		case RINEXFile::V3:
			{
				if (s=="C1C"){c=C1C;}
				else if (s=="C1P"){c=C1P;}
				else if (s=="C2P"){c=C2P;}
				else if (s=="C2L"){c=C2L;}
				else if (s=="C1B"){c=C1B;}
				else if (s=="C2C"){c=C2C;}
				else if (s=="C2M"){c=C2M;}
				else if (s=="C2I"){c=C2I;}
				else if (s=="C7I"){c=C7I;}
				else if (s=="C7Q"){c=C7Q;}
				else if (s=="L1C"){c=L1C;}
				else if (s=="L1P"){c=L1P;}
				else if (s=="L2P"){c=L2P;}
				else if (s=="L2C"){c=L2C;}
				else if (s=="L2L"){c=L2L;}
				else if (s=="L2I"){c=L2I;}
				else if (s=="L7I"){c=L7I;}
			}
			break;
	}
	
	return c;
}
