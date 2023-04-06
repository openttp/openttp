//
//
// The MIT License (MIT)
//
// Copyright (c) 2023  Michael J. Wouters
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

#define APP_NAME "rnx2cggtts"
#define APP_AUTHORS "Michael Wouters"
#define APP_VERSION "0.0.1"
#define APP_CONFIG "rnx2cggtts.conf"

class CGGTTSOutput;

class Application
{
	public:
		
		Application(int argc,char **argv);
		~Application();
		
		void run();
		
		void logMessage(std::string msg);
		
	private:
		
		enum CGGTTSNamingConvention {Plain,BIPM};
		
		int mjd;
		
		std::string homeDir;
		std::string rootDir;
		std::string configurationFile;
		std::string logFile;
		
		std::string RINEXpath;
		std::string RINEXobsPath;
		std::string RINEXnavPath;
		std::string stationName;
		
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
		
		std::string CGGTTSPath;
		int CGGTTSnamingConvention;
		
		void init();
		std::string relativeToAbsolutePath(std::string);
		bool canOpenFile(std::string);
		
		void showHelp();
		void showVersion();
		
		bool loadConfig();
		bool setConfig(ListEntry *,const char *,const char *,std::string &,bool *ok,bool required=true);
		bool setConfig(ListEntry *,const char *,const char *,double *,bool *ok,bool required=true);
		bool setConfig(ListEntry *,const char *,const char *,int *,bool *ok, bool required=true);
	
		std::string FindRINEXObsFile(int,int,std::vector<std::string> &);
		std::string FindRINEXNavFile(int,int,std::vector<std::string> &);
		
		std::string makeCGGTTSFilename(CGGTTSOutput & cggtts, int MJD);
};
#endif
