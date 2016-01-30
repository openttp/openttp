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

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <configurator.h>

#include "Debug.h"
#include "Antenna.h"
#include "CGGTTS.h"
#include "Counter.h"
#include "CounterMeasurement.h"
#include "Javad.h"
#include "NVS.h"
#include "ReceiverMeasurement.h"
#include "Receiver.h"
#include "RINEX.h"
#include "MakeRINEX.h"
#include "MeasurementPair.h"
#include "SVMeasurement.h"
#include "TrimbleResolution.h"
#include "Utility.h"

extern ostream *debugStream;
extern string   debugFileName;
extern ofstream debugLog;
extern int verbosity;

static struct option longOptions[] = {
		{"configuration",required_argument, 0,  0 },
		{"counter-path",required_argument, 0,  0 },
		{"comment",  required_argument, 0,  0 },
		{"debug",         required_argument, 0,  0 },
		{"help",          no_argument, 0, 0 },
		{"no-navigation", no_argument, 0,  0 },
		{"receiver-path",required_argument, 0,  0 },
		{"verbosity",     required_argument, 0,  0 },
		{"timing-diagnostics",no_argument, 0,  0 },
		{"version",       no_argument, 0,  0 },
		{"sv-diagnostics",no_argument, 0,  0 },
		{0,         			0,0,  0 }
};

//
//	Public
//

MakeRINEX::MakeRINEX(int argc,char **argv)
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
							showHelp();
							exit(EXIT_SUCCESS);
							break;
						
						case 5:
							generateNavigationFile=false;
							break;
							
						case 6:
							receiverPath=optarg;
							break;
							
						case 7:
							{
								if (1!=sscanf(optarg,"%i",&verbosity)){
									cerr << "Bad value for option --verbosity" << endl;
									showHelp();
									exit(EXIT_FAILURE);
								}
							}
							break;
						case 8:
							timingDiagnosticsOn=true;
							break;
							
						case 9:
							showVersion();
							exit(EXIT_SUCCESS);
							break;
						case 10:
							SVDiagnosticsOn=true;
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

MakeRINEX::~MakeRINEX()
{
	for (unsigned int i =0;i<86400;i++)
		delete mpairs[i];
	delete[] mpairs;
}

void MakeRINEX::run()
{
	makeFilenames();
	
	CGGTTS cggtts(antenna,counter,receiver);
	cggtts.ref=CGGTTSref;
	cggtts.lab=CGGTTSlab;
	cggtts.comment=CGGTTScomment;
	cggtts.writeObservationFile(CGGTTS::V1,"test.cctf",MJD,mpairs);
	
	if (!receiver->readLog(receiverFile,MJD)){
		cerr << "Exiting" << endl;
		exit(EXIT_FAILURE);
	}
	
	if (!counter->readLog(counterFile)){
		cerr << "Exiting" << endl;
		exit(EXIT_FAILURE);
	}

	matchMeasurements(receiver,counter);
	
	RINEX rnx(antenna,counter,receiver);
	rnx.agency = agency;
	rnx.observer=observer;
	
	if (generateNavigationFile) 
		rnx.writeNavigationFile(RINEXversion,RINEXnavFile,MJD);
	rnx.writeObservationFile(RINEXversion,RINEXobsFile,MJD,interval,mpairs);
	
	if (timingDiagnosticsOn) 
		writeReceiverTimingDiagnostics(receiver,counter,"timing.dat");
	
	if (SVDiagnosticsOn) 
		writeSVDiagnostics(receiver,tmpPath);
	
	delete receiver;
	delete counter;
	delete antenna;
}

