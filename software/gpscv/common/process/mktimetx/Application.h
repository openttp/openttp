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

#ifndef __APPLICATION_H_
#define __APPLICATION_H_

#include <sys/types.h>
#include <unistd.h>
       
#include <string>
#include <vector>

#include <boost/concept_check.hpp>
#include <configurator.h>

#define APP_NAME "mktimetx"
#define APP_AUTHORS "Michael Wouters,Peter Fisk,Bruce Warrington,Malcolm Lawn"
#define APP_VERSION "0.1.0"
#define APP_CONFIG "gpscv.conf"

#define CVACUUM 299792458

using namespace std;

class Antenna;
class Counter;
class Receiver;
class CounterMeasurement;
class ReceiverMeasurement;
class MeasurementPair;

class CGGTTSOutput{
	public:
		CGGTTSOutput();
		CGGTTSOutput(int constellation,int code,string path,string calID,double internalDelay):
			constellation(constellation),code(code),path(path),
			calID(calID),internalDelay(internalDelay){};
		
		int constellation;
		int code;
		string path;
		string calID;
		double internalDelay;
};

class Application
{
	public:
		
		Application(int argc,char **argv);
		~Application();
		
		void setMJD(int);
		void run();
		
		void showHelp();
		void showVersion();
		
		string timeStamp();
		void logMessage(string msg);
		
	private:
	
		enum CGGTTSNamingConvention {Plain,BIPM};
		
		void init();
		string relativeToAbsolutePath(string);
		void   makeFilenames();
		bool decompress(string);
		void compress(string);
		string makeCGGTTSFilename(CGGTTSOutput & cggtts, int MJD);
		
		
		bool loadConfig();
		bool setConfig(ListEntry *,const char *,const char *,string &,bool required=true);
		bool setConfig(ListEntry *,const char *,const char *,double *,bool required=true);
		bool setConfig(ListEntry *,const char *,const char *,int *,bool required=true);
		
		bool writeRIN2CGGTTSParamFile(Receiver *,Antenna *,string);
		
		void matchMeasurements(Receiver *,Counter *);
		void writeReceiverTimingDiagnostics(Receiver *,Counter *,string);
		void writeSVDiagnostics(Receiver *,string);
		
		string appName;
		
		Antenna *antenna;
		Receiver *receiver;
		Counter *counter;
		
		bool createCGGTTS,createRINEX;
		vector<CGGTTSOutput> CGGTTSoutputs;
		
		string CGGTTSref;
		string CGGTTScomment;
		string CGGTTSlab;
		string CGGTTSreceiverID; // two letter code
		string CGGTTSlabCode;    // two letter code
		
		int CGGTTSversion;
		int CGGTTSRevDateYYYY,CGGTTSRevDateMM,CGGTTSRevDateDD;
		int CGGTTSminTrackLength;
		double CGGTTSminElevation, CGGTTSmaxDSG;
		
		string observer;
		string agency;
		
		double antCableDelay,refCableDelay;
		
		string logFile;
		
		int MJD;
		int interval;
		int RINEXversion;
		string homeDir;
		string configurationFile;
		string counterPath,counterExtension,counterFile;
		string receiverPath,receiverExtension,receiverFile;
		string RINEXPath,RINEXnavFile,RINEXobsFile;
		string CGGTTSPath;
		int CGGTTSnamingConvention;
		string timingDiagnosticsFile;
		string processingLog;
		string tmpPath;
		
		string gzip;
		
		MeasurementPair **mpairs;
		
		pid_t pid;
		bool timingDiagnosticsOn;
		bool SVDiagnosticsOn;
		bool generateNavigationFile;
		
};
#endif

