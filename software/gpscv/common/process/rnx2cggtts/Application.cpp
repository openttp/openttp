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


#include <getopt.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>

#include <sys/types.h>
#include <sys/stat.h>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp> // nb C++11 has this

#include <configurator.h>

#include "Application.h"
#include "Antenna.h"
#include "CGGTTS.h"
#include "CGGTTSOutput.h"
#include "Debug.h"
#include "GNSSSystem.h"
#include "Receiver.h"
#include "RINEXNavFile.h"
#include "RINEXObsFile.h"
#include "Timer.h"
#include "Utility.h"

extern Application *app;
extern std::ostream *debugStream;
extern std::string   debugFileName;
extern std::ofstream debugLog;
extern int verbosity;
extern bool shortDebugMessage;

Application *app;

static struct option longOptions[] = {
		{"configuration",required_argument, 0,  0 },
		{"debug",        optional_argument, 0,  0 },
		{"help",         no_argument, 0, 0 },
		{"mjd",          required_argument, 0,  0 },
		{"verbosity",    required_argument, 0,  0 },
		{"version",      no_argument, 0,  0 },
		{"shorten",no_argument, 0,  0 },
		{"licence",no_argument, 0,  0 },
		{"r2cggtts",no_argument, 0,  0 },
};

using boost::lexical_cast;
using boost::bad_lexical_cast;

//
//	Public members
//

Application::Application(int argc,char **argv)
{
	app = this;
	
	init();

	// Process the command line options
	// These override anything in the configuration file, default or specified
	int longIndex;
	int c;
	
	while ((c=getopt_long(argc,argv,"c:d:hm:r",longOptions,&longIndex)) != -1)
	{
		
		switch(c)
		{
			
			case 0: // long options
				{
					switch (longIndex)
					{
						case 0: // --configuration
							configurationFile=optarg;
							break;
						case 1: // --debug
							{
								if (optarg == NULL){
									debugStream = & std::cerr;
								}
								else{
									std::string dbgout = optarg;
									
									if ((std::string::npos != dbgout.find("stderr"))){
										debugStream = & std::cerr;
									}
									else{
										debugFileName = dbgout;
										debugLog.open(debugFileName.c_str(),std::ios_base::out);
										if (!debugLog.is_open()){
											fatalError("Error! Unable to open " + dbgout);
										}
										debugStream = & debugLog;
									}
								}
								break;
							}
						case 2: // --help
							showHelp();
							exit(EXIT_SUCCESS);
							break;
						case 3: // --mjd
							{
								if (1!=std::sscanf(optarg,"%i",&mjd)){
									fatalError("Error! Bad value for option --mjd",true);
								}
								break;
							}
						case 4: // --verbosity
							{
								if (1!=std::sscanf(optarg,"%i",&verbosity)){
									fatalError("Error! Bad value for option --verbosity",true);
								}
								break;
							}
						case 5: // --version
							showVersion();
							exit(EXIT_SUCCESS);
							break;
						case 6:// --shorten
							shortDebugMessage=true;
							break;
						case 7:// --licence
							showLicence();
							exit(EXIT_SUCCESS);
							break;
						case 8:// --licence
							r2cggttsMode = true;
							break;
						default:
							showHelp();
							exit(EXIT_FAILURE);
							break;
					}
				}
				break;
			case 'c':
				configurationFile = optarg;
				break;
			case 'd':
			{
				if (optarg == NULL){
					debugStream = & std::cerr;
				}
				else{
					std::string dbgout = optarg;
					
					if ((std::string::npos != dbgout.find("stderr"))){
						debugStream = & std::cerr;
					}
					else{
						debugFileName = dbgout;
						debugLog.open(debugFileName.c_str(),std::ios_base::out);
						if (!debugLog.is_open()){
							fatalError("! Unable to open " + dbgout);
						}
						debugStream = & debugLog;
					}
				}
				break;
			}
			case 'h':
				showHelp();
				exit(EXIT_SUCCESS);
				break;
			case 'm':
				{
					if (1!=std::sscanf(optarg,"%i",&mjd)){
						fatalError("Error! Bad value for option --mjd",true);
					}
				}
				break;
			case 'r':
				r2cggttsMode = true;
				break;
			default:
				showHelp();
				exit(EXIT_FAILURE);
				break;
		}
	}

	// Two modes of operation 
	// (1) Use a configuration file, which allows a bit more flexibility and removes the need for pre- and post- processing of files
	// (2) r2cggtts compatibility mode, which uses the standard r2cggtts configuration file and naming conventions for input and output files
	if (r2cggttsMode){
		configurationFile = "r2cggtts.conf";
		if (!loadConfig()){
			fatalError("Error! Configuration failed");
		}
	}
	else{
		if (!loadConfig()){
			fatalError("Error! Configuration failed");
		}
	}
}

Application::~Application()
{
	
}

void Application::run()
{
	
	DBGMSG(debugStream,INFO,"Running for MJD " << mjd);
	
	Timer timer;
	timer.start();
	
	if (r2cggttsMode){
		runR2CGGTTSMode();
	}
	else{
		runNativeMode();
	}
	timer.stop();
	DBGMSG(debugStream,INFO,"elapsed time: " << timer.elapsedTime(Timer::SECS) << " s");
}

void Application::fatalError(std::string msg,bool displayHelp){
	if (displayHelp){
		showHelp();
	}
	std::cerr << msg << std::endl;
	exit(EXIT_FAILURE);
}

void Application::logMessage(std::string msg)
{
	std::ofstream ofs;
	ofs.open(logFile.c_str(),std::ios::app);
	ofs << msg << std::endl;
	ofs.close();
	
	DBGMSG(debugStream,INFO,msg);
}

//
//	Private members
//