void MakeRINEX::showHelp()
{
	cout << endl << APP_NAME << " version " << MKRINEX_VERSION << endl;
	cout << "Usage: " << APP_NAME << " [options]" << endl;
	cout << "Available options are" << endl;
	cout << "--configuration <file> full path to the configuration file" << endl;
	cout << "--counter-path <path>  path to counter/timer measurements" << endl; 
	cout << "--comment <message>    comment for the CGGTTS file" << endl;
	cout << "--debug <file>         turn on debugging to <file> (use 'stderr' for output to stderr)" << endl;
	cout << "-h,--help              print this help message" << endl;
	cout << "-m <n>                 set the mjd" << endl;
	cout << "--receiver-path <path> path to GNSS receiver logs" << endl;
	cout << "--sv-diagnostics   		write SV diagnostics files" << endl;
	cout << "--timing-diagnostics   write receiver timing diagnostics file" << endl;
	cout << "--verbosity <n>        set debugging verbosity" << endl;
	cout << "--version              print version" << endl;
}

void MakeRINEX::showVersion()
{
	cout << APP_NAME <<  " version " << MKRINEX_VERSION << endl;
	cout << "Written by " << AUTHORS << endl;
	cout << "This ain't no stinkin' Perl script!" << endl;
}


//	
//	Private
//	

void MakeRINEX::init()
{
	pid = getpid();
	
	appName = APP_NAME;
	
	antenna = new Antenna();
	counter = new Counter();
	// defer instantiating the receiver until we know what kind is configured
	receiver = NULL;
	
	RINEXversion=RINEX::V2;
	
	CGGTTSversion=CGGTTS::V1;
	CGGTTScomment="NONE";
	CGGTTSref="REF";
	CGGTTSlab="KAOS";
	
	observer="Time and Frequency";
	agency="NMIx";
	
	refCableDelay=0.0;
	C1InternalDelay=0.0;
	antCableDelay=0.0;
	
	interval=30;
	MJD = int(time(0)/86400)+40587 - 1;// yesterday
	
	timingDiagnosticsOn=false;
	SVDiagnosticsOn=false;
	generateNavigationFile=true;
	
	char *penv;
	homeDir="";
	if ((penv = getenv("HOME"))){
		homeDir=penv;
	}
	
	configurationFile = homeDir+"/etc/cggtts.conf";
	counterPath = homeDir+"/raw";
	counterExtension= "tic";
	receiverPath= homeDir+"/raw";
	receiverExtension="rx";
	RINEXPath=homeDir+"/rinex";
	tmpPath=homeDir+"/tmp";
	
	mpairs= new MeasurementPair*[86400];
	for (int i=0;i<86400;i++)
		mpairs[i]=new MeasurementPair();

}

void  MakeRINEX::makeFilenames()
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
	sprintf(fname,"%s%03d0.%02dN",antenna->markerName.c_str(),yday,yy);
	ss5 << RINEXPath << "/" << fname; // at least no problem with length of RINEXPath
	RINEXnavFile=ss5.str();
	
	ostringstream ss6;
	sprintf(fname,"%s%03d0.%02dO",antenna->markerName.c_str(),yday,yy);
	ss6 << RINEXPath << "/" << fname;
	RINEXobsFile=ss6.str();
	
}

