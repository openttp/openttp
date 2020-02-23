//
// The MIT License (MIT)
//
// Copyright (c) 2016 Michael J. Wouters
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

//
// Provides a simple, command line interface to the PRS-10
//
// 

// Modification history
//
// 09-01-02 MJW First version
// 14-01-02 MJW History logging
// 18-01-02 MJW adjustRate(), checks on EEPROM write,setup file
// 21-01-02 MJW frequency adjustment via SF command
// 23-02-02 MJW adjustRateSF() Fixed mistake where I forgot to take old SF into
//              into account when computing new SF
// 01-02-02 MJW Cleared status bits before executing any command which
//							writes to the EEPROM. Test for serial errors at startup.
// 19-02-02 MJW Fixed up failure to remove lockfile when port open failed
//              for whatever reason
// 02-05-02 MJW Added modification of cctf_header when phase/frequency is
//							changed. 
// 03-05-02 MJW Setup file format changed. Version change -> 1.1
// 15-08-02 MJW Changed default  location for the setup file. Removed 
//						  pps lock toggling from menu
//              Version change to 1.1.1
// 28-02-03 MJW Added command line option to step the 1 pps and option to report
//							whether PRS10 has lost power
//							Version change to 1.2.0
//
//
//
// 13-01-03 MJW Fixed bug in timestamp in log file. Wrong option (%G) used for
// 		year.
// 29-03-03 MJW Fixed for gripey C++ compiler Version -> 1.2.1
//
// 04-04-08 MJW Extended search paths for directories Version -> 1.2.2
// 25-02-09 MJW Configuration file searched for in multiple places/names
//							Version->1.2.3
// 
// 14-04-2014 MJW Compiler fixups
//							Version->1.2.4
//
// FIXME Bug ?? Saw an I/O timeout once on startup
// FIXME Should use libconfigurator one day 

#include <stdlib.h>

#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "prs10.h"

#define PRS10C_VERSION "Version 1.2.4"
#define PRS10C_HISTORY_FILE "prs10.history"
#define CCTF_HEADER         "cctf_header"

#define N_MENU_ITEMS 7

static string mjdstr();

static void printHelp();
static void printVersion();

enum menuIDs 
{
	MID_DUMMY,
	MID_SET_1PPS_PHASE,
	MID_ADJUST_RATE,
	MID_ADJUST_RATE_SF,
	MID_STATUS,
	MID_TEST_POINTS,
	MID_LIST_VARIABLES,
	MID_QUIT
};

const char* menuItems[] =
	{ 
		"",
		"Set 1 PPS phase",
		"Adjust rate via magnetic field offset (permanent)",
		"Adjust rate via SetFrequency command (non-permanent)",
		"Print status",
		"Print test point voltages",
		"Print variables",
		"Quit"
	};
	
const char *testStrings[]=
	{
		"", 
	 	"1.  Heater supply",
		"2.  Electronics supply ",
		"3.  Lamp FET drain voltage",
		"4.  Lamp FET gate voltage",
		"5.  Crystal heater control",
		"6.  Resonance cell heater control",
		"7.  Discharge lamp heater control",
	 	"8.  Amplified ac photosignal",
	 	"9.  Photocell I/V converter",
	 	"10. Case temperature (degrees C)",
	 	"11. Crystal thermistors",
	 	"12. Cell thermistors",
	 	"13. Lamp thermistors",
	 	"14. Frequency calibration pot/external calibration",
	 	"15. Analog ground",
	 	"16. 22.48 MHz VCXO varactor",
	 	"17. 360 MHz VCO varactor",
	 	"18. Gain control for amp driving frequency multiplier",
	 	"19. RF synthesizer lock indicator"
	 };


class EZPRS10
{
	public:
		
		EZPRS10();
		~EZPRS10();
		
		void run();
		
		void printStatus();
		void printTestVoltages();
		void printVariables();
	
		void step1pps(int);
		void checkReset();
		
		void setInteractive(bool i){interactive=i;}	
	private:
		
		void adjust1PPSphase();
		void setExternalLock();
		void adjustRate();
		void adjustRateSF();
		
		void printMenu();
		void doMenu(int menuID);
		void waitForKeyPress();
		
		bool checkEEPROMWrite();
		
