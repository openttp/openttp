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
extern ostream *debugStream;
extern string   debugFileName;
extern ofstream debugLog;
extern int verbosity;
extern bool shortDebugMessage;

Application *app;

static struct option longOptions[] = {
		{"configuration",required_argument, 0,  0 },
		{"debug",         required_argument, 0,  0 },
		{"disable-tic", no_argument, 0,  0 },
		{"help",          no_argument, 0, 0 },
		{"start",required_argument, 0,  0 },
		{"stop",required_argument, 0,  0 },
		{"verbosity",     required_argument, 0,  0 },
		{"timing-diagnostics",no_argument, 0,  0 },
		{"version",       no_argument, 0,  0 },
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
	
	string RINEXHeaderFile("");
	
	while ((c=getopt_long(argc,argv,"hm:",longOptions,&longIndex)) != -1)
	{
		switch(c)
		{
			case 0: // long options
				{
					switch (longIndex)
					{
						case 0:
							configurationFile=optarg;
							break;
						case 1:
							{
								string dbgout = optarg;
								if ((string::npos != dbgout.find("stderr"))){
									debugStream = & std::cerr;
								}
								else{
									debugFileName = dbgout;
									debugLog.open(debugFileName.c_str(),ios_base::app);
									if (!debugLog.is_open()){
										cerr << "Error! Unable to open " << dbgout << endl;
										exit(EXIT_FAILURE);
									}
									debugStream = & debugLog;
								}
								break;
							}
						case 2:
							TICenabled=false;
							break;
						case 3:
							showHelp();
							exit(EXIT_SUCCESS);
							break;
						case 4:
							if (!Utility::TODStrtoTOD(optarg,&hh,&mm,&ss)){
								cerr << "Error! Bad value for option --start" << endl;
								showHelp();
								exit(EXIT_FAILURE);
							}
							startTime = hh*3600 + mm*60 + ss;
							break;
						case 5:
							if (!Utility::TODStrtoTOD(optarg,&hh,&mm,&ss)){
								cerr << "Error! Bad value for option --stop" << endl;
								showHelp();
								exit(EXIT_FAILURE);
							}
							stopTime = hh*3600 + mm*60 + ss;
							break;
						case 6:
							{
								if (1!=sscanf(optarg,"%i",&verbosity)){
									cerr << "Error! Bad value for option --verbosity" << endl;
									showHelp();
									exit(EXIT_FAILURE);
								}
							}
							break;
						case 7:
							timingDiagnosticsOn=true;
							break;
							
						case 8:
							showVersion();
							exit(EXIT_SUCCESS);
							break;
						case 9:
							SVDiagnosticsOn=true;
							break;
						case 10:
							shortDebugMessage=true;
							break;
					}
				}
				break;
			case 'h':
				showHelp();
				exit(EXIT_SUCCESS);
				break;
			case 'm':
				{
					if (1!=sscanf(optarg,"%i",&MJD)){
						cerr << "Error! Bad value for option --mjd" << endl;
						showHelp();
						exit(EXIT_FAILURE);
					}
				}
				break;
			default:
				break;
			
		}
	}

	if (startTime > stopTime){
		cerr  << "Error! The start time is after the stop time" << endl;
		exit(EXIT_FAILURE);
	}
	
	if (!loadConfig()){
		cerr << "Error! Configuration failed" << endl;
		exit(EXIT_FAILURE);
	}
	
	// Note: can't get any debugging output until the command line is parsed !
	
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
	// from the MJD that we are processing for. But make it  8 days less to be sure.
	refTime = (MJD - 40587 - 8 )*86400;
	makeFilenames();
	