void Application::init()
{
	r2cggttsMode = false;
	r2cParamsFile = "paramCGGTTS.dat";
	r2cCalRef = "NONE";
	r2cCabDly = 0.0;
	r2cRefDly = 0.0;
	
	antenna = new Antenna();
	receiver = new Receiver();
	
	configurationFile = APP_CONFIG;
	
	mjd = int(time(0)/86400)+40587 - 1; // default MJD for processing is yesterday
	
	char *penv;
	homeDir="";
	if ((penv = getenv("HOME"))){
		homeDir=penv;
	}
	rootDir=homeDir;
	
	configurationFile = rootDir + "/etc/" + APP_CONFIG;
	
	RINEXobsPath = rootDir + "/rinex";
	RINEXnavPath = rootDir + "/rinex";
	stationName  = "";
	
	CGGTTS cdef;
	// Use the default values from a CGGTTS object
	CGGTTSmaxURA = cdef.maxURA;
	CGGTTScomment = cdef.comment;
	CGGTTSref = cdef.ref;
	CGGTTSlab = cdef.lab;
	CGGTTSminElevation = cdef.minElevation;
	CGGTTSmaxDSG = cdef.maxDSG;
	CGGTTSmaxURA = cdef.maxURA;
	CGGTTSminTrackLength = cdef.minTrackLength;
	CGGTTSreceiverID = ""; // two letter code
	CGGTTSlabCode = "";    // two letter code
	CGGTTScalID = "none";
	CGGTTSRevDateMM = cdef.revDateMM;
	CGGTTSRevDateDD = cdef.revDateDD;
	CGGTTSRevDateYYYY = cdef.revDateYYYY;
		
	CGGTTSPath=rootDir+"/cggtts";
	CGGTTSnamingConvention=Plain;
	
	gpsDelay.constellation = GNSSSystem::GPS;
		
}


std::string Application::relativeToAbsolutePath(std::string path)
{
	std::string absPath=path;
	if (path.size() > 0){ 
		if (path.at(0) == '/')
			absPath = path;
		else 
			absPath=rootDir+"/"+path;
	}
	return absPath;
}


bool Application::canOpenFile(std::string f)
{
	DBGMSG(debugStream,TRACE,"Trying to open " << f);
	struct stat statBuf;
	if (0 == stat(f.c_str(),&statBuf)){
		if (S_ISDIR(statBuf.st_mode)){
			//app->logMessage(fname + " is not a regular file");
			return false;
		}
		return true;
	}
	return false;
}

void Application::showHelp()
{
	std::cout << std::endl << APP_NAME << " version " << APP_VERSION << std::endl;
	std::cout << "Usage: " << APP_NAME << " [options]" << std::endl;
	std::cout << "Available options are" << std::endl;
	std::cout << "-c,--configuration <file> full path to the configuration file" << std::endl;
	std::cout << "-d,--debug <file>         turn on debugging to <file> (use 'stderr' for output to stderr)" << std::endl;
	std::cout << "-h,--help                 print this help message" << std::endl;
	std::cout << "-m,--mjd <n>              set the mjd" << std::endl;
	std::cout << "-r,--r2cggtts             r2cggtts mode" << std::endl;
	std::cout << "--shorten                 shorten debugging messages" << std::endl;
	std::cout << "--verbosity <n>           set debugging verbosity" << std::endl;
	std::cout << "--version                 show version" << std::endl;
	std::cout << "--licence                 show licence" << std::endl;

}

void Application::showVersion()
{
	std::cout << APP_NAME <<  " version " << APP_VERSION << std::endl;
	std::cout << "Written by " << APP_AUTHORS << std::endl;
	std::cout << "This ain't no stinkin' Perl script!" << std::endl;
}

void Application::showLicence()
{

std::cout <<  " The MIT License (MIT)" << std::endl;
std::cout << std::endl;
std::cout <<  " Copyright (c) 2023  Michael J. Wouters" << std::endl;
std::cout << std::endl; 
std::cout <<  " Permission is hereby granted, free of charge, to any person obtaining a copy" << std::endl;
std::cout <<  " of this software and associated documentation files (the \"Software\"), to deal" << std::endl;
std::cout <<  " in the Software without restriction, including without limitation the rights" << std::endl;
std::cout <<  " to use, copy, modify, merge, publish, distribute, sublicense, and/or sell" << std::endl;
std::cout <<  " copies of the Software, and to permit persons to whom the Software is" << std::endl;
std::cout <<  " furnished to do so, subject to the following conditions:" << std::endl;
std::cout << std::endl;
std::cout <<  " The above copyright notice and this permission notice shall be included in" << std::endl;
std::cout <<  " all copies or substantial portions of the Software." << std::endl;
std::cout << std::endl;
std::cout <<  " THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR" << std::endl;
std::cout <<  " IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY," << std::endl;
std::cout <<  " FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE" << std::endl;
std::cout <<  " AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER" << std::endl;
std::cout <<  " LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM," << std::endl;
std::cout <<  " OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN" << std::endl;
std::cout <<  " THE SOFTWARE." << std::endl;
}