		bool loadSetup(string);
		void logIt(string m);
		void updateCCTFComment();
	
		bool testDirectory(string);
		bool testFile(string);

		PRS10 p;
		bool quit;
		string historyFile;
		bool cctfComments;
		string home;
		bool interactive;

};

//
// Public members
//

EZPRS10::EZPRS10()
{
	char *hd;
	        
	if ((hd=getenv("HOME"))==NULL) // get our home directory
	{ 
		cerr << "Couldn't get $HOME" << endl;
		exit(-1);
	}
  	
	home = hd;
	historyFile = hd;

	if (testDirectory(historyFile+"/Log_Files"))
		historyFile+=string("/Log_Files/")+string(PRS10C_HISTORY_FILE);
	else if (testDirectory(historyFile+"/etc"))
		historyFile+=string("/logs/")+string(PRS10C_HISTORY_FILE);
	else
		cerr << "Warning! Couldn't find a directory for prs10.history" << endl;
	
	if (!loadSetup(hd)) exit(-1);
	
	quit=false;
	interactive=true;
	int nerr;
	if (!p.openPort())
	{
		cerr << p.getErrorMessage(&nerr);
		exit(-1);
	}
	
	// Attempt to verify serial comms before going too far
	p.command(PRS10::PRS10_SN);
	if (p.isErrorMessage())
	{
		cerr << "Hmmm. Attempted to get the serial number and this "
	  		"failed with the error : " << endl;
		cerr << p.getErrorMessage(&nerr) << endl;
		cerr << "Most likely the PRS10 is not talking." << endl;
		cerr << "You may wish to quit." << endl;
	}
}

EZPRS10::~EZPRS10()
{
	p.closePort();
}

void EZPRS10::run()
{
	int menuID;
	while (!quit)
	{
		printMenu();
		while(!(cin >> menuID))
		{
			cin.clear();
			cin.ignore(10000,'\n');
			cout << "Select (1.." << N_MENU_ITEMS << "): ";
		}
		
		doMenu(menuID);
	}
}

void EZPRS10::step1pps(int step)
{
	// Intended for command line adjustment of 1 pps epoch
	int nerr;
	if (!(p.command(PRS10::PRS10_PP,PRS10::PRS10_SET,step)))
		cout << p.getErrorMessage(&nerr);
	else
	{
		cout << "1 pps step command succeeded" << endl;
		ostringstream o;
		o << "1 pps stepped " << step << " ns";
		logIt(o.str());
		updateCCTFComment();
	}
}

void EZPRS10::checkReset()
{
	// Checks whether PRS10 has been reset by checking 
	// status bytes
	// Prints 'Unit reset' / 'Unit not reset'
	unsigned char buf[6];
	p.getStatus(buf);
	PRS10Settings *s = p.getSettings();
	
	// ST6 bit 7 flags a unit reset
	if (s->status[5] & 128)
	{
		cout << "Unit reset" << endl;
	}
	else
	{
		cout << "Unit not reset" << endl;
	}
}

//
// Private members
//

void EZPRS10::adjust1PPSphase()
{
	
	cout << "Input 1 pps step in ns (1 - 999999999) : ";
	int offset,nerr;
	while(!(cin >> offset))
	{
			cin.clear();
			cin.ignore(10000,'\n');
			cout << "(1 - 999999999) : ";
	}
	if (!(p.command(PRS10::PRS10_PP,PRS10::PRS10_SET,offset)))
		cout << p.getErrorMessage(&nerr);
	else
	{
		cout << "1 pps step command succeeded" << endl;
		ostringstream o;
		o << "1 pps stepped " << offset << " ns";
		logIt(o.str());
		updateCCTFComment();
	}
	
	// Note that the value cannot be queried or stored permanently
	
	p.clearErrorMessage();
}