	// Create the log file, erasing any existing file
	ofstream ofs;
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
		cerr << "Exiting" << endl;
		exit(EXIT_FAILURE);
	}
	if (recompress) compress(receiverFile);
	
	recompress = decompress(counterFile);
	if (!counter->readLog(counterFile,startTime,sloppyStopTime)){
		cerr << "Exiting" << endl;
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
					receiver->gps.deleteEphemeris();
					RINEX rnx;
					string fname=rnx.makeFileName(CGGTTSoutputs.at(i).ephemerisFile,MJD);
					if (fname.empty()){
						cerr << "Unable to make a RINEX navigation file name from the specified pattern: " << CGGTTSoutputs.at(i).ephemerisFile << endl;
						exit(EXIT_FAILURE);
					}
					string navFile=CGGTTSoutputs.at(i).ephemerisPath+"/"+fname;
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
		
			string CGGTTSfile =makeCGGTTSFilename(CGGTTSoutputs.at(i),MJD);
			cggtts.writeObservationFile(CGGTTSfile,MJD,startTime,stopTime,mpairs,TICenabled);
	
		}
	} // if createCGGTTS
	
	if (createRINEX){
		RINEX rnx;
		rnx.agency = agency;
		rnx.observer=observer;
		rnx.allObservations=allObservations;
		
		if (generateNavigationFile) 
			rnx.writeNavigationFile(receiver,GNSSSystem::GPS,RINEXversion,RINEXnavFile,MJD);
		
		rnx.writeObservationFile(antenna,counter,receiver,RINEXversion,RINEXobsFile,MJD,interval,mpairs,TICenabled);
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
	cout << endl << APP_NAME << " version " << APP_VERSION << endl;
	cout << "Usage: " << APP_NAME << " [options]" << endl;
	cout << "Available options are" << endl;
	cout << "--configuration <file> full path to the configuration file" << endl;
	cout << "--debug <file>         turn on debugging to <file> (use 'stderr' for output to stderr)" << endl;
	cout << "--disable-tic          disables use of sawtooth-corrected TIC measurements" << endl;
	cout << "-h,--help              print this help message" << endl;
	cout << "-m <n>                 set the mjd" << endl;
	cout << "--start HH:MM:SS/HHMMSS  set start time" << endl;
	cout << "--stop  HH:MM:SS/HHMMSS  set stop time" << endl;
	cout << "--short-debug-message  shorter debugging messages" << endl;
	cout << "--sv-diagnostics       write SV diagnostics files" << endl;
	cout << "--timing-diagnostics   write receiver timing diagnostics file" << endl;
	cout << "--verbosity <n>        set debugging verbosity" << endl;
	cout << "--version              print version" << endl;
}

void Application::showVersion()
{
	cout << APP_NAME <<  " version " << APP_VERSION << endl;
	cout << "Written by " << APP_AUTHORS << endl;
	cout << "This ain't no stinkin' Perl script!" << endl;
}

string Application::timeStamp(){
	time_t tt = time(NULL);
	struct tm *gmt = gmtime(&tt);
	char ts[32];
	sprintf(ts,"%4d-%02d-%02d %02d:%02d:%02d ",gmt->tm_year+1900,gmt->tm_mon+1,gmt->tm_mday,
		gmt->tm_hour,gmt->tm_min,gmt->tm_sec);
	return string(ts);
}

void Application::logMessage(string msg)
{
	ofstream ofs;
	ofs.open(logFile.c_str(),ios::app);
	ofs << msg << endl;
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
	
	createCGGTTS=createRINEX=true;
	
	RINEXversion=RINEX::V2;
	
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
	CGGTTSPath=rootDir+"/cggtts";
	CGGTTSnamingConvention=Plain;
	tmpPath=rootDir+"/tmp";
	
	gzip="/bin/gzip";
	
	mpairs= new MeasurementPair*[MPAIRS_SIZE];
	for (int i=0;i<MPAIRS_SIZE;i++)
		mpairs[i]=new MeasurementPair();

}

string Application::relativeToAbsolutePath(string path)
{
	string absPath=path;
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
	ostringstream ss;
	ss << "./" << "timing." << pid << "." << MJD << ".dat";  
	timingDiagnosticsFile=ss.str();
	
	ostringstream ss2;
	ss2 << counterPath << "/" << MJD << "." << counterExtension;
	counterFile=ss2.str();
	
	ostringstream ss3;
	ss3 << receiverPath << "/" << MJD << "." << receiverExtension;
	receiverFile = ss3.str();
	
	ostringstream ss4;
	ss4 << "./" << "processing." << pid << "." << MJD << ".log";
	processingLog=ss4.str();
	
	int year,mon,mday,yday;
	Utility::MJDtoDate(MJD,&year,&mon,&mday,&yday);
	int yy = year - (year/100)*100;
	
	
	// this is ugly but C++ stream manipulators are even uglier
	
	char fname[64];
	ostringstream ss5;
	if (RINEX::V2 == RINEXversion || forceV2name)
		snprintf(fname,15,"%s%03d0.%02dN",antenna->markerName.c_str(),yday,yy);
	else if (RINEXversion == RINEX::V3)
		snprintf(fname,63,"%s_R_%d%03d0000_01D_MN.rnx",v3name.c_str(),year,yday);
	
	ss5 << RINEXPath << "/" << fname; // at least no problem with length of RINEXPath
	RINEXnavFile=ss5.str();
	
	ostringstream ss6;
	if (RINEX::V2 == RINEXversion  || forceV2name)
		snprintf(fname,15,"%s%03d0.%02dO",antenna->markerName.c_str(),yday,yy);
	else if (RINEXversion == RINEX::V3)
		snprintf(fname,63,"%s_R_%d%03d0000_01D_30S_MO.rnx",v3name.c_str(),year,yday);
	
	ss6 << RINEXPath << "/" << fname;
	RINEXobsFile=ss6.str();
	
	
	logFile = processingLogPath + "/" + "mktimetx.log";
	
}

