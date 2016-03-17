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
//
// Modification history

#include <sys/time.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>

#include "Debug.h"
#include "OKCounterD.h"
#include "Server.h"

#define NCHANNELS 6
#define BASEADDR 0x20

extern ostream *debugStream;

//
// public members
//

OKCounterD::OKCounterD(int argc,char **argv)
{
	init();
}

OKCounterD::~OKCounterD()
{
	server->stop();
	delete server;
}

void OKCounterD::showHelp()
{
	cout << endl << APP_NAME << " version " << OKCOUNTERD_VERSION << endl;
	cout << "Usage: " << APP_NAME << " [options]" << endl;
	cout << "Available options are" << endl;
	cout << "-d <file> Turn on debugging to <file> (use 'stderr' for output to stderr)" << endl;
	cout << "-h print this help message" << endl;
	cout << "-v print version" << endl;
} 

void OKCounterD::showVersion()
{
	cout << APP_NAME <<  " version " << OKCOUNTERD_VERSION << endl;
	cout << "Written by " << AUTHOR << endl;
	cout << "This ain't no stinkin' Perl script!" << endl;
}

void OKCounterD::run()
{
	// Collects data from the FPGA
	
	vector<int> measurements;
	
	DBGMSG(debugStream,"Hello");
	
	server = new Server(this,port);
	server->go();
	
	DBGMSG(debugStream,"Hello");
				 
	xem->UpdateTriggerOuts();
	xem->UpdateWireOuts();
	
	for (;;){
		usleep(10000);
		measurements.clear();
		xem->UpdateTriggerOuts();
		int triggered=0;
		int bitmask=0x01;
		for (int i=0;i<NCHANNELS;i++){
			triggered = triggered || (xem->IsTriggered(0x60,bitmask) && (channelMask & bitmask));
			bitmask=bitmask << 1;
		}
		
		if (triggered){
			int addr=BASEADDR;
			int bitmask=0x01;
			struct timeval tv;
			int rdg;
			unsigned int upperbits,lowerbits;
			xem->UpdateWireOuts();
			for (int i=0;i<NCHANNELS;i++){
				if (channelMask & bitmask){
					if (xem->IsTriggered(0x60,bitmask)){
						gettimeofday(&tv,NULL);
						upperbits=xem->GetWireOutValue(addr+1) & 0xffff;
						lowerbits=xem->GetWireOutValue(addr) & 0xffff;
						rdg = (upperbits  << 16) + lowerbits;
						rdg= (int)rdg*5.0E-9*1.0E9/4.0;
						if (rdg>500000000) rdg -= 1000000000;
						measurements.push_back(i+1);
						measurements.push_back((int) tv.tv_sec);
						measurements.push_back((int) tv.tv_usec);
						measurements.push_back(rdg);
					}
				}
				bitmask=bitmask << 1;
				addr += 2;
			}
			server->sendData(measurements);
		}// if triggered
	}	
}

void OKCounterD::log(string msg)
{
	cout << msg << endl;
}

//
// private members
//

void OKCounterD::init()
{
	xem=NULL;
	dbgOn=false;
	port=21577;
	server=NULL;
	channelMask=0xffff;
}

bool OKCounterD::initializeFPGA(string bitfile)
{
	
	// Open the first XEM - try all board types.
#ifdef OKFRONTPANEL
	xem = new okCFrontPanel;
	if (okCFrontPanel::NoError != xem->OpenBySerial()) {
#else
	xem = new OpenOK;
	if (OpenOK::NoError != xem->OpenBySerial()) {
#endif
		delete xem;
		cerr << "Device could not be opened.  Is one connected?" << endl;
		return false;
	}
	
	DBGMSG(debugStream, "Found  a device: " << xem->GetBoardModelString(xem->GetBoardModel()));

	xem->LoadDefaultPLLConfiguration();	

	// Get some general information about the XEM.
	DBGMSG(debugStream, "Device firmware version: " << xem->GetDeviceMajorVersion() << "." << xem->GetDeviceMinorVersion());
	DBGMSG(debugStream, "Device serial number:" << xem->GetSerialNumber());
	DBGMSG(debugStream, "Device ID: " << xem->GetDeviceID());
	
	// Download the configuration file, if one has been specified on the command line
	if (!bitfile.empty()){
#ifdef OKFRONTPANEL
		if (okCFrontPanel::NoError != xem->ConfigureFPGA(bitfile.c_str())) {
#else
		if (OpenOK::NoError != xem->ConfigureFPGA(bitfile.c_str())) {
#endif
			cerr << "FPGA configuration failed";
			delete xem;
			return false;
		}
	}
	// Check for FrontPanel support in the FPGA configuration.
	DBGMSG(debugStream, "FrontPanel support is " << (xem->IsFrontPanelEnabled()?"":"not ") << "enabled");
	
	return true;
}