bool Application::loadConfig()
{
	
	// Our conventional config file format is used to maintain compatibility with existing scripts
	ListEntry *last;
	if (!configfile_parse_as_list(&last,configurationFile.c_str())){
		std::cerr << "Unable to open the configuration file " << configurationFile << " - exiting" << std::endl;
		exit(EXIT_FAILURE);
	}
	
	bool configOK=true;
	int itmp=2;
	std::string stmp;
	
	//
	// Paths
	//
	std::string path="";	
	
	// Parse root path first so that other paths can be constructed correctly
	if (setConfig(last,"paths","root",path,&configOK,false)){
		rootDir=path;
		if (rootDir.at(0) != '/') // not an absolute path so make it relative to the home directory
			rootDir=homeDir+"/"+rootDir;
	}
	
	// root directory has been set so other paths may be constructed now
	
	if (setConfig(last,"paths","rinex observations",path,&configOK))
		RINEXobsPath=relativeToAbsolutePath(path);
	DBGMSG(debugStream,TRACE,"RINEX observation path: " << RINEXobsPath);
	
	if (setConfig(last,"paths","rinex navigation",path,&configOK))
		RINEXnavPath=relativeToAbsolutePath(path);
	DBGMSG(debugStream,TRACE,"RINEX navigation path: " << RINEXobsPath);
	
	
	//
	// RINEX input
	//
	
	setConfig(last,"rinex","station",stationName,&configOK);
	
	//
	// Antenna
	//
	setConfig(last,"antenna","x",&(antenna->x),&configOK);
	setConfig(last,"antenna","y",&(antenna->y),&configOK);
	setConfig(last,"antenna","z",&(antenna->z),&configOK);
	setConfig(last,"antenna","frame",antenna->frame,&configOK,false);
	
	Utility::ECEFtoLatLonH(antenna->x,antenna->y,antenna->z, // latitude and longitude used in ionosphere model
		&(antenna->latitude),&(antenna->longitude),&(antenna->height));
	
	//
	// Receiver
	//
	setConfig(last,"receiver","manufacturer",receiver->manufacturer,&configOK,false);
	setConfig(last,"receiver","model",receiver->model,&configOK,false);
	setConfig(last,"receiver","serial number",receiver->serialNumber,&configOK,false);
	setConfig(last,"receiver","commissioning year",&(receiver->commissionYYYY),&configOK,false);
	setConfig(last,"receiver","channels",&(receiver->nChannels),&configOK,false);
	//
	// CGGTTS output
	//
	
	if (setConfig(last,"cggtts","version",stmp,&configOK,false)){
		boost::to_upper(stmp);
		if (stmp=="V1")
			CGGTTSversion=CGGTTS::V1;
		else if (stmp=="V2E")
			CGGTTSversion=CGGTTS::V2E;
		else{
			std::cerr << "unknown CGGTTS version " << stmp << std::endl;
			configOK=false;
		}
	}
		
	if (setConfig(last,"cggtts","outputs",stmp,&configOK)){
		std::vector<std::string> configs;
		boost::split(configs, stmp,boost::is_any_of(","), boost::token_compress_on);
		int constellation=0,code=0;
		bool isP3=false;
		std::string ephemerisFile,ephemerisPath;
		bool genCTTS=false;
		
		for (unsigned int i=0;i<configs.size();i++){
			std::string calID="";
			if (setConfig(last,configs.at(i).c_str(),"constellation",stmp,&configOK)){
				boost::to_upper(stmp);
				if (stmp == "GPS")
					constellation = GNSSSystem::GPS;
				else if (stmp == "GLONASS")
					constellation = GNSSSystem::GLONASS;
				else if (stmp=="BEIDOU")
					constellation = GNSSSystem::BEIDOU;
				else if (stmp=="GALILEO")
					constellation = GNSSSystem::GALILEO;
				else{
					std::cerr << "unknown constellation " << stmp << " in [" << configs.at(i) << "]" << std::endl;
					configOK=false;
					continue;
				}
			}
			std::string rnxcode1(""),rnxcode2(""),rnxcode3(""),frc;
			bool reportMSIO=false;
			if (setConfig(last,configs.at(i).c_str(),"code",stmp,&configOK)){
				boost::to_upper(stmp);
				if (splitDualCode(stmp,rnxcode1,rnxcode2)){
					isP3 = true;
				}
				else{
					boost::trim(stmp);
					rnxcode1 = stmp;
				}
				if (CodesToFRC(constellation,rnxcode1,rnxcode2,frc,&isP3)){
					// all good
				}
				else{
					std::cerr << "unknown code  " << stmp << " in [" << configs.at(i) << "]" << std::endl;
					configOK=false;
					continue;
				}
			}
			
			// This is for single frequency outputs, where dual frequency ionosphere corrections are available
			if (!isP3){
				if (setConfig(last,configs.at(i).c_str(),"report msio",stmp,&configOK,false)){
					boost::to_upper(stmp);
					if (stmp=="YES" or stmp=="TRUE"){
						reportMSIO = true;
						// the code pair to compute this is needed
						if (setConfig(last,configs.at(i).c_str(),"msio codes",stmp,&configOK,true)){
							if (splitDualCode(stmp,rnxcode2,rnxcode3)){
								// Our convention will be that rnxcode2 is higher frequency than rnxcode3
								DBGMSG(debugStream,INFO,"msio codes " << rnxcode2 << " " << rnxcode3);
								if (rnxcode2[1] == '2'){
									std::string tmp = rnxcode2;
									rnxcode2 = rnxcode3;
									rnxcode3 = tmp;
									DBGMSG(debugStream,INFO,"Swapped msio codes " << rnxcode2 << " " << rnxcode3);
								}
								// Now read the delays
								
							}
							else{
								std::cerr << "bad msio code  " << stmp << " in [" << configs.at(i) << "]" << std::endl;
								configOK=false;
								continue;
							}
						}
					}
				}
			}
			
			if (setConfig(last,configs.at(i).c_str(),"generate 30s file",stmp,&configOK,false)){
				boost::to_upper(stmp);
				if (stmp=="YES" or stmp=="TRUE"){
					genCTTS = true;
				}
			}
			
			if (setConfig(last,configs.at(i).c_str(),"path",stmp,&configOK)){ // got everything
				// FIXME check compatibility of constellation+code
				stmp=relativeToAbsolutePath(stmp);
				ephemerisPath = relativeToAbsolutePath(ephemerisPath);
				CGGTTSoutputs.push_back(CGGTTSOutput(constellation,rnxcode1,rnxcode2,rnxcode3,isP3,reportMSIO,frc,stmp,
					ephemerisPath,ephemerisFile,genCTTS));
			}
			
			
		} // for
	} // if setConfig
		
	setConfig(last,"cggtts","reference",CGGTTSref,&configOK,false);
	setConfig(last,"cggtts","lab",CGGTTSlab,&configOK,false);
	setConfig(last,"cggtts","comments",CGGTTScomment,&configOK,false);
	setConfig(last,"cggtts","minimum track length",&CGGTTSminTrackLength,&configOK,false);
	setConfig(last,"cggtts","maximum dsg",&CGGTTSmaxDSG,&configOK,false);
	setConfig(last,"cggtts","maximum ura",&CGGTTSmaxURA,&configOK,false);
	setConfig(last,"cggtts","minimum elevation",&CGGTTSminElevation,&configOK,false);
	if (setConfig(last,"cggtts","naming convention",stmp,&configOK,false)){
		boost::to_upper(stmp);
		if (stmp == "BIPM")
			CGGTTSnamingConvention = BIPM;
		else if (stmp=="PLAIN")
			CGGTTSnamingConvention = Plain;
		else if (stmp=="R2CGGTTS")
			CGGTTSnamingConvention = R2CGGTTS;
		else{
			std::cerr << "unknown value for cggtts::naming convention " << std::endl;
			configOK=false;
		}
		if (CGGTTSnamingConvention == BIPM){
			setConfig(last,"cggtts","lab id",CGGTTSlabCode,&configOK);
			setConfig(last,"cggtts","receiver id",CGGTTSreceiverID,&configOK);
		}
	}
	
	if (setConfig(last,"cggtts","revision date",stmp,&configOK,false)){
		std::vector<std::string> vals;
		boost::split(vals, stmp,boost::is_any_of("-"), boost::token_compress_on);
		if (vals.size()==3){
			try{
				CGGTTSRevDateYYYY=lexical_cast<int>(vals.at(0));
				CGGTTSRevDateMM=lexical_cast<int>(vals.at(1));
				CGGTTSRevDateDD=lexical_cast<short>(vals.at(2));
			}
			catch(const bad_lexical_cast &){
				std::cerr << "Couldn't parse cggtts::revision date = " << stmp << std::endl;
				configOK=false;
			}
		}
		else{
			std::cerr << "Syntax error in cggtts::revision date - " << stmp << " should be YYYY-MM-DD " << std::endl;
			configOK=false;
		}
	}
	
	setConfig(last,"cggtts","bipm cal id",CGGTTScalID,&configOK,false);
	
	// Delays
	double cabdly=0.0,refdly=0.0;
	setConfig(last,"cggtts","refdly",&refdly,&configOK,false);
	setConfig(last,"cggtts","cabdly",&cabdly,&configOK,false);
	
	DBGMSG(debugStream,TRACE,"parsed CGGTTS config ");
	
	if (!r2cggttsMode){
		setDelay(last,"gps delays",&gpsDelay,CGGTTScalID,refdly,cabdly);
	}
	
	return configOK;
}


