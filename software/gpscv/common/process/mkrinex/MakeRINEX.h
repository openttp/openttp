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

#ifndef __MAKE_RINEX_H_
#define __MAKE_RINEX_H_

#include <sys/types.h>
#include <unistd.h>
       
#include <string>
#include <configurator.h>

#define APP_NAME "mkrinex"
#define APP_AUTHORS "Michael Wouters"
#define APP_VERSION "0.1.0"
#define APP_CONFIG "mkrinex.conf"

#define CVACUUM 299792458

using namespace std;

class Antenna;
class Counter;
class Receiver;
class CounterMeasurement;
class ReceiverMeasurement;
class MeasurementPair;

class MakeRINEX
{
	public:
		
		MakeRINEX(int argc,char **argv);
		~MakeRINEX();
		
		void setMJD(int);
		void run();
		
		void showHelp();
		void showVersion();
		
	private:
	
		void init();
		void makeFilenames();
		bool loadConfig();
		bool setConfig(ListEntry *,const char *,const char *,string &);
		bool setConfig(ListEntry *,const char *,const char *,double *);
		bool setConfig(ListEntry *,const char *,const char *,int *);
		
		bool writeRIN2CGGTTSParamFile(Receiver *,Antenna *,string);
		
		void matchMeasurements(Receiver *,Counter *);
		void writeReceiverTimingDiagnostics(Receiver *,Counter *,string);
		void writeSVDiagnostics(Receiver *,string);
		
		string appName;
		
		Antenna *antenna;
		Receiver *receiver;
		Counter *counter;
		
		string CGGTTSref;
		string CGGTTScomment;
		string CGGTTSlab;
		int CGGTTSversion;
		int CGGTTSRevDateYYYY,CGGTTSRevDateMM,CGGTTSRevDateDD;
		
		
		string observer;
		string agency;
		
		double antCableDelay,refCableDelay,C1InternalDelay;
		
		int MJD;
		int interval;
		int RINEXversion;
		string homeDir;
		string configurationFile;
		string counterPath,counterExtension,counterFile;
		string receiverPath,receiverExtension,receiverFile;
		string RINEXPath,RINEXnavFile,RINEXobsFile;
		string timingDiagnosticsFile;
		string processingLog;
		string tmpPath;
		
		MeasurementPair **mpairs;
		
		pid_t pid;
		bool timingDiagnosticsOn;
		bool SVDiagnosticsOn;
		bool generateNavigationFile;
		
};
#endif