void EZPRS10::setExternalLock()
{
	// Sets/unsets locking of PRS10 to an external 1 PPS
	unsigned char buf[6];
	p.getStatus(buf); // Clear the status buffer
	
	p.command(PRS10::PRS10_PL);
	cout << "External lock is ";
	PRS10Settings *s = p.getSettings();
	if (0==s->ppsPLLOn.value())
		cout << "OFF" << endl;
	else if (1==s->ppsPLLOn.value())
		cout << "ON" << endl;
	cout << "Toggle lock and write to EEPROM (Y/N) ? ";
	char r;
	while(!(cin >> r))
	{
			cin.clear();
			cin.ignore(10000,'\n');
			cout << "(Y/N)? : " << endl;
	}
	if (r == 'Y' || 'y' == r)
	{
		int newVal;
		if (0==s->ppsPLLOn.value())
			newVal=1;
		else if (1==s->ppsPLLOn.value())
			newVal=0;
		p.command(PRS10::PRS10_PL,PRS10::PRS10_SET,newVal);
		p.command(PRS10::PRS10_PL,PRS10::PRS10_WRITE_EEPROM,newVal);
		sleep(1); // wait for commands to execute before verifying
		
		// Now verify that the write to EEPROM was succesful
		if (!checkEEPROMWrite())
		{
			cout << endl << "EEPROM write failure !" << endl;
			return;
		}
		
		// Otherwise repeat current settings for user comfort
		p.command(PRS10::PRS10_PL);
		cout << "Changes OK!" << endl;
		cout << "External lock is currently ";
		if (0==s->ppsPLLOn.value())
			cout << "OFF" << endl;
		else if (1==s->ppsPLLOn.value())
			cout << "ON" << endl;
		p.command(PRS10::PRS10_PL,PRS10::PRS10_QUERY_EEPROM);
		cout << "Stored value is ";
		if (0==s->eeprom.ppsPLLOn.value())
			cout << "OFF" << endl;
		else if (1==s->eeprom.ppsPLLOn.value())
			cout << "ON" << endl;
		ostringstream o;
		o << "External locking is now ";
		if (0==s->ppsPLLOn.value())
			o << "OFF";
		else if (1==s->ppsPLLOn.value())
			o << "ON";
		logIt(o.str());
		
	}
	else
		cout << "External lock was not changed\n";
	
	waitForKeyPress();
}

void EZPRS10::adjustRate()
{
	// According to the PRS10 manual pg 32 the output frequency
	// changes
	
	unsigned char buf[6];
	p.getStatus(buf); // clear the status buffer
	
	cout << "NB this rate adjustment will permanently change the" << endl
			<< "value of the magnetic offset (MO) " << endl << endl;
	cout << "Input the required fractional frequency offset" << endl
			<< " (in units of 1 in 10^12, smallest change approx 5 in 10^12) : "
			<< endl;
	double ffo;
	
	while(!(cin >> ffo))
	{
			cin.clear();
			cin.ignore(10000,'\n');
			cout << "Input the required fractional frequency offset" << endl
					 << " (in units of 1 in 10^12, smallest change approx 5 in 10^12) : ";
	} 
	
	// Now need to test whether this is possible
	// First, get the old magnetic offset
	cout << "Getting current settings ..." << endl;
	p.getAllVariables();
	p.clearErrorMessage();
	PRS10Settings *s=p.getSettings();
	cout << "\tMO (magnetic field offset) = " << s->magneticOffset.value()<< endl;
	cout << "\tSS (slope calibration factor) = " << s->slopeCalibration.value()<< endl;
	int oldMagneticOffset = s->magneticOffset.value(); // save for logging
	
	double df=10.0E6*ffo*1.0E-12; // required frequency change
	// Use the SS value to calculate a scaling factor for the
	// magnetic field
	double sf=0.0;
	if (s->slopeCalibration.value() != 0)
	{
		sf = 4096.0*4096.0*1.0E-5/s->slopeCalibration.value();
		cout << "\tEstimated calibration factor is (nominal 0.08) " << sf << endl;
	}
	else
	{
		cout << "Uh oh! SS=0 ! " 
				" Can't estimate a scaling factor for the magnetic field." << endl;
		waitForKeyPress();
		return;
	}
	// Estimate the new MO
	double mo = sqrt(4096.0*4096.0*df/sf + oldMagneticOffset*oldMagneticOffset);
	// Check whether this is in the allowed range
	if ((mo < s->magneticOffset.minValue()) || (mo > s->magneticOffset.maxValue()))
	{
		cout << "Uh oh! The required magnetic field offset " << mo
				 << " is out of the range [" << s->magneticOffset.minValue() << ","
				 <<  s->magneticOffset.maxValue() << "]." << endl; 
		cout << "You will first need to make a coarse adjustment to the frequency "
				 << endl << "as detailed in the PRS10 manual." << endl;
		waitForKeyPress();
		return;
	}
	
	// Make the changes
	int nerr;
	if (!p.command(PRS10::PRS10_MO,PRS10::PRS10_SET,(int) mo))
	{
		cout << p.getErrorMessage(&nerr) << endl;
		p.clearErrorMessage();
		waitForKeyPress();
		return;
	}
	
	ostringstream o;
	o << "Current MO value changed from "<< oldMagneticOffset << " to " << (int) mo;		
	logIt(o.str());
	
	if (!p.command(PRS10::PRS10_MO,PRS10::PRS10_WRITE_EEPROM,(int) mo))
	{
		cout << p.getErrorMessage(&nerr) << endl;
		p.clearErrorMessage();
		waitForKeyPress();
		return;
	}
	
	sleep(1); // Zzzzz...
	
	if (!checkEEPROMWrite())
	{
			logIt("MO: EEPROM write failure");
			cout << endl << "EEPROM write failure !" << endl;
			waitForKeyPress();
			return;
	}
	
	ostringstream os;
	os << "MO = " << (int) mo << " written to EEPROM";		
	logIt(os.str());
	updateCCTFComment();
	
	cout << "Changes OK!" << endl;
	p.command(PRS10::PRS10_MO);
	p.command(PRS10::PRS10_MO,PRS10::PRS10_QUERY_EEPROM);
	s=p.getSettings();
	cout << "Current magnetic offset is " << s->magneticOffset.value() <<endl;
	cout << "Stored value is " << s->eeprom.magneticOffset.value() << endl;
	
	waitForKeyPress();
}