bool Application::setDelay(ListEntry *last,const char *section,GNSSDelay *dly,std::string & calID,double refdly,double cabdly)
{
	bool configOK=true;
	std::string stmp;
	
	dly->refDelay = refdly;
	dly->cabDelay = cabdly;
	dly->calID    = calID;
	
	if (setConfig(last,section,"kind",stmp,&configOK)){
		boost::trim(stmp);
		boost::to_lower(stmp);
		if (stmp == "internal"){
			dly->kind = GNSSDelay::INTDLY;
		}
		else if (stmp == "system"){
			dly->kind = GNSSDelay::SYSDLY;
		}
		else if (stmp == "total"){
			dly->kind = GNSSDelay::TOTDLY;
		}
	}
	
	if (setConfig(last,section,"codes",stmp,&configOK)){
	  std::vector<std::string> codes;
		boost::split(codes, stmp,boost::is_any_of(","), boost::token_compress_on);
		for (unsigned int i=0;i<codes.size();i++){
			boost::trim(codes[i]);
			dly->code.push_back(codes[i]);
		}
	}
	
	if (setConfig(last,section,"delays",stmp,&configOK)){
	  std::vector<std::string> delays;
		boost::split(delays, stmp,boost::is_any_of(","), boost::token_compress_on);
		if (delays.size() != dly->code.size()){
			std::cerr << "Code/delay mismatch in "<< section << std::endl;
			return false;
		}
		for (unsigned int i=0;i<delays.size();i++){
			boost::trim(delays[i]);
			dly->delay.push_back(lexical_cast<double>(delays[i]));
		}
	}
	
	setConfig(last,section,"bipm cal id",dly->calID,&configOK,false);
	
	return configOK;
	
}

bool Application::setConfig(ListEntry *last,const char *section,const char *token,std::string &val,bool *ok,bool required)
{
	char *stmp;
	if (list_get_string(last,section,token,&stmp)){
		// ok is not set
		val=stmp;
	}
	else{
		int err = config_file_get_last_error(NULL,0);
		if (err==TokenNotFound && required){
			std::cerr << "Missing entry for " << section << "::" << token << std::endl;
			*ok = false;
			return false;
		}
		else if (err==TokenNotFound){
			// ok is not false
			return false;
		}
		else if (err==ParseFailed){
			std::cerr << "Syntax error in " << section << "::" << token << std::endl;
			*ok = false;
			return false;
		}
	}
	return true;
}

bool Application::setConfig(ListEntry *last,const char *section,const char *token,double *val,bool *ok,bool required)
{
	double dtmp;
	if (list_get_double(last,section,token,&dtmp)){
		*val=dtmp;
		// ok is not set
		return true;
	}
	else{
		int err = config_file_get_last_error(NULL,0);
		if (err==TokenNotFound && required){
			std::cerr << "Missing entry for " << section << "::" << token << std::endl;
			*ok = false;
			return false;
		}
		else if (err==TokenNotFound){
			// ok is not false
			return false;
		}
		else if (err==ParseFailed){
			std::cerr << "Syntax error in " << section << "::" << token << std::endl;
			*ok=false;
			return false;
		}
	}
	return true;
}

bool Application::setConfig(ListEntry *last,const char *section,const char *token,int *val,bool *ok,bool required)
{
	int itmp;
	if (list_get_int(last,section,token,&itmp)){
		*val=itmp;
		return true;
	}
	else{
		int err = config_file_get_last_error(NULL,0);
		if (err==TokenNotFound && required){
			std::cerr << "Missing entry for " << section << "::" << token << std::endl;
			*ok = false;
			return false;
		}
		else if (err==TokenNotFound){
			// ok is not false
			return false;
		}
		else if (err==ParseFailed){
			std::cerr << "Syntax error in " << section << "::" << token << std::endl;
			*ok = false;
			return false;
		}
	}
	return true;
}


