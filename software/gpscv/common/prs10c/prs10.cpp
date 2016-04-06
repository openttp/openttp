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
// Modification history
//
// 07-02-2000 MJW Started writing this program
// 26-02-2000 MJW Still working on this ...
// 07-01-2002 MJW Hmm rather a long time before this software saw use
//                Tidy up for production use on ADF system.
// 09-01-2002 MJW Port locking
// 10-01-2002 MJW writeEEPROM, C++ification
// 14-04-2014 MJW Fixups to make the compiler happy

#include <cstring>
#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#include <string>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>

#include "prs10.h"

#define BAUDRATE B9600
#define DEFAULT_PORT "/dev/prs10"

// FIXME what is the right syntax for initializing this in a class def
static const float scaleFactors[20]=
			{1.0,10.0,10.0,10.0,10.0,
			 1.0,1.0,1.0,1.0,4.0,
			 100.0,1.0,1.0,1.0,1.0,
			 1.0,4.0,4.0,4.0,1.0}; 

		 
// 
// IntRange - an integer range control
//



IntRange::IntRange(int value,int minValue,int maxValue)
{
	
	setMinMax(minValue,maxValue);
	if (value < minValue) 
		val=minValue;
	else if (value < maxValue)
		val=maxValue;
	else
		val=value;
		
}


bool IntRange::setValue(int value)
{
	if (value >= min && value <= max)
	{
		val=value;
		return true;
	}
	else
		return false; 
}

bool IntRange::setMinMax(int minValue,int maxValue)
{
	if (maxValue < minValue)
	{
		min=maxValue;
		max=minValue;
	}
	else
	{
		min=minValue;
		max=maxValue;
	}
	return true;
}
		
//
// PRS10Settings - holds all PRS10 settings
//

PRS10Settings::PRS10Settings()
{
	verboseMode.setMinMax(0,1);
	lockModePinConfiguration.setMinMax(0,3); 
	fLockOn.setMinMax(0,1); 
	fControlHigh.setMinMax(0,4095),
	fControlLow.setMinMax(1024,3072); 
	fOffset.setMinMax(-2000,2000); 
	slopeCalibration.setMinMax(1000,1900); 
	gainParameter.setMinMax(0,10); 
	phase.setMinMax(0,31);
	fsR.setMinMax(1500,8191);
	fsN.setMinMax(800,4095);
	fsA.setMinMax(0,63); 
	magneticSwitching.setMinMax(0,1); 
	magneticOffset.setMinMax(2300,3600); 
	magneticRead.setMinMax(1000,4095); 
	timeTag.setMinMax(-1,999999999); 
	timeSlope.setMinMax(7000,25000); 
	timeOffset.setMinMax(-32767,32768); 
	ppsOffset.setMinMax(0,999999999);
	ppsSlopeCalibration.setMinMax(100,255); 
	ppsPLLOn.setMinMax(0,1); 
	ppsPLLTimeConstant.setMinMax(0,14); 
	ppsPLLStabilityFactor.setMinMax(0,4); 
	ppsPLLIntegrator.setMinMax(-2000,2000); 
	for (int i=0;i<20;i++)
	  testVoltages[i]=-1.0;
		
	// EEPROM settings
	// FIXME incomplete
	eeprom.lockModePinConfiguration.setMinMax(0,3); 
	eeprom.fControlHigh.setMinMax(0,4095),
	eeprom.fControlLow.setMinMax(1024,3072); 
	eeprom.slopeCalibration.setMinMax(1000,1900);
	eeprom.magneticOffset.setMinMax(2300,3600);
	eeprom.timeSlope.setMinMax(7000,25000); 
	eeprom.timeOffset.setMinMax(-32767,32768);
	eeprom.ppsPLLOn.setMinMax(0,1); 
}


//
// PRS10 - class for controlling the PRS10
//

PRS10::PRS10()
{
	port=DEFAULT_PORT;
	debug=false;

	// SIGALRM is used to detect IO timeouts
	//sa.sa_handler = signalHandler;
	//sigemptyset(&(sa.sa_mask)); // we block nothing 
	//sa.sa_flags=0;
                                
	//sigaction(SIGALRM,&sa,NULL); 

	nErr=0;
	isErr=false;
	errBuf="";
	fd=0;
}

PRS10::~PRS10()
{
	closePort();
}
		
		
bool PRS10::openPort()
{
		
	struct termios newtio;
	int result;
	
	// Try to lock the port
	string buf = LOCKPORT + port + " prs10c";
	FILE *p=popen(buf.c_str(),"r");
        
	if (fscanf(p,"%i",&result) != 1) 
  {
		error("Failed to check the serial port lock file\n");
		fclose(p);
		buf = LOCKPORT;// remove the lockfile
		buf += "-r " + port + " > /dev/null";
 		system(buf.c_str());
		return false;
	}

	if (0==result) 
  {
		error("The serial port may be in use. Check /var/lock/\n");
		fclose(p);
		buf = LOCKPORT;// remove the lockfile
		buf += "-r " + port + " > /dev/null";
 		system(buf.c_str());
		return false;
	} 
	
  // Open serial port for reading and writing and not as controlling tty
 	// because we don't want to get killed if linenoise sends CTRL-C.
	
	if ((fd = open(port.c_str(), O_RDWR | O_NOCTTY ))<0)
	{
		perror(port.c_str());
		buf = LOCKPORT;// remove the lockfile
		buf += "-r " + port + " > /dev/null";
 		system(buf.c_str());
		exit(-1);
	}

	tcgetattr(fd,&oldtio);  // save current serial port settings 
	bzero(&newtio, sizeof(newtio)); // clear struct for new port settings 
	
	
	// BAUDRATE: Set bps rate.
  // CS8     : 8n1 (8bit,no parity,1 stopbit)
  // CLOCAL  : local connection, no modem contol
  // CREAD   : enable receiving characters
	//
	 
	newtio.c_cflag = BAUDRATE  | CS8 | CLOCAL | CREAD;
 
  // IGNPAR  : ignore bytes with parity errors
  // ICRNL   : map CR to NL (otherwise a CR input on the other computer
  //             will not terminate input)
  // otherwise make device raw (no other input processing)
	
	newtio.c_iflag = IGNPAR | ICRNL;
 
	newtio.c_oflag = 0; // raw output

  // ICANON  : enable canonical input
  // disable all echo functionality, and don't send signals to calling program

	newtio.c_lflag = ICANON;
 
  // Initialize all control characters 
  // Default values can be found in /usr/include/termios.h, and are given
  // in the comments, but we don't need them here
	
	newtio.c_cc[VINTR]    = 0;     // Ctrl-c  
	newtio.c_cc[VQUIT]    = 0;     // Ctrl-backslash
	newtio.c_cc[VERASE]   = 0;     // del 
	newtio.c_cc[VKILL]    = 0;     // @ 
	newtio.c_cc[VEOF]     = 4;     // Ctrl-d 
	newtio.c_cc[VTIME]    = 0;     // inter-character timer unused 
	newtio.c_cc[VMIN]     = 1;     // blocking read until 1 character arrives 
	newtio.c_cc[VSWTC]    = 0;     // '\0' 
	newtio.c_cc[VSTART]   = 0;     // Ctrl-q 
	newtio.c_cc[VSTOP]    = 0;     // Ctrl-s 
	newtio.c_cc[VSUSP]    = 0;     // Ctrl-z 
	newtio.c_cc[VEOL]     = 0;     // '\0'
	newtio.c_cc[VREPRINT] = 0;     // Ctrl-r 
	newtio.c_cc[VDISCARD] = 0;     // Ctrl-u 
	newtio.c_cc[VWERASE]  = 0;     // Ctrl-w 
	newtio.c_cc[VLNEXT]   = 0;     // Ctrl-v 
	newtio.c_cc[VEOL2]    = 0;     // '\0' 

	// Now clean the modem line and activate the settings for the port
	
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd,TCSANOW,&newtio);
	
	return true;
}

bool PRS10::closePort()
{
	if (0==fd) return false;
	// Remove the lock file
	string buf = LOCKPORT;// FIXME
	buf += "-r " + port + " > /dev/null";
  system(buf.c_str());

	tcsetattr(fd,TCSANOW,&oldtio); // restore old terminal settings 
	close(fd);
	fd=0;
	return true;
}

bool PRS10::writeString(const char *msg)
{
	// This is a low level command which sends a string to the PRS10
	// FIXME should use a timer here to make sure that we don't get hung
	size_t  count;
	ssize_t nWritten;
	char *msgp;
	
	#ifdef DEBUG
		if (debug) fprintf(stderr,"writeString():%s\n",msg);
	#endif
	
	count = strlen(msg);
	msgp=(char *) msg;
	//startTimer(100000);
	while (count >0){
		if ((nWritten = write(fd,msgp,count)) > -1)
		{
			msgp += nWritten;
			count -= nWritten;
		}
	}
	//stopTimer();
	return true;
}

bool PRS10::readString(char *buf,int bufSize)
{
	// This is a low level command which reads a string from the PRS10
	// The string is expected to be terminated with a \n
	// as for canonical input
	
	// FIXME should use a timer here to make sure that we don't get hung
	
	ssize_t nRead;
	
	// To deal with I/O timeouts we use select() to test whether the descriptor
	// is ready for reading
	struct timeval tv;
	tv.tv_sec=0;
	tv.tv_usec=100000; // 100 ms is enough
	fd_set readset; // only want to read
	FD_ZERO(&readset);
	FD_SET(fd,&readset);
	int res;
	if ((res=select(fd+1,&readset,NULL,NULL,&tv)) > 0)
	{
		// No need to test whether fd is set since only 1 fd in set
		nRead = read(fd,buf,bufSize);
		buf[nRead]=0; // terminate that string 
	}
	else if (res==-1)
		error("I/O error");
	else if (res==0)
		error("I/O timeout");
	#ifdef DEBUG
		if (debug) fprintf(stderr,"readString():%s\n",buf);
	#endif
	
	return (res>0);
}

bool PRS10::query(const char *cmdString,int *i)
{
	// For when we expect an integer back
	char buf[255];
	int bufSize=255;
	int ibuf;
	
	if (writeString(cmdString))
	{
		if (readString(buf,bufSize))
		{
			if (sscanf(buf,"%i",&ibuf)==1)
			{
				*i=ibuf;
				return true;
			}
		}
	}
	return false;
}

bool PRS10::query(const char *cmdString,float *f)
{
	// For when we expect a float back
	char buf[255];
	int bufSize=255;
	float fbuf;
	
	if (writeString(cmdString))
	{
		if (readString(buf,bufSize))
		{
			if (sscanf(buf,"%f",&fbuf)==1)
			{
				*f=fbuf;
				return true;
			}
		}
	}
	return false;
}

bool PRS10::query(const char *cmdString,string *s)
{
	// For when we expect a string back
	char buf[255];
	int bufSize=255;
	
	if (writeString(cmdString))
	{
		if (readString(buf,bufSize))
		{
			*s=buf;
			return true;
		}
	}
	return false;
}

bool PRS10::query(const char *cmdString,int i[],int n)
{
	// For when we expect a comma separated list of integers 
	char buf[255];
	int bufSize=255;
	char *p;
	if (writeString(cmdString))
	{
		if (readString(buf,bufSize))
		{
			p=strtok(buf,",");
			int cnt=0;
			while (p && cnt<n)
			{
				sscanf(p,"%i",&(i[cnt++]));
				p=strtok(NULL,",");
			}
			return true;
		}
	}

	return false;
}

bool PRS10::query(const char *cmdString,float f[],int n)
{
	// For when we expect a comma separated list of floats 
	char buf[255];
	int bufSize=255;
	char *p;
	if (writeString(cmdString))
	{
		if (readString(buf,bufSize))
		{
			p=strtok(buf,",");
			int cnt=0;
			while (p && cnt<n)
			{
				sscanf(p,"%f",&(f[cnt++]));
				p=strtok(NULL,",");
			}
			return true;
		}
	}
	return false;
}

bool PRS10::command(int cmd,int cmdModifier,int NewValue,int chan)
{
	// chan is only used by the SD and AD commands
	
	bool res;
	char msgbuf[256];
	int i,itmp,ivtmp[8];
	float ftmp;
	string stmp;
	
	switch (cmd)
	{
		case PRS10_RS:
			if (!(res=writeString("RS 1\r")))
				error("RS 1 failed");
			break;
		case PRS10_VB:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.verboseMode),"VB");break;
				case PRS10_SET:res=setIntParam(&(s.verboseMode),"VB",NewValue);break;
			}
			break;
		case PRS10_ID:
			if ((res=query("ID?\r",&stmp)))
				s.IDString=stmp;
			else
				error("ID? failed");
			break;
		case PRS10_SN:
			if ((res = query("SN?\r",&itmp)))
				s.serialNumber=itmp;
			else
				error("SN failed");
			break;
		case PRS10_ST:
			if ((res=query("ST?\r",ivtmp,6)))
			{
				for (i=0;i<6;i++)
					s.status[i]=ivtmp[i];	
			}
			else
				error("ST? failed");
			break;
		case PRS10_LM:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.lockModePinConfiguration),"LM");break;
				case PRS10_SET:res=setIntParam(&(s.lockModePinConfiguration),"LM",NewValue);break;
				case PRS10_QUERY_EEPROM:break;
				case PRS10_WRITE_EEPROM:break;
			}
			break;
		case PRS10_RC:
			if (!(res=writeString("RC 1\r")))
				error("RC 1 failed");
			break;
		case PRS10_LO:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.fLockOn),"LO");break;
				case PRS10_SET:res=setIntParam(&(s.fLockOn),"LO",NewValue);break;
			}
			break;
		case PRS10_FC:
			switch (cmdModifier)
			{
				case PRS10_QUERY:
					if ((res=query("FC?\r",ivtmp,2)))
					{
						res=s.fControlHigh.setValue(ivtmp[0]);
						res=res && s.fControlLow.setValue(ivtmp[1]);
						if (!res) error("FC? out of range error");	
					}
					else
						error("FC? failed");
					break;
				case PRS10_SET:break;
				case PRS10_QUERY_EEPROM:break;
				case PRS10_WRITE_EEPROM:break;
			}
			break;
		case PRS10_DS:
			if ((res=query("DS?\r",ivtmp,2)))
			{
				s.errorSignal=ivtmp[0];
				s.signalStrength=ivtmp[1];	
			}
			else
				error("DS? failed");
			break;
		case PRS10_SF:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.fOffset),"SF");break;
				case PRS10_SET:res=setIntParam(&(s.fOffset),"SF",NewValue);break;
			}
			break;
		case PRS10_SS:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.slopeCalibration),"SS");break;
				case PRS10_SET:res = setIntParam(&(s.slopeCalibration),"SS",NewValue);break;break;
				case PRS10_QUERY_EEPROM:break;
				default:error("SS wrong modifier");break;
			}
			break;
		case PRS10_GA:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.gainParameter),"GA");break;
				case PRS10_SET:res=setIntParam(&(s.gainParameter),"GA",NewValue);break;
				case PRS10_QUERY_EEPROM:break;
				case PRS10_WRITE_EEPROM:break;
				default:error("GA wrong modifier");break;
			}
			break;
			break;
		case PRS10_PH:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.phase),"PH");break;
				case PRS10_QUERY_EEPROM:break;
				default:error("PH wrong modifier");break;
			}
			break;
		case PRS10_SP:
			switch (cmdModifier)
			{
				case PRS10_QUERY:
					if ((res=query("SP?\r",ivtmp,3)))
					{
						res=s.fsR.setValue(ivtmp[0]);
						res=res && s.fsN.setValue(ivtmp[1]);
						res=res && s.fsA.setValue(ivtmp[2]);
						if (!res) error("SP? out of range error");	
					}
					else
						error("SP? failed");
					break;
				case PRS10_SET:break; // FIXME
				case PRS10_QUERY_EEPROM:break;
				case PRS10_WRITE_EEPROM:break;
			}
			break;
		case PRS10_MS:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.magneticSwitching),"MS");break;
				case PRS10_SET:res=setIntParam(&(s.magneticSwitching),"MS",NewValue);break;break;
				default:error("MS wrong modifier");break;
			}
			break;
		case PRS10_MO:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.magneticOffset),"MO");break;
				case PRS10_SET:res=setIntParam(&(s.magneticOffset),"MO",NewValue);break;
				case PRS10_QUERY_EEPROM:
					res =  getIntParam(&(s.eeprom.magneticOffset),"MO!");break;
				case PRS10_WRITE_EEPROM:
					res =  writeEEPROM("MO");break;
				default:error("MO wrong modifier");break;
			}
			break;
		case PRS10_MR:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.magneticRead),"MR");break;
				default:error("MR wrong modifier");break;
			}
			break;
		case PRS10_TT:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.timeTag),"TT");break;
				default:error("TT wrong modifier");break;
			}
			break;
		case PRS10_TS:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.timeSlope),"TS");break;
				case PRS10_QUERY_EEPROM:break;
				default:error("TS wrong modifier");break;
			}
			break;
		case PRS10_TO:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.timeOffset),"TO");break;
				case PRS10_QUERY_EEPROM:break;
				default:error("TO wrong modifier");break;
			}
			break;
		case PRS10_PP:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.ppsOffset),"PP");break;
				case PRS10_SET:res=setIntParam(&(s.ppsOffset),"PP",NewValue);break;
				default:error("PP wrong modifier");break;
			}
			break;
		case PRS10_PS:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.ppsSlopeCalibration),"PS");break;
				case PRS10_QUERY_EEPROM:break;
				default:error("PS wrong modifier");break;
			}
			break;
		case PRS10_PL:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.ppsPLLOn),"PL");break;
				case PRS10_SET:res=setIntParam(&(s.ppsPLLOn),"PL",NewValue);break;
				case PRS10_QUERY_EEPROM:
					res =  getIntParam(&(s.eeprom.ppsPLLOn),"PL!");break;
				case PRS10_WRITE_EEPROM:
					res =  writeEEPROM("PL");break;
				default:error("PL wrong modifier");break;
			}
			break;
		case PRS10_PT:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.ppsPLLTimeConstant),"PT");break;
				case PRS10_SET:res=setIntParam(&(s.ppsPLLTimeConstant),"PT",NewValue);break;
				case PRS10_QUERY_EEPROM:break;
				case PRS10_WRITE_EEPROM:break;
				default:error("PT wrong modifier");break;
			}
			break;
		case PRS10_PF:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.ppsPLLStabilityFactor),"PF");break;
				case PRS10_SET:res=setIntParam(&(s.ppsPLLStabilityFactor),"PF",NewValue);break;
				case PRS10_QUERY_EEPROM:break;
				case PRS10_WRITE_EEPROM:break;
				default:error("PF wrong modifier");break;
			}
			break;
		case PRS10_PI:
			switch (cmdModifier)
			{
				case PRS10_QUERY:res = getIntParam(&(s.ppsPLLIntegrator),"PI");break;
				case PRS10_SET:res=setIntParam(&(s.ppsPLLIntegrator),"PI",NewValue);break;break;
				default:error("PI wrong modifier");break;
			}
			break;
		case PRS10_SD:
			switch (cmdModifier)
			{
				case PRS10_QUERY:
					sprintf(msgbuf,"SD%i?\r",chan);
					if ((res=query(msgbuf,&itmp)))
							s.DACSettings[chan]=itmp;
					else
						error("SD? failed");
					break;
				case PRS10_QUERY_EEPROM:break;
				default:error("SD wrong modifier");break;
			}
			break;
		case PRS10_AD:
			sprintf(msgbuf,"AD%i?\r",chan);
			if ((res=query(msgbuf,&ftmp)))
				s.testVoltages[chan]=scaleFactors[chan]*ftmp;
			break;
	}
	
	return res;		
}

		

bool PRS10::getTestVoltages(float tv[20])
{
	// Collects all the ADC readings
	 
	for (int i=0;i<=19;i++)
	{
		command(PRS10_AD,PRS10_QUERY,0,i);
		tv[i]=s.testVoltages[i];
	}
	return true;
}


bool PRS10::getStatus(unsigned char status[6])
{
	
	command(PRS10_ST);
	for (int i=0;i<6;i++) 
		status[i]=s.status[i];
	return true;
}

bool PRS10::getAllVariables()
{
	bool res=true;
	// Note the order of operands in the following expressions
	// We don't want short circuited evaluation of Boolean expressions
	// to result in non-execution of command()
	res = command(PRS10_VB) && res;
	res = command(PRS10_ID) && res;
	res = command(PRS10_SN) && res;
	res = command(PRS10_LM) && res;
	res = command(PRS10_LO) && res;
	res = command(PRS10_FC) && res;
	res = command(PRS10_DS) && res;
	res = command(PRS10_SF) && res;
	res = command(PRS10_SS) && res;
	res = command(PRS10_GA) && res;
	res = command(PRS10_PH) && res;
	res = command(PRS10_SP) && res;
	res = command(PRS10_MS) && res;
	res = command(PRS10_MO) && res;
	res = command(PRS10_MR) && res;
	res = command(PRS10_TT) && res;
	res = command(PRS10_TS) && res;
	res = command(PRS10_TO) && res;
	res = command(PRS10_PP) && res; // FIXME? This command does not work
	res = command(PRS10_PS) && res;
	res = command(PRS10_PL) && res;
	res = command(PRS10_PT) && res;
	res = command(PRS10_PF) && res;
	res = command(PRS10_PI) && res;
	for (int i=0;i<8;i++)
		res = command(PRS10_SD,PRS10_QUERY,0,i) && res;
	return res;
}

string PRS10::getErrorMessage(int *numErrors)
{
	if (numErrors != NULL)
		*numErrors=nErr;
	return errBuf;
}

void PRS10::clearErrorMessage()
{
	nErr=0;
	isErr=false;
	errBuf="";
}
		
//
//
//

void PRS10::error(string msg)
{
	// Adds an error message to the error buffer.
	// Messages are delimited by \n
	errBuf += msg+"\n";
	isErr=true;
	nErr++;
}

bool PRS10::writeEEPROM(const char *cmd)
{
	// Convenience function for writing to the EEPROM
	char msgbuf[255];
	sprintf(msgbuf,"%s!\r",cmd);
	if (!(writeString(msgbuf)))
	{ 
		sprintf(msgbuf,"%s! failed",cmd);
		error(msgbuf);
		return false;
	}
	
	return true;
}

bool PRS10::setIntParam(IntRange *r,const char *cmd,int v)
{
	// Convenience function for setting an integer parameter
	char msgbuf[255];
	if (r->setValue(v))
	{
		sprintf(msgbuf,"%s%i\r",cmd,r->value());
		if (!(writeString(msgbuf)))
		{ 
			sprintf(msgbuf,"%s%i failed",cmd,r->value());
			error(msgbuf);
			return false;
		}
		else
			return true;
	}
	else
	{
		sprintf(msgbuf,"%s%i failed - out of range [%i,%i ]",cmd,r->value(),
			r->minValue(),r->maxValue());
		error(msgbuf);
		return false;
	}
}

bool PRS10::getIntParam(IntRange *r,const char *cmd)
{
	// Convenience function for getting an integer parameter
	char msgbuf[255];
	int ibuf;
	sprintf(msgbuf,"%s?\r",cmd);
	if (query(msgbuf,&ibuf))
	{
		r->setValue(ibuf);
		return true;
	}
	else
	{
		sprintf(msgbuf,"%s? failed",cmd);
		error(msgbuf);
		return false;
	}
}

void PRS10::startTimer(long usecs)
{
	// Starts an itimer - used for handling I/O timeouts 
	struct itimerval itv; 
 
	itv.it_interval.tv_sec=0;
	itv.it_interval.tv_usec=0; // This stops the timer on timeout
	itv.it_value.tv_sec=usecs/1000000;
	itv.it_value.tv_usec=usecs-itv.it_value.tv_sec*1000000;
	if (setitimer(ITIMER_REAL,&itv,0)!=0)
		error("Start timer failed"); 
}

void PRS10::stopTimer()
{  
	struct itimerval itv;
	itv.it_interval.tv_sec=0;
	itv.it_interval.tv_usec=0;
	itv.it_value.tv_sec=0;
	itv.it_value.tv_usec=0;
	if (setitimer(ITIMER_REAL,&itv,0)!=0)
		error("Stop timer failed");   
}

void PRS10::signalHandler(int sig)
{
	// Note: this is a static member function
	switch (sig)
	{
		case SIGALRM:break;
	}
}