void EZPRS10::adjustRateSF()
{
	// This adjusts the rate using the SF commands
	cout << "NB. Changes made using this command will be lost when the " << endl 
			 << "power is cycled" << endl;
			 
	// If external lock is on then we can't do this
	p.command(PRS10::PRS10_PL);
	PRS10Settings *s = p.getSettings();
	p.command(PRS10::PRS10_SF);
	int oldfoffset=s->fOffset.value();
	if (1==s->ppsPLLOn.value())
	{
		cout << "Lock to external 1 PPS is ON - cannot do rate adjustment" << endl
				<<  "until the lock is disabled" << endl;
		return;
	}
	
	// Let's go on our merry way	
	double ffo;
	bool ok=false;
	int min=s->fOffset.minValue() - oldfoffset;
	int max=s->fOffset.maxValue() - oldfoffset;
	while (!ok)
	{
		cout << "Input the required fractional frequency offset" << endl
			 << " (in units of 1 in 10^12,"
			 << " min= " << min << " max= " << max << ") : ";
		while(!(cin >> ffo) )
		{
			cin.clear();
			cin.ignore(10000,'\n');
			cout << "Input the required fractional frequency offset" << endl
			 << " (in units of 1 in 10^12,"
			 << " min= " << min << " max= " << max << ") : ";
		} 
		if (!(s->fOffset.setValue((int) (ffo + oldfoffset))))
			cout << "Out of range!" << endl;
		else
			ok=true;
	}
	
	// Do it
	int nerr;
	if (!p.command(PRS10::PRS10_SF,PRS10::PRS10_SET,s->fOffset.value()))
	{
		cout << p.getErrorMessage(&nerr)<< endl;
		p.clearErrorMessage();
		waitForKeyPress();
		return;
	}
	
	// Query
	p.command(PRS10::PRS10_SF);
	cout << "SF previously " << oldfoffset << endl;
	cout << "SF currently " << s->fOffset.value() << endl;
	
	// Log it
	ostringstream os;
	os << "SF adjusted. Was  " << oldfoffset << ", now " << s->fOffset.value();		
	logIt(os.str());
	updateCCTFComment();	
	waitForKeyPress();
	
}