void Application::runNativeMode()
{
	// Check that the required files are present
	// Minimum requirement is the RINEX observation and navigation files for the processed MJD
	// Files for the next day are used if available
	std::vector<std::string> obsFileExtensions;
	obsFileExtensions.push_back("O");
	obsFileExtensions.push_back("o");
	obsFileExtensions.push_back("rnx");
	
	std::string obsFile1 = FindRINEXObsFile(mjd,mjd,obsFileExtensions);
	if (obsFile1.empty()){
		fatalError("Error! Unable to find a RINEX observation file");
	}
	DBGMSG(debugStream,INFO,"Got RINEX obs file " << obsFile1);
	
	RINEXObsFile obs1,obs2;
	obs1.read(obsFile1,0,86400);
	
	std::string obsFile2 = FindRINEXObsFile(mjd,mjd+1,obsFileExtensions);
	if (obsFile2.empty()){
		DBGMSG(debugStream,INFO,"Didn't get RINEX obs file for succeeding day " << obsFile2);
	}
	else{
		DBGMSG(debugStream,INFO,"Got RINEX obs file for succeeding day " << obsFile2);
		obs2.read(obsFile2,0,86400);
		obs1.merge(obs2);
	}
	
	//exit(0);
	std::vector<std::string> navFileExtensions;
	navFileExtensions.push_back("N");
	navFileExtensions.push_back("n");
	
	navFileExtensions.push_back("P");
	navFileExtensions.push_back("p");

	navFileExtensions.push_back("rnx");
	
	std::string navFile1 = FindRINEXNavFile(mjd,mjd,navFileExtensions);
	if (navFile1.empty()){
		fatalError("Error! Unable to find a RINEX navigation file");
	}
	DBGMSG(debugStream,INFO,"Got RINEX nav file " << navFile1);
	RINEXNavFile nav1;
	nav1.read(navFile1);
	
// 	
// 	std::string navFile2 = FindRINEXNavFile(mjd,mjd+1,navFileExtensions);
// 	if (navFile2.empty()){
// 		DBGMSG(debugStream,INFO,"Didn't get RINEX nav file for succeeding day " << navFile2);
// 		obsFile2="";
// 	}
// 	else{
// 		DBGMSG(debugStream,INFO,"Got RINEX nav file for succeeding day " << navFile2);
// 	}
	
	// Fiddle with leap seconds
	if (obs1.leapSecsValid()){
		DBGMSG(debugStream,INFO, obsFile1 << " leap secs " << obs1.leapsecs);
	}
	else{
		obs1.leapsecs = 18; 
		DBGMSG(debugStream,INFO, obsFile1 << " leap secs " << obs1.leapsecs);
	}
	
	int leapsecs1 = obs1.leapsecs;
	
	DBGMSG(debugStream,INFO,"Creating CGGTTS outputs ");	
	
	CGGTTS cggtts;
	cggtts.antenna = antenna;
	cggtts.receiver = receiver;
	cggtts.ref = CGGTTSref;
	cggtts.lab = CGGTTSlab;
	cggtts.comment = CGGTTScomment;
	cggtts.revDateYYYY = CGGTTSRevDateYYYY;
	cggtts.revDateMM = CGGTTSRevDateMM;
	cggtts.revDateDD = CGGTTSRevDateDD;
	cggtts.minElevation = CGGTTSminElevation;
	cggtts.maxDSG = CGGTTSmaxDSG;
	cggtts.maxURA = CGGTTSmaxURA;
	cggtts.minTrackLength = CGGTTSminTrackLength;
	cggtts.ver = CGGTTSversion;
	
	cggtts.generateSchedule(mjd);  // only need to do this once 
	
	for (unsigned int i=0;i<CGGTTSoutputs.size();i++){

		cggtts.constellation = CGGTTSoutputs.at(i).constellation;
		cggtts.rnxcode1 = CGGTTSoutputs.at(i).rnxcode1;
		cggtts.rnxcode2 = CGGTTSoutputs.at(i).rnxcode2;
		cggtts.rnxcode3 = CGGTTSoutputs.at(i).rnxcode3;
		cggtts.isP3 = CGGTTSoutputs.at(i).isP3;
		cggtts.FRC = CGGTTSoutputs.at(i).FRC;
		cggtts.reportMSIO = CGGTTSoutputs.at(i).reportMSIO;
		
		std::string CGGTTSfile = makeCGGTTSFilename(CGGTTSoutputs.at(i),mjd);
		DBGMSG(debugStream,INFO,"Creating CGGTTS file " << CGGTTSfile);
		std::string CTTSfile;
		
		switch (cggtts.constellation)
		{
			case GNSSSystem::BEIDOU:
				break;
			case GNSSSystem::GALILEO:
				break;
			case GNSSSystem::GLONASS:
				break;
			case GNSSSystem::GPS:
				cggtts.write(&(obs1.gps),&(nav1.gps),&gpsDelay,leapsecs1,CGGTTSfile,mjd,0,86400);
				break;
			default:
				break;
		}
	}

}

