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

#include <configurator.h>

#include "Application.h"
#include "Antenna.h"
#include "CGGTTS.h"
#include "Counter.h"
#include "CounterMeasurement.h"
#include "Debug.h"
#include "Javad.h"
#include "MeasurementPair.h"
#include "NVS.h"
#include "Receiver.h"
#include "ReceiverMeasurement.h"
#include "RINEX.h"
#include "SVMeasurement.h"
#include "Timer.h"
#include "TrimbleResolution.h"
#include "Ublox.h"
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
		{"disable-tic",  no_argument, 0,  0 },
		{"help",         no_argument, 0, 0 },
		{"mjd",          required_argument, 0,  0 },
		{"positioning",  no_argument, 0,  0 },
		{"start",        required_argument, 0,  0 },
		{"stop",         required_argument, 0,  0 },
		{"verbosity",    required_argument, 0,  0 },
		{"timing-diagnostics",no_argument, 0,  0 },
		{"version",      no_argument, 0,  0 },
		{"sv-diagnostics",no_argument, 0,  0 },
		{"short-debug-message",no_argument, 0,  0 },
		{0,         			0,0,  0 }
};

using boost::lexical_cast;
using boost::bad_lexical_cast;

//
//	Public
//

Application::Application(int argc,char **argv)
{
	app = this;
	
	init();

	// Process the command line options
	// These override anything in the configuration file, default or specified
	int longIndex;
	int c,hh,mm,ss;
	
	std::string RINEXHeaderFile("");
	
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
						case 2: // --disable-tic
							TICenabled=false;
							break;
						case 3: // --help
							showHelp();
							exit(EXIT_SUCCESS);
							break;
						case 4: // --mjd
						{
							if (1!=std::sscanf(optarg,"%i",&MJD)){
								std::cerr << "Error! Bad value for option --mjd" << std::endl;
								showHelp();
								exit(EXIT_FAILURE);
							}
							break;
						}
						case 5: // --positioning
						{
							positioningMode=true;
							allObservations=true;
							break;
						}
						case 6: //--start
							if (!Utility::TODStrtoTOD(optarg,&hh,&mm,&ss)){
								std::cerr << "Error! Bad value for option --start" << std::endl;
								showHelp();
								exit(EXIT_FAILURE);
							}
							startTime = hh*3600 + mm*60 + ss;
							break;
						case 7: // --stop
							if (!Utility::TODStrtoTOD(optarg,&hh,&mm,&ss)){
								std::cerr << "Error! Bad value for option --stop" << std::endl;
								showHelp();
								exit(EXIT_FAILURE);
							}
							stopTime = hh*3600 + mm*60 + ss;
							break;
						case 8: // --verbosity
							{
								if (1!=std::sscanf(optarg,"%i",&verbosity)){
									std::cerr << "Error! Bad value for option --verbosity" << std::endl;
									showHelp();
									exit(EXIT_FAILURE);
								}
							}
							break;
						case 9: // --timing-diagnostics
							timingDiagnosticsOn=true;
							break;
						case 10: // --version
							showVersion();
							exit(EXIT_SUCCESS);
							break;
						case 11: // --sv-diagnostics
							SVDiagnosticsOn=true;
							break;
						case 12:// --short-debug-message
							shortDebugMessage=true;
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
					if (1!=std::sscanf(optarg,"%i",&MJD)){
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

	if (startTime > stopTime){
		std::cerr  << "Error! The start time is after the stop time" << std::endl;
		exit(EXIT_FAILURE);
	}
	
	if (!loadConfig()){
		std::cerr << "Error! Configuration failed" << std::endl;
		exit(EXIT_FAILURE);
	}
	
	// Note: can't get any debugging output until the command line is parsed !
	
	if (positioningMode){
		TICenabled=false; // no TIC correction needed for positioning and it will only add noise anyway
		allObservations=true; // configuration file may say otherwise
		if (!createRINEX){
			std::cerr << std::endl;
			std::cerr << "Warning! RINEX output is not enabled in the configuration files and this is needed for" << std::endl;
			std::cerr << "positioning mode. Has a valid RINEX output been configured ?" << std::endl;
			exit(EXIT_FAILURE);
		}
	}
	
}

Application::~Application()
{
	for (unsigned int i =0;i<MPAIRS_SIZE;i++)
		delete mpairs[i];
	delete[] mpairs;
}

void Application::run()
{
	Timer timer;
	timer.start();
	
	// Set the reference time for resolving GPS week number ambiguity
	// from the MJD that we are processing for
	refTime = (MJD - 40587)*86400;
	makeFilenames();
	
	// Create the log file, erasing any existing file
	std::ofstream ofs;
	ofs.open(logFile.c_str());
	ofs.close();
	
	logMessage(timeStamp() + APP_NAME +  " version " + APP_VERSION + " run started");
	
	// Subtract 4 hours to make sure we get ephemeris, UTC, ionosphere ...
	int sloppyStartTime = startTime - 4*3600;
	if (sloppyStartTime < 0) sloppyStartTime = 0;
	
	// add 960 s to capture CGGTTS tracks which don't end before stopTime
	int sloppyStopTime = stopTime + 960;
	if (sloppyStopTime > 86399) sloppyStopTime = 86399;
			
	bool recompress = decompress(receiverFile);
	if (!receiver->readLog(receiverFile,MJD,sloppyStartTime,sloppyStopTime,interval)){
		std::cerr << "Exiting" << std::endl;
		exit(EXIT_FAILURE);
	}
	if (recompress) compress(receiverFile);
	
	recompress = decompress(counterFile);
	if (!counter->readLog(counterFile,startTime,sloppyStopTime)){
		std::cerr << "Exiting" << std::endl;
		exit(EXIT_FAILURE);
	}
	if (recompress) compress(counterFile);
	
	matchMeasurements(receiver,counter); // only do this once
	
	if (fixBadSawtooth) // this attempts to fix TIC measurements made wrt to GPSDO
		fixBadSawtoothCorrection(receiver,counter);
	
	// Each system+code generates a CGGTTS file
	if (createCGGTTS){
		
		bool ephemerisReplaced=false;
		
		for (unsigned int i=0;i<CGGTTSoutputs.size();i++){
			if (CGGTTSoutputs.at(i).ephemerisSource==CGGTTSOutput::UserSupplied){
				if (CGGTTSoutputs.at(i).constellation == GNSSSystem::GPS){
					receiver->gps.deleteEphemerides();
					RINEX rnx;
					std::string fname=rnx.makeFileName(CGGTTSoutputs.at(i).ephemerisFile,MJD);
					if (fname.empty()){
						std::cerr << "Unable to make a RINEX navigation file name from the specified pattern: " << CGGTTSoutputs.at(i).ephemerisFile << std::endl;
						exit(EXIT_FAILURE);
					}
					std::string navFile=CGGTTSoutputs.at(i).ephemerisPath+"/"+fname;
					DBGMSG(debugStream,INFO,"using nav file " << navFile);
					if (!rnx.readNavigationFile(receiver,GNSSSystem::GPS,navFile)){
						exit(EXIT_FAILURE);
					}
				}
			}
			CGGTTS cggtts(antenna,counter,receiver);
			cggtts.ref=CGGTTSref;
			cggtts.lab=CGGTTSlab;
			cggtts.comment=CGGTTScomment;
			cggtts.revDateYYYY=CGGTTSRevDateYYYY;
			cggtts.revDateMM=CGGTTSRevDateMM;
			cggtts.revDateDD=CGGTTSRevDateDD;
			cggtts.cabDly=antCableDelay;
			cggtts.intDly=CGGTTSoutputs.at(i).internalDelay;
			cggtts.intDly2=CGGTTSoutputs.at(i).internalDelay2;
			cggtts.delayKind=CGGTTSoutputs.at(i).delayKind;
			cggtts.refDly=refCableDelay;
			cggtts.minElevation=CGGTTSminElevation;
			cggtts.maxDSG = CGGTTSmaxDSG;
			cggtts.maxURA = CGGTTSmaxURA;
			cggtts.minTrackLength=CGGTTSminTrackLength;
			cggtts.ver=CGGTTSversion;
			cggtts.constellation=CGGTTSoutputs.at(i).constellation;
			cggtts.code=CGGTTSoutputs.at(i).code;
			cggtts.calID=CGGTTSoutputs.at(i).calID;
			cggtts.isP3=CGGTTSoutputs.at(i).isP3;
			cggtts.useMSIO=cggtts.isP3; // FIXME not the whole story
			std::string CGGTTSfile =makeCGGTTSFilename(CGGTTSoutputs.at(i),MJD);
			cggtts.writeObservationFile(CGGTTSfile,MJD,startTime,stopTime,mpairs,TICenabled);
	
		}
	} // if createCGGTTS
	
	if (createRINEX){
		RINEX rnx;
		rnx.agency = agency;
		rnx.observer=observer;
		rnx.allObservations=allObservations;
		
		if (generateNavigationFile) {
			if (RINEXmajorVersion == 2){
				if (receiver->constellations == GNSSSystem::GPS){
					// FIXME needs rework
					rnx.writeNavigationFile(receiver,receiver->constellations,RINEXmajorVersion,RINEXminorVersion,RINEXnavFile,MJD);
				}
			}
			else{
				rnx.writeNavigationFile(receiver,receiver->constellations,RINEXmajorVersion,RINEXminorVersion,RINEXnavFile,MJD);
			}
		}
		rnx.writeObservationFile(antenna,counter,receiver,RINEXmajorVersion,RINEXminorVersion
            ,RINEXobsFile,MJD,interval,mpairs,TICenabled);
	} // if createRINEX
	
	if (timingDiagnosticsOn) 
		writeReceiverTimingDiagnostics(receiver,counter,"timing.dat");
	
	if (SVDiagnosticsOn) 
		writeSVDiagnostics(receiver,tmpPath);
	
	// Memory usage statistics
	unsigned int rxMem=receiver->memoryUsage();
	unsigned int ctMem=counter->memoryUsage();
	
	timer.stop();
	DBGMSG(debugStream,INFO,"elapsed time: " << timer.elapsedTime(Timer::SECS) << " s");
	
	DBGMSG(debugStream,INFO,"receiver data memory usage: " << rxMem << " bytes");
	DBGMSG(debugStream,INFO,"counter data memory usage: " << ctMem << " bytes");
	DBGMSG(debugStream,INFO,"total memory usage: " << rxMem + ctMem << " bytes");
	
	delete receiver;
	delete counter;
	delete antenna;
	
	logMessage(timeStamp() + " run finished");
}

void Application::showHelp()
{
	std::cout << std::endl << APP_NAME << " version " << APP_VERSION << std::endl;
	std::cout << "Usage: " << APP_NAME << " [options]" << std::endl;
	std::cout << "Available options are" << std::endl;
	std::cout << "-c,--configuration <file> full path to the configuration file" << std::endl;
	std::cout << "-d,--debug <file>         turn on debugging to <file> (use 'stderr' for output to stderr)" << std::endl;
	std::cout << "--disable-tic             disables use of sawtooth-corrected TIC measurements" << std::endl;
	std::cout << "-h,--help                 print this help message" << std::endl;
	std::cout << "-m,--mjd <n>              set the mjd" << std::endl;
	std::cout << "--positioning             produce output suitable for PPP positioning" << std::endl;
	std::cout << "--start HH:MM:SS/HHMMSS   set start time" << std::endl;
	std::cout << "--stop  HH:MM:SS/HHMMSS   set stop time" << std::endl;
	std::cout << "--short-debug-message     shorter debugging messages" << std::endl;
	std::cout << "--sv-diagnostics          write SV diagnostics files" << std::endl;
	std::cout << "--timing-diagnostics      write receiver timing diagnostics file" << std::endl;
	std::cout << "--verbosity <n>           set debugging verbosity" << std::endl;
	std::cout << "--version                 print version" << std::endl;


}

void Application::showVersion()
{
	std::cout << APP_NAME <<  " version " << APP_VERSION << std::endl;
	std::cout << "Written by " << APP_AUTHORS << std::endl;
	std::cout << "This ain't no stinkin' Perl script!" << std::endl;
}

std::string Application::timeStamp(){
	time_t tt = time(NULL);
	struct tm *gmt = gmtime(&tt);
	char ts[32];
	std::sprintf(ts,"%4d-%02d-%02d %02d:%02d:%02d ",gmt->tm_year+1900,gmt->tm_mon+1,gmt->tm_mday,
		gmt->tm_hour,gmt->tm_min,gmt->tm_sec);
	return std::string(ts);
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
//	Private
//	

void Application::init()
{
	pid = getpid();
	
	appName = APP_NAME;
	
	antenna = new Antenna();
	counter = new Counter();
	// defer instantiating the receiver until we know what kind is configured
	receiver = NULL;
	
	positioningMode=false;
	createCGGTTS=createRINEX=true;
	
	RINEXmajorVersion=RINEX::V2;
	RINEXminorVersion=11;
    
	CGGTTSversion=CGGTTS::V1;
	CGGTTScomment="NONE";
	CGGTTSref="REF";
	CGGTTSlab="KAOS";
	CGGTTSminElevation=10.0;
	CGGTTSmaxDSG=10.0;
	CGGTTSmaxURA=3.0;
	CGGTTSminTrackLength=390;
	
	observer="Time and Frequency";
	agency="NMIx";
	v3name="TEST00AUS";
	forceV2name=false;
	allObservations=false;
	
	refCableDelay=0.0;
	antCableDelay=0.0;
	
	interval=30;
	MJD = int(time(0)/86400)+40587 - 1;// yesterday
	startTime=0;
	stopTime=86399;
	
	timingDiagnosticsOn=false;
	SVDiagnosticsOn=false;
	generateNavigationFile=true;
	TICenabled=true;
	fixBadSawtooth=false;
	sawtoothStepThreshold= -1000000000.0; // ie 1 s so it does nothing by default
	
	char *penv;
	homeDir="";
	if ((penv = getenv("HOME"))){
		homeDir=penv;
	}
	rootDir=homeDir;
	
	logFile = "mktimetx.log";
	processingLogPath =  rootDir+"/logs";
	configurationFile = rootDir+"/etc/gpscv.conf";
	counterPath = rootDir+"/raw";
	counterExtension= "tic";
	receiverPath= rootDir+"/raw";
	receiverExtension="rx";
	RINEXPath=rootDir+"/rinex";
	CGGTTSnamingConvention=Plain;
	tmpPath=rootDir+"/tmp";
	
	gzip="/bin/gzip";
	
	mpairs= new MeasurementPair*[MPAIRS_SIZE];
	for (int i=0;i<MPAIRS_SIZE;i++)
		mpairs[i]=new MeasurementPair();

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

void  Application::makeFilenames()
{
	std::ostringstream ss;
	ss << "./" << "timing." << pid << "." << MJD << ".dat";  
	timingDiagnosticsFile=ss.str();
	
	std::ostringstream ss2;
	ss2 << counterPath << "/" << MJD << "." << counterExtension;
	counterFile=ss2.str();
	
	std::ostringstream ss3;
	ss3 << receiverPath << "/" << MJD << "." << receiverExtension;
	receiverFile = ss3.str();
	
	std::ostringstream ss4;
	ss4 << "./" << "processing." << pid << "." << MJD << ".log";
	processingLog=ss4.str();
	
	int year,mon,mday,yday;
	Utility::MJDtoDate(MJD,&year,&mon,&mday,&yday);
	int yy = year - (year/100)*100;
	
	
	// this is ugly but C++ stream manipulators are even uglier
	
	char fname[64];
	std::ostringstream ss5;
	if (RINEX::V2 == RINEXmajorVersion || forceV2name){
		switch (receiver->constellations)
		{
			case GNSSSystem::GPS:
				std::snprintf(fname,15,"%s%03d0.%02dN",antenna->markerName.c_str(),yday,yy);
				break;
			case GNSSSystem::GALILEO:
				std::snprintf(fname,15,"%s%03d0.%02dL",antenna->markerName.c_str(),yday,yy);
				break;
			default:
				break;
		}
	}
	else if (RINEXmajorVersion == RINEX::V3){
		switch (receiver->constellations)
		{
			case GNSSSystem::GPS:
				std::snprintf(fname,63,"%s_R_%d%03d0000_01D_GN.rnx",v3name.c_str(),year,yday);
				break;
			case GNSSSystem::GALILEO:
				std::snprintf(fname,63,"%s_R_%d%03d0000_01D_EN.rnx",v3name.c_str(),year,yday);
				break;
			default:
				std::snprintf(fname,63,"%s_R_%d%03d0000_01D_MN.rnx",v3name.c_str(),year,yday);
				break;
		}
		
	}
	
	ss5 << RINEXPath << "/" << fname; // at least no problem with length of RINEXPath
	RINEXnavFile=ss5.str();
	
	std::ostringstream ss6;
	if (RINEX::V2 == RINEXmajorVersion  || forceV2name)
		std::snprintf(fname,15,"%s%03d0.%02dO",antenna->markerName.c_str(),yday,yy);
	else if (RINEXmajorVersion == RINEX::V3)
		std::snprintf(fname,63,"%s_R_%d%03d0000_01D_30S_MO.rnx",v3name.c_str(),year,yday);
	
	ss6 << RINEXPath << "/" << fname;
	RINEXobsFile=ss6.str();
	
	
	logFile = processingLogPath + "/" + "mktimetx.log";
	
}

bool Application::decompress(std::string f)
{
	struct stat statBuf;
	int ret = stat(f.c_str(),&statBuf);
	if (ret !=0 ){ // decompressed file is not there
		std::string fgz = f + ".gz";
		if ((ret = stat(fgz.c_str(),&statBuf))==0){ // gzipped file is there
			DBGMSG(debugStream,INFO,"decompressing " << fgz);
			std::string cmd = gzip + " -d " + fgz;
			if ((ret=system(cmd.c_str()))!=0){
				std::cerr << "\"" << cmd << "\"" << " failed (return value = " << ret << ")" << std::endl;
				exit(EXIT_FAILURE);
			}
			return true;
		}
		else{ // file is missing/wrong permissions on path 
			std::cerr << " can't open " << f << std::endl;
			exit(EXIT_FAILURE);
		}
	}
	return false;
}

void Application::compress(std::string f){
	struct stat statBuf;
	int ret = stat(f.c_str(),&statBuf);
	if (ret ==0 ){ // file exists
		DBGMSG(debugStream,INFO,"compressing " << f);
		std::string cmd = gzip + " " + f;
		system(cmd.c_str());
	}
	else{ // file is missing/wrong permissions on path
		std::cerr << " can't open " << f << " (gzip failed)" << std::endl;
		// not fatal
	}
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
	
	// Paths
	// Parse first so that other paths can be constructed correctly
	//
	
	std::string path="";	
	
	if (setConfig(last,"paths","root",path,&configOK,false)){
		rootDir=path;
		if (rootDir.at(0) != '/')
			rootDir=homeDir+"/"+rootDir;
	}
	
	if (setConfig(last,"paths","rinex",path,&configOK))
		RINEXPath=relativeToAbsolutePath(path);
	
	path="";
	if (setConfig(last,"paths","receiver data",path,&configOK))
		receiverPath=relativeToAbsolutePath(path);
	
	path="";
	if (setConfig(last,"paths","counter data",path,&configOK))
		counterPath=relativeToAbsolutePath(path);
	
	path="";
	if (setConfig(last,"paths","tmp",path,&configOK))
		tmpPath=relativeToAbsolutePath(path);
	
	path="";
	if (setConfig(last,"paths","processing log",path,&configOK,false))
		processingLogPath=relativeToAbsolutePath(path);
	
	DBGMSG(debugStream,TRACE,"parsed Paths config");
	
	// CGGTTS generation
	if ((setConfig(last,"cggtts","create",stmp,&configOK,false))){
		boost::to_upper(stmp);
		if (stmp=="NO"){
			createCGGTTS=false;
		}
	}

	// In positioning mode, we don't care about CGGTTS so suppress it
	if (positioningMode){
		createCGGTTS = false;
		DBGMSG(debugStream,INFO,"CGGTTS creation suppressed");
	}
	
	if (createCGGTTS){
		
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
			int ephemerisSource=CGGTTSOutput::GNSSReceiver;
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
				if (setConfig(last,configs.at(i).c_str(),"code",stmp,&configOK)){
					boost::to_upper(stmp);
					
					if (0== (code = CGGTTS::strToCode(stmp,&isP3))){
						std::cerr << "unknown code " << stmp << " in [" << configs.at(i) << "]" << std::endl;
						configOK=false;
						continue;
					}
				}
				setConfig(last,configs.at(i).c_str(),"bipm cal id",calID,&configOK,false);
				double intdly=0.0,intdly2=0.0;
				int delayKind = CGGTTS::INTDLY; 
				
				switch (CGGTTSversion){
					case CGGTTS::V1:
						if (!setConfig(last,configs.at(i).c_str(),"internal delay",&intdly,&configOK)){
							continue;
						}
						setConfig(last,configs.at(i).c_str(),"internal delay 2",&intdly2,&configOK,false);
						break;
					case CGGTTS::V2E:
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
						break;
				}
				
				if (setConfig(last,configs.at(i).c_str(),"ephemeris",stmp,&configOK,false)){
					boost::to_upper(stmp);
					if (stmp == "RECEIVER"){
						ephemerisSource=CGGTTSOutput::GNSSReceiver;
					}
					else if (stmp=="USER"){
						// path and pattern are now required
						ephemerisSource=CGGTTSOutput::UserSupplied;
						if (!setConfig(last,configs.at(i).c_str(),"ephemeris path",ephemerisPath,&configOK,true)){
							continue;
						}
						if (!setConfig(last,configs.at(i).c_str(),"ephemeris file",ephemerisFile,&configOK,true)){
							continue;
						}
					}
					else{
						std::cerr << "Syntax error in [CGGTTS] ephemeris - " << stmp << " should be receiver/user " << std::endl;
						configOK = false;
						continue;
					}
				}
				
				if (setConfig(last,configs.at(i).c_str(),"path",stmp,&configOK)){ // got everything
					// FIXME check compatibility of constellation+code
					stmp=relativeToAbsolutePath(stmp);
					ephemerisPath = relativeToAbsolutePath(ephemerisPath);
					CGGTTSoutputs.push_back(CGGTTSOutput(constellation,code,isP3,stmp,calID,intdly,intdly2,delayKind,
						ephemerisSource,ephemerisPath,ephemerisFile));
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
				std::cerr << "Syntax error in [CGGTTS] revision date - " << stmp << " should be YYYY-MM-DD " << std::endl;
				configOK=false;
			}
		}
	}
	
	DBGMSG(debugStream,TRACE,"parsed CGGTTS config ");
	
	// RINEX generation
	if (setConfig(last,"rinex","create",stmp,&configOK,false)){
		boost::to_upper(stmp);
		if (stmp=="NO")
			createRINEX=false;
	}
	
	if (createRINEX){
		if (setConfig(last,"rinex","version",&itmp,&configOK)){
			switch (itmp)
			{
				case 2:RINEXmajorVersion=RINEX::V2;
                    RINEXminorVersion=11;
                    break;
				case 3:RINEXmajorVersion=RINEX::V3;
                    RINEXminorVersion=3;
                    break;
				default:configOK=false;
			}
		}
		
		if (setConfig(last,"rinex","observer",observer,&configOK)){
			if (observer=="user"){
				char *uenv;
				if ((uenv = getenv("USER")))
					observer=uenv;
			}
		}

		if (setConfig(last,"rinex","observations",stmp,&configOK)){
			if (stmp=="all"){
				allObservations = true;
			}
			else if (stmp == "code"){
				allObservations = false;
			}
		}
		
		if (setConfig(last,"rinex","v3 name",v3name,&configOK,false)){
			boost::to_upper(v3name);	
		}
	
		if (setConfig(last,"rinex","force v2 name",stmp,&configOK,false)){
			boost::to_upper(stmp);
			if (stmp=="YES")
				forceV2name=true;
		}
	
		setConfig(last,"rinex","agency",agency,&configOK);
	
	}
	
	DBGMSG(debugStream,TRACE,"parsed RINEX config");
	
	// Antenna
	setConfig(last,"antenna","marker name",antenna->markerName,&configOK);
	setConfig(last,"antenna","marker number",antenna->markerNumber,&configOK);
	setConfig(last,"antenna","marker type",antenna->markerType,&configOK);
	setConfig(last,"antenna","antenna number",antenna->antennaNumber,&configOK);
	setConfig(last,"antenna","antenna type",antenna->antennaType,&configOK);
	setConfig(last,"antenna","x",&(antenna->x),&configOK);
	setConfig(last,"antenna","y",&(antenna->y),&configOK);
	setConfig(last,"antenna","z",&(antenna->z),&configOK);
	setConfig(last,"antenna","delta h",&(antenna->deltaH),&configOK,false);
	setConfig(last,"antenna","delta e",&(antenna->deltaE),&configOK,false);
	setConfig(last,"antenna","delta n",&(antenna->deltaN),&configOK,false);
	setConfig(last,"antenna","frame",antenna->frame,&configOK);

	Utility::ECEFtoLatLonH(antenna->x,antenna->y,antenna->z,
		&(antenna->latitude),&(antenna->longitude),&(antenna->height));
	
	DBGMSG(debugStream,TRACE,"parsed Antenna config");
	
	// Receiver
	std::string rxModel,rxManufacturer;
	setConfig(last,"receiver","model",rxModel,&configOK);
	
	if (setConfig(last,"receiver","manufacturer",rxManufacturer,&configOK)){
		if (rxManufacturer.find("Trimble") != std::string::npos){
			receiver = new TrimbleResolution(antenna,rxModel); 
		}
		else if (rxManufacturer.find("Javad") != std::string::npos){
			receiver = new Javad(antenna,rxModel); 
		}
		else if (rxManufacturer.find("NVS") != std::string::npos){
			receiver = new NVS(antenna,rxModel); 
		}
		else if (rxManufacturer.find("ublox") != std::string::npos){
			receiver = new Ublox(antenna,rxModel); 
		}
		else{
			std::cerr << "A valid receiver model/manufacturer has not been configured - exiting" << std::endl;
			exit(EXIT_FAILURE);
		}
	}
	
	if (setConfig(last,"receiver","observations",stmp,&configOK,false)){
		boost::to_upper(stmp);
		receiver->constellations = 0; // overrride the default
		if (stmp.find("GPS") != std::string::npos)
			receiver->addConstellation(GNSSSystem::GPS); // this takes care of setting available signals too
		if (stmp.find("GLONASS") != std::string::npos)
			receiver->addConstellation(GNSSSystem::GLONASS);
		if (stmp.find("BEIDOU") != std::string::npos)
			receiver->addConstellation(GNSSSystem::BEIDOU);
		if (stmp.find("GALILEO") != std::string::npos)
			receiver->addConstellation(GNSSSystem::GALILEO);
	}
	
	if (setConfig(last,"receiver","version",stmp,&configOK,false))
		receiver->setVersion(stmp);
	
	setConfig(last,"receiver","serial number",receiver->serialNumber,&configOK,false);
		
	setConfig(last,"receiver","pps offset",&receiver->ppsOffset,&configOK);
	setConfig(last,"receiver","file extension",receiverExtension,&configOK,false);
	setConfig(last,"receiver","sawtooth size",&receiver->sawtooth,&configOK,false);
	setConfig(last,"receiver","year commissioned",&receiver->commissionYYYY,&configOK,false);
	
	if (setConfig(last,"receiver","sawtooth phase",stmp,&configOK,false)){
		boost::to_lower(stmp);
		if (stmp == "current second")
			receiver->sawtoothPhase=Receiver::CurrentSecond;
		else if (stmp == "next second")
			receiver->sawtoothPhase=Receiver::NextSecond;
		else if (stmp == "receiver specified")
			receiver->sawtoothPhase=Receiver::ReceiverSpecified;
		else{
			std::cerr << "Unrecognized option for sawtooth phase: " << stmp << std::endl;
			configOK=false;
		}
	}
	
	// Counter
	if (setConfig(last,"counter","flip sign",stmp,&configOK,false)){
		boost::to_upper(stmp);
		if (stmp=="YES")
			counter->flipSign=true;
	}
	
	DBGMSG(debugStream,TRACE,"parsed Receiver config");
	
	setConfig(last,"counter","file extension",counterExtension,&configOK,false);
	
	DBGMSG(debugStream,TRACE,"parsed Counter config");
	
	// Delays
	//if (!setConfig(last,"delays","internal",&internalDelay)) configOK=false;
	setConfig(last,"delays","antenna cable",&antCableDelay,&configOK);
	setConfig(last,"delays","reference cable",&refCableDelay,&configOK);
	
	DBGMSG(debugStream,TRACE,"parsed Delays config");
	
	setConfig(last,"misc","gzip",gzip,&configOK,false);
	if (setConfig(last,"misc","fix bad sawtooth correction",stmp,&configOK,false)){
		boost::to_upper(stmp);
		if (stmp=="YES"){
			fixBadSawtooth=true;
			DBGMSG(debugStream,INFO,"Fixing bad sawtooth");
		}
	}
	if (setConfig(last,"misc","sawtooth step threshold",&sawtoothStepThreshold,&configOK,false)){
		DBGMSG(debugStream,INFO,"Sawtooth step threshhold " << sawtoothStepThreshold);
	}
		
	DBGMSG(debugStream,TRACE,"parsed Misc config");
	
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

bool Application::writeRIN2CGGTTSParamFile(Receiver *rx, Antenna *ant, std::string fname)
{
	FILE *fout;
	if (!(fout = std::fopen(fname.c_str(),"w"))){
		std::cerr << "Unable to open " << fname << std::endl;
		return false;
	}
	
	std::fprintf(fout,"REV DATE\n");
	
	std::fprintf(fout,"RCVR\n");
	std::fprintf(fout,"%s\n",rx->manufacturer.c_str());
	std::fprintf(fout,"CH\n");
	
	std::fprintf(fout,"LAB NAME\n");
	
	std::fprintf(fout,"X COORDINATE\n");
	std::fprintf(fout,"%14.4lf\n",ant->x);
	std::fprintf(fout,"Y COORDINATE\n");
	std::fprintf(fout,"%14.4lf\n",ant->y);
	std::fprintf(fout,"Z COORDINATE\n");
	std::fprintf(fout,"%14.4lf\n",ant->z);
	std::fprintf(fout,"COMMENTS\n");
	
	std::fprintf(fout,"REF\n");
	
	std::fprintf(fout,"INT DELAY P1 XR+XS (in ns)\n");  // FIXME multiple problems here
	std::fprintf(fout,"0.0\n");
	std::fprintf(fout,"INT DELAY P2 XR+XS (in ns)\n");
	std::fprintf(fout,"0.0\n");
	std::fprintf(fout,"INT DELAY C1 XR+XS (in ns)\n");
	std::fprintf(fout,"%11.1lf\n",0.0);
	std::fprintf(fout,"ANT CAB DELAY (in ns)\n");
	std::fprintf(fout,"%11.1lf",antCableDelay);
	std::fprintf(fout,"CLOCK CAB DELAY XP+XO (in ns)\n");
	std::fprintf(fout,"%11.1lf",refCableDelay);
	
	std::fprintf(fout,"LEAP SECOND\n");
	std::fprintf(fout,"%d\n",rx->leapsecs);
	
	std::fclose(fout);
	
	return true;
}

void Application::matchMeasurements(Receiver *rx,Counter *cntr)
{
	// Measurements are matched using PC time stamps
	if (cntr->measurements.size() == 0 || rx->measurements.size()==0)
		return;

	// Instead of a complicated search, use an array that records whether the required measurements exist for 
	// each second. This approach:
	// (1) Flags gaps in either record
	// (2) Allows the PC clock to step forward (this just looks like a gap)
	// (3) Allows the PC clock to step back (as might happen on a reboot, and ntpd has not synced up yet).
	//     In this case, data between from the (previous) time of the step to before the step is discarded.
	
	// Clear the array each time its called
	for (int i=0;i<MPAIRS_SIZE;i++){ 
		mpairs[i]->flags=0;
	}
		
	for (unsigned int i=0;i<cntr->measurements.size();i++){
		CounterMeasurement *cm= cntr->measurements[i];
		int tcntr=((int) cm->hh)*3600 +  ((int) cm->mm)*60 + ((int) cm->ss);
		if (tcntr>=0 && tcntr<86400){
			if (mpairs[tcntr]->flags & 0x01){
				mpairs[tcntr]->flags |= 0x04; // duplicate
				DBGMSG(debugStream,WARNING,"duplicate counter measurement " << (int) cm->hh << ":" << (int) cm->mm << ":" <<(int) cm->ss);
			}
			else{
				mpairs[tcntr]->flags |= 0x01;
				mpairs[tcntr]->cm=cm;
			}
		}
	}
	
	//
	// Sometimes messages will be buffered for a few seconds,
	// resulting in messages with duplicate timestamps
	// It makes sense to use the last of these in this case
	//
	for (unsigned int i=0;i<rx->measurements.size();i++){
		ReceiverMeasurement *rxm = rx->measurements[i];
		int trx=((int) rxm->pchh)*3600 +  ((int) rxm->pcmm)*60 + ((int) rxm->pcss);
		if (trx>=0 && trx<MPAIRS_SIZE){
			if (mpairs[trx]->flags & 0x02){
				mpairs[trx]->flags |= 0x08; // duplicate
				DBGMSG(debugStream,WARNING,"duplicate receiver measurement " << (int) rxm->pchh << ":" << (int) rxm->pcmm << ":" <<(int) rxm->pcss);
			}
			else{
				mpairs[trx]->flags |= 0x02;
				mpairs[trx]->rm=rxm;
			}
		}
	}
	
	int matchcnt=0;
	for (unsigned int i=0;i<MPAIRS_SIZE;i++){
		if (mpairs[i]->flags == 0x03){
			mpairs[i]->rm->cm = mpairs[i]->cm;
			matchcnt++;
		}
	}
	
	logMessage(boost::lexical_cast<std::string>(matchcnt) + " matched measurements");
	
	// Paranoia
	// Some downstream algorithms require that the data be time-ordered
	// so check this.
	for (unsigned int i=1;i<MPAIRS_SIZE;i++){
		if (mpairs[i]->flags == 0x03 && mpairs[i-1]->flags == 0x03){
			ReceiverMeasurement *rxm = mpairs[i-1]->rm;
			int trx0=((int) rxm->pchh)*3600 +  ((int) rxm->pcmm)*60 + ((int) rxm->pcss);
			rxm = mpairs[i]->rm;
			int trx1=((int) rxm->pchh)*3600 +  ((int) rxm->pcmm)*60 + ((int) rxm->pcss);
			if (trx1 < trx0){ // duplicates are already filtered
				std::cerr << "Application::matchMeasurements() not monotonically ordered!" << std::endl;
				exit(EXIT_FAILURE);
			}
		}
	}
}

void Application::fixBadSawtoothCorrection(Receiver *rx,Counter *)
{
	// will miss initial (possibly) and last points but who cares 
	for (unsigned int i=0;i<MPAIRS_SIZE-1;i++){
		if (mpairs[i]->flags == 0x03 && mpairs[i+1]->flags == 0x03){
			CounterMeasurement   *cm= mpairs[i]->cm;
			ReceiverMeasurement  *rxm = mpairs[i]->rm;
			CounterMeasurement   *cmnext= mpairs[i+1]->cm;
			ReceiverMeasurement  *rxmnext = mpairs[i+1]->rm;
			double corr = (cm->rdg+rxm->sawtooth)*1.0E9;
			double corrnext = (cmnext->rdg+rxmnext->sawtooth)*1.0E9;
			if (corrnext-corr < sawtoothStepThreshold)
				cmnext->rdg += rx->sawtooth*1.0E-9; // correct reading towards +ve
		}
	}
}

void Application::writeReceiverTimingDiagnostics(Receiver *rx,Counter *cntr,std::string fname)
{
	FILE *fout;
	
	if (!(fout = std::fopen(fname.c_str(),"w"))){
		std::cerr << "Unable to open " << fname << std::endl;
		return;
	}
	
	DBGMSG(debugStream,INFO,"writing to " << fname);
	
	for (unsigned int i=0;i<MPAIRS_SIZE;i++){
		if (mpairs[i]->flags == 0x03){
			CounterMeasurement *cm= mpairs[i]->cm;
			ReceiverMeasurement *rxm = mpairs[i]->rm;
			int tmatch=((int) cm->hh)*3600 +  ((int) cm->mm)*60 + ((int) cm->ss);
			std::fprintf(fout,"%i %g %g %.16e\n",tmatch,cm->rdg,rxm->sawtooth,rxm->timeOffset);
		}
	}
	std::fclose(fout);
}

void Application::writeSVDiagnostics(Receiver *rx,std::string path)
{
	FILE *fout;
	
	DBGMSG(debugStream,INFO,"writing to " << path);
	
	for (int g = GNSSSystem::GPS; g<= GNSSSystem::GALILEO; (g <<= 1)){ 
		
		if (!(rx->constellations & g)) continue;
		
		GNSSSystem *gnss;
		switch (g){
			case GNSSSystem::BEIDOU:gnss = &(rx->beidou);
			case GNSSSystem::GALILEO:gnss = &(rx->galileo) ;
			case GNSSSystem::GLONASS:gnss = &(rx->glonass) ;
			case GNSSSystem::GPS:gnss = &(rx->gps) ;
		}
		
		for (unsigned int code = GNSSSystem::C1C;code <=GNSSSystem::L7I; (code <<= 1)){
			
			if (!(rx->codes & code)) continue;
			
			for (int svn=1;svn<=gnss->maxSVN();svn++){ // loop over all svn for constellation+code combination
				std::ostringstream sstr;
				sstr << path << "/" << gnss->oneLetterCode() << svn << ".dat";
				if (!(fout = std::fopen(sstr.str().c_str(),"w"))){
					std::cerr << "Unable to open " << sstr.str().c_str() << std::endl;
					return;
				}
				for (unsigned int m=0;m<rx->measurements.size();m++){
					for (unsigned int svm=0; svm < rx->measurements.at(m)->meas.size();svm++){
						SVMeasurement *sv=rx->measurements.at(m)->meas.at(svm);
						if ((svn == sv->svn) && (g==sv->constellation) && (code==sv->code)){
							int tod = rx->measurements[m]->tmUTC.tm_hour*3600+ rx->measurements[m]->tmUTC.tm_min*60 + rx->measurements[m]->tmUTC.tm_sec;
							// The default here is that df1 contains the raw (non-interpolated) pseudo range and df2 contains 
							// corrected pseudoranges when CGGTTS output has been generated (which can be useful to look at) 
							std::fprintf(fout,"%d %.16e %.16e %.16e %.16e\n",tod,sv->meas,sv->dbuf1,sv->dbuf2,sv->dbuf3);
							break;
						}
					}
				}
				std::fclose(fout);
			} //for (int svn= ...
		}// for (int code = ...
	} // for (int g =
}
