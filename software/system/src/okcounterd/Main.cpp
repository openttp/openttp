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

#include "Debug.h"
#include "OKCounterD.h"

#define PID_FILE _PATH_VARRUN "okcounterd.pid"

ostream *debugStream;
string   debugFileName;
ofstream debugLog;

OKCounterD *app;

int main(
	int argc,
	char **argv)
{
	
	char c;
	FILE *str;
	pid_t pid;
	
	struct sched_param	sched;
	debugStream= NULL;
	
	OKCounterD *app = new OKCounterD(argc,argv);
	
	// Process the command line options
	while ((c=getopt(argc,argv,"d:hv")) != EOF)
	{
		switch(c)
		{
			case 'd':	// enable debugging 
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
					app->setDebugOn(true);
					break;
				}
				break;
			case 'h':
				app->showHelp();
				exit(EXIT_SUCCESS);
				break;
			case 'v': 
				app->showVersion();
				exit(EXIT_SUCCESS);
				break;
		}
	}
	
	// Check whether the daemon is already running and exit if it is 
	if (!access(PID_FILE, R_OK)){
		if ((str = fopen(PID_FILE, "r"))){
			fscanf(str, "%d", &pid);
			fclose(str);
			DBGMSG(debugStream,APP_NAME << " with PID " << pid << " found : " << kill(pid, 0));
			if (!kill(pid, 0) || errno == EPERM){
				cerr << APP_NAME << " is already running as process " << pid << endl 
					<< "If it is no longer running, remove " << PID_FILE << endl;
    			exit(EXIT_FAILURE);
			}
			else{
				unlink(PID_FILE);
			}
		}
	}
	
	if ((str = fopen(PID_FILE, "w"))){
		fprintf(str,"%d\n",getpid());
		fclose(str);
	}
	
	
	// Are we root ? 
	if (0!=getuid()){
		cerr << APP_NAME << ": must be run as root" << endl;
		exit(EXIT_FAILURE);
	}
	
	// Set up system logging 
	// We log to stderr if debugging is on and the console if it is 
	// available, using our pid. We are an "other system daemon" 
	openlog("okcounterd",(app->debugOn() ? LOG_PERROR : 0) | LOG_PID | LOG_CONS, LOG_DAEMON);
	
	// Change working directory to /. This is to avoid the wd being   
	// on a mounted file system, which cannot then be unmounted during
	// a shutdown because daemons will persist to the last CPU cycle  
	chdir("/");
		
	// Close unneeded file descriptors.
	//close(0);
	//close(1);

	syslog(LOG_INFO,"okcounterd version %s started",OKCOUNTERD_VERSION);

	// Become a low priority real-time process 
	sched.sched_priority = sched_get_priority_min(SCHED_FIFO);
	if ( sched_setscheduler(0, SCHED_FIFO, &sched) == -1 )
	{
		syslog(LOG_ERR,"%s",strerror(errno));
		syslog(LOG_ERR,"exiting");
		unlink(PID_FILE);
		exit(EXIT_FAILURE);
	}
	// Lock memory so we don't get paged out
	if ( mlockall(MCL_FUTURE|MCL_CURRENT) == -1 )
	{
		syslog(LOG_ERR,"%s",strerror(errno));
		syslog(LOG_ERR,"exiting");
		unlink(PID_FILE);
		exit(EXIT_FAILURE);
	}
	
	if (false == okFrontPanelDLL_LoadLib("/usr/local/lib/libokFrontPanel.so")) {
		syslog(LOG_ERR,"FrontPanel DLL could not be loaded");
		return(EXIT_FAILURE);
	}
	
	app->initializeFPGA("");	
	app->run();

	unlink(PID_FILE);
	syslog(LOG_INFO,"stopped");	
	closelog();	
	
	return EXIT_SUCCESS;
}