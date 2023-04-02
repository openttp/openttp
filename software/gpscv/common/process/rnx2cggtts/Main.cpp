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


#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <paths.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "Debug.h"
#include "Application.h"

std::ostream *debugStream;
std::string   debugFileName;
std::ofstream debugLog;
int verbosity=1;
bool shortDebugMessage=false;

extern Application *app;

int main(
	int argc,
	char **argv)
{
	
	// make sure we are using UTC so mktime() works the way we want
	setenv("TZ","UTC",1);
	tzset();
	
	debugStream= NULL;
	new Application(argc,argv); // this will initialize 'app'
	app->run();
	
	return EXIT_SUCCESS;
}

