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
#define APP_AUTHORS "Michael Wouters,Peter Fisk,Bruce Warrington,Louis Marais,Malcolm Lawn"
#define APP_VERSION "0.1.3"
#define APP_CONFIG "gpscv.conf"

#define CVACUUM 299792458

class Antenna;
class Counter;
class Receiver;
class CounterMeasurement;
class ReceiverMeasurement;
class MeasurementPair;

class CGGTTSOutput{
	public:
		
		enum EphemerisSource{ GNSSReceiver, UserSupplied};
		
		CGGTTSOutput();
		CGGTTSOutput(int constellation,int code,std::string path,std::string calID,
			double internalDelay,int delayKind,
			int ephemerisSource,std::string ephemerisPath,std::string ephemerisFile
			):
			constellation(constellation),code(code),path(path),
			calID(calID),internalDelay(internalDelay),delayKind(delayKind),
			ephemerisSource(ephemerisSource),ephemerisPath(ephemerisPath),ephemerisFile(ephemerisFile){};
		
		int constellation;
		int code;
		std::string path;
		std::string calID;
		double internalDelay;
		int delayKind;
		int ephemerisSource;
		std::string ephemerisPath;
		std::string ephemerisFile;
		
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
		
		std::string timeStamp();
		void logMessage(std::string msg);
		
		long referenceTime(){return refTime;}
		
	private:
	
		enum CGGTTSNamingConvention {Plain,BIPM};
		
		void init();
		std::string relativeToAbsolutePath(std::string);
		void   makeFilenames();
		bool decompress(std::string);
		void compress(std::string);
		std::string makeCGGTTSFilename(CGGTTSOutput & cggtts, int MJD);
		
		
		bool loadConfig();
		bool setConfig(ListEntry *,const char *,const char *,std::string &,bool *ok,bool required=true);
		bool setConfig(ListEntry *,const char *,const char *,double *,bool *ok,bool required=true);
		bool setConfig(ListEntry *,const char *,const char *,int *,bool *ok, bool required=true);
		
		bool writeRIN2CGGTTSParamFile(Receiver *,Antenna *,std::string);
		
		void matchMeasurements(Receiver *,Counter *);
		void fixBadSawtoothCorrection(Receiver *,Counter *);
		void writeReceiverTimingDiagnostics(Receiver *,Counter *,std::string);
		void writeSVDiagnostics(Receiver *,std::string);
		
		std::string appName;
		
		Antenna *antenna;
		Receiver *receiver;
		Counter *counter;
		
		bool createCGGTTS,createRINEX;
		std::vector<CGGTTSOutput> CGGTTSoutputs;
		
		std::string CGGTTSref;
		std::string CGGTTScomment;
		std::string CGGTTSlab;
		std::string CGGTTSreceiverID; // two letter code
		std::string CGGTTSlabCode;    // two letter code
		
		int CGGTTSversion;
		int CGGTTSRevDateYYYY,CGGTTSRevDateMM,CGGTTSRevDateDD;
		int CGGTTSminTrackLength;
		double CGGTTSminElevation, CGGTTSmaxDSG,CGGTTSmaxURA;
		
		// RINEX generation
		std::string observer;
		std::string agency;
		std::string v3name;
		bool forceV2name;
		
		bool   allObservations;
		
		double antCableDelay,refCableDelay;
		
		std::string logFile;
		
		int MJD,startTime,stopTime;
		long refTime;
		int interval;
		int RINEXversion;
		std::string homeDir;
		std::string rootDir;
		std::string configurationFile;
		std::string counterPath,counterExtension,counterFile;
		std::string receiverPath,receiverExtension,receiverFile;
		std::string RINEXPath,RINEXnavFile,RINEXobsFile;
		std::string CGGTTSPath;
		int CGGTTSnamingConvention;
		std::string timingDiagnosticsFile;
		std::string processingLogPath,processingLog;
		std::string tmpPath;
		
		std::string gzip;
		
		MeasurementPair **mpairs;
		
		pid_t pid;
		bool timingDiagnosticsOn;
		bool SVDiagnosticsOn;
		bool generateNavigationFile;
		bool TICenabled;
		bool fixBadSawtooth;
		double sawtoothStepThreshold;
};
#endif

