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

#include <time.h>

#include "Utility.h"

std::string Utility::trim(std::string const& str)
{
	std::size_t first = str.find_first_not_of(' ');

	// If there is no non-whitespace character, both first and last will be std::string::npos (-1)
	// There is no point in checking both, since if either doesn't work, the
	// other won't work, either.
	if(first == std::string::npos)
		return "";

	std::size_t last  = str.find_last_not_of(' ');

	return str.substr(first, last-first+1);
}


void Utility::MJDtoDate(int mjd,int *year,int *mon, int *mday, int *yday)
{
	time_t tt = (mjd - 40587)*86400;
	struct tm *utc = gmtime(&tt);
	*year = 1900 + utc->tm_year;
	*mon  = utc->tm_mon+1;
	*mday = utc->tm_mday;
	*yday = utc->tm_yday+1;
}

bool Utility::linearFit(double x[], double y[],int n,double results[])
{
	return true;
}