bool MakeRINEX::loadConfig()
{
	// Our conventional config file format is used to maintain compatibility with existing scripts
	ListEntry *last;
	if (!configfile_parse_as_list(&last,configurationFile.c_str())){
		cerr << "Unable to open the configuration file " << configurationFile << endl;
		return false;
	}
	
	bool configOK=true;
	int itmp=2;
	string stmp;
	
	// CGGTTS generation
	configOK= configOK && setConfig(last,"cggtts","version",stmp);
	configOK= configOK && setConfig(last,"cggtts","reference",CGGTTSref);
	configOK= configOK && setConfig(last,"cggtts","lab",CGGTTSlab);
	configOK= configOK && setConfig(last,"cggtts","comments",CGGTTScomment);
	configOK= configOK && setConfig(last,"cggtts","revision date",stmp);
	std::vector<std::string> vals;
	boost::split(vals, stmp,boost::is_any_of(":"), boost::token_compress_on);
			
	// RINEX generation
	configOK= configOK && setConfig(last,"rinex","version",&itmp);
	switch (itmp)
	{
		case 2:RINEXversion=RINEX::V2;break;
		case 3:RINEXversion=RINEX::V3;break;
		default:configOK=false;
	}
	configOK= configOK && setConfig(last,"rinex","observer",observer);
	if (observer=="user"){
		char *uenv;
		if ((uenv = getenv("USER")))
			observer=uenv;
	}
	configOK= configOK && setConfig(last,"rinex","agency",agency);
	
	// Antenna
	configOK= configOK && setConfig(last,"antenna","marker name",antenna->markerName);
	configOK= configOK && setConfig(last,"antenna","marker number",antenna->markerNumber);
	configOK= configOK && setConfig(last,"antenna","marker type",antenna->markerType);
	configOK= configOK && setConfig(last,"antenna","antenna number",antenna->antennaNumber);
	configOK= configOK && setConfig(last,"antenna","antenna type",antenna->antennaType);
	configOK= configOK && setConfig(last,"antenna","x",&(antenna->x));
	configOK= configOK && setConfig(last,"antenna","y",&(antenna->y));
	configOK= configOK && setConfig(last,"antenna","z",&(antenna->z));
	configOK= configOK && setConfig(last,"antenna","delta h",&(antenna->deltaH));
	configOK= configOK && setConfig(last,"antenna","delta e",&(antenna->deltaE));
	configOK= configOK && setConfig(last,"antenna","delta n",&(antenna->deltaN));
	configOK= configOK && setConfig(last,"antenna","frame",antenna->frame);
	
	// Receiver
	//configOK= configOK && setConfig(last,"antenna","marker name",antenna->markerName);
	string rxModel,rxManufacturer;
	configOK= configOK && setConfig(last,"receiver","model",rxModel);
	configOK= configOK && setConfig(last,"receiver","manufacturer",rxManufacturer);
	if (rxManufacturer.find("Trimble") != string::npos){
		if (rxModel.find("Resolution") != string::npos){
			receiver = new TrimbleResolution(antenna,rxModel); 
		}
	}
	else if (rxManufacturer.find("Javad") != string::npos){
		if (rxModel.find("Javad") != string::npos){
			receiver = new Javad(antenna,rxModel); 
		}
	}
	else if (rxManufacturer.find("NVS") != string::npos){
		if (rxModel.find("NV08") != string::npos){
			receiver = new NVS(antenna,rxModel); 
		}
	}
	
	if (NULL==receiver){ // bail out
		cerr << "A valid receiver model/manufacturer has not been configured" << endl;
		return false;
	}
	
	configOK= configOK && setConfig(last,"receiver","observations",stmp);
	
	if (stmp.find("GPS") != string::npos)
		receiver->constellations |=Receiver::GPS;
	else if (stmp.find("GLONASS") != string::npos)
		receiver->constellations |=Receiver::GLONASS;
	else if (stmp.find("BeiDou") != string::npos)
		receiver->constellations |=Receiver::BEIDOU;
	else if (stmp.find("Galileo") != string::npos)
		receiver->constellations |=Receiver::GALILEO;
	
	configOK= configOK && setConfig(last,"receiver","pps offset",&receiver->ppsOffset);
	configOK= configOK && setConfig(last,"receiver","channels",&receiver->channels);
	configOK= configOK && setConfig(last,"receiver","file extension",receiverExtension);
	
	// Counter
	configOK= configOK && setConfig(last,"counter","file extension",counterExtension);
	
	// Delays
	
	configOK= configOK && setConfig(last,"delays","C1 internal",&C1InternalDelay);
	configOK= configOK && setConfig(last,"delays","antenna cable",&antCableDelay);
	configOK= configOK && setConfig(last,"delays","reference cable",&refCableDelay);
	
	// Paths FIXME

	string path="";	
	configOK = configOK && setConfig(last,"paths","rinex",path);
	if (path.size() > 0){
		if (path.at(0) == '/')
			RINEXPath = path;
		else 
			RINEXPath=homeDir+"/"+path;
	}
	path="";
	configOK = configOK && setConfig(last,"paths","receiver data",path);
	if (path.size() > 0){
		if (path.at(0) == '/')
			receiverPath = path;
		else 
			receiverPath=homeDir+"/"+path;
	}
	path="";
	configOK = configOK && setConfig(last,"paths","counter data",path);
	if (path.size() > 0){
		if (path.at(0) == '/')
			counterPath = path;
		else 
			counterPath=homeDir+"/"+path;
	}
	path="";
	configOK = configOK && setConfig(last,"paths","tmp",path);
	if (path.size() > 0){
		if (path.at(0) == '/')
			tmpPath = path;
		else 
			tmpPath=homeDir+"/"+path;
	}
	return configOK;
}