void EZPRS10::printStatus()
{	
	string status;
	unsigned char buf[6];
	
	p.getStatus(buf);
	PRS10Settings *s = p.getSettings();
	
	// Power supply and discharge lamp status
	
	if (s->status[0] & 1)
		status="< 22V";
	else if(s->status[0] & 2)
		status="> 30V";
	else
		status="OK";
	cout << "24V P/S for electronics: " << status << endl;
	
	if (s->status[0] & 4)
		status="< 22V";
	else if(s->status[0] & 8)
		status="> 30V";
	else
		status="OK";
	cout << "24V P/S for heaters: " << status << endl;
	
	if (s->status[0] & 16)
		status="LOW";
	else if(s->status[0] & 32)
		status="HIGH";
	else
		status="OK";
	cout << "Lamp light level: " << status << endl;
	
	if (s->status[0] & 64)
		status="LOW";
	else if(s->status[0] & 128)
		status="HIGH";
	else
		status="OK";
	cout << "Gate voltage: " << status << endl;
	
	/* RF synthesizer status */
	if (s->status[1] & 1)
		status="UNLOCKED";
	else
		status="OK";
	cout << "RF synthesizer PLL: " << status << endl;
	
	if (s->status[1] & 2)
		status="LOW";
	else if(s->status[1] & 4)
		status="HIGH";
	else
		status="OK";
	cout << "RF crystal varactor: " << status << endl;
	
	if (s->status[1] & 8)
		status="LOW";
	else if(s->status[1] & 16)
		status="HIGH";
	else
		status="OK";
	cout << "RF VCO control: " << status << endl;
	
	if (s->status[1] & 32)
		status="LOW";
	else if(s->status[1] & 64)
		status="HIGH";
	else
		status="OK";
	cout << "RF AGC control: " << status << endl;
	
	if (s->status[1] & 128)
		status="BAD";
	else
		status="OK";
	cout << "PLL parameter: " << status << endl;
	
	// Temperature controllers 
	
	if (s->status[2] & 1 )
		status="LOW";
	else if(s->status[2] & 2)
		status="HIGH";
	else
		status="OK";
	cout << "Lamp temperature: " << status << endl;
	
	if (s->status[2] & 4 )
		status="LOW";
	else if(s->status[2] & 8 )
		status="HIGH";
	else
		status="OK";
	cout << "Crystal temperature: " << status << endl;
	
	if (s->status[2] & 16)
		status="LOW";
	else if(s->status[2] & 32)
		status="HIGH";
	else
		status="OK";
	cout << "Cell temperature: " << status << endl;
	
	if (s->status[2] & 64)
		status="LOW";
	else if(s->status[2] & 128)
		status="HIGH";
	else
		status="OK";
	cout << "Case temperature: " << status << endl;
	
	// Frequency lock-loop control 
	
	if (s->status[3] & 1)
		cout << "Frequency lock control is off" << endl;
	
	if (s->status[3] & 2)
		cout << "Frequency lock is disabled" << endl;
		
	if (s->status[3] & 4)
		status="HIGH";
	else if(s->status[3] & 8)
		status="LOW";
	else
		status="OK";
	cout << "10 MHz EFC: " << status << endl;
	
	if (s->status[3] & 16)
		status="> 4.9V";
	else if(s->status[3] & 32)
		status="< 0.1V";
	else
		status="OK";
	cout << "Analog cal voltage: " << status << endl;
	
	// Frequency lock to external 1 PPS 
	if (s->status[4] & 1)
		cout << "Frequency lock to external 1 pps: PLL disabled" << endl;
	else if (s->status[4] & 4)
		cout << "Frequency lock to external 1 pps: PLL active" << endl;
		
	if (s->status[4] & 2)
		cout << "Frequency lock to external 1 pps: < 256 good 1 pps inputs" << endl;
			
	if (s->status[4] & 8)
		cout << "Frequency lock to external 1 pps: > 256 bad 1 pps inputs" << endl;
		
	if (s->status[4] & 16)
		cout << "Frequency lock to external 1 pps: excessive time interval" << endl;
		
	if (s->status[4] & 32)
		cout << "Frequency lock to external 1 pps: PLL restarted" << endl;
		
	if (s->status[4] & 64)
		cout << "Frequency lock to external 1 pps: f control saturated" << endl;
		
	if (s->status[4] & 128)
		cout << "Frequency lock to external 1 pps: no 1 PPS input" << endl;
				
	// System level events 
	if (s->status[5] & 1)
		cout << "System event: lamp restart" << endl;
	
	if (s->status[5] & 2)
		cout << "System event: watchdog timeout and reset" << endl;
	
	if (s->status[5] & 4)
		cout << "System event: bad interrupt vector" << endl;
	
	if (s->status[5] & 8)
		cout << "System event: EEPROM write failure" << endl;
	
	if (s->status[5] & 16)
		cout << "System event: EEPROM data corruption" << endl;
	
	if (s->status[5] & 32)
		cout << "System event: bad command syntax" << endl;
	
	if (s->status[5] & 64)
		cout << "System event: bad command parameter" << endl;
	
	if (s->status[5] & 128)
		cout << "System event: unit has been reset" << endl;
		
	p.clearErrorMessage();

	if (interactive) waitForKeyPress();
}

