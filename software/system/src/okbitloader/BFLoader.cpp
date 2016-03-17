//
// Bitfile loader for OpalKelly boards
//
// Modification history
// 2015-05-07 MJW First version 
//

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <okFrontPanelDLL.h>

using namespace std;

#define APPNAME "bfloader"
#define VERSION "0.1"
#define AUTHOR  "Michael Wouters"

okCFrontPanel * initializeFPGA(char *bitfile);
static void printHelp();
static void printVersion();

#define LOGMSG( os, msg ) os << msg << std::endl
       
static int channelMask=1;

okCFrontPanel * 
initializeFPGA(
	string bitfile)
{
	okCFrontPanel *dev;
	okTDeviceInfo devInfo;
	// Open the first XEM - try all board types.
	dev = new okCFrontPanel;
	if (okCFrontPanel::NoError != dev->OpenBySerial()) {
		delete dev;
		LOGMSG(cerr,"Device could not be opened.  Is one connected?");
		return(NULL);
	}
	dev->GetDeviceInfo(&devInfo);
	LOGMSG(cout,"Found a device: " << devInfo.productName);

	dev->LoadDefaultPLLConfiguration();	

	// Get some general information about the XEM.

	LOGMSG(cout,"Device firmware version: " << devInfo.deviceMajorVersion << " " << devInfo.deviceMinorVersion);
	LOGMSG(cout,"Device serial number: " <<  devInfo.serialNumber);
	LOGMSG(cout,"Device product ID: " << devInfo.productID);

	// Download the configuration file.
	if (okCFrontPanel::NoError != dev->ConfigureFPGA(bitfile.c_str())) {
		LOGMSG(cerr,"FPGA configuration failed");
		delete dev;
		return(NULL);
	}

	// Check for FrontPanel support in the FPGA configuration.
	if (dev->IsFrontPanelEnabled())
		LOGMSG(cout,"FrontPanel support is enabled");
	else
		LOGMSG(cerr,"FrontPanel support is not enabled");

	return(dev);
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
	
	if (FALSE == okFrontPanelDLL_LoadLib("/usr/local/lib/libokFrontPanel.so")) {
		LOGMSG(cerr,"FrontPanel DLL could not be loaded");
		return(EXIT_FAILURE);
	}

	// Initialize the FPGA with our bitfile.
	okCFrontPanel *xem;
	xem = initializeFPGA(bitfile);
	if (NULL == xem) {
		LOGMSG(cerr,"FPGA could not be initialized");
		return(EXIT_FAILURE);
	}

	
	return(EXIT_SUCCESS);

}