bool Application::decompress(string f)
{
	struct stat statBuf;
	int ret = stat(f.c_str(),&statBuf);
	if (ret !=0 ){ // decompressed file is not there
		string fgz = f + ".gz";
		if ((ret = stat(fgz.c_str(),&statBuf))==0){ // gzipped file is there
			DBGMSG(debugStream,INFO,"decompressing " << fgz);
			string cmd = gzip + " -d " + fgz;
			if ((ret=system(cmd.c_str()))!=0){
				cerr << "\"" << cmd << "\"" << " failed (return value = " << ret << ")" << endl;
				exit(EXIT_FAILURE);
			}
			return true;
		}
		else{ // file is missing/wrong permissions on path 
			cerr << " can't open " << f << endl;
			exit(EXIT_FAILURE);
		}
	}
	return false;
}

void Application::compress(string f){
	struct stat statBuf;
	int ret = stat(f.c_str(),&statBuf);
	if (ret ==0 ){ // file exists
		DBGMSG(debugStream,INFO,"compressing " << f);
		string cmd = gzip + " " + f;
		system(cmd.c_str());
	}
	else{ // file is missing/wrong permissions on path
		cerr << " can't open " << f << " (gzip failed)" << endl;
		// not fatal
	}
}

		
string Application::makeCGGTTSFilename(CGGTTSOutput & cggtts, int MJD){
	ostringstream ss;
	char fname[16];
	if (CGGTTSnamingConvention == Plain)
		ss << cggtts.path << "/" << MJD << ".cctf";
	else if (CGGTTSnamingConvention == BIPM){
		snprintf(fname,15,"%2i.%03i",MJD/1000,MJD%1000); // tested for 57400,57000
		string constellation;
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
		cerr << "Unable to open the configuration file " << configurationFile << " - exiting" << endl;
		exit(EXIT_FAILURE);
	}
	
	bool configOK=true;
	int itmp=2;
	string stmp;
	
	// Paths
	// Parse first so that other paths can be constructed correctly
	//
	
	string path="";	
	
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
	if (setConfig(last,"paths","cggtts",path,&configOK))
		CGGTTSPath=relativeToAbsolutePath(path);
	
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

	if (createCGGTTS){
		
		if (setConfig(last,"cggtts","version",stmp,&configOK,false)){
			boost::to_upper(stmp);
			if (stmp=="V1")
				CGGTTSversion=CGGTTS::V1;
			else if (stmp=="V2E")
				CGGTTSversion=CGGTTS::V2E;
			else{
				cerr << "unknown CGGTTS version " << stmp << endl;
				configOK=false;
			}
		}
		
		if (setConfig(last,"cggtts","outputs",stmp,&configOK)){
			std::vector<std::string> configs;
			boost::split(configs, stmp,boost::is_any_of(","), boost::token_compress_on);
			int constellation=0,code=0;
			int ephemerisSource=CGGTTSOutput::GNSSReceiver;
			string ephemerisFile,ephemerisPath;
			
			for (unsigned int i=0;i<configs.size();i++){
				string calID="";
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
						cerr << "unknown constellation " << stmp << " in [" << configs.at(i) << "]" << endl;
						configOK=false;
						continue;
					}
				}
				if (setConfig(last,configs.at(i).c_str(),"code",stmp,&configOK)){
					boost::to_upper(stmp);
					if (stmp=="C1")
						code=GNSSSystem::C1;
					else if (stmp=="P1")
						code=GNSSSystem::P1;
					else if (stmp=="P2")
						code=GNSSSystem::P2;
					else{
						cerr << "unknown code " << stmp << " in [" << configs.at(i) << "]" << endl;
						configOK=false;
						continue;
					}
				}
				setConfig(last,configs.at(i).c_str(),"bipm cal id",calID,&configOK,false);
				double intdly=0.0;
				int delayKind = CGGTTS::INTDLY; 
				
				switch (CGGTTSversion){
					case CGGTTS::V1:
						if (!setConfig(last,configs.at(i).c_str(),"internal delay",&intdly,&configOK)){
							continue;
						}
						break;
					case CGGTTS::V2E:
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
						cerr << "Syntax error in [CGGTTS] ephemeris - " << stmp << " should be receiver/user " << endl;
						configOK = false;
						continue;
					}
				}
				
				if (setConfig(last,configs.at(i).c_str(),"path",stmp,&configOK)){ // got everything
					// FIXME check compatibility of constellation+code
					stmp=relativeToAbsolutePath(stmp);
					ephemerisPath = relativeToAbsolutePath(ephemerisPath);
					CGGTTSoutputs.push_back(CGGTTSOutput(constellation,code,stmp,calID,intdly,delayKind,
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
				cerr << "unknown value for cggtts::naming convention " << endl;
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
					cerr << "Couldn't parse cggtts::revision date = " << stmp << endl;
					configOK=false;
				}
			}
			else{
				cerr << "Syntax error in [CGGTTS] revision date - " << stmp << " should be YYYY-MM-DD " << endl;
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
				case 2:RINEXversion=RINEX::V2;break;
				case 3:RINEXversion=RINEX::V3;break;
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
				allObservations=true;
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
	string rxModel,rxManufacturer;
	setConfig(last,"receiver","model",rxModel,&configOK);
	
	if (setConfig(last,"receiver","manufacturer",rxManufacturer,&configOK)){
		if (rxManufacturer.find("Trimble") != string::npos){
			receiver = new TrimbleResolution(antenna,rxModel); 
		}
		else if (rxManufacturer.find("Javad") != string::npos){
			receiver = new Javad(antenna,rxModel); 
		}
		else if (rxManufacturer.find("NVS") != string::npos){
			receiver = new NVS(antenna,rxModel); 
		}
		else if (rxManufacturer.find("ublox") != string::npos){
			receiver = new Ublox(antenna,rxModel); 
		}
		else{
			cerr << "A valid receiver model/manufacturer has not been configured - exiting" << endl;
			exit(EXIT_FAILURE);
		}
	}
	
	if (setConfig(last,"receiver","observations",stmp,&configOK,false)){
		boost::to_upper(stmp);
		receiver->constellations = 0; // overrride the default
		if (stmp.find("GPS") != string::npos)
			receiver->constellations |=GNSSSystem::GPS;
		if (stmp.find("GLONASS") != string::npos)
			receiver->constellations |=GNSSSystem::GLONASS;
		if (stmp.find("BEIDOU") != string::npos)
			receiver->constellations |=GNSSSystem::BEIDOU;
		if (stmp.find("GALILEO") != string::npos)
			receiver->constellations |=GNSSSystem::GALILEO;
	}
	
	if (setConfig(last,"receiver","version",stmp,&configOK,false))
		receiver->setVersion(stmp);
	setConfig(last,"receiver","pps offset",&receiver->ppsOffset,&configOK);
	setConfig(last,"receiver","file extension",receiverExtension,&configOK,false);
	setConfig(last,"receiver","sawtooth size",&receiver->sawtooth,&configOK,false);
	
	if (setConfig(last,"receiver","sawtooth phase",stmp,&configOK,false)){
		boost::to_lower(stmp);
		if (stmp == "current second")
			receiver->sawtoothPhase=Receiver::CurrentSecond;
		else if (stmp == "next second")
			receiver->sawtoothPhase=Receiver::NextSecond;
		else if (stmp == "receiver specified")
			receiver->sawtoothPhase=Receiver::ReceiverSpecified;
		else{
			cerr << "Unrecognized option for sawtooth phase: " << stmp << endl;
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

bool Application::setConfig(ListEntry *last,const char *section,const char *token,string &val,bool *ok,bool required)
{
	char *stmp;
	if (list_get_string(last,section,token,&stmp)){
		// ok is not set
		val=stmp;
	}
	else{
		int err = config_file_get_last_error(NULL,0);
		if (err==TokenNotFound && required){
			cerr << "Missing entry for " << section << "::" << token << endl;
			*ok = false;
			return false;
		}
		else if (err==TokenNotFound){
			// ok is not false
			return false;
		}
		else if (err==ParseFailed){
			cerr << "Syntax error in " << section << "::" << token << endl;
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
			cerr << "Missing entry for " << section << "::" << token << endl;
			*ok = false;
			return false;
		}
		else if (err==TokenNotFound){
			// ok is not false
			return false;
		}
		else if (err==ParseFailed){
			cerr << "Syntax error in " << section << "::" << token << endl;
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
			cerr << "Missing entry for " << section << "::" << token << endl;
			*ok = false;
			return false;
		}
		else if (err==TokenNotFound){
			// ok is not false
			return false;
		}
		else if (err==ParseFailed){
			cerr << "Syntax error in " << section << "::" << token << endl;
			*ok = false;
			return false;
		}
	}
	return true;
}

bool Application::writeRIN2CGGTTSParamFile(Receiver *rx, Antenna *ant, string fname)
{
	FILE *fout;
	if (!(fout = fopen(fname.c_str(),"w"))){
		cerr << "Unable to open " << fname << endl;
		return false;
	}
	
	fprintf(fout,"REV DATE\n");
	
	fprintf(fout,"RCVR\n");
	fprintf(fout,"%s\n",rx->manufacturer.c_str());
	fprintf(fout,"CH\n");
	
	fprintf(fout,"LAB NAME\n");
	
	fprintf(fout,"X COORDINATE\n");
	fprintf(fout,"%14.4lf\n",ant->x);
	fprintf(fout,"Y COORDINATE\n");
	fprintf(fout,"%14.4lf\n",ant->y);
	fprintf(fout,"Z COORDINATE\n");
	fprintf(fout,"%14.4lf\n",ant->z);
	fprintf(fout,"COMMENTS\n");
	
	fprintf(fout,"REF\n");
	
	fprintf(fout,"INT DELAY P1 XR+XS (in ns)\n");  // FIXME multiple problems here
	fprintf(fout,"0.0\n");
	fprintf(fout,"INT DELAY P2 XR+XS (in ns)\n");
	fprintf(fout,"0.0\n");
	fprintf(fout,"INT DELAY C1 XR+XS (in ns)\n");
	fprintf(fout,"%11.1lf\n",0.0);
	fprintf(fout,"ANT CAB DELAY (in ns)\n");
	fprintf(fout,"%11.1lf",antCableDelay);
	fprintf(fout,"CLOCK CAB DELAY XP+XO (in ns)\n");
	fprintf(fout,"%11.1lf",refCableDelay);
	
	fprintf(fout,"LEAP SECOND\n");
	fprintf(fout,"%d\n",rx->leapsecs);
	
	fclose(fout);
	
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
	
	logMessage(boost::lexical_cast<string>(matchcnt) + " matched measurements");
	
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
				cerr << "Application::matchMeasurements() not monotonically ordered!" << endl;
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

void Application::writeReceiverTimingDiagnostics(Receiver *rx,Counter *cntr,string fname)
{
	FILE *fout;
	
	if (!(fout = fopen(fname.c_str(),"w"))){
		cerr << "Unable to open " << fname << endl;
		return;
	}
	
	DBGMSG(debugStream,INFO,"writing to " << fname);
	
	for (unsigned int i=0;i<MPAIRS_SIZE;i++){
		if (mpairs[i]->flags == 0x03){
			CounterMeasurement *cm= mpairs[i]->cm;
			ReceiverMeasurement *rxm = mpairs[i]->rm;
			int tmatch=((int) cm->hh)*3600 +  ((int) cm->mm)*60 + ((int) cm->ss);
			fprintf(fout,"%i %g %g %.16e\n",tmatch,cm->rdg,rxm->sawtooth,rxm->timeOffset);
		}
	}
	fclose(fout);
}

void Application::writeSVDiagnostics(Receiver *rx,string path)
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
		
		for (int code = GNSSSystem::C1;code <=GNSSSystem::L2; (code <<= 1)){
			
			if (!(rx->codes & code)) continue;
			
			for (int svn=1;svn<=gnss->nsats();svn++){ // loop over all svn for constellation+code combination
				ostringstream sstr;
				sstr << path << "/" << gnss->oneLetterCode() << svn << ".dat";
				if (!(fout = fopen(sstr.str().c_str(),"w"))){
					cerr << "Unable to open " << sstr.str().c_str() << endl;
					return;
				}
				for (unsigned int m=0;m<rx->measurements.size();m++){
					for (unsigned int svm=0; svm < rx->measurements.at(m)->meas.size();svm++){
						SVMeasurement *sv=rx->measurements.at(m)->meas.at(svm);
						if ((svn == sv->svn) && (g==sv->constellation) && (code==sv->code)){
							int tod = rx->measurements[m]->tmUTC.tm_hour*3600+ rx->measurements[m]->tmUTC.tm_min*60 + rx->measurements[m]->tmUTC.tm_sec;
							// The default here is that df1 contains the raw (non-interpolated) pseudo range and df2 contains 
							// corrected pseudoranges when CGGTTS output has been generated (which can be useful to look at) 
							fprintf(fout,"%d %.16e %.16e %.16e %.16e\n",tod,sv->meas,sv->dbuf1,sv->dbuf2,sv->dbuf3);
							break;
						}
					}
				}
				fclose(fout);
			} //for (int svn= ...
		}// for (int code = ...
	} // for (int g =
}