void EZPRS10::printTestVoltages()
{
	float fbuf[20];
	p.getTestVoltages(fbuf);
	for (int i=1;i<=19;i++)
		cout << testStrings[i] << " " << fbuf[i] << endl;
	
	cout << "Note: scaling factors for these voltages have been applied"
			<< endl;
	cout << "as described in the PRS10 manual." << endl;
	p.clearErrorMessage();
	
        if (interactive) waitForKeyPress();
}


void EZPRS10::printVariables()
{
	p.getAllVariables();
	PRS10Settings *s=p.getSettings();
			
	cout << "-- Initialization" << endl;
	cout << "VB Verbose mode " << s->verboseMode.value() << endl;
	cout << "SN Serial number " << s->serialNumber << endl;
	cout << "LM Lock mode pin config "
			<< s->lockModePinConfiguration.value() << endl;
			
	cout << "-- Frequency lock loop parameters " << endl;
	cout << "LO lock on " << s->fLockOn.value()<< endl;
	cout << "FC f control high " << s->fControlHigh.value()<< endl;
	cout << "FC f control low " << s->fControlLow.value()<< endl;
	// ?? EEPROM values
	cout << "DS error signal " << s->errorSignal<< endl;
	cout << "DS signal strength " << s->signalStrength<< endl;
	cout << "SF f offset " << s->fOffset.value()<< endl;
	cout << "SS slope calibration factor " <<
		s->slopeCalibration.value()<< endl;
	cout << "GA gain " << s->gainParameter.value()<< endl;
	cout << "PH phase " << s->phase.value()<< endl;

	cout << "-- Frequency synthesizer control " << endl;
	cout << "SP R parameter " << s->fsR.value()<< endl;
	cout << "SP N parameter " << s->fsN.value()<< endl;
	cout << "SP A parameter " << s->fsA.value()<< endl;

	cout << "-- Magnetic field control " << endl;
	cout << "MS magnetic field " << s->magneticSwitching.value()<< endl;
	cout << "MO offset " << s->magneticOffset.value()<< endl;
	cout << "MR magnetic reading " << s->magneticRead.value()<< endl;

	if (interactive) waitForKeyPress();

	cout << "-- 1 PPS control" << endl;
	cout << "TT time tag (ns) " << s->timeTag.value()<< endl;
	cout << "TS time slope " << s->timeSlope.value()<< endl;
	cout << "TO time offset (ns) " << s->timeOffset.value()<< endl;
	//cout << "PP pulse offset (ns) " << s->ppsOffset.value()<< endl;
	cout << "PP pulse offset (ns) N/A" << endl;
	cout << "PS pulse slope calibration " << s->ppsSlopeCalibration.value()<< endl;

	cout << "-- 1 PPS locking control " << endl;
	cout << "PL PLL on " << s->ppsPLLOn.value()<< endl;
	cout << "PT PLL time constant " << s->ppsPLLTimeConstant.value()<< endl;
	cout << "PF PLL stability factor " << s->ppsPLLStabilityFactor.value()<< endl;
	cout << "PI PLL integrator " << s->ppsPLLIntegrator.value()<< endl;

	cout << "-- Analog control" << endl;

	cout << "SD0 RF amplitude " << s->DACSettings[0]<< endl;
	cout << "SD1 analog portion of 1 pps delay " << s->DACSettings[1]<< endl;
	cout << "SD2 discharge lamp FET oscillator drain voltage " << s->DACSettings[2]<< endl;
	cout << "SD3 discharge lamp temperature " << s->DACSettings[3]<< endl;
	cout << "SD4 10 MHz crystal temperature " << s->DACSettings[4]<< endl;
	cout << "SD5 Resonance cell temperature " << s->DACSettings[5]<< endl;
	cout << "SD6 10 MHz oscillator amplitude " << s->DACSettings[6]<< endl;
	cout << "SD7 RF phase modulation peak amplitude " << s->DACSettings[7]<< endl;
	
	p.clearErrorMessage();
	if (interactive) waitForKeyPress();
}

