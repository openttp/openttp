//
//
// The MIT License (MIT)
//
// Copyright (c) 2026  Michael J. Wouters
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

#include "Debug.h"

#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/timex.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <utime.h>

#include <algorithm>
#include <iostream>
#include <stack>
#include <fstream>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include "LCDCmd.h"
#include "Version.h"

#define LCDCMD_VERSION "1.0"

#define BAUD 115200
#define PORT "/dev/lcd"

using namespace std;
using namespace::boost;

bool LCDCmd::timeout=false;
using namespace std;

ostream *debugStream;
string   debugFileName;
ofstream debugLog;
int verbosity=3;
bool shortDebugMessage=false;


#define CMD_SHOW_VERSION  1
#define CMD_RESET_DEVICE  2

LCDCmd *app;

LCDCmd::LCDCmd(int,char **)
{
	init();
}

LCDCmd::~LCDCmd()
{
	Uninit_Serial();
	log("Shutdown");
}


void LCDCmd::showVersion()
{

}

void LCDCmd::reset(){

}

void LCDCmd::signalHandler(int sig)
{
	switch (sig){
		case SIGALRM:
		{
			DBGMSG(debugStream,TRACE, "SIGALRM");
			LCDCmd::timeout=true;
			break;// can't call Debug()
		}
		case SIGTERM: case SIGQUIT: case SIGINT: case SIGHUP:
		{
			DBGMSG(debugStream,TRACE, "SIGxxx");
			delete app;
			exit(EXIT_SUCCESS);
			break;
		}
	}

}

void LCDCmd::startTimer(long secs)
{

	struct itimerval itv; 

	LCDCmd::timeout=false;

	itv.it_interval.tv_sec=0;
	itv.it_interval.tv_usec=0; // this stops the timer on timeout
	itv.it_value.tv_sec=secs;
	itv.it_value.tv_usec=0;
	if (setitimer(ITIMER_REAL,&itv,0)!=0)
		log("Start timer failed"); 

}

void LCDCmd::stopTimer()
{
	struct itimerval itv;
	itv.it_interval.tv_sec=0;
	itv.it_interval.tv_usec=0;
	itv.it_value.tv_sec=0;
	itv.it_value.tv_usec=0;
	if (setitimer(ITIMER_REAL,&itv,0)!=0)
		log("Stop timer failed");  
}

void LCDCmd::log(std::string msg)
{
	std::cout << msg << std::endl;
}

void LCDCmd::init()
{

	if(Serial_Init(PORT,BAUD)){
		DBGMSG(debugStream,TRACE, "Could not open port " << PORT << " at " << BAUD << " baud.");
		exit(EXIT_FAILURE);
	}
	else{
    DBGMSG(debugStream,TRACE, PORT << " opened at "<< BAUD <<" baud");
	}

	// Clear the buffer
	while(BytesAvail())
		GetByte();

	// use SIGALRM to detect lack of UI activity and return to status display

	sa.sa_handler = signalHandler;
	sigemptyset(&(sa.sa_mask)); // we block nothing
	sa.sa_flags=0;

	sigaction(SIGALRM,&sa,NULL);

	sigaction(SIGTERM,&sa,NULL);

	sigaction(SIGQUIT,&sa,NULL);

	sigaction(SIGINT,&sa,NULL);

	sigaction(SIGHUP,&sa,NULL);

}


void LCDCmd::getResponse()
{
	int timed_out =1;
	for(int k=0;k<=100;k++){
		usleep(10000);
		if(packetReceived()){
			ShowReceivedPacket();
			timed_out = 0; 
			break;
		}
	}
	if(timed_out){
		DBGMSG(debugStream,TRACE,"Timed out waiting for a response");
	}
}


void showHelp()
{
	cout << "Usage: lcdconfig [options]" << endl;
	cout << "Available options are" << endl;
	cout << "\t-d" << endl << "\t Turn on debugging" << endl;
	cout << "\t-f" << endl << "\t Show firmware version" << endl;
	cout << "\t-r" << endl << "\t Reset device" << endl;
	cout << "\t-h" << endl << "\t Show this help" << endl;
	cout << "\t-v" << endl << "\t Show version" << endl;
}

void showVersion()
{
	cout << "lcdcmd v" << LCDCMD_VERSION << ", last modified " << LAST_MODIFIED << endl;
	cout << "This ain't no stinkin' Perl script!" << endl;
}

int main(int argc,char **argv)
{
	debugStream = NULL;

	int c;
	int cmd = 0;

	while ((c=getopt(argc,argv,"hvd:fr")) != EOF){
		switch(c)
		{
			case 'h':showHelp(); exit(EXIT_SUCCESS);
			case 'v':showVersion();exit(EXIT_SUCCESS);
			case 'd':
			{
				string dbgout = optarg;
				if ((string::npos != dbgout.find("stderr"))){
					debugStream = & std::cerr;
				}
				else{
					debugFileName = dbgout;
					debugLog.open(debugFileName.c_str(),ios_base::app);
					if (!debugLog.is_open()){
						cerr << "Unable to open " << dbgout << endl;
						exit(EXIT_FAILURE);
					}
					debugStream = & debugLog;
				}
				break;
			}
			case 'f':
			{
				cmd = CMD_SHOW_VERSION;
				break;
			}
		}
	}

	app = new LCDCmd(argc,argv);

	switch (cmd)
	{
		case CMD_SHOW_VERSION:
			app->showVersion();
			break;
	}

	return(EXIT_SUCCESS);
}
