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

#ifndef __PRS10_H_
#define __PRS10_H_

#include <string>
#include <termios.h>
#include <signal.h>

#define LOCKPORT "/usr/local/bin/lockport "

using namespace std;

//
// Integer range control
//

class IntRange
{
	public:
	
		IntRange(){;}
		IntRange(int value,int minValue,int MaxValue);
	
		bool setValue(int value);
		bool setMinMax(int minValue,int maxValue);
		
		int value(){return val;}
		int minValue(){return min;}
		int maxValue(){return max;}
				
	private:
	
		int val,min,max; 
};

//
// Holds PRS10 EEPROM settings
//

struct EEPROMSettings
{
	int serialNumber;
	IntRange lockModePinConfiguration;
	IntRange fControlHigh,fControlLow;
	int nPowerCycles,nfWrites;
	IntRange slopeCalibration;
	IntRange magneticOffset;
	IntRange timeSlope;
	IntRange timeOffset;
	IntRange ppsPLLOn;
};

//
// Use this class for passing data around - too many for the usual
// method of using member function(s) for access to be neat
//

class PRS10Settings
{
	public:
	
	PRS10Settings();
	
	IntRange verboseMode;   /* VB */
	string IDString; /* ID */
	int serialNumber; 
	unsigned char status[6]; /* ST  */
	IntRange lockModePinConfiguration; /* LM */
	IntRange fLockOn; /* LO */
	IntRange fControlHigh,fControlLow; /* FC */
	
	int errorSignal,signalStrength; /* DS */
	IntRange fOffset; /* SF */
	IntRange slopeCalibration; /* SS */
	IntRange gainParameter; /* GA */
	IntRange phase;/* PH */
	IntRange fsR,fsN,fsA; /* SP */
	IntRange magneticSwitching; /* MS */
	IntRange magneticOffset; /* MO */
	IntRange magneticRead; /* MR */
	IntRange timeTag; /* TT */
	IntRange timeSlope; /* TS */
	IntRange timeOffset; /* TO */
	IntRange ppsOffset; /* PP */
	IntRange ppsSlopeCalibration; /*PS */
	IntRange ppsPLLOn; /* PL */
	IntRange ppsPLLTimeConstant; /* PT */
	IntRange ppsPLLStabilityFactor; /* PF */
	IntRange ppsPLLIntegrator; /* PI */
	int DACSettings[8]; /* SD */
	float testVoltages[20]; /* AD - note these are raw */
	
	EEPROMSettings eeprom;
	
};

//
// Main class for interfacing to the PRS10
//

class PRS10
{
	
	public:
	
		PRS10();
		~PRS10();
		
		void setPort(string p){port = p;}
		bool openPort();
		bool closePort();
		bool writeString(const char *msg);
		bool readString(char *buf,int bufSize);
		bool query(const char *cmdString,int *i);
		bool query(const char *cmdString,float *f);
		bool query(const char *cmdString,int i[],int n);
		bool query(const char *cmdString,float f[],int n);
		bool query(const char *cmdString,string *s);
		bool command(int cmd,int cmdModifier=PRS10_QUERY,int NewValue=-1,int chan=-1);
		
		bool getAllVariables();
		bool getTestVoltages(float tv[20]);
		bool getStatus(unsigned char status[6]);
		
		string getErrorMessage(int *numErrors);
		bool   isErrorMessage(){return isErr;}
		void   clearErrorMessage();
	
		void setDebugging(bool d){debug=d;}
		PRS10Settings *getSettings(){return &s;}
		
		enum PRS10CommandModifiers
		{
			PRS10_QUERY,
			PRS10_SET,
			PRS10_QUERY_EEPROM,
			PRS10_WRITE_EEPROM,
			PRS10_NO_MOD
		};

		enum PRS10CommandCodes
		{ 
			PRS10_RS, /* initialization */
 			PRS10_VB,
 			PRS10_ID,
 			PRS10_SN,
 			PRS10_ST,
 			PRS10_LM,
 			PRS10_RC,
 			PRS10_LO, /* frequency lock loop parameters */
 			PRS10_FC,
 			PRS10_DS,
 			PRS10_SF,
 			PRS10_SS,
 			PRS10_GA,
 			PRS10_PH,
 			PRS10_SP, /* frequency synthesizer control */
 			PRS10_MS, /* magnetic field control */
 			PRS10_MO,
 			PRS10_MR,
 			PRS10_TT, /* 1 PPS control */
 			PRS10_TS,
 			PRS10_TO,
 			PRS10_PP,
 			PRS10_PS, 
 			PRS10_PL, /* 1 PPS locking control */
 			PRS10_PT,
 			PRS10_PF,
 			PRS10_PI,
 			PRS10_SD, /* analog control */
 			PRS10_AD, /* analog test voltages */
		};
		
	private:
	
		void error(string errmsg);
		string errBuf;
		bool isErr;
		int nErr;
		
		bool writeEEPROM(const char *cmd);
		bool setIntParam(IntRange *r,const char *cmd,int v);
		bool getIntParam(IntRange *r,const char *cmd);
		
		
		void startTimer(long);
		void stopTimer();
		static void signalHandler(int); // must be static so that we pass
																		// the right kind of pointer
		
		string port; /* serial port */
		int fd;     /* file descriptor for the port */
		struct termios oldtio; /* for restoring terminal setings */
	
		PRS10Settings s;
		bool debug;
		
		struct sigaction sa;
    

};

#endif


