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
// THE SOFTWARE
//
// Bitfile loader for OpalKelly boards
//
// Modification history
// 2015-05-07 MJW First version 
// 2015-03-17 MJW,ELM OpenOK support
//

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef OKFRONTPANEL
#include <okFrontPanelDLL.h>
#else
#include "OpenOK.h"
#endif

using namespace std;

#define APPNAME "okbfloader"
#define VERSION "0.2"
#define AUTHOR  "Michael Wouters, Louis Marais"

#ifdef OKFRONTPANEL
okCFrontPanel * initializeFPGA(char *bitfile);
#else
OpenOK* initializeFPGA(char *bitfile);
#endif

static void printHelp();
static void printVersion();

#define LOGMSG( os, msg ) os << msg << std::endl
       
static int channelMask=1;

#ifdef OKFRONTPANEL
okCFrontPanel * 
#else
OpenOK *
#endif
initializeFPGA(
	string bitfile)
{
#ifdef OKFRONTPANEL
	okCFrontPanel *xem;
#else
	OpenOK *xem;
#endif

	// Open the first XEM - try all board types.
#ifdef OKFRONTPANEL
	xem = new okCFrontPanel;
	if (okCFrontPanel::NoError != xem->OpenBySerial()) {
#else
	xem = new OpenOK;
	if (OpenOK::NoError != xem->OpenBySerial()) {	
#endif
		delete xem;
		LOGMSG(cerr,"Device could not be opened.  Is one connected?");
		return(NULL);
	}
	
	LOGMSG(cout,"Found a device: " << xem->GetBoardModelString(xem->GetBoardModel()));

	xem->LoadDefaultPLLConfiguration();	

	// Get some general information about the XEM.

	LOGMSG(cout, "Device firmware version: " << xem->GetDeviceMajorVersion() << "." << xem->GetDeviceMinorVersion());
	LOGMSG(cout, "Device serial number:" << xem->GetSerialNumber());
	LOGMSG(cout, "Device ID: " << xem->GetDeviceID());

	// Download the configuration file.
#ifdef OKFRONTPANEL
	if (okCFrontPanel::NoError != xem->ConfigureFPGA(bitfile.c_str())) {
#else
	if (OpenOK::NoError != xem->ConfigureFPGA(bitfile.c_str())) {
#endif
		LOGMSG(cerr,"FPGA configuration failed");
		delete xem;
		return(NULL);
	}

	// Check for FrontPanel support in the FPGA configuration.
	if (xem->IsFrontPanelEnabled())
		LOGMSG(cout,"FrontPanel support is enabled");
	else
		LOGMSG(cerr,"FrontPanel support is not enabled");

	return xem;
}


static void
printHelp(
	)
{
	cout << "Usage: " << APPNAME << "[-chv] <bitfile>" << endl;
	cout << "-h  show this help" << endl;
	cout << "-v  print version" << endl;
}

static void
printVersion(
	)
{
	cout <<  APPNAME << " version " << VERSION << endl;
	cout << "Compiled against ";
#ifdef OKFRONTPANEL
	cout << " okFrontPanel" << endl;
#else 
	cout << " OpenOK2" << endl;
#endif
	cout << "Written by " << AUTHOR << endl;
	cout << "This ain't no stinkin' Perl script!" << endl;
}

int
main(int argc, 
		 char *argv[])
{
	int opt;
	string bitfile;
	
	while ((opt=getopt(argc,argv,"hv")) != -1){
		switch (opt)
		{
			case 'h':
				printHelp();
				exit(EXIT_SUCCESS);
				break;
			case 'v':
				printVersion();
				exit(EXIT_SUCCESS);
				break;
			default:
				LOGMSG(cerr,"Unknown option");
				printHelp();
				exit(EXIT_FAILURE);
				break;
		}
	}
	
	if (optind >= argc) {
		cerr << "Expected bitfile name" << endl;
		printHelp();
		return(EXIT_FAILURE);
	}
	
	bitfile = argv[optind];
	FILE *fp = fopen(bitfile.c_str(),"r");
	if (!fp){
		LOGMSG(cerr,"Unable to open " + bitfile);
		return(EXIT_FAILURE);
	}
	fclose(fp);
	
#ifdef OKFRONTPANEL
	if (FALSE == okFrontPanelDLL_LoadLib("/usr/local/lib/libokFrontPanel.so")) {
		LOGMSG(cerr,"FrontPanel DLL could not be loaded");
		return(EXIT_FAILURE);
	}
#endif
	// Initialize the FPGA with our bitfile.

#ifdef OKFRONTPANEL
	okCFrontPanel *xem;
#else
	OpenOK *xem;
#endif
	xem = initializeFPGA(bitfile);
	if (NULL == xem) {
		LOGMSG(cerr,"FPGA could not be initialized");
		return(EXIT_FAILURE);
	}

	return(EXIT_SUCCESS);

}