bool MakeRINEX::setConfig(ListEntry *last,const char *section,const char *token,string &val)
{
	char *stmp;
	if (list_get_string(last,section,token,&stmp)){
		val=stmp;
		return true;
	}
	else
		cerr << "Missing parameter/bad value for " << section << "::" << token << endl;
	return false;
}

bool MakeRINEX::setConfig(ListEntry *last,const char *section,const char *token,double *val)
{
	double dtmp;
	if (list_get_double(last,section,token,&dtmp)){
		*val=dtmp;
		return true;
	}
	else
		cerr << "Missing parameter/bad value for " << section << "::" << token << endl;
	return false;
}

bool MakeRINEX::setConfig(ListEntry *last,const char *section,const char *token,int *val)
{
	int itmp;
	if (list_get_int(last,section,token,&itmp)){
		*val=itmp;
		return true;
	}
	else
		cerr << "Missing parameter/bad value for " << section << "::" << token << endl;
	return false;
}

bool MakeRINEX::writeRIN2CGGTTSParamFile(Receiver *rx, Antenna *ant, string fname)
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
	
	fprintf(fout,"INT DELAY P1 XR+XS (in ns)\n");
	fprintf(fout,"0.0\n");
	fprintf(fout,"INT DELAY P2 XR+XS (in ns)\n");
	fprintf(fout,"0.0\n");
	fprintf(fout,"INT DELAY C1 XR+XS (in ns)\n");
	fprintf(fout,"%11.1lf\n",C1InternalDelay);
	fprintf(fout,"ANT CAB DELAY (in ns)\n");
	fprintf(fout,"%11.1lf",antCableDelay);
	fprintf(fout,"CLOCK CAB DELAY XP+XO (in ns)\n");
	fprintf(fout,"%11.1lf",refCableDelay);
	
	fprintf(fout,"LEAP SECOND\n");
	fprintf(fout,"%d\n",rx->leapsecs);
	
	fclose(fout);
	
	return true;
}

void MakeRINEX::matchMeasurements(Receiver *rx,Counter *cntr)
{
	if (cntr->measurements.size() == 0 || rx->measurements.size()==0)
		return;


	// Instead of a complicated search, use an array that records whether the required measurements exist for 
	// each second. This approach:
	// (1) Flags gaps in either record
	// (2) Allows the PC clock to step forward (this just looks like a gap)
	// (3) Allows the PC clock to step back (as might happen on a reboot, and ntpd has not synced up yet).
	//     In this case, data between from the (previous) time of the step to before the step is discarded.
	
	
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
		if (trx>=0 && trx<86400){
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
	for (unsigned int i=0;i<86400;i++){
		if (mpairs[i]->flags == 0x03){
			matchcnt++;
		}
	}
	DBGMSG(debugStream,INFO,matchcnt << " matches");
	
}

void MakeRINEX::writeReceiverTimingDiagnostics(Receiver *rx,Counter *cntr,string fname)
{
	FILE *fout;
	
	if (!(fout = fopen(fname.c_str(),"w"))){
		cerr << "Unable to open " << fname << endl;
		return;
	}
	
	for (unsigned int i=0;i<86400;i++){
		if (mpairs[i]->flags == 0x03){
			CounterMeasurement *cm= mpairs[i]->cm;
			ReceiverMeasurement *rxm = mpairs[i]->rm;
			int tmatch=((int) cm->hh)*3600 +  ((int) cm->mm)*60 + ((int) cm->ss);
			fprintf(fout,"%i %g %g %g\n",tmatch,cm->rdg,rxm->sawtooth,rxm->timeOffset);
		}
	}
	fclose(fout);
}

void MakeRINEX::writeSVDiagnostics(Receiver *rx,string path)
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

