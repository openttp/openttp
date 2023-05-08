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
	
	while ((c=getopt_long(argc,argv,"c:d:hm:",longOptions,&longIndex)) != -1)
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
											std::cerr << "Error! Unable to open " << dbgout << std::endl;
											exit(EXIT_FAILURE);
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
									std::cerr << "Error! Bad value for option --mjd" << std::endl;
									showHelp();
									exit(EXIT_FAILURE);
								}
								break;
							}
						case 4: // --verbosity
							{
								if (1!=std::sscanf(optarg,"%i",&verbosity)){
									std::cerr << "Error! Bad value for option --verbosity" << std::endl;
									showHelp();
									exit(EXIT_FAILURE);
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
							std::cerr << "r2cggtts compatibility mode not available yet" << std::endl;
							exit(EXIT_FAILURE);
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
							std::cerr << "Error! Unable to open " << dbgout << std::endl;
							exit(EXIT_FAILURE);
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
						std::cerr << "Error! Bad value for option --mjd" << std::endl;
						showHelp();
						exit(EXIT_FAILURE);
					}
				}
				break;
			default:
				showHelp();
				exit(EXIT_FAILURE);
				break;
		}
	}

	// Two modes of operation 
	// Use a configuration file, which allows a bit more flexibility and removes the need for pre- and post- processing of files
	// r2cggtts compatibility mode, which uses the standard r2cggtts configuration file and naming conventions for input and output files
	if (!r2cggttsMode){
		if (!loadConfig()){
			std::cerr << "Error! Configuration failed" << std::endl;
			exit(EXIT_FAILURE);
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
	DBGMSG(debugStream,TRACE,"RINEX observations: " << RINEXobsPath);
	
	if (setConfig(last,"paths","rinex navigation",path,&configOK))
		RINEXnavPath=relativeToAbsolutePath(path);
	DBGMSG(debugStream,TRACE,"RINEX navigation: " << RINEXobsPath);
	
	
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
	setConfig(last,"antenna","frame",antenna->frame,&configOK);
	
	//
	// Receiver
	//
	setConfig(last,"receiver","manufacturer",receiver->manufacturer,&configOK);
	setConfig(last,"receiver","model",receiver->model,&configOK);
	setConfig(last,"receiver","serial number",receiver->serialNumber,&configOK);
	setConfig(last,"receiver","commissioning year",&(receiver->commissionYYYY),&configOK);
	setConfig(last,"receiver","channels",&(receiver->nChannels),&configOK);
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
				if (std::string::npos != stmp.find("+")){
					
				}
				else if (std::string::npos != stmp.find(",")){
					
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
								if (rnxcode2[1] == '2'){
									std::string tmp = rnxcode2;
									rnxcode2 = rnxcode3;
									rnxcode3 = tmp;
									DBGMSG(debugStream,INFO,"Swapped msio codes " << rnxcode2 << " " << rnxcode3)
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
			
			
			double intdly=0.0,intdly2=0.0,intdly3=0.0;
			int delayKind = CGGTTS::INTDLY; 
			
			
			if (isP3)
				setConfig(last,configs.at(i).c_str(),"internal delay 2",&intdly2,&configOK,false);
			if (!setConfig(last,configs.at(i).c_str(),"internal delay",&intdly,&configOK,false)){
				if (!setConfig(last,configs.at(i).c_str(),"system delay",&intdly,&configOK,false)){
					DBGMSG(debugStream,INFO,"Got there");
					if (setConfig(last,configs.at(i).c_str(),"total delay",&intdly,&configOK)){
						delayKind=CGGTTS::TOTDLY;
					}
					else{
						continue;
					}
				}
				else{
					delayKind=CGGTTS::SYSDLY;
				}
			}							
		
			if (setConfig(last,configs.at(i).c_str(),"path",stmp,&configOK)){ // got everything
				// FIXME check compatibility of constellation+code
				stmp=relativeToAbsolutePath(stmp);
				ephemerisPath = relativeToAbsolutePath(ephemerisPath);
				CGGTTSoutputs.push_back(CGGTTSOutput(constellation,rnxcode1,rnxcode2,rnxcode3,isP3,reportMSIO,frc,stmp,intdly,intdly2,intdly3,delayKind,
					ephemerisPath,ephemerisFile));
			}
			
			
		} // for
	} // if setConfig
		
	setConfig(last,"cggtts","reference",CGGTTSref,&configOK);
	setConfig(last,"cggtts","lab",CGGTTSlab,&configOK);
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
		else{
			std::cerr << "unknown value for cggtts::naming convention " << std::endl;
			configOK=false;
		}
		if (CGGTTSnamingConvention == BIPM){
			setConfig(last,"cggtts","lab id",CGGTTSlabCode,&configOK);
			setConfig(last,"cggtts","receiver id",CGGTTSreceiverID,&configOK);
		}
	}
	
	if (setConfig(last,"cggtts","revision date",stmp,&configOK)){
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
	
	DBGMSG(debugStream,TRACE,"parsed CGGTTS config ");
	
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
	obsFileExtensions.push_back("O.gz");
	obsFileExtensions.push_back("O.Z");
	obsFileExtensions.push_back("o");
	obsFileExtensions.push_back("o.gz");
	obsFileExtensions.push_back("o.Z");
	obsFileExtensions.push_back("rnx");
	obsFileExtensions.push_back("rnx.gz");
	obsFileExtensions.push_back("rnx.Z");
	std::string obsFile1 = FindRINEXObsFile(mjd,mjd,obsFileExtensions);
	if (obsFile1.empty()){
		std::cerr << "Error! Unable to find a RINEX observation file" << std::endl;
		exit(EXIT_FAILURE);
	}
	DBGMSG(debugStream,INFO,"Got RINEX obs file " << obsFile1);
	
	RINEXObsFile obs1;
	obs1.read(obsFile1,0,86400);
	
	std::string obsFile2 = FindRINEXObsFile(mjd,mjd+1,obsFileExtensions);
	if (obsFile2.empty()){
		DBGMSG(debugStream,INFO,"Didn't get RINEX obs file for succeeding day " << obsFile2);
	}
	else{
		DBGMSG(debugStream,INFO,"Got RINEX obs file for succeeding day " << obsFile2);
	}
	
	//exit(0);
	std::vector<std::string> navFileExtensions;
	navFileExtensions.push_back("N");
	navFileExtensions.push_back("N.gz");
	navFileExtensions.push_back("N.Z");
	navFileExtensions.push_back("n");
	navFileExtensions.push_back("n.gz");
	navFileExtensions.push_back("n.Z");
	navFileExtensions.push_back("rnx");
	navFileExtensions.push_back("rnx.gz");
	navFileExtensions.push_back("rnx.Z");
	
	std::string navFile1 = FindRINEXNavFile(mjd,mjd,navFileExtensions);
	if (navFile1.empty()){
		std::cerr << "Error! Unable to find a RINEX navigation file" << std::endl;
		exit(EXIT_FAILURE);
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
	cggtts.calID = CGGTTScalID;
	cggtts.ver = CGGTTSversion;
	//cggtts.cabDly=antCableDelay;
		//cggtts.refDly=refCableDelay;
	
	for (unsigned int i=0;i<CGGTTSoutputs.size();i++){
	
		cggtts.intDly = CGGTTSoutputs.at(i).internalDelay; // FIXME maybe output could be supplied and other fields in CGGTTS simply duplicated ...
		cggtts.intDly2 = CGGTTSoutputs.at(i).internalDelay2;
		cggtts.delayKind = CGGTTSoutputs.at(i).delayKind;
		cggtts.constellation = CGGTTSoutputs.at(i).constellation;
		cggtts.rnxcode1 = CGGTTSoutputs.at(i).rnxcode1;
		cggtts.rnxcode2 = CGGTTSoutputs.at(i).rnxcode2;
		cggtts.isP3 = CGGTTSoutputs.at(i).isP3;
		cggtts.FRC = CGGTTSoutputs.at(i).FRC;
		cggtts.reportMSIO = CGGTTSoutputs.at(i).reportMSIO;
		
		std::string CGGTTSfile = makeCGGTTSFilename(CGGTTSoutputs.at(i),mjd);
		DBGMSG(debugStream,INFO,"Creating CGGTTS file " << CGGTTSfile);
		
		switch (cggtts.constellation)
		{
			case GNSSSystem::BEIDOU:
				break;
			case GNSSSystem::GALILEO:
				break;
			case GNSSSystem::GLONASS:
				break;
			case GNSSSystem::GPS:
				cggtts.write(&(obs1.gps),&(nav1.gps),leapsecs1,CGGTTSfile,mjd,0,86400);
				break;
			default:
				break;
		}
	}

}

void Application::runR2CGGTTSMode()
{
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
		// FIXME single frequency observation files only
		ss << cggtts.path << "/" << constellation << "M" << CGGTTSlabCode << CGGTTSreceiverID << fname;
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
				if (c1=="C1P" or c1=="C1W"){
					frc = "L1P";
					return true;
				}
				else if (c1 == "C2P" or c2=="C2W"){
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
				if (( c1of == "C1" and c2of == "C2") or 
					(c1of == "C2" and c2of == "C1")){
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



