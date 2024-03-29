//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Michael J. Wouters
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
// 2018-09-03 ELM Conditional added for LCD "splash" message to distinguish TTS / OPENTTP


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

#include "LCDConfigure.h"
#include "Version.h"

#define LCDCONFIGURE_VERSION "1.0"

#define BAUD 115200
#define PORT "/dev/lcd"

using namespace std;
using namespace::boost;

bool LCDConfigure::timeout=false;
using namespace std;

ostream *debugStream;
string   debugFileName;
ofstream debugLog;
int verbosity=1;
bool shortDebugMessage=false;

LCDConfigure *app;

LCDConfigure::LCDConfigure(int argc,char **argv)
{

	//char c; // does not work where char is unsigned, such as on BBB
	int c;
	while ((c=getopt(argc,argv,"hvd:u:")) != EOF)
	{
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
			case 'u':user=optarg;break;
		}
	}

	init();

}

LCDConfigure::~LCDConfigure()
{
	Uninit_Serial();
	log("Shutdown");
	unlink(lockFile.c_str());
}

void LCDConfigure::touchLock()
{
	// the lock is touched periodically to signal that
	// the process is still alive
	utime(lockFile.c_str(),0);
}

void LCDConfigure::clearDisplay()
{
	for (int i=0;i<4;i++)
		status[i]="";
	outgoing_response.command = 6;
	outgoing_response.data_length = 0;
	send_packet();
	getResponse();
}

void LCDConfigure::run()
{
	clearDisplay();
	#ifdef TTS
	updateLine(1,"   NMI Australia");
	#endif
	//            12345678901234567890
	#ifdef OTTP
	updateLine(1,"      OpenTTP");
	#endif
	updateLine(2,"Time Transfer System");
	storeState();
}

void LCDConfigure::signalHandler(int sig)
{
	switch (sig)
	{
		case SIGALRM: 
			DBGMSG(debugStream,TRACE, "SIGALRM");
			LCDConfigure::timeout=true;
			break;// can't call Debug()
		case SIGTERM: case SIGQUIT: case SIGINT: case SIGHUP:
			DBGMSG(debugStream,TRACE, "SIGxxx");
			delete app;
			exit(EXIT_SUCCESS);
			break;
	}

}

void LCDConfigure::startTimer(long secs)
{

	struct itimerval itv; 

	LCDConfigure::timeout=false;

	itv.it_interval.tv_sec=0;
	itv.it_interval.tv_usec=0; // this stops the timer on timeout
	itv.it_value.tv_sec=secs;
	itv.it_value.tv_usec=0;
	if (setitimer(ITIMER_REAL,&itv,0)!=0)
		log("Start timer failed"); 

}

void LCDConfigure::stopTimer()
{
	struct itimerval itv;
	itv.it_interval.tv_sec=0;
	itv.it_interval.tv_usec=0;
	itv.it_value.tv_sec=0;
	itv.it_value.tv_usec=0;
	if (setitimer(ITIMER_REAL,&itv,0)!=0)
		log("Stop timer failed");  
}

void LCDConfigure::log(std::string msg)
{
	std::cout << msg << std::endl;
}

void LCDConfigure::init()
{

	if(Serial_Init(PORT,BAUD)){
		DBGMSG(debugStream,TRACE, "Could not open port " << PORT << " at " << BAUD << " baud.");
		exit(EXIT_FAILURE);
	}
	else{
    DBGMSG(debugStream,TRACE, PORT << " opened at "<< BAUD <<" baud");
	}

	/* Clear the buffer */
	while(BytesAvail())
    GetByte();

	for (int i=0;i<4;i++)
	{
		status[i]="";
	}

	// use SIGALRM to detect lack of UI activity and return to status display

	sa.sa_handler = signalHandler;
	sigemptyset(&(sa.sa_mask)); // we block nothing
	sa.sa_flags=0;

	sigaction(SIGALRM,&sa,NULL);

	sigaction(SIGTERM,&sa,NULL);

	sigaction(SIGQUIT,&sa,NULL);

	sigaction(SIGINT,&sa,NULL);

	sigaction(SIGHUP,&sa,NULL);

	statusLEDsOn();
}



void LCDConfigure::showHelp()
{
	cout << "Usage: lcdconfig [options]" << endl;
	cout << "Available options are" << endl;
	cout << "\t-d" << endl << "\t Turn on debuggging" << endl;
	cout << "\t-h" << endl << "\t Show this help" << endl;
	cout << "\t-v" << endl << "\t Show version" << endl;
}

void LCDConfigure::showVersion()
{
	cout << "lcdconfig v" << LCDCONFIGURE_VERSION << ", last modified " << LAST_MODIFIED << endl;
	cout << "This ain't no stinkin' Perl script!" << endl;
}

void LCDConfigure::getResponse()
{
	int timed_out =1;
	for(int k=0;k<=100;k++)
	{
		usleep(10000);
		if(packetReceived())
		{
			ShowReceivedPacket();
			timed_out = 0; 
			break;
		}
	}
	if(timed_out){
		DBGMSG(debugStream,TRACE,"Timed out waiting for a response");
	}
}

void LCDConfigure::updateLine(int row,std::string buf)
{
	// a bit of optimization to cut down on I/O and flicker
	if (buf==status[row]) return;
	status[row]=buf;
	std::string tmp=buf;
	if (buf.length() < 20) // pad out to clear the line
		tmp+=std::string(20-buf.length(),' ');

	outgoing_response.command = 31;
	outgoing_response.data[0]=0; //col
	outgoing_response.data[1]=row; //row
	int nch = tmp.length();
	if (nch > 20) nch=20;
	memcpy(&outgoing_response.data[2],tmp.c_str(),nch);
	outgoing_response.data_length =2+nch;
	send_packet();
	getResponse();
}

void LCDConfigure::updateStatusLED(int row,LEDState s)
{
	if (statusLED[row] == s) return;// nothing to do
	statusLED[row] = s;

	int idx = 12 - row*2;
	int rstate,gstate;
	switch (s)
	{
		case Off: rstate=gstate=0;break;
		case RedOn: rstate=100;gstate=0;break;
		case GreenOn: rstate=0;gstate=100;break;
	}

	outgoing_response.command = 34;
	outgoing_response.data[0]=idx;
	outgoing_response.data[1]=rstate;
	outgoing_response.data_length =2;
	send_packet();
	getResponse();

	outgoing_response.command = 34;
	outgoing_response.data[0]=idx-1;
	outgoing_response.data[1]=gstate;
	outgoing_response.data_length =2;
	send_packet();
	getResponse();
}

void LCDConfigure::statusLEDsOn()
{
	for (int i=0;i<4;i++)
		updateStatusLED(i,GreenOn);
	//updateStatusLED(0,GreenOn);
	//updateStatusLED(1,RedOn);
	//updateStatusLED(2,GreenOn);
	//updateStatusLED(3,RedOn);
}

void LCDConfigure::storeState()
{
	outgoing_response.command = 4;
	outgoing_response.data_length = 0;
	send_packet();
	getResponse();
}

int main(int argc,char **argv)
{
	debugStream = NULL;
	app = new LCDConfigure(argc,argv);
	app->run();
	return(EXIT_SUCCESS);
}