void Application::runR2CGGTTSMode()
{
	
	if (!readR2CGGTTSParams(r2cParamsFile)){
		fatalError("Error! Unable to open paramCGGTTS.dat");
	}
	
	// We're going on a file hunt
	// All required files are presumed to be in the current directory
	std::string obsFile1 = "rinex_obs";
	if (!(canOpenFile(obsFile1))){
		fatalError("Error! Unable to find  RINEX observation file");
	}
	DBGMSG(debugStream,INFO,"Got RINEX obs file " << obsFile1);
	
	RINEXObsFile obs1;
	obs1.read(obsFile1,0,86400);
	
	std::string obsFile2 = "rinex_obs_p";
	if (!(canOpenFile(obsFile2))){
		DBGMSG(debugStream,INFO,"Didn't get RINEX obs file for succeeding day " << obsFile2);
		obsFile2 =""; // empty string flags missing
	}
	else{
		DBGMSG(debugStream,INFO,"Got RINEX obs file for succeeding day " << obsFile2);
	}
	
	// Fiddle with leap seconds
	if (obs1.leapSecsValid()){
		DBGMSG(debugStream,INFO, obsFile1 << " leap secs " << obs1.leapsecs);
	}
	else{
		obs1.leapsecs = 18; 
		DBGMSG(debugStream,INFO, obsFile1 << " leap secs " << obs1.leapsecs);
	}
	int leapsecs1 = obs1.leapsecs;
		
	RINEXNavFile nav1;
	std::string mixNavFile1 = "rinex_nav_mix";
	if (!(canOpenFile(mixNavFile1))){
		DBGMSG(debugStream,INFO,"Didn't get mixed  RINEX nav file");
		mixNavFile1 ="";
		// not fatal
	}
	else{
		DBGMSG(debugStream,INFO,"Got mixed RINEX nav file");
		nav1.read(mixNavFile1);
	}
	
	if (!mixNavFile1.empty()){ // look for next day 
	}
	
	std::string gpsNavFile1="rinex_nav_gps";
	std::string gpsNavFile2="rinex_nav_p_gps";
	std::string bdsNavFile1="rinex_nav_bds";
	std::string bdsNavFile2="rinex_nav_p_bds";
	std::string galNavFile1="rinex_nav_gal";
	std::string galNavFile2="rinex_nav_p_gal";
	std::string gloNavFile1="rinex_nav_glo";
	std::string gloNavFile2="rinex_nav_p_glo";

	if (mixNavFile1.empty()){ // Didn't find a mixed nav file so try per constellation
		if (canOpenFile(gpsNavFile1)){
			if (!(canOpenFile(gpsNavFile2))){
				gpsNavFile2="";
			}
			DBGMSG(debugStream,INFO,"Got GPS RINEX nav file");
		}
		else{
			gpsNavFile1="";
		}
		
		if (canOpenFile(bdsNavFile1)){
			if (!(canOpenFile(bdsNavFile2))){
				bdsNavFile2="";
			}
			DBGMSG(debugStream,INFO,"Got BDS RINEX nav file");
		}
		else{
			bdsNavFile1="";
		}
		
		if (canOpenFile(galNavFile1)){
			if (!(canOpenFile(galNavFile2))){
				galNavFile2="";
			}
			DBGMSG(debugStream,INFO,"Got GAL RINEX nav file");
		}
		else{
			galNavFile1="";
		}
		
		if (canOpenFile(gloNavFile1)){
			if (!(canOpenFile(gloNavFile2))){
				gloNavFile2="";
			}
			DBGMSG(debugStream,INFO,"Got GLO RINEX nav file");
		}
		else{
			gloNavFile1="";
		}
	}
	
	DBGMSG(debugStream,INFO,"Creating CGGTTS outputs ");	
	
	
	CGGTTS cggtts;
	cggtts.antenna = antenna;
	cggtts.receiver = receiver;
	cggtts.ref = CGGTTSref;
	cggtts.lab = CGGTTSlab;
	cggtts.comment = CGGTTScomment;
	cggtts.revDateYYYY = CGGTTSRevDateYYYY;
	cggtts.revDateMM = CGGTTSRevDateMM;
	cggtts.revDateDD = CGGTTSRevDateDD;
	cggtts.minElevation = CGGTTSminElevation;
	cggtts.maxDSG = CGGTTSmaxDSG;
	cggtts.maxURA = CGGTTSmaxURA;
	cggtts.minTrackLength = CGGTTSminTrackLength;
	cggtts.ver = CGGTTSversion;
	
	cggtts.generateSchedule(mjd);  // only need to do this once 
	
	for (unsigned int i=0;i<CGGTTSoutputs.size();i++){

		cggtts.constellation = CGGTTSoutputs.at(i).constellation;
		cggtts.rnxcode1 = CGGTTSoutputs.at(i).rnxcode1;
		cggtts.rnxcode2 = CGGTTSoutputs.at(i).rnxcode2;
		cggtts.rnxcode3 = CGGTTSoutputs.at(i).rnxcode3;
		cggtts.isP3 = CGGTTSoutputs.at(i).isP3;
		cggtts.FRC = CGGTTSoutputs.at(i).FRC;
		cggtts.reportMSIO = CGGTTSoutputs.at(i).reportMSIO;
		
		std::string CGGTTSfile = makeCGGTTSFilename(CGGTTSoutputs.at(i),mjd);
		DBGMSG(debugStream,INFO,"Creating CGGTTS file " << CGGTTSfile);
		std::string CTTSfile;
		
		switch (cggtts.constellation)
		{
			case GNSSSystem::BEIDOU:
				break;
			case GNSSSystem::GALILEO:
				break;
			case GNSSSystem::GLONASS:
				break;
			case GNSSSystem::GPS:
				cggtts.write(&(obs1.gps),&(nav1.gps),&gpsDelay,leapsecs1,CGGTTSfile,mjd,0,86400);
				break;
			default:
				break;
		}
	}


}
		

