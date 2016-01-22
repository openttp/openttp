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

#include <configurator.h>

#include "Debug.h"
#include "Antenna.h"
#include "Counter.h"
#include "CounterMeasurement.h"
#include "Javad.h"
#include "NVS.h"
#include "ReceiverMeasurement.h"
#include "Receiver.h"
#include "MakeRINEX.h"
#include "SVMeasurement.h"
#include "TrimbleResolution.h"
#include "Utility.h"

extern ostream *debugStream;
extern string   debugFileName;
extern ofstream debugLog;
extern int verbosity;

const char * RINEXVersionName[]= {"2.11","3.02"};

#define CVACUUM 299792458

// Lookup table to convert URA index to URA value in m for SV accuracy
//      0   1 2   3 4    5  6  7  8   9  10  11   12   13   14  15
double URA[]={2,2.8,4,5.7,8,11.3,16,32,64,128,256,512,1024,2048,4096,0.0};


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
							CGGTSComment=optarg;
							cerr << "Got comment " << optarg  << endl;
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
	
	DBGMSG(debugStream,3,"counter path = " << counterPath << ",receiver path = " << receiverPath);
	
	
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
	
	receiver->readLog(receiverFile,MJD);
	
	counter->readLog(counterFile);

	matchMeasurements(receiver,counter);
	
	if (generateNavigationFile) writeRINEXNavFile(RINEXversion,receiver,RINEXnavFile,MJD);
	writeRINEXObsFile(RINEXversion,receiver,counter,antenna,RINEXobsFile,MJD);
	
	if (timingDiagnosticsOn) writeReceiverTimingDiagnostics(receiver,counter,"timing.dat");
	if (SVDiagnosticsOn) writeSVDiagnostics(receiver,tmpPath);
	
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
	
	antenna = new Antenna();
	counter = new Counter();
	// defer instantiating the receiver until we know what kind is configured
	receiver = NULL;
	
	RINEXversion=V2;
	
	observer="Time and Frequency";
	agency="NMIx";
	CGGTSComment="";
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
	receiverPath= homeDir+"/raw";
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
	ss2 << counterPath << "/" << MJD << ".cvtime";
	counterFile=ss2.str();
	
	ostringstream ss3;
	ss3 << receiverPath << "/" << MJD << ".rxrawdata";
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
		cerr << "Failed to load the configuration file " << configurationFile << endl;
		return false;
	}
	
	bool configOK=true;
	int itmp=2;
	string stmp;
	
	// RINEX generation
	configOK= configOK && setConfig(last,"rinex","version",&itmp);
	switch (itmp)
	{
		case 2:RINEXversion=V2;break;
		case 3:RINEXversion=V3;break;
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

bool MakeRINEX::writeRINEXObsFile(int ver,Receiver *rx,Counter *cntr,Antenna *ant,string fname,int mjd)
{
	char buf[81];
	FILE *fout;
	if (!(fout = fopen(fname.c_str(),"w"))){
		return false;
	}
	
	char obs;
	switch (rx->constellations){
		case Receiver::GPS: obs='G';break;
		case Receiver::GLONASS: obs='R';break;
		case Receiver::GALILEO: obs='E';break;
		case Receiver::BEIDOU:obs='X';break; // not defined yet
		default: obs= 'M';break;
		
	}
	fprintf(fout,"%9s%11s%-20s%c%-19s%-20s\n",RINEXVersionName[ver],"","O",obs,"","RINEX VERSION / TYPE");
	
	time_t tnow = time(NULL);
	struct tm *tgmt = gmtime(&tnow);
	
	switch (RINEXversion){
		case V2:
		{
			strftime(buf,80,"%d-%b-%y %T",tgmt);
			fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			break;
		}
		case V3:
		{
			snprintf(buf,80,"%04d%02d%02d %02d%02d%02d UTC",tgmt->tm_year+1900,tgmt->tm_mon+1,tgmt->tm_mday,
					 tgmt->tm_hour,tgmt->tm_min,tgmt->tm_sec);
			fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			break;
		}
		default:break;
	}
	
	fprintf(fout,"%-60s%-20s\n",ant->markerName.c_str(),"MARKER NAME");
	fprintf(fout,"%-20s%40s%-20s\n",ant->markerNumber.c_str(),"","MARKER NUMBER");
	fprintf(fout,"%-20s%-40s%-20s\n",observer.c_str(),agency.c_str(),"OBSERVER / AGENCY");
	fprintf(fout,"%-20s%-20s%-20s%-20s\n",
		rx->serialNumber.c_str(),rx->modelName.c_str(),rx->version1.c_str(),"REC # / TYPE / VERS");
	fprintf(fout,"%-20s%-20s%-20s%-20s\n",ant->antennaNumber.c_str(),ant->antennaType.c_str()," ","ANT # / TYPE");
	fprintf(fout,"%14.4lf%14.4lf%14.4lf%-18s%-20s\n",ant->x,ant->y,ant->z," ","APPROX POSITION XYZ");
	fprintf(fout,"%14.4lf%14.4lf%14.4lf%-18s%-20s\n",ant->deltaH,ant->deltaE,ant->deltaN," ","ANTENNA: DELTA H/E/N");
	
	int nobs = 0;
	string obsTypes="";
	if (rx->constellations & Receiver::GPS) {
		nobs++;
		obsTypes = "    C1";
	}
	if (rx->constellations & Receiver::GLONASS) {
		nobs++;
		obsTypes = "    C1";
	}
	if (rx->constellations & Receiver::GALILEO) {
		nobs++;
		obsTypes += "    C5"; 
	}
	if (rx->constellations & Receiver::BEIDOU) {
		// FIXME observation types undefined ...
		nobs++;
	}
	
	switch (RINEXversion){
		case V2:
		{
			fprintf(fout,"%6d%-54s%-20s\n",nobs,obsTypes.c_str(),"# / TYPES OF OBSERV");
			break;
		}
		case V3:
		{
			fprintf(fout,"%1s  %3d %3s%50s%-20s\n","G",1,"C1C"," ","SYS / # / OBS TYPES"); // FIXME
			break;
		}
		default:break;
	}
	
	// Find the first observation
	
	int obsTime=0;
	int currMeas=0;
	while (currMeas < 86400 && obsTime <= 86400){
		if (mpairs[currMeas]->flags==0x03){
			ReceiverMeasurement *rm = mpairs[currMeas]->rm;
			// Round the measurement time to the nearest second, accounting for any fractional part of the second)
			int tMeas=(int) rint(rm->tmGPS.tm_hour*3600+rm->tmGPS.tm_min*60+rm->tmGPS.tm_sec + rm->tmfracs);
			if (tMeas==obsTime){
				fprintf(fout,"%6d%6d%6d%6d%6d%13.7lf%-5s%3s%-9s%-20s\n",
					rm->tmGPS.tm_year+1900,rm->tmGPS.tm_mon+1,rm->tmGPS.tm_mday,rm->tmGPS.tm_hour,rm->tmGPS.tm_min,
					(double) (rm->tmGPS.tm_sec+rm->tmfracs),
					" ", "GPS"," ","TIME OF FIRST OBS");
				break;
			}
			else if (tMeas < obsTime)
				currMeas++;
			else if (tMeas > obsTime)
				obsTime += interval;
		}
		else
			currMeas++;
	}
	fprintf(fout,"%6d%54s%-20s\n",rx->leapsecs," ","LEAP SECONDS");
	fprintf(fout,"%60s%-20s\n","","END OF HEADER");
	
	obsTime=0;
	currMeas=0;
	while (currMeas < 86400 && obsTime <= 86400){
		if (mpairs[currMeas]->flags==0x03){
			ReceiverMeasurement *rm = mpairs[currMeas]->rm;
			// Round the measurement time to the nearest second, accounting for any fractional part of the second)
			int tMeas=(int) rint(rm->tmGPS.tm_hour*3600+rm->tmGPS.tm_min*60+rm->tmGPS.tm_sec + rm->tmfracs);
			if (tMeas==obsTime){
				switch (RINEXversion){
					case V2:
					{
						int yy = rm->tmGPS.tm_year - 100*(rm->tmGPS.tm_year/100);
						fprintf(fout," %02d %2d %2d %2d %2d%11.7lf  %1d%3d",
							yy,rm->tmGPS.tm_mon+1,rm->tmGPS.tm_mday,rm->tmGPS.tm_hour,rm->tmGPS.tm_min,
							(double) (rm->tmGPS.tm_sec+rm->tmfracs),
							rm->epochFlag,(int) rm->gps.size());
						
						int svcount=0;
						int nsv = rm->gps.size();
						for (unsigned int sv=0;sv<rm->gps.size();sv++){
							svcount++;
							fprintf(fout,"G%02d",rm->gps[sv]->svn);
							if ((nsv > 12) && ((svcount % 12)==1)){ // more to do so start a new line
								fprintf(fout,"\n%32s","");
							}
						}
						fprintf(fout,"\n"); // CHECK does this work OK when there are no observations
						
						for (unsigned int sv=0;sv<rm->gps.size();sv++)
							fprintf(fout,"%14.3lf%1i%1i\n",rm->gps[sv]->meas*CVACUUM,rm->gps[sv]->lli,rm->gps[sv]->signal);
						break;
					}
					case V3:
					{
						fprintf(fout,"> %4d %2.2d %2.2d %2.2d %2.2d%11.7f %1d%3d%6s%15.12lf\n",
							rm->tmGPS.tm_year+1900,rm->tmGPS.tm_mon+1,rm->tmGPS.tm_mday,rm->tmGPS.tm_hour,rm->tmGPS.tm_min,(double) rm->tmGPS.tm_sec,
							rm->epochFlag,(int) rm->gps.size()," ",0.0);
						for (unsigned int i=0;i<rm->gps.size();i++)
							fprintf(fout,"G%2.2d%14.3lf%1i%1i\n",rm->gps[i]->svn,rm->gps[i]->meas*CVACUUM,rm->gps[i]->lli,rm->gps[i]->signal);
					} // case V3
				} // switch (RINEXversion)
				
				obsTime+=interval;
				currMeas++;
			}
			else if (tMeas < obsTime){
				currMeas++;
			}
			else if (tMeas > obsTime){
				obsTime += interval;
			}
		}
		else{
			currMeas++;
		}
	}
	fclose(fout);

	return true;
}

bool MakeRINEX::writeRINEXNavFile(int ver,Receiver *rx,string fname,int mjd)
{
	char buf[81];
	FILE *fout;
	if (!(fout = fopen(fname.c_str(),"w"))){
		return false;
	}
	
	switch (RINEXversion)
	{
		case V2:
			fprintf(fout,"%9s%11s%1s%-39s%-20s\n",RINEXVersionName[ver],"","N","","RINEX VERSION / TYPE");
			break;
		case V3:
			fprintf(fout,"%9s%11s%-20s%-20s%-20s\n",RINEXVersionName[ver],"","N: GNSS NAV DATA","G: GPS","RINEX VERSION / TYPE");
			break;
		default:
			break;
	}
	
	time_t tnow = time(NULL);
	struct tm *tgmt = gmtime(&tnow);
	
	// Determine the GPS week number
	// GPS week 0 begins midnight 5/6 Jan 1980, MJD 44244
	int gpsWeek=int ((mjd-44244)/7);	
	switch (RINEXversion)
	{
		case V2:
		{
			strftime(buf,80,"%d-%b-%y %T",tgmt);
			fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			fprintf(fout,"%2s%12.4e%12.4e%12.4e%12.4e%10s%-20s\n","",
				rx->ionoData.a0,rx->ionoData.a1,rx->ionoData.a2,rx->ionoData.a3,"","ION ALPHA");
			fprintf(fout,"%2s%12.4e%12.4e%12.4e%12.4e%10s%-20s\n","",
				rx->ionoData.B0,rx->ionoData.B1,rx->ionoData.B2,rx->ionoData.B3,"","ION BETA");
			fprintf(fout,"%3s%19.12e%19.12e%9d%9d %-20s\n","",
				rx->utcData.A0,rx->utcData.A1,(int) rx->utcData.t_ot, gpsWeek,"DELTA-UTC: A0,A1,T,W");
			break;
		}
		case V3:
		{
			snprintf(buf,80,"%04d%02d%02d %02d%02d%02d UTC",tgmt->tm_year+1900,tgmt->tm_mon+1,tgmt->tm_mday,
					 tgmt->tm_hour,tgmt->tm_min,tgmt->tm_sec);
			fprintf(fout,"%-20s%-20s%-20s%-20s\n",APP_NAME,agency.c_str(),buf,"PGM / RUN BY / DATE");
			fprintf(fout,"GPSA %12.4e%12.4e%12.4e%12.4e%7s%-20s\n",
					rx->ionoData.a0,rx->ionoData.a1,rx->ionoData.a2,rx->ionoData.a3,"","IONOSPHERIC CORR");
			fprintf(fout,"GPSB %12.4e%12.4e%12.4e%12.4e%7s%-20s\n",
					rx->ionoData.B0,rx->ionoData.B1,rx->ionoData.B2,rx->ionoData.B3,"","IONOSPHERIC CORR");
			fprintf(fout,"GPUT %17.10e%16.9e%7d%5d %5s %2d %-20s\n",rx->utcData.A0,rx->utcData.A1,(int) rx->utcData.t_ot,gpsWeek," ", 0,"TIME SYSTEM CORR");
			break;
		}
	}
	
	fprintf(fout,"%6d%54s%-20s\n",rx->leapsecs," ","LEAP SECONDS");
	fprintf(fout,"%60s%-20s\n"," ","END OF HEADER");
	
	// Write out the ephemeris entries
	int lastGPSWeek=-1;
	int lastToc=-1;
	int weekRollovers=0;
	struct tm tmGPS0;
	tmGPS0.tm_sec=tmGPS0.tm_min=tmGPS0.tm_hour=0;
	tmGPS0.tm_mday=6;tmGPS0.tm_mon=0;tmGPS0.tm_year=1980-1900,tmGPS0.tm_isdst=0;
	time_t tGPS0=mktime(&tmGPS0);
	for (unsigned int i=0;i<rx->ephemeris.size();i++){
			
		// Account for GPS rollover:
		// GPS week 0 begins midnight 5/6 Jan 1980, MJD 44244
		// GPS week 1024 begins midnight 21/22 Aug 1999, MJD 51412
		// GPS week 2048 begins midnight 6/7 Apr 2019, MJD 58580
		int tmjd=mjd;
		int GPSWeek=rx->ephemeris[i]->week_number;
		
		while (tmjd>=51412) {
			GPSWeek+=1024;
			tmjd-=(7*1024);
		}
		if (-1==lastGPSWeek){lastGPSWeek=GPSWeek;}
		// Convert GPS week + $Toc to epoch as year, month, day, hour, min, sec
		// Note that the epoch should be specified in GPS time
		double Toc=rx->ephemeris[i]->t_OC;
		if (-1==lastToc) {lastToc = Toc;}
		// If GPS week is unchanged and Toc has gone backwards by more than 2 days, increment GPS week
		if ((GPSWeek == lastGPSWeek) && (Toc-lastToc < -2*86400)){
			weekRollovers=1;
		}
		else if (GPSWeek == lastGPSWeek+1){//OK now 
			weekRollovers=0; 	
		}
		
		lastGPSWeek=GPSWeek;
		lastToc=Toc;
		
		GPSWeek = GPSWeek + weekRollovers;
		
		double t=Toc;
		int day=(int) t/86400;
		t-=86400*day;
		int hour=(int) t/3600;
		t-=3600*hour;
		int minute=(int) t/60;
		t-=60*minute;
		int second=t;
	
		time_t tgps = tGPS0+GPSWeek*86400*7+Toc;
		struct tm *tmGPS = gmtime(&tgps);
		
		switch (RINEXversion)
		{
			case V2:
			{
				int yy = tmGPS->tm_year-100*(tmGPS->tm_year/100);
				fprintf(fout,"%02d %02d %02d %02d %02d %02d%5.1f%19.12e%19.12e%19.12e\n",rx->ephemeris[i]->SVN,
					yy,tmGPS->tm_mon+1,tmGPS->tm_mday,hour,minute,(float) second,
					rx->ephemeris[i]->a_f0,rx->ephemeris[i]->a_f1,rx->ephemeris[i]->a_f2);
				
				snprintf(buf,80,"%%3s%%19.12e%%19.12e%%19.12e%%19.12e\n");
			
				break;
			}
			case V3:
			{
				fprintf(fout,"G%02d %4d %02d %02d %02d %02d %02d%19.12e%19.12e%19.12e\n",rx->ephemeris[i]->SVN,
					tmGPS->tm_year+1900,tmGPS->tm_mon+1,tmGPS->tm_mday,hour,minute,second,
					rx->ephemeris[i]->a_f0,rx->ephemeris[i]->a_f1,rx->ephemeris[i]->a_f2);
				
				snprintf(buf,80,"%%4s%%19.12e%%19.12e%%19.12e%%19.12e\n");
				
				break;
			}
		}
		
		fprintf(fout,buf," ", // broadcast orbit 1
					(double) rx->ephemeris[i]->IODE,rx->ephemeris[i]->C_rs,rx->ephemeris[i]->delta_N,rx->ephemeris[i]->M_0);
				
		fprintf(fout,buf," ", // broadcast orbit 2
			rx->ephemeris[i]->C_uc,rx->ephemeris[i]->e,rx->ephemeris[i]->C_us,rx->ephemeris[i]->sqrtA);
		
		fprintf(fout,buf," ", // broadcast orbit 3
			rx->ephemeris[i]->t_oe,rx->ephemeris[i]->C_ic,rx->ephemeris[i]->OMEGA_0,rx->ephemeris[i]->C_is);
		
		fprintf(fout,buf," ", // broadcast orbit 4
			rx->ephemeris[i]->i_0,rx->ephemeris[i]->C_rc,rx->ephemeris[i]->OMEGA,rx->ephemeris[i]->OMEGADOT);
		
		fprintf(fout,buf," ", // broadcast orbit 5
			rx->ephemeris[i]->IDOT,1.0,(double) GPSWeek,0.0);
	
		fprintf(fout,buf," ", // broadcast orbit 6
			URA[rx->ephemeris[i]->SV_accuracy_raw],(double) rx->ephemeris[i]->SV_health,rx->ephemeris[i]->t_GD,(double) rx->ephemeris[i]->IODC);
		
		fprintf(fout,buf," ", // broadcast orbit 7
			rx->ephemeris[i]->t_ephem,0.0,0.0,0.0);
	}
	
	fclose(fout);
	
	return true;
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
				DBGMSG(debugStream,1,"duplicate counter measurement " << (int) cm->hh << ":" << (int) cm->mm << ":" <<(int) cm->ss);
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
				DBGMSG(debugStream,1,"duplicate receiver measurement " << (int) rxm->pchh << ":" << (int) rxm->pcmm << ":" <<(int) rxm->pcss);
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
	DBGMSG(debugStream,1,matchcnt << " matches");
	
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
			fprintf(fout,"%i %g %g %g\n",tmatch,cm->rdg,rxm->sawtooth,rxm->timeOffset*1.09E-9);
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