void EZPRS10::printMenu()
{
	cout << endl;
	cout << "-- PRS10 control" << endl;
	for (int i=1; i<=N_MENU_ITEMS;i++)
		cout << i << ". " << menuItems[i] << endl;
	
	cout << endl;
	cout << "Select (1.." << N_MENU_ITEMS << "): ";
}


void EZPRS10::doMenu(int menuID)
{
 switch (menuID)
 {
 	case MID_SET_1PPS_PHASE:
		adjust1PPSphase();
		break;
	case MID_ADJUST_RATE:
		adjustRate();
		break;
	case MID_ADJUST_RATE_SF:
		adjustRateSF();
		break;
	case MID_STATUS:
		printStatus();
		break;
	case MID_TEST_POINTS:
		printTestVoltages();
		break;
	case MID_LIST_VARIABLES:
		printVariables();
		break;
	case MID_QUIT:
		quit=true;
		break;
	default: 
		cout << menuID << " is not a valid option" << endl << endl;
 }
}

bool EZPRS10::checkEEPROMWrite()
{
	unsigned char buf[6];
	p.getStatus(buf);
	return !(buf[5] & 8);
}

void EZPRS10::logIt(string m)
{

	char buf[256];

	ofstream out(historyFile.c_str(),ios::app);
	if (!out)
	{
		cerr << "Couldn't open the history file " << historyFile << endl;
		return;
	}

	time_t t=time(NULL);
	struct tm *tp=gmtime(&t);
	strftime(buf,256,"%Y-%m-%d %T ",tp);
	out << buf << m << endl;
	out.close(); // done by destructor anyway
}

bool EZPRS10::loadSetup(string hd)
{
	
	const char *configFile[8] =
	{
		"/prs10.conf",
		"/etc/prs10.conf",
		"/Parameter_Files/prs10.conf",
		"/utilities/prs10.conf",
		"/prs10.setup",
		"/etc/prs10.setup",
		"/Parameter_Files/prs10.setup",
		"/utilities/prs10.setup",
	};

	string path;
	for (int i=0;i<8;i++)
	{
		path = hd + configFile[i];
		if (testFile(path))
			break;
	}
	
	ifstream in(path.c_str()); // this will fail if we didn't find it
	if (!in)
	{
		// try prs10.setup as well
		
		cerr << "Couldn't open the configuration file "<< endl;
		return false;
	}

	bool result=true;
	string sarg,token;
	while (!in.eof())
	{
		in >> token;
		if (token == "PORT")
		{
			in >> sarg;
			p.setPort(sarg);
		}
		else if (token == "CCTF_COMMENTS")
		{
			in >> sarg;
			if (sarg == "yes")
				cctfComments=true;
			else if (sarg == "no")
				cctfComments=false;
			else
			{
				cerr << "Unrecognised argument to token " <<
					token << " in setup file " <<
					path << endl;
				result = false;
			}
		}
		else 
		{
			cerr << "Unrecognised token " << token << endl;
		}
	}
	in.close();
	return result;
}

void EZPRS10::waitForKeyPress()
{
	cout << "<Press ENTER to continue>" << endl;
	char ch;
	cin.ignore(10000,'\n');
	cin.get(ch);
}

void EZPRS10::updateCCTFComment()
{
	// the cctf_header COMMENT line contains the dates of the
	// two most recent steers. Two steers are kept to guard against
	// the following situation : if a steer is made on two successive
	// days, with the second steer performed before processing of the
	// previous day's data, then information about the first steer
	// will be lost.
	// Tests performed
	// (1) Appends to empty comment line
	// (2) Appends to comment line with a comment
	// (3) When Steer comment is present
	//		(i) doesn't change line if Steer for present MJD
	//	    is present.

	if (cctfComments) // pedantic
	{
		cout << "Updating cctf_header_comments" << endl;
		string sf(home);
		sf += "/";
		sf += CCTF_HEADER;
		
		if (testDirectory(home+"/etc"))
			sf=home+string("/etc/")+string(CCTF_HEADER);
		else if (testDirectory(home+"/Parameter_Files"))
			sf=home+string("/Parameter_Files/")+string(CCTF_HEADER);
			
		// Does  cctf_header exist ?
		ifstream in(sf.c_str());
		if (!in)
		{
			cerr << "Error! Couldn't open the CCTF header file "
				<< sf << endl;
			return;
		}
		// First, we need to get the existing COMMENT line
		// and see what's in it
		char line[1024];
		string buf;
		while (in.getline(line,1024,'\n') && !in.eof())
		{	
			//in.getline(line,1024,'\n');
			// Have we got the comment line ??
			if (strstr(line,"COMMENTS"))
			{
				// This needs to be parsed. In particular
				// we want to leave any comment which is
				// not related to the steer. So search for
				// the keyword Steered. If it's not there,
				// this is the first time and we just append
				// Otherwise we have to check the MJDs after
				// the steer to see what should be changed
				char *ps = strstr(line,"Steered");
				if (!ps)
				{
					// No steer yet so just append
					strcat(line," Steered " );
					strcat(line, mjdstr().c_str()); 
				}
				else
				{
					// This is trickier
					// Only add an MJD if it is not
					// already logged
					if (!strstr(ps,mjdstr().c_str()))
					{
						// There are either 1 or
						// 2 MJDs logged
						int mjd1,mjd2;
						int nargs = sscanf(ps,"%*s %i %i",&mjd1,&mjd2);
						if (nargs==1)
						{
							strcat(line," ");
							strcat(line,mjdstr().c_str());
						}
						else if (nargs==2)
						{
							char buf[16];
							sprintf(buf,"%i",mjd2);
							ps += 8;
							strcpy(ps,buf);
							strcat(line," ");
							strcat(line,mjdstr().c_str());
						}
					}
					
				}
			
			}		
			// getline removes terminator ...
			buf += line;
			buf += "\n";
		} // while()
		in.close();
		ofstream out(sf.c_str());
		out << buf;
		out.close();
			
	}
}

bool EZPRS10::testDirectory(string d)
{
	struct stat statbuf;
	if (0==stat(d.c_str(),&statbuf))
	{
		return S_ISDIR(statbuf.st_mode);
	}
	return false;
}

bool EZPRS10::testFile(string f)
{
	struct stat statbuf;
	if (0==stat(f.c_str(),&statbuf))
	{
		return S_ISREG(statbuf.st_mode);
	}
	return false;
}

static string mjdstr()
{
	int mjd = time(NULL)/86400+40587;
	ostringstream os;
	os << mjd ;
	return os.str();
}

//
// Stuff for non-interactive use
//

static void printHelp()
{
	printVersion();
	cout << "Command line options" << endl;
	cout << "-h     print this help" << endl;
	cout << "-o     adjust 1 pps" << endl;
	cout << "-q     query all variables" << endl;
	cout << "-t     print test point voltages" << endl;
	cout << "-s     print status" << endl;
	//cout << "-r x   adjust rate by x" << endl;
	cout << "-v     print version" << endl;
}

static void printVersion()
{
	cout << "PRS10C " << PRS10C_VERSION << endl;
}


int main(int argc,char **argv)
{
	
	EZPRS10 prsc;
	
	// Process any command line arguments
	char c;
	if (1 == argc) // no arguments so interactive mode
		prsc.run();
	else
	{
		prsc.setInteractive(false);
		while ((c=getopt(argc,argv,"hvo:tsqx")) != EOF)
		{
			switch(c)
    			{
				case 'h':printHelp();break;
      	case 'v':printVersion();break;
				case 'o':prsc.step1pps(atoi(optarg));break;
				case 't':prsc.printTestVoltages();break;
				case 's':prsc.printStatus();break;
				case 'q':prsc.printVariables();break;
				case 'x':prsc.checkReset();break;
			}
		}
	}	
	return 0;
}