bool Application::readR2CGGTTSParams(std::string paramsFile)
{
	DBGMSG(debugStream,INFO,"reading " << paramsFile);
	std::ifstream fin(paramsFile.c_str());
	std::string line;
	
	if (!fin.good()){
		DBGMSG(debugStream,INFO,"unable to open" << paramsFile);
		return false;
	}
	
	// This file should be strictly formatted but the keywords
	// may be in arbitrary order
	// Read all key value pairs
	
	std::vector<std::string> keys;
	std::vector<std::string> vals;
	

	while (!fin.eof()){
		
		getline(fin,line);
		boost::trim(line);
		keys.push_back(line);
		
		//if (fin.good()){
		//}
		getline(fin,line);
		boost::trim(line);
		vals.push_back(line);
	}
	
	fin.close();
	
	bool ok =true;
	bool gotParam;
	
	// define all the delays we need
	gpsDelay.kind = GNSSDelay::INTDLY;
	gpsDelay.addDelay("C1C",0.0);
	gpsDelay.addDelay("C1W",0.0);
	gpsDelay.addDelay("C2W",0.0);
	
	std::string stmp;
	double dtmp;
	
	getR2CGGTTSParam(keys,vals,"REV DATE",stmp);
	getR2CGGTTSParam(keys,vals,"RCVR",receiver->model); // bit of a fudge since we have separated into manufacturer+model+s/n
	getR2CGGTTSParam(keys,vals,"CH",&receiver->nChannels);

	// no placeholder for these 
	receiver->manufacturer = "";
	receiver->serialNumber = "";
	getR2CGGTTSParam(keys,vals,"LAB NAME",CGGTTSlab);
	
	ok = ok && getR2CGGTTSParam(keys,vals,"X COORDINATE",&(antenna->x));
	ok = ok && getR2CGGTTSParam(keys,vals,"Y COORDINATE",&(antenna->y));
	ok = ok && getR2CGGTTSParam(keys,vals,"Z COORDINATE",&(antenna->z));

	getR2CGGTTSParam(keys,vals,"COMMENTS",CGGTTScomment);
	
	getR2CGGTTSParam(keys,vals,"FRAME",antenna->frame);
	getR2CGGTTSParam(keys,vals,"REF",CGGTTSref);
	getR2CGGTTSParam(keys,vals,"CALIBRATION REFERENCE",r2cCalRef);
	
	if (getR2CGGTTSParam(keys,vals,"INT DELAY C1 GPS",&dtmp)){
		gpsDelay.addDelay("C1C",dtmp);
	}
	
	if (getR2CGGTTSParam(keys,vals,"INT DELAY P1 GPS",&dtmp)){
		gpsDelay.addDelay("C1W",dtmp);
	}
	
	if (getR2CGGTTSParam(keys,vals,"INT DELAY P2 GPS",&dtmp)){
		gpsDelay.addDelay("C2W",dtmp);
	}
	
	getR2CGGTTSParam(keys,vals,"ANT CAB DELAY",&r2cCabDly);
	getR2CGGTTSParam(keys,vals,"CLOCK CAB DELAY XP+XO",&r2cRefDly);
	getR2CGGTTSParam(keys,vals,"LEAP SECOND",&r2cLeapSeconds);

	
	// don't forget to do this!
	Utility::ECEFtoLatLonH(antenna->x,antenna->y,antenna->z, // latitude and longitude used in ionosphere model
	&(antenna->latitude),&(antenna->longitude),&(antenna->height));
	
	// or this
	gpsDelay.refDelay = r2cRefDly;
	gpsDelay.cabDelay = r2cCabDly;
	gpsDelay.calID    = r2cCalRef;
	
	return ok;
}

bool Application::getR2CGGTTSParam(std::vector<std::string> &keys,std::vector<std::string> &vals,std::string param,std::string &val)
{
	for (unsigned int i=0;i<keys.size();i++){
		if (0==keys.at(i).find(param)){
			val=vals.at(i);
			DBGMSG(debugStream,INFO,param << " = " << val);
			return true;
		}
	}
	return false;
}

bool Application::getR2CGGTTSParam(std::vector<std::string> &keys,std::vector<std::string> &vals,std::string param,int *val)
{
	for (unsigned int i=0;i<keys.size();i++){
		if (0==keys.at(i).find(param)){
			std::stringstream ss(vals.at(i));
			ss >> *val;
			DBGMSG(debugStream,INFO,param << " = " << *val);
			return true;
		}
	}
	return false;
}


bool Application::getR2CGGTTSParam(std::vector<std::string> &keys,std::vector<std::string> &vals,std::string param,double *val)
{
	for (unsigned int i=0;i<keys.size();i++){
		if (0==keys.at(i).find(param)){
			std::stringstream ss(vals.at(i));
			ss >> *val;
			DBGMSG(debugStream,INFO,param << " = " << std::fixed << *val);
			return true;
		}
	}
	return false;
}


std::string Application::FindRINEXObsFile(int nomMJD,int reqMJD,std::vector<std::string> &ext)
{
	
	std::string fname = "";
	
	
	// For compatibility with scripts which have been written for r2cggtts use their names for files
	// if the station name is undefined
	if (stationName.empty()){
		if (nomMJD == reqMJD) // indicates the rinex_obs file
			fname = RINEXobsPath + "/" + "rinex_obs";
		else if (nomMJD == reqMJD-1)
			fname = RINEXobsPath + "/" + "rinex_obs_p";
		if (canOpenFile(fname))
			return fname;
		else
			return "";
	}
	
	int year,mon,mday,yday;
	Utility::MJDtoDate(reqMJD,&year,&mon,&mday,&yday);
	
	if (stationName.length() == 4){ // Try a V2 style name
		int yy = year - (year/100)*100;
		char v2base[17];
		// this is ugly but C++ stream manipulators are even uglier
		for (unsigned int i=0;i<ext.size();i++){
			std::snprintf(v2base,16,"%s%03d0.%02d%s",stationName.c_str(),yday,yy,ext.at(i).c_str());
			fname = RINEXobsPath + "/" + std::string(v2base);
			if (canOpenFile(fname))
				return fname;
		}
		return "";
	}

	if (stationName.length() == 9){ // Try a V3 style name
		char v3base[65];
		for (unsigned int i=0;i<ext.size();i++){
			std::snprintf(v3base,64,"%s_R_%04d%03d0000_01D_30S_MO.%s",stationName.c_str(),year,yday,ext.at(i).c_str());
			fname = RINEXobsPath + "/" + std::string(v3base);
			if (canOpenFile(fname))
				return fname;
		}
		return "";
	}
	
	return "";
}


std::string Application::FindRINEXNavFile(int nomMJD,int reqMJD,std::vector<std::string> &ext)
{
	
	std::string fname = "";
	
	
	// For compatibility with scripts which have been written for r2cggtts use their names for files
	// if the station name is undefined
	if (stationName.empty()){
		if (nomMJD == reqMJD) // indicates the rinex_nav file
			fname = RINEXnavPath + "/" + "rinex_nav";
		else if (nomMJD == reqMJD-1)
			fname = RINEXnavPath + "/" + "rinex_nav_p";
		if (canOpenFile(fname))
			return fname;
		else
			return "";
	}
	
	int year,mon,mday,yday;
	Utility::MJDtoDate(reqMJD,&year,&mon,&mday,&yday);
	
	if (stationName.length() == 4){ // Try a V2 style name
		int yy = year - (year/100)*100;
		char v2base[17];
		for (unsigned int i=0;i<ext.size();i++){
			std::snprintf(v2base,16,"%s%03d0.%02d%s",stationName.c_str(),yday,yy,ext.at(i).c_str());
			fname = RINEXnavPath + "/" + std::string(v2base);
			if (canOpenFile(fname))
				return fname;
		}
		return "";
	}

	if (stationName.length() == 9){ // Try a V3 style name
		int yy = year - (year/100)*100;
		char v3base[65];
		for (unsigned int i=0;i<ext.size();i++){
			std::snprintf(v3base,64,"%s_R_%04d%03d0000_01D_MN.%s",stationName.c_str(),year,yday,ext.at(i).c_str());
			fname = RINEXnavPath + "/" + std::string(v3base);
			if (canOpenFile(fname))
				return fname;
		}
		return "";
	}
	
	return "";
}

std::string Application::makeCGGTTSFilename(CGGTTSOutput & cggtts, int MJD){
	std::ostringstream ss;
	char fname[16];
	if (CGGTTSnamingConvention == Plain)
		ss << cggtts.path << "/" << MJD << ".cctf";
	else if (CGGTTSnamingConvention == BIPM){
		std::snprintf(fname,15,"%2i.%03i",MJD/1000,MJD%1000); // tested for 57400,57000
		std::string constellation;
		switch (cggtts.constellation){
			case GNSSSystem::GPS:constellation="G";break;
			case GNSSSystem::GLONASS:constellation="R";break;
			case GNSSSystem::BEIDOU:constellation="E";break;
			case GNSSSystem::GALILEO:constellation="C";break;
		}
		char obsCode = 'Z';
		if (!(cggtts.isP3))
			obsCode = 'M';
		ss << cggtts.path << "/" << constellation << obsCode << CGGTTSlabCode << CGGTTSreceiverID << fname;
	}
	else if (CGGTTSnamingConvention == R2CGGTTS){
		std::string constellation,code;
		switch (cggtts.constellation){
			case GNSSSystem::GPS:
				constellation="GPS";
				if (cggtts.rnxcode1 == "C1C")
					code = "C1";
				break;
			case GNSSSystem::GLONASS:constellation="GLO";
				break;
			case GNSSSystem::BEIDOU:constellation="BDS";
				break;
			case GNSSSystem::GALILEO:constellation="GAL";
				break;
		}
		ss << "CGGTTS_" << constellation << "_" << code;
	}
	return ss.str();
}

std::string Application::makeCTTSFilename(CGGTTSOutput & cggtts, int MJD){
	std::ostringstream ss;
	char fname[16];
	if (CGGTTSnamingConvention == Plain)
		ss << cggtts.path << "/" << MJD << ".ctts";
	else if (CGGTTSnamingConvention == BIPM){
		std::snprintf(fname,15,"%2i.%03i.ctts",MJD/1000,MJD%1000); // tested for 57400,57000
		std::string constellation;
		switch (cggtts.constellation){
			case GNSSSystem::GPS:constellation="G";break;
			case GNSSSystem::GLONASS:constellation="R";break;
			case GNSSSystem::BEIDOU:constellation="E";break;
			case GNSSSystem::GALILEO:constellation="C";break;
		}
		char obsCode = 'Z';
		if (!(cggtts.isP3))
			obsCode = 'M';
		ss << cggtts.path << "/" << constellation << obsCode << CGGTTSlabCode << CGGTTSreceiverID << fname;
	}
	else if (CGGTTSnamingConvention == R2CGGTTS){
		std::string constellation;
		switch (cggtts.constellation){
			case GNSSSystem::GPS:constellation="GPS";break;
			case GNSSSystem::GLONASS:constellation="GLO";break;
			case GNSSSystem::BEIDOU:constellation="BDS";break;
			case GNSSSystem::GALILEO:constellation="GAL";break;
		}
		ss << "CTTS_" << constellation << "30s_";
	}
	return ss.str();
}

bool Application::splitDualCode(std::string &codeStr,std::string &c1,std::string &c2){
	
	std::vector<std::string> codes;
	boost::split(codes,codeStr,boost::is_any_of(",+"), boost::token_compress_on);
	if (codes.size() == 2){
		
		c1 = codes[0];
		boost::trim(c1);
		c2 = codes[1];
		boost::trim(c2);
		return (c1[1] != c2[1]);
	}
	return false;
}

// RINEX observation codes in, CGGTTS FRC out
bool Application::CodesToFRC(int constellation,std::string &c1,std::string &c2,std::string &frc,bool *isP3)
{
	if (c2.empty()){ // single frequency code
		*isP3 = false;
		if (c1 == "C1C"){ // for all constellations
			frc = "L1C";
			return true;
		}
		switch (constellation){
			case GNSSSystem::GPS:
				if (c1=="C1P" || c1=="C1W"){
					frc = "L1P";
					return true;
				}
				else if (c2 == "C2P" || c2=="C2W"){
					frc = "L2P"; // FIXME not actually defined in CGGTTS spec
					return true;
				}
				break;
			default:
				break;
		}
		return false;
	}
	
	// otherwise, it's a dual frequency combination
	// Just do a basic check. Experts still make typos. 
	std::string c1of,c2of; // C1,C2 observation+frequency
	switch (constellation){
		case GNSSSystem::GPS: // true for GLONASS too
				c1of = c1.substr(0,2);
				c2of = c2.substr(0,2);
				if (( c1of == "C1" && c2of == "C2") || 
					(c1of == "C2" && c2of == "C1")){
					frc="L3P";
					*isP3 = true;
					return true;
				}
				break;
			default:
				break;
	}
	return false;
}



