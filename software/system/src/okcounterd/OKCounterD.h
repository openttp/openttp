//
//
// The MIT License (MIT)
//
// Copyright (c) 2014  Michael J. Wouters
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


#ifndef __OK_COUNTERD_H_
#define __OK_COUNTERD_H_

#include <string>

#ifdef OKFRONTPANEL
	#include <okFrontPanelDLL.h>
#else
	#include "OpenOK.h"
#endif 

#define APP_NAME "okcounterd"
#define AUTHOR "Michael Wouters"
#define OKCOUNTERD_VERSION "0.2.0"
#define OKCOUNTERD_CONFIG "/usr/local/etc/okcounterd.conf"

using namespace std;

class Server;

class OKCounterD
{
	public:
		OKCounterD(int argc,char **argv);
		~OKCounterD();

		bool debugOn(){return dbgOn;};
		void setDebugOn(bool dbg){dbgOn=dbg;}
		
		bool initializeFPGA(string bitfile);
		
		void showHelp();
		void showVersion();
		void run();
		
		void log(string);
		
private:
	
		void init();
		bool dbgOn;
#ifdef OKFRONTPANEL
		okCFrontPanel *xem;
#else
		OpenOK *xem;
#endif
		Server *server;
		long port;
		
		unsigned int channelMask;
};

#endif