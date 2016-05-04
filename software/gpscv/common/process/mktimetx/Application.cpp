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
#include "Utility.h"

extern ostream *debugStream;
extern string   debugFileName;
extern ofstream debugLog;
extern int verbosity;
extern bool shortDebugMessage;

static struct option longOptions[] = {
		{"configuration",required_argument, 0,  0 },
		{"counter-path",required_argument, 0,  0 },
		{"comment",  required_argument, 0,  0 },
		{"debug",         required_argument, 0,  0 },
		{"disable-tic", no_argument, 0,  0 },
		{"help",          no_argument, 0, 0 },
		{"no-navigation", no_argument, 0,  0 },
		{"receiver-path",required_argument, 0,  0 },
		{"verbosity",     required_argument, 0,  0 },
		{"timing-diagnostics",no_argument, 0,  0 },
		{"version",       no_argument, 0,  0 },
		{"sv-diagnostics",no_argument, 0,  0 },
		{"short-debug-message",no_argument, 0,  0 },
		{0,         			0,0,  0 }
};

using boost::lexical_cast;
using boost::bad_lexical_cast;

#define MPAIRS_SIZE 86400

//
//	Public
//

Application::Application(int argc,char **argv)
{

	init();
	
	// Process the command line options
	// These override anything in the configuration file, default or specified
	int longIndex;
	char c;
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
							counterPath=optarg;
							break;
						
						case 2:
							//CGGTSComment=optarg;
							break;
						case 3:
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
						case 4:
							TICenabled=false;
							break;
						case 5:
							showHelp();
							exit(EXIT_SUCCESS);
							break;
						
						case 6:
							generateNavigationFile=false;
							break;
							
						case 7:
							receiverPath=optarg;
							break;
							
						case 8:
							{
								if (1!=sscanf(optarg,"%i",&verbosity)){
									cerr << "Bad value for option --verbosity" << endl;
									showHelp();
									exit(EXIT_FAILURE);
								}
							}
							break;
						case 9:
							timingDiagnosticsOn=true;
							break;
							
						case 10:
							showVersion();
							exit(EXIT_SUCCESS);
							break;
						case 11:
							SVDiagnosticsOn=true;
							break;
						case 12:
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
						cerr << "Bad value for option --mjd" << endl;
						showHelp();
						exit(EXIT_FAILURE);
					}
				}
				break;
			default:
				break;
			
		}
	}
	
	if (!loadConfig()){
		cerr << "Configuration failed - exiting" << endl;
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
	
	makeFilenames();
		
	// Create the log file, erasing any existing file
	ofstream ofs;
	ofs.open(logFile.c_str());
	ofs.close();
	
	logMessage(timeStamp() + APP_NAME +  " version " + APP_VERSION + " run started");
	
	bool recompress = decompress(receiverFile);
	if (!receiver->readLog(receiverFile,MJD)){
		cerr << "Exiting" << endl;
		exit(EXIT_FAILURE);
	}
	if (recompress) compress(receiverFile);
	
	recompress = decompress(counterFile);
	if (!counter->readLog(counterFile)){
		cerr << "Exiting" << endl;
		exit(EXIT_FAILURE);
	}
	if (recompress) compress(counterFile);
	
	matchMeasurements(receiver,counter); // only do this once
	
	// Each system+code generates a CGGTTS file
	if (createCGGTTS){
		for (unsigned int i=0;i<CGGTTSoutputs.size();i++){
			CGGTTS cggtts(antenna,counter,receiver);
			cggtts.ref=CGGTTSref;
			cggtts.lab=CGGTTSlab;
			cggtts.comment=CGGTTScomment;
			cggtts.revDateYYYY=CGGTTSRevDateYYYY;
			cggtts.revDateMM=CGGTTSRevDateMM;
			cggtts.revDateDD=CGGTTSRevDateDD;
			cggtts.cabDly=antCableDelay;
			cggtts.intDly=CGGTTSoutputs.at(i).internalDelay;
			cggtts.refDly=refCableDelay;
			cggtts.minElevation=CGGTTSminElevation;
			cggtts.maxDSG = CGGTTSmaxDSG;
			cggtts.minTrackLength=CGGTTSminTrackLength;
			cggtts.ver=CGGTTSversion;
			cggtts.constellation=CGGTTSoutputs.at(i).constellation;
			cggtts.code=CGGTTSoutputs.at(i).code;
			cggtts.calID=CGGTTSoutputs.at(i).calID;
			string CGGTTSfile =makeCGGTTSFilename(CGGTTSoutputs.at(i),MJD);
			cggtts.writeObservationFile(CGGTTSfile,MJD,mpairs,TICenabled);
		}
	} // if createCGGTTS
	
	
	// Only one RINEX file per GNSS system is created
	
	if (createRINEX){
		RINEX rnx(antenna,counter,receiver);
		rnx.agency = agency;
		rnx.observer=observer;
		
		if (generateNavigationFile) 
			rnx.writeNavigationFile(RINEXversion,RINEXnavFile,MJD);
		rnx.writeObservationFile(RINEXversion,RINEXobsFile,MJD,interval,mpairs,TICenabled);
	}
	
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
	cout << "--counter-path <path>  path to counter/timer measurements" << endl; 
	cout << "--comment <message>    comment for the CGGTTS file" << endl;
	cout << "--debug <file>         turn on debugging to <file> (use 'stderr' for output to stderr)" << endl;
	cout << "--disable-tic          disables use of sawtooth-corrected TIC measurements" << endl;
	cout << "-h,--help              print this help message" << endl;
	cout << "-m <n>                 set the mjd" << endl;
	cout << "--receiver-path <path> path to GNSS receiver logs" << endl;
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
	CGGTTSminTrackLength=390;
	
	observer="Time and Frequency";
	agency="NMIx";
	
	refCableDelay=0.0;
	antCableDelay=0.0;
	
	interval=30;
	MJD = int(time(0)/86400)+40587 - 1;// yesterday
	
	timingDiagnosticsOn=false;
	SVDiagnosticsOn=false;
	generateNavigationFile=true;
	TICenabled=true;
	
	char *penv;
	homeDir="";
	if ((penv = getenv("HOME"))){
		homeDir=penv;
	}
	
	logFile = "mktimetx.log";
	processingLogPath =  homeDir+"/logs";
	configurationFile = homeDir+"/etc/gpscv.conf";
	counterPath = homeDir+"/raw";
	counterExtension= "tic";
	receiverPath= homeDir+"/raw";
	receiverExtension="rx";
	RINEXPath=homeDir+"/rinex";
	CGGTTSPath=homeDir+"/cggtts";
	CGGTTSnamingConvention=Plain;
	tmpPath=homeDir+"/tmp";
	
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
			absPath=homeDir+"/"+path;
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
	
	ostringstream ss5;
	// this is ugly but C++ stream manipulators are even uglier
	char fname[16];
	snprintf(fname,15,"%s%03d0.%02dN",antenna->markerName.c_str(),yday,yy);
	ss5 << RINEXPath << "/" << fname; // at least no problem with length of RINEXPath
	RINEXnavFile=ss5.str();
	
	ostringstream ss6;
	snprintf(fname,15,"%s%03d0.%02dO",antenna->markerName.c_str(),yday,yy);
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
			case Receiver::GPS:constellation="G";break;
			case Receiver::GLONASS:constellation="R";break;
			case Receiver::BEIDOU:constellation="E";break;
			case Receiver::GALILEO:constellation="C";break;
			case Receiver::QZSS:constellation="J";break;
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
	bool configResult;
	int itmp=2;
	string stmp;
	
	// CGGTTS generation
	if ((configResult=setConfig(last,"cggtts","create",stmp,false))){
		boost::to_upper(stmp);
		if (stmp=="NO"){
			createCGGTTS=false;
		}
	}
	configOK = configOK && configResult;
	
	if (createCGGTTS){
		if ((configResult=setConfig(last,"cggtts","outputs",stmp))){
			std::vector<std::string> configs;
			boost::split(configs, stmp,boost::is_any_of(","), boost::token_compress_on);
			int constellation=0,code=0;
			for (unsigned int i=0;i<configs.size();i++){
				string calID="";
				if (setConfig(last,configs.at(i).c_str(),"constellation",stmp)){
					boost::to_upper(stmp);
					if (stmp == "GPS")
						constellation = Receiver::GPS;
					else if (stmp == "GLONASS")
						constellation = Receiver::GLONASS;
					else if (stmp=="BEIDOU")
						constellation = Receiver::BEIDOU;
					else if (stmp=="GALILEO")
						constellation = Receiver::GALILEO;
					else if (stmp=="QZSS")
						constellation = Receiver::QZSS;
					else{
						cerr << "unknown constellation " << stmp << " in [" << configs.at(i) << "]" << endl;
						configOK=false;
						continue;
					}
				}
				if (setConfig(last,configs.at(i).c_str(),"code",stmp)){
					boost::to_upper(stmp);
					if (stmp=="C1")
						code=Receiver::C1;
					else if (stmp=="P1")
						code=Receiver::P1;
					else if (stmp=="P2")
						code=Receiver::P2;
					else if (stmp=="B1")
						code=Receiver::B1;
					else if (stmp=="E1")
						code=Receiver::E1;
					else{
						cerr << "unknown code " << stmp << " in [" << configs.at(i) << "]" << endl;
						configOK=false;
						continue;
					}
				}
				setConfig(last,configs.at(i).c_str(),"bipm cal id",calID,false);
				double intdly=0.0;
				if (!setConfig(last,configs.at(i).c_str(),"internal delay",&intdly)){
					configOK=false;
					continue;
				}
				if (setConfig(last,configs.at(i).c_str(),"path",stmp)){ // got everything
					// FIXME check compatibility of constellation+code
					stmp=relativeToAbsolutePath(stmp);
					CGGTTSoutputs.push_back(CGGTTSOutput(constellation,code,stmp,calID,intdly));
				}
				else{
					configOK=false;
				}
			} // for
		} // if setConfig
		
		if (setConfig(last,"cggtts","version",stmp,false)){
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
		else{
			configOK=false;
		}
		
		if (!setConfig(last,"cggtts","reference",CGGTTSref)) configOK=false;
		if (!setConfig(last,"cggtts","lab",CGGTTSlab)) configOK=false;
		if (!setConfig(last,"cggtts","comments",CGGTTScomment,false)) configOK=false;
		if (!setConfig(last,"cggtts","minimum track length",&CGGTTSminTrackLength,false)) configOK=false;
		if (!setConfig(last,"cggtts","maximum dsg",&CGGTTSmaxDSG,false)) configOK=false;
		if (!setConfig(last,"cggtts","minimum elevation",&CGGTTSminElevation,false)) configOK=false;
		if (setConfig(last,"cggtts","naming convention",stmp,false)){
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
				if (!setConfig(last,"cggtts","lab id",CGGTTSlabCode)) configOK=false;
				if (!setConfig(last,"cggtts","receiver id",CGGTTSreceiverID)) configOK=false;
			}
		}
		
		if (setConfig(last,"cggtts","revision date",stmp)){
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
		else{
			configOK=false;
		}
	}
	
	// RINEX generation
	if (setConfig(last,"rinex","create",stmp,false)){
		boost::to_upper(stmp);
		if (stmp=="NO")
			createRINEX=false;
	}
	else{
		configOK=false;
	}
	
	if (createRINEX){
		if (setConfig(last,"rinex","version",&itmp)){
			switch (itmp)
			{
				case 2:RINEXversion=RINEX::V2;break;
				case 3:RINEXversion=RINEX::V3;break;
				default:configOK=false;
			}
		}
		else{
			configOK=false;
		}
		
		if (setConfig(last,"rinex","observer",observer)){
			if (observer=="user"){
				char *uenv;
				if ((uenv = getenv("USER")))
					observer=uenv;
			}
		}
		else{
			configOK=false;
		}
		if (!setConfig(last,"rinex","agency",agency)) configOK=false;
	}
	
	// Antenna
	if (!setConfig(last,"antenna","marker name",antenna->markerName)) configOK=false;
	if (!setConfig(last,"antenna","marker number",antenna->markerNumber)) configOK=false;
	if (!setConfig(last,"antenna","marker type",antenna->markerType)) configOK=false;
	if (!setConfig(last,"antenna","antenna number",antenna->antennaNumber)) configOK=false;
	if (!setConfig(last,"antenna","antenna type",antenna->antennaType)) configOK=false;
	if (!setConfig(last,"antenna","x",&(antenna->x))) configOK=false;
	if (!setConfig(last,"antenna","y",&(antenna->y))) configOK=false;
	if (!setConfig(last,"antenna","z",&(antenna->z))) configOK=false;
	if (!setConfig(last,"antenna","delta h",&(antenna->deltaH),false)) configOK=false;
	if (!setConfig(last,"antenna","delta e",&(antenna->deltaE),false)) configOK=false;
	if (!setConfig(last,"antenna","delta n",&(antenna->deltaN),false)) configOK=false;
	if (!setConfig(last,"antenna","frame",antenna->frame)) configOK=false;

	Utility::ECEFtoLatLonH(antenna->x,antenna->y,antenna->z,
		&(antenna->latitude),&(antenna->longitude),&(antenna->height));
	
	// Receiver

	string rxModel,rxManufacturer;
	if (!setConfig(last,"receiver","model",rxModel)) configOK=false;
	if ((configResult = setConfig(last,"receiver","manufacturer",rxManufacturer))){
		if (rxManufacturer.find("Trimble") != string::npos){
			receiver = new TrimbleResolution(antenna,rxModel); 
		}
		else if (rxManufacturer.find("Javad") != string::npos){
			receiver = new Javad(antenna,rxModel); 
		}
		else if (rxManufacturer.find("NVS") != string::npos){
			receiver = new NVS(antenna,rxModel); 
		}
		else{
			cerr << "Unknown receiver manufacturer " << rxManufacturer << endl;
		}
	}
	
	if (NULL==receiver){ // bail out, since no receiver object
		cerr << "A valid receiver model/manufacturer has not been configured - exiting" << endl;
		exit(EXIT_FAILURE);
	}
	
	if (setConfig(last,"receiver","observations",stmp,false)){
		boost::to_upper(stmp);
		if (stmp.find("GPS") != string::npos)
			receiver->constellations |=Receiver::GPS;
		if (stmp.find("GLONASS") != string::npos)
			receiver->constellations |=Receiver::GLONASS;
		if (stmp.find("BEIDOU") != string::npos)
			receiver->constellations |=Receiver::BEIDOU;
		if (stmp.find("GALILEO") != string::npos)
			receiver->constellations |=Receiver::GALILEO;
		if (stmp.find("QZSS") != string::npos)
			receiver->constellations |=Receiver::QZSS;
	}
	else
		configOK=false;
	
	setConfig(last,"receiver","version",receiver->version); 
	if (!setConfig(last,"receiver","pps offset",&receiver->ppsOffset)) configOK=false;
	if (!setConfig(last,"receiver","file extension",receiverExtension,false)) configOK=false;
	
	// Counter
	if (!setConfig(last,"counter","file extension",counterExtension,false)) configOK=false;
	
	// Delays
	//if (!setConfig(last,"delays","internal",&internalDelay)) configOK=false;
	if (!setConfig(last,"delays","antenna cable",&antCableDelay)) configOK=false;
	if (!setConfig(last,"delays","reference cable",&refCableDelay)) configOK=false;
	
	// Paths

	string path="";	
	if (setConfig(last,"paths","rinex",path))
		RINEXPath=relativeToAbsolutePath(path);
	
	path="";
	if (setConfig(last,"paths","receiver data",path))
		receiverPath=relativeToAbsolutePath(path);
	
	path="";
	if (setConfig(last,"paths","counter data",path))
		counterPath=relativeToAbsolutePath(path);
	
	path="";
	if (setConfig(last,"paths","tmp",path))
		tmpPath=relativeToAbsolutePath(path);
	
	path="";
	if (setConfig(last,"paths","cggtts",path))
		CGGTTSPath=relativeToAbsolutePath(path);
	
	path="";
	if (setConfig(last,"paths","processing log",path))
		processingLogPath=relativeToAbsolutePath(path);
	
	setConfig(last,"misc","gzip",gzip);
	
	return configOK;
}

bool Application::setConfig(ListEntry *last,const char *section,const char *token,string &val,bool required)
{
	char *stmp;
	if (list_get_string(last,section,token,&stmp)){
		val=stmp;
	}
	else{
		int err = config_file_get_last_error(NULL,0);
		if (err==TokenNotFound && required){
			cerr << "Missing entry for " << section << "::" << token << endl;
			return false;
		}
		else if (err==ParseFailed){
			cerr << "Syntax error in " << section << "::" << token << endl;
			return false;
		}
	}
	return true;
}

bool Application::setConfig(ListEntry *last,const char *section,const char *token,double *val,bool required)
{
	double dtmp;
	if (list_get_double(last,section,token,&dtmp)){
		*val=dtmp;
		return true;
	}
	else{
		int err = config_file_get_last_error(NULL,0);
		if (err==TokenNotFound && required){
			cerr << "Missing entry for " << section << "::" << token << endl;
			return false;
		}
		else if (err==ParseFailed){
			cerr << "Syntax error in " << section << "::" << token << endl;
			return false;
		}
	}
	return true;
}

bool Application::setConfig(ListEntry *last,const char *section,const char *token,int *val,bool required)
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
			return false;
		}
		else if (err==ParseFailed){
			cerr << "Syntax error in " << section << "::" << token << endl;
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

void Application::writeReceiverTimingDiagnostics(Receiver *rx,Counter *cntr,string fname)
{
	FILE *fout;
	
	if (!(fout = fopen(fname.c_str(),"w"))){
		cerr << "Unable to open " << fname << endl;
		return;
	}
	
	for (unsigned int i=0;i<MPAIRS_SIZE;i++){
		if (mpairs[i]->flags == 0x03){
			CounterMeasurement *cm= mpairs[i]->cm;
			ReceiverMeasurement *rxm = mpairs[i]->rm;
			int tmatch=((int) cm->hh)*3600 +  ((int) cm->mm)*60 + ((int) cm->ss);
			fprintf(fout,"%i %g %g %g\n",tmatch,cm->rdg,rxm->sawtooth,rxm->timeOffset);
		}
	}
	fclose(fout);
}

void Application::writeSVDiagnostics(Receiver *rx,string path)
{
	FILE *fout;
	
	// GPS
	
	for (int prn=1;prn<=32;prn++){
		ostringstream sstr;
		sstr << path << "/G" << prn << ".dat";
		if (!(fout = fopen(sstr.str().c_str(),"w"))){
			cerr << "Unable to open " << sstr.str().c_str() << endl;
			return;
		}
		for (unsigned int m=0;m<rx->measurements.size();m++){
			for (unsigned int msv=0;msv<rx->measurements[m]->gps.size();msv++){
				SVMeasurement *svm = rx->measurements[m]->gps[msv];
				if (svm->svn == prn){
					int tod = rx->measurements[m]->tmUTC.tm_hour*3600+ rx->measurements[m]->tmUTC.tm_min*60 + rx->measurements[m]->tmUTC.tm_sec;
					fprintf(fout,"%d %14.3lf \n",tod,svm->meas*CVACUUM);
				}
			}
		}
		fclose(fout);
	}
}

