//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Michael J. Wouters
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


#include "Sys.h"
#include "Debug.h"

#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/timex.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <utime.h>

#include <algorithm>
#include <iostream>
#include <stack>
#include <fstream>
#include <sstream>

#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "configurator.h"

#include "Button.h"
#include "ConfirmationDialog.h"
#include "ContrastWidget.h"
#include "Dialog.h"
#include "IntensityWidget.h"
#include "IPWidget.h"
#include "KeyEvent.h"
#include "Label.h"
#include "LCDMonitor.h"
#include "Menu.h"
#include "MessageBox.h"
#include "SliderWidget.h"
#include "Version.h"
#include "Widget.h"
#include "WidgetCallback.h"
#include "Wizard.h"

#define LCDMONITOR_VERSION "1.2.2"

#define BAUD 115200
#define PORT "/dev/lcd"
#define DEFAULT_LOG_FILE        "/usr/local/log/lcdmonitor.log"
#define DEFAULT_LOCK_FILE       "/usr/local/log/lcdmonitor.lock"
#define DEFAULT_CONFIG          "/usr/local/etc/lcdmonitor.conf"

#define PRETTIFIER "*********************************************"

#define BOOT_GRACE_PERIOD 300 // in seconds
#define MAX_FILE_AGE 300


using namespace std;
using namespace::boost;

bool LCDMonitor::timeout=false;
extern LCDMonitor *app;

LCDMonitor::LCDMonitor(int argc,char **argv)
{


	char c;
	while ((c=getopt(argc,argv,"hvd")) != EOF)
	{
		switch(c)
  	{
			case 'h':showHelp(); exit(EXIT_SUCCESS);
			case 'v':showVersion();exit(EXIT_SUCCESS);
			case 'd':Debug(dc::trace.on());break;
		}
	}
	
	init();
	
	makeMenu();
	
}

LCDMonitor::~LCDMonitor()
{
	Uninit_Serial();
	log("Shutdown");
	unlink(lockFile.c_str());
}


void LCDMonitor::touchLock()
{
	// the lock is touched periodically to signal that
	// the process is still alive
	utime(lockFile.c_str(),0);
}

void LCDMonitor::showAlarms()
{
	clearDisplay();
	
	MessageBox *mb = new MessageBox(" "," "," "," ");
	
	unsigned int nalarms=alarms.size();
	if (nalarms > 4) nalarms=4;
	
	if (nalarms == 0)
	{
		mb->setLine(1,"   No alarms");
	}
	else
	{
		for (unsigned int i=0;i<nalarms;i++)
			mb->setLine(i,alarms[i]);
	}
	
	execDialog(mb);
	delete mb;
}


void LCDMonitor::showSysInfo()
{
	clearDisplay();
	
	MessageBox *mb = new MessageBox(" "," "," "," ");
	
	int nline=0;
	
	ifstream fin(sysInfoConf.c_str());
	if (!fin.good())
		mb->setLine(1,"File not found");
	else
	{
		string tmp;
		
		while (!fin.eof())
		{	
			getline(fin,tmp);
			if (fin.eof())
				break;
			if (fin.fail())
			{
				mb->setLine(0," ");mb->setLine(2," ");mb->setLine(3," ");
				mb->setLine(1,"Bad sysinfo file");
				break;
			}
			mb->setLine(nline,tmp);
			nline++;
		}
	}
	
	execDialog(mb);
	delete mb;
}


void LCDMonitor::networkDisable()
{
	
	clearDisplay();
	updateLine(0,"Disabling network");
	sleep(1);
	updateLine(1,"Done");
	sleep(1);
	log("networking disabled");
}

void LCDMonitor::networkConfigDHCP()
{
	
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm DHCP");
	bool ret = execDialog(dlg);
	std::string lastError="No error";
	if (ret)
	{
	
		string ftmp("/etc/sysconfig/network-scripts/tmp.ifcfg-eth0");
		ofstream fout(ftmp.c_str());
		// A minimal DHCP configuration
		fout << "DEVICE=eth0" << endl;
		fout << "BOOTPROTO=dhcp" << endl;
		fout << "ONBOOT=yes" << endl;
		
		string tmp;
		string ifcfg("/etc/sysconfig/network-scripts/ifcfg-eth0");
		ifstream fin(ifcfg.c_str());
		if (!fin.good())
		{
			lastError="ifcfg-eth0 not found";
			goto DIE;
		}
		while (!fin.eof())
		{
			getline(fin,tmp);
			if (fin.eof())
				break;
			if (fin.fail())
			{
				lastError="Bad ifcfg-eth0 ";
				goto DIE;
			}
			if (string::npos != tmp.find("HWADDR"))	 // preserve this in case of multiple ethernet interfaces
				fout << tmp;
		}
		
		fin.close();
		
		fout.close();
		int retval;
		if (0 != (retval =rename(ftmp.c_str(),ifcfg.c_str())))
		{
			Dout(dc::trace,"Rename of " << ftmp << " to " << ifcfg << " failed err = " << errno);
			lastError = "Rename of tmp.ifcfg-eth0 failed";
			goto DIE;
		}
		
		restartNetworking();
	
	}
	
	{
	delete dlg;
	
	MenuItem *mi = protocolM->itemAt(midDHCP);
	mi->setChecked(ret);
	mi = protocolM->itemAt(midStaticIP4);
	mi->setChecked(!ret);
	
	return;
	}
	
	DIE:
		delete dlg;
		Dout(dc::trace,"last error: "<< lastError);
		clearDisplay();
		updateLine(1,"Reconfig failed !");
		sleep(2);
}

void LCDMonitor::networkConfigStaticIP4()
{
	clearDisplay();
	Wizard *dlg = new Wizard();
	
	Widget *w = dlg->addPage("IP address");
	w->setGeometry(0,0,20,4);
	IPWidget *ipw = new IPWidget(ipv4addr,IPWidget::IPV4,w);
	ipw->setGeometry(0,1,15,1);
	ipw->setFocusWidget(true);
	
	w = dlg->addPage("Net mask");
	w->setGeometry(0,0,20,4);
	IPWidget *nmw = new IPWidget(ipv4nm,IPWidget::IPV4,w);
	nmw->setGeometry(0,1,15,1);
	nmw->setFocusWidget(true);
	
	w = dlg->addPage("Gateway");
	w->setGeometry(0,0,20,4);
	IPWidget *gww = new IPWidget(ipv4gw,IPWidget::IPV4,w);
	gww->setGeometry(0,1,15,1);
	gww->setFocusWidget(true);
	
	w = dlg->addPage("Nameserver");
	w->setGeometry(0,0,20,4);
	IPWidget *nsw = new IPWidget(ipv4ns,IPWidget::IPV4,w);
	nsw->setGeometry(0,1,15,1);
	nsw->setFocusWidget(true);
	
	bool ret = execDialog(dlg);
	std::string lastError="No error";
	if (ret)
	{
		ipv4addr = ipw->ipAddress();
		ipv4nm =   nmw->ipAddress();
		ipv4gw =   gww->ipAddress();
		ipv4ns =   nsw->ipAddress();
		
		// make temporary files and copy across
		// note that temporay files are made in the same directory
		// as the target because rename() does not work across devices (partitions)
		// network
		string netcfg("/etc/sysconfig/network");
		ifstream fin(netcfg.c_str());
		if (!fin.good())
		{
			lastError="Missing /etc/sysconfig/network";
			goto DIE;
		}	
		
		string tmp;
		string ftmp("/etc/sysconfig/network.tmp");
		ofstream fout(ftmp.c_str());
		bool gotGATEWAY=false;
		while (!fin.eof())
		{
			getline(fin,tmp);
			if (fin.eof())
				break;
			if (fin.fail())
			{
				lastError="Error in /etc/sysconfig/network";
				goto DIE;
			}
			if (string::npos != tmp.find("GATEWAY"))
			{
				fout << "GATEWAY=" << ipv4gw << endl;
				gotGATEWAY=true;
			}
			else
				fout << tmp << endl;
			
		}
		
		if (!gotGATEWAY)
			fout << "GATEWAY=" << ipv4gw << endl;
			
		fin.close();
		fout.close();
		
		int retval;
		if (0 != (retval = rename(ftmp.c_str(),netcfg.c_str())))
		{
			Dout(dc::trace,"Rename of " << ftmp << " to " << netcfg << " failed err = " << errno);
			lastError="Rename of network failed";
			goto DIE;
		}
		
		// ifcfg-eth0
		string ifcfg("/etc/sysconfig/network-scripts/ifcfg-eth0");
		ifstream fin2(ifcfg.c_str());
		if (!fin2.good())
		{
			lastError="ifcfg-eth0 not found";
			goto DIE;
		}
		ftmp="/etc/sysconfig/network-scripts/tmp.ifcfg-eth0"; // take care not to leave an ifcfg that the init scripts could pick up
		ofstream fout2(ftmp.c_str());
		// the existing configuration is read in and copied except for IPADDR and NETMASK
		bool gotIPADDR=false;
		bool gotNETMASK=false;
		bool gotBOOTPROTO=false;
		
		while (!fin2.eof())
		{
			getline(fin2,tmp);
			if (fin2.eof())
				break;
			if (fin2.fail())
			{
				lastError="Error in ifcfg-eth0";
				goto DIE;
			}
			if (string::npos != tmp.find("IPADDR"))
			{
				fout2 << "IPADDR=" << ipv4addr << endl;
				gotIPADDR=true;
			}
			else if ( (string::npos != tmp.find("NETMASK")) || (string::npos != tmp.find("PREFIX")))
			{
				// replace NETMASK with PREFIX
				// FIXME could do better
				fout2 << "NETMASK=" << ipv4nm << endl;
				gotNETMASK=true;
			}
			else if (string::npos != tmp.find("BOOTPROTO"))
			{
				fout2 << "BOOTPROTO=none" << endl; // none==static
				gotBOOTPROTO=true;
			}
			else if (string::npos != tmp.find("DNS1"))
			{
				fout2 << "DNS1=" << ipv4ns << endl;
				gotBOOTPROTO=true;
			}
			else
				fout2 << tmp << endl;
		}
		
		if (!gotIPADDR)
			fout2 << "IPADDR=" << ipv4addr << endl;
		if (!gotNETMASK)
			fout2 << "NETMASK=" << ipv4nm << endl;
		if (!gotBOOTPROTO)
			fout2 << "BOOTPROTO=none" << endl;
		
		fin2.close();
		fout2.close();
	
		if (0 != (retval =rename(ftmp.c_str(),ifcfg.c_str())))
		{
			Dout(dc::trace,"Rename of " << ftmp << " to " << ifcfg << " failed err = " << errno);
			lastError = "Rename of tmp.ifcfg-eth0 failed";
			goto DIE;
		}
		
		// /etc/resolv.conf
		string nscfg("/etc/resolv.conf");
		ifstream fin3(nscfg.c_str());
		if (!fin3.good())
		{
			lastError="resolv.conf not found";
			goto DIE;
		}	
		ftmp="/etc/resolv.conf.tmp";
		ofstream fout3(ftmp.c_str());
		bool gotNS=false; // should only be one NS defined but if someone has manually fiddled
										// then make sure we only change the first one configured in resolv.conf
		while (!fin3.eof())
		{
			getline(fin3,tmp);
			if (fin3.eof())
				break;
			if (fin3.fail())
			{
				lastError="Error in resolv.conf";
				goto DIE;
			}
			if ((!gotNS) && (string::npos != tmp.find("nameserver")))
			{
				fout3 << "nameserver " << ipv4ns << endl;
				gotNS=true;
			}
			else
				fout3 << tmp << endl;
			
		}
		if (!gotNS)
			fout3 << "nameserver " << ipv4ns << endl;
			
		fin3.close();
		fout3.close();
		
		if (0 != (retval =rename(ftmp.c_str(),nscfg.c_str())))
		{
			Dout(dc::trace,"Rename of " << ftmp << " to " << ifcfg << " failed err = " << errno);
			lastError="Rename of resolv.conf.tmp failed";
			goto DIE;
		}
		
		restartNetworking();
	}
	
	delete dlg;
	return;
	
	DIE:
		delete dlg;
		Dout(dc::trace,"last error: "<< lastError);
		clearDisplay();
		updateLine(1,"Reconfig failed !");
		sleep(2);
}

void LCDMonitor::restartNetworking()
{
	clearDisplay();

	updateLine(1,"Restarting network");
	runSystemCommand("/sbin/service network restart","Restarted OK","Restart failed !");
	sleep(1);

	if (serviceEnabled("S55sshd")) // don't start if disabled
	{
		updateLine(1,"Restarting ssh");
		runSystemCommand("/sbin/service sshd restart","Restarted OK","Restart failed !");
		sleep(1);
	}

	updateLine(1,"Restarting ntpd");
	runSystemCommand(ntpdRestartCommand,"Restarted OK","Restart failed !");
	sleep(1);

	if (serviceEnabled("S85httpd")) // don't start if disabled
	{
		updateLine(1,"Restarting httpd");
		runSystemCommand("/sbin/service httpd restart","Restarted OK","Restart failed !");
		sleep(1);
	}
	
}
	
void LCDMonitor::LCDConfig()
{
	clearDisplay();
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	IntensityWidget *iw = new IntensityWidget("Intensity",intensity,dlg);
	iw->setGeometry(0,0,20,1);
	iw->setFocusWidget(true);
	ContrastWidget *cw = new ContrastWidget("Contrast",contrast,dlg);
	cw->setRange(0,255);
	cw->setNumSteps(10);
	cw->setGeometry(0,1,20,1);
	std::string help = "  Adjust with ";
	help += 225;
	help += " ";
	help += 223;
	Label *l= new Label(help,dlg);
	l->setGeometry(0,2,20,1);
	
	WidgetCallback<Dialog> *cb = new WidgetCallback<Dialog>(dlg,&Dialog::ok);
	Button *b = new Button("OK",cb,dlg);
	b->setGeometry(5,3,2,1);
	
	cb = new WidgetCallback<Dialog>(dlg,&Dialog::cancel);
	b = new Button("Cancel",cb,dlg);
	b->setGeometry(10,3,6,1);
	
	bool ret = execDialog(dlg);
	if (ret)
	{
		if (intensity != iw->value())
		{
			intensity = iw->value();
			std::string sbuf;
			ostringstream ossbuf(sbuf);
			ossbuf << intensity;
			updateConfig("ui","lcd intensity",ossbuf.str());
		}
		if (contrast != cw->value())
		{
			contrast = cw->value();
			std::string sbuf;
			ostringstream ossbuf(sbuf);
			ossbuf << contrast;
			updateConfig("ui","lcd contrast",ossbuf.str());
		}
	}
	else // we cancelled
	{
		// bit ugly but ...
		COMMAND_PACKET cmd;
		cmd.command=13;
		cmd.data[0]=contrast;
		cmd.data_length=1;
		sendCommand(cmd);
		
		cmd.command=14;
		cmd.data[0]=intensity;
		cmd.data_length=1;
		sendCommand(cmd);
	}
	delete dlg;
}

void LCDMonitor::setGPSDisplayMode()
{
	if (displayMode==GPS) return;
	
	
	displayMode=GPS;
	MenuItem *mi = displayModeM->itemAt(midGPSDisplayMode);
	mi->setChecked(true);
	mi = displayModeM->itemAt(midNTPDisplayMode);
	mi->setChecked(false);
	
	updateConfig("ui","display mode","GPS");
	
	clearDisplay();
}

void LCDMonitor::setNTPDisplayMode()
{
	if (displayMode==NTP) return;
	
	displayMode=NTP;
	MenuItem *mi = displayModeM->itemAt(midGPSDisplayMode);
	mi->setChecked(false);
	mi = displayModeM->itemAt(midNTPDisplayMode);
	mi->setChecked(true);
	lastNTPtrafficPoll.tv_sec=0;
	
	updateConfig("ui","display mode","NTP");
	
	clearDisplay();
}
		
void LCDMonitor::restartGPS()
{
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm GPS rx restart");
	bool ret = execDialog(dlg);
	if (ret)
	{
		clearDisplay();
		
		updateLine(1,"  Restarting GPS rx");
		// first kill the logging process if it is running
		struct stat statbuf;
		if ((0 == stat(gpsLoggerLockFile.c_str(),&statbuf)))
		{
			
			std::ifstream fin(gpsLoggerLockFile.c_str());
			if (!fin.good())
			{
				log("Couldn't open " + gpsLoggerLockFile);
				goto fail;
			}
   		pid_t pid;
			fin >> pid;
			fin.close();
			if (pid >0) // be careful here, since we are superdude
				kill(pid,SIGTERM);
			else
				goto fail;
			sleep(2); // wait a bit for OS to do its thing
		}
		Dout(dc::trace,"LCDMonitor::restartGPS() " << gpsRxRestartCommand );
		int sysret = system(gpsRxRestartCommand.c_str());
		Dout(dc::trace,"LCDMonitor::restartGPS() system() returns " << sysret);
		if (sysret == -1)
			goto fail;
		else
		{
			updateLine(2,"  Done");
			log("GPS rx restarted");
		}
		sleep(2);
		
		delete dlg;
		return;
	}
	else //cancelled the dialog
	{
		delete dlg;
		return;
	}

	fail:
		updateLine(2,"  Restart failed");
		log("GPS rx restart failed");
		sleep(2);
		delete dlg;

}

void LCDMonitor::restartNtpd()
{
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm NTP restart");
	bool ret = execDialog(dlg);
	if (ret)
	{
		clearDisplay();
		updateLine(1,"  Restarting ntpd");
		runSystemCommand(ntpdRestartCommand,"ntpd restarted","ntpd restart failed");
	}
	delete dlg;
	
}

void LCDMonitor::reboot()
{
	
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm reboot");
	bool ret = execDialog(dlg);
	if (ret)
	{
		clearDisplay();
		updateLine(1,"  Rebooting");
		runSystemCommand(rebootCommand,"Rebooting ...","Reboot failed !");
	}
	delete dlg;
}

void LCDMonitor::poweroff()
{
	
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm poweroff");
	bool ret = execDialog(dlg);
	if (ret)
	{
		clearDisplay();
		updateLine(1,"  Powering down");
		runSystemCommand(poweroffCommand,"Powering off ...","Poweroff failed !");
	}
	delete dlg;
}
		
void LCDMonitor::clearDisplay()
{
	for (int i=0;i<4;i++)
		status[i]="";
	outgoing_response.command = 6;
	outgoing_response.data_length = 0;
	send_packet();
	getResponse();
}

void LCDMonitor::run()
{
	clearDisplay();
	while (1)
	{
		// use usleep timing otherwise the displayed time jumps 1s occasionally because of rounding
		struct timeval tv;
		gettimeofday(&tv,0);
		usleep(1000000-tv.tv_usec+10000); // wait until a bit after the second rollover so we're not fighting ntpd 
		if(packetReceived())
		{
			// check the packet type - timeouts can generate unexpected packets
			if (incoming_command.command==0x80) // key events only
			{
				ShowReceivedPacket();
				clearDisplay();
				execMenu();
				lastLazyCheck=0; // trigger immediate update
				clearDisplay();
			}
		}
		else
			showStatus();
	}
}

void LCDMonitor::showStatus()
{
	char buf[21];/* maximum of 20 characters per line */
	
	// Construct a message for display 
	time_t tnow = time(NULL);
	struct tm *tmnow = gmtime(&tnow);
	sprintf(buf,"%d-%02d-%02d %02d:%02d:%02d",
		tmnow->tm_year+1900,tmnow->tm_mon+1,tmnow->tm_mday,
		tmnow->tm_hour,tmnow->tm_min,tmnow->tm_sec);
	
	updateLine(0,buf);
	
	// only run these checks every 10s or so
	if (tnow - lastLazyCheck > 10)
	{
		
		touchLock();
	
		switch (displayMode)
		{
			case NTP:
			{
				gettimeofday(&currNTPtrafficPoll,NULL);
				if (currNTPtrafficPoll.tv_sec - lastNTPtrafficPoll.tv_sec < 60) break;
			
				int oldpkts=-1,newpkts=-1,badpkts=-1;
				getNTPstats(&oldpkts,&newpkts,&badpkts);
				if (oldpkts >=0 && newpkts >=0 && badpkts >=0)
				{
					
					currNTPPacketCount=oldpkts+newpkts+badpkts;
					if (currNTPPacketCount < lastNTPPacketCount) // counter rollover
					{
						lastNTPtrafficPoll = currNTPtrafficPoll;
						lastNTPPacketCount = currNTPPacketCount; 
						break; // no update
					}
					if (lastNTPtrafficPoll.tv_sec>0) // got an interval
					{
						double pktrate = 60.0 * (currNTPPacketCount -  lastNTPPacketCount)/
							(currNTPtrafficPoll.tv_sec - lastNTPtrafficPoll.tv_sec +
							 1.0E-6*(currNTPtrafficPoll.tv_usec - lastNTPtrafficPoll.tv_usec));
						std::string sbuf;
						ostringstream ossbuf(sbuf);
						ossbuf.precision(1); // just one after the decimal point
						ossbuf << std::fixed << pktrate << " pkts/min";
						updateLine(1,ossbuf.str().c_str());
						lastNTPtrafficPoll = currNTPtrafficPoll;
						lastNTPPacketCount = currNTPPacketCount; 
					}
					else // first time thru
					{
						updateLine(1,"???? pkts/min");
						lastNTPtrafficPoll = currNTPtrafficPoll;
						lastNTPPacketCount = currNTPPacketCount; 
					}
					
				}
				break;
			}
			case GPS:
			{
				std::string prn="";
				int nsats=0;
				bool unexpectedEOF;
				checkGPS(&nsats,prn,&unexpectedEOF); 
				if (unexpectedEOF)
					Dout(dc::trace,"LCDMonitor::showStatus() Unexpected EOF");
			
				if (showPRNs && !unexpectedEOF)
				{
					// split this over two lines
					// Have 15 characters per line == 6 per line
					std::vector<std::string> sprns;
					boost::split(sprns,prn,is_any_of(",")); // note that if no split, then input is returned
					std::vector<int> prns;
					for (unsigned int i=0;i<sprns.size();i++)
					{
						prns.push_back(atoi(sprns.at(i).c_str()));
					}
					sort(prns.begin(),prns.end());
					int nprns = prns.size();
					if (nprns > 6) nprns=6;
					std::string sbuf;
					ostringstream ossbuf(sbuf);
					ossbuf << "SV";
		
					for (int s=0;s<nprns;s++)
					{
						ossbuf.width(3);
						ossbuf << prns[s];
					}
					updateLine(1,ossbuf.str().c_str());
		
					nprns = prns.size();
					if (nprns > 12) nprns=12; // can only do 12
					nprns -=6; // number left to show
					ossbuf.clear(); // clear any errors 
					ossbuf.str("");
					ossbuf << "  ";
					for (int s=0;s<nprns;s++)
					{
						ossbuf.width(3);
						ossbuf << prns[s+6];
					}
					updateLine(2,ossbuf.str().c_str());
				}
				else if (!unexpectedEOF)
				{
					sprintf(buf,"GPS sats=%d",nsats);
					updateLine(1,buf);
				}
				break;
			}
		}
		//if (GPSOK)
		//	updateStatusLED(1,GreenOn);
		//else
		//	updateStatusLED(1,RedOn);

		if (checkAlarms())
		{
			updateLine(3,"SYSTEM OK");
			updateStatusLED(3,GreenOn);
		}
		else
		{
			updateLine(3,"SYSTEM ALARM");
			updateStatusLED(3,RedOn);
		}
		lastLazyCheck=tnow;
	}
	
	// leap second status 
	if (displayMode == NTP)
	{
		struct timex tx;
		tx.modes=0;
		int clkstate=adjtimex(&tx);
		switch (clkstate)
		{
			case TIME_OK:
				snprintf(buf,20,"Time OK");
				break;
			case TIME_INS:
				snprintf(buf,20,"Leap second today");
				break;
			case TIME_DEL:
				snprintf(buf,20,"Leap second today");
				break;
			case TIME_OOP:
				snprintf(buf,20,"Leap second now");
				break;
			case TIME_WAIT:
				snprintf(buf,20,"Leap second occurred");
				break;
			case TIME_BAD:
				snprintf(buf,20,"Unsynchronized");
				break;	
		}
		updateLine(2,buf);
	}
	
	#ifdef CWDEBUG
	{
		Dout(dc::trace,"-------------------");
		Dout(dc::trace,"  12345678901234567890");
		for (int i=0;i<4;i++)
		{
			char l;
			switch (statusLED[i])
			{
				case Off: l='O';break;
				case RedOn: l='R';break;
				case GreenOn:l='G';break;
				case Unknown:l='U';break; 
			}
			Dout(dc::trace,l << " " << status[i].c_str());
		}
		Dout(dc::trace,"-------------------");
	}
	#endif
}

void LCDMonitor::execMenu()
{
	bool showMenu=true;
	std::stack<Menu *> menus;
	
	parseNetworkConfig(); // keep this up to date
	
	int currRow=0;
	statusLEDsOff();
	showCursor(true);
	
	menus.push(menu);
	Menu *currMenu=menus.top();
	
	startTimer(300);
	while (showMenu && !LCDMonitor::timeout)
	{
		// show the menu
		clearDisplay();
		int numItems=currMenu->numItems();
		for (int i=0;i<numItems;i++)
		{
			std::string buf = ">" + currMenu->itemAt(i)->text();
			if (buf.length() < 20) // pad the string
				buf+=std::string(20-buf.length(),' ');
			if (currMenu->itemAt(i)->checked())	
				buf.at(19)='*';
			updateLine(i,buf);
		}
		updateCursor(currRow,0);
		while (!LCDMonitor::timeout)
		{
			usleep(10000);
			if(packetReceived())
			{
				stopTimer();
				startTimer(300);
				if(incoming_command.command==0x80)
				{
					if (incoming_command.data[0]==10 || incoming_command.data[0]==11)// RIGHT RELEASE/ENTER - next menu/exec item
					{
						Dout(dc::trace,"LCDMonitor::execMenu() selected " << currRow);
						MenuItem *currItem = currMenu->itemAt(currRow);
						if (currItem)
						{
							if (currItem->isMenu())
							{
								Dout(dc::trace,"LCDMonitor::execMenu() selected menu " << currRow << "(menu)");
								currMenu = (Menu *) currItem;
								menus.push(currMenu);
								currRow=0;
							}
							else
							{
								stopTimer();
								currItem->exec();
								startTimer(300);
							}
						}
						
						break;
					}
					else if (incoming_command.data[0]==7) // UP RELEASE - prev item
					{
						if (currRow>0) currRow--;
						updateCursor(currRow,0);
					}
					else if (incoming_command.data[0]==8) // DOWN RELEASE  - next itme
					{
						if (currRow<numItems-1) currRow++;
						updateCursor(currRow,0);
					}
					else if (incoming_command.data[0]==9) // LEFT_RELEASE - prev menu
					{
						// What's on the stack
						if (menus.size() == 1)
      				showMenu=false; // bybye
						else
						{
							menus.pop();
							currMenu=menus.top();
							currRow=0;
						}
						break;
					}
					else if (incoming_command.data[0]==12) // EXIT_RELEASE - back to main screen
					{
						showMenu=false;	
						break;
					}
				}
			}
		}
	}
	showCursor(false);
	stopTimer();
}

bool LCDMonitor::execDialog(Dialog *dlg)
{
	// dispatch events to the dialog 
	// until the widget returns OK or ESC/CANCEL
	
	int currRow=0;
	int currCol=0;
	dlg->focus(&currCol,&currRow);
	
	bool exec=true;
	std::vector<std::string> display;
	std::string blank;
	blank.append(20,' ');
	
	display.push_back(blank);display.push_back(blank);display.push_back(blank);display.push_back(blank);
	
	bool retval = false;
	
	Dout(dc::trace,"LCDMonitor::execDialog()");
	startTimer(300);
	while (exec && !LCDMonitor::timeout)
	{
	
		repaintWidget(dlg,display,true);
		
		updateCursor(currRow,currCol);
		while (!LCDMonitor::timeout)
		{
			usleep(10000);
			
			if(packetReceived())
			{
				stopTimer();
				startTimer(300);
				
				if(incoming_command.command==0x80)
				{
					int event=0;
					
					if (incoming_command.data[0]==7) // UP RELEASE
						event = KeyEvent::KeyRelease|KeyEvent::KeyUp;
					else if (incoming_command.data[0]==8) // DOWN RELEASE
						event=KeyEvent::KeyRelease|KeyEvent::KeyDown;
					else if (incoming_command.data[0]==9) // LEFT RELEASE
						event=KeyEvent::KeyRelease|KeyEvent::KeyLeft;
					else if (incoming_command.data[0]==10) // RIGHT RELEASE
						event=KeyEvent::KeyRelease|KeyEvent::KeyRight;
					else if (incoming_command.data[0]==11 )// ENTER
						event = KeyEvent::KeyEnter;
					else if (incoming_command.data[0]==12) // EXIT_RELEASE
					{
						exec=false;
						retval=false;
						Dout(dc::trace,"Dialog cancelled");
						break;
					}
					
					if (event != 0)
					{
						KeyEvent ke(event,currCol,currRow);
						dlg->keyEvent(ke);
						if (dlg->finished())
						{
							retval = dlg->exitCode()==Dialog::OK;
							exec=false;
							break;
						}
						else
						{
							Dout(dc::trace,"!!!!repainting");
							repaintWidget(dlg,display);
							dlg->focus(&currCol,&currRow);
							updateCursor(currRow,currCol);
						}
					}
						
				}
			}
		}
	}
	Dout(dc::trace,"Dialog finished");
	stopTimer();
	return retval;
}

void LCDMonitor::sendCommand(COMMAND_PACKET &cmd)
{
	Dout(dc::trace,"LCDMonitor::sendCommand");
	outgoing_response.command =cmd.command;
	outgoing_response.data_length=cmd.data_length;
	for (unsigned int i=0;i<cmd.data_length;i++)
		outgoing_response.data[i]=cmd.data[i];
	send_packet();
	getResponse();
}

//
// private
//

void LCDMonitor::signalHandler(int sig)
{
	switch (sig)
	{
		case SIGALRM: 
			Dout(dc::trace,"LCDMonitor::signalHandler SIGALRM");
			LCDMonitor::timeout=true;
			break;// can't call Debug()
		case SIGTERM: case SIGQUIT: case SIGINT: case SIGHUP:
			Dout(dc::trace,"LCDMonitor::signalHandler SIGxxx");
			delete app;
			exit(EXIT_SUCCESS);
			break;
	}

}

void LCDMonitor::startTimer(long secs)
{

	struct itimerval itv; 
 	
	LCDMonitor::timeout=false;
	
	itv.it_interval.tv_sec=0;
	itv.it_interval.tv_usec=0; // this stops the timer on timeout
	itv.it_value.tv_sec=secs;
	itv.it_value.tv_usec=0;
	if (setitimer(ITIMER_REAL,&itv,0)!=0)
		log("Start timer failed"); 

}

void LCDMonitor::stopTimer()
{
	struct itimerval itv;
	itv.it_interval.tv_sec=0;
	itv.it_interval.tv_usec=0;
	itv.it_value.tv_sec=0;
	itv.it_value.tv_usec=0;
	if (setitimer(ITIMER_REAL,&itv,0)!=0)
		log("Stop timer failed");  
}
		
void LCDMonitor::init()
{
	

	FILE *fd;
	pid_t oldpid;
	
	
	logFile=DEFAULT_LOG_FILE;
	
	lockFile = DEFAULT_LOCK_FILE;
	
	/* Check that there isn't already a process running */
	if ((fd=fopen(lockFile.c_str(),"r")))
	{
		fscanf(fd,"%i",&oldpid);
		fclose(fd);
		/* this doesn't send a signal - just error checks */
		if (!kill(oldpid,0) || errno == EPERM) /* still running */
		{
			cerr << "lcdmonitor with pid " << oldpid << " is still running" << endl;
			exit(EXIT_FAILURE);	
		}
	}
	
	log(PRETTIFIER);
	ostringstream sbuf;
	sbuf << "lcdmonitor v" << LCDMONITOR_VERSION << ", last modified " << LAST_MODIFIED; 
	log(sbuf.str());
	log(PRETTIFIER);
	
	/* make a new lock */
	if ((fd = fopen(lockFile.c_str(),"w")))
	{
		fprintf(fd,"%i\n",(int) getpid());
		fclose(fd);
	}
	else
	{
		log("failed to make a lock");
		exit(EXIT_FAILURE);
	}
	
	lastNTPtrafficPoll.tv_sec=0;
	lastNTPPacketCount=0;
	
	configure();
	
	parseNetworkConfig();
	
	// this will be true for compact systems
	NTPProtocolVersion=4;
	NTPMajorVersion=2;
	NTPMinorVersion=2;
	
	detectNTPVersion();
	if (4==NTPProtocolVersion)
	{
		if (NTPMajorVersion < 2)
		{	
			currPacketsTag="new version packets";
			oldPacketsTag="old version packets";
			badPacketsTag="unknown version number";
		}
		else
		{
			currPacketsTag="current version";
			oldPacketsTag="previous version";
			badPacketsTag="bad version";
		}
	}

	if(Serial_Init(PORT,BAUD))
	{
		Dout(dc::trace,"Could not open port " << PORT << " at " << BAUD << " baud.");
		exit(EXIT_FAILURE);
	}
	else
	{
		Dout(dc::trace,PORT << " opened at "<< BAUD <<" baud");
	}
	
	/* Clear the buffer */
	while(BytesAvail())
    GetByte();
	
	for (int i=0;i<4;i++)
	{
		status[i]="";
		statusLED[i]=Unknown;
	}
	
	// use SIGALRM to detect lack of UI activity and return to status display
	
	sa.sa_handler = signalHandler;
	sigemptyset(&(sa.sa_mask)); // we block nothing 
	sa.sa_flags=0;
                                
	sigaction(SIGALRM,&sa,NULL); 

	sigaction(SIGTERM,&sa,NULL);
	
	sigaction(SIGQUIT,&sa,NULL);
	
	sigaction(SIGINT,&sa,NULL);
	
	sigaction(SIGHUP,&sa,NULL);

	lastLazyCheck=0; // trigger an immediate check
	
	statusLEDsOff();
	
	/* set up intensity and contrast */
	COMMAND_PACKET cmd;
	cmd.command=13;
	cmd.data[0]=contrast;
	cmd.data_length=1;
	sendCommand(cmd);
		
	cmd.command=14;
	cmd.data[0]=intensity;
	cmd.data_length=1;
	sendCommand(cmd);
		
}


void LCDMonitor::configure()
{

	char *stmp;
	int itmp;

	// set some sensible defaults

	displayMode = GPS;
	
	poweroffCommand="/sbin/poweroff";
	rebootCommand="/sbin/shutdown -r now";
	ntpdRestartCommand="/sbin/service ntpd restart";
	gpsRxRestartCommand="su - cvgps -c '/home/cvgps/bin/check_rx'";
	gpsLoggerLockFile="/home/cvgps/logs/rx.lock";

	ipv4addr="192.168.1.2";
	ipv4nm  ="255.255.255.0";
	ipv4gw  ="192.168.1.1";
	ipv4ns  ="192.168.1.1";
	
	NTPuser="ntpadmin";
	GPSCVuser="cvgps";
	DNSconf="/etc/resolv.conf";
	networkConf="/etc/sysconfig/network";
	eth0Conf="/etc/sysconfig/network-scripts/ifcfg-eth0";
	sysInfoConf="/usr/local/etc/sysinfo.conf";
	receiverName="resolutiont";
	alarmPath="/home/cvgps/logs/alarms";
	rbStatusFile="/home/cvgps/logs/rb.status";
	GPSStatusFile="/home/cvgps/logs/rx.status";

	string squealerConfig("/home/cvgps/etc/squealer.conf");
	string cctfConfig("/home/cvgps/etc/cctf.setup");
	
	showPRNs=false;

	string config = DEFAULT_CONFIG;
	
	intensity=80;
	contrast=95;
	
	ListEntry *last;
	if (!configfile_parse_as_list(&last,config.c_str()))
	{
		ostringstream msg;
		msg << "failed to read " << config;
		log(msg.str());
		exit(EXIT_FAILURE);
	}
	
	// General
	if (list_get_string_value(last,"General","Ntp user",&stmp))
		NTPuser=stmp;
	else
		log("NTP user not found in config file");
		
	if (list_get_string_value(last,"General","Squealer config",&stmp))
		squealerConfig=stmp;
	else
		log("Squealer config not found in config file");
	
	// Network	
	if (list_get_string_value(last,"Network","DNS",&stmp))
		DNSconf=stmp;
	else
		log("DNS not found in config file");

	if (list_get_string_value(last,"Network","Network",&stmp))
		networkConf=stmp;
	else
		log("Network not found in config file");
		
	if (list_get_string_value(last,"Network","Eth0",&stmp))
		eth0Conf=stmp;
	else
		log("Eth0 not found in config file");
	
	// GPSCV
	if (list_get_string_value(last,"GPSCV","GPSCV user",&stmp))
		GPSCVuser=stmp;
	else
		log("GPSCV user not found in config file");

	if (list_get_string_value(last,"GPSCV","CCTF config",&stmp))
		cctfConfig=stmp;
	else
		log("CCTF config not found in config file");

	if (list_get_string_value(last,"GPSCV","GPS restart command",&stmp))
		gpsRxRestartCommand=stmp;
	else
		log("GPS restart command not found in config file");

	if (list_get_string_value(last,"GPSCV","GPS logger lock file",&stmp))
		gpsLoggerLockFile=stmp;
	else
		log("GPS logger lock file not found in config file");
	
	// OS
	if (list_get_string_value(last,"OS","Reboot command",&stmp))
		rebootCommand= stmp;
	else
		log("Reboot command not found in config file");
			
	if (list_get_string_value(last,"OS","Poweroff command",&stmp))
		poweroffCommand= stmp;
	else
		log("Poweroff command not found in config file");

	if (list_get_string_value(last,"OS","ntpd restart command",&stmp))
		ntpdRestartCommand= stmp;
	else
		log("ntpd restart command not found in config file");

	// UI
	if (list_get_int_value(last,"UI","Show PRNs",&itmp))
		showPRNs = (itmp==1);
	else
		log("Show PRNs not found in config file");
				
	if (list_get_int_value(last,"UI","LCD intensity",&itmp))
	{
		intensity = itmp;
		if (intensity <0) intensity=0;
		if (intensity >100) intensity=100;
	}
	else
		log("LCD intensity not found in config file");
				
	if (list_get_int_value(last,"UI","LCD contrast",&itmp))
	{
		contrast= itmp;
		if (contrast <0) contrast=0;
		if (contrast >255) contrast=255;
	}
	else
		log("LCD contrast not found in config file");
	
	if (list_get_string_value(last,"UI","Display mode",&stmp))
	{
		boost::to_upper(stmp);
		if (0==strcmp(stmp,"GPS"))
			displayMode = GPS;
		else if (0==strcmp(stmp,"NTP"))
			displayMode = NTP;
		else
		{
			ostringstream msg;
			msg << "Unknown display mode " << stmp;
			log(msg.str());
		}
	}
		
	list_clear(last);
	
	
	if (!configfile_parse_as_list(&last,cctfConfig.c_str()))
	{
		ostringstream msg;
		msg << "failed to read " << cctfConfig;
		log(msg.str());
		exit(EXIT_FAILURE);
	}
	
	if (list_get_string_value(last,"Main","Receiver",&stmp))
		receiverName=stmp;
	else
		log("receiver type not found in cctf.setup");

	if (list_get_string_value(last,"Main","Rb Status",&stmp))
		rbStatusFile=stmp;
	else
		log("Rb Status not found in cctf.setup");
			
	if (list_get_string_value(last,"Main","receiver status",&stmp))
		GPSStatusFile=stmp;
	else
		log("Receiver Status not found in cctf.setup");
		
	list_clear(last);
	
	
	if (!configfile_parse_as_list(&last,squealerConfig.c_str()))
	{
		ostringstream msg;
		msg << "failed to read " << squealerConfig;
		log(msg.str());
		exit(EXIT_FAILURE);
	}
	
	if (list_get_string_value(last,"Alarms","Status File Directory",&stmp))
	{
		alarmPath=stmp;
		alarmPath+="/*";
	}
	else
		log("Status File Directory not found in config file");
				
	list_clear(last);
	
}

void LCDMonitor::updateConfig(std::string section,std::string token,std::string val)
{
	string config = DEFAULT_CONFIG;
	Dout(dc::trace,"Updating " << config);
	configfile_update(section.c_str(),token.c_str(),val.c_str(),config.c_str());
}

void LCDMonitor::log(std::string msg)
{
	FILE *fd;
	
	Dout(dc::trace,"LCDMonitor::log() " << msg);
	time_t tt =  time(0);
	struct tm *gmt = gmtime(&tt);
	char tc[128];
	
	strftime(tc,128,"%F %T",gmt);

	if ((fd = fopen(logFile.c_str(),"a")))
	{
		fprintf(fd,"%s %s\n",tc,msg.c_str());
		fclose(fd);
	}
}

void LCDMonitor::showHelp()
{
	cout << "Usage: lcdmonitor [options]" << endl;
	cout << "Available options are" << endl;
	cout << "\t-d" << endl << "\t Turn on debuggging" << endl;
	cout << "\t-h" << endl << "\t Show this help" << endl;
	cout << "\t-v" << endl << "\t Show version" << endl;
}

void LCDMonitor::showVersion()
{
	cout << "lcdmonitor v" << LCDMONITOR_VERSION << ", last modified " << LAST_MODIFIED << endl;
	cout << "This ain't no stinkin' Perl script!" << endl;
}

		
void LCDMonitor::makeMenu()
{
	menu = new Menu("Main menu");
	
	Menu *setupM = new Menu("Setup...");
	menu->insertItem(setupM);

	
		protocolM = new Menu("Network boot protocol...");
		setupM->insertItem(protocolM);
			
			WidgetCallback<LCDMonitor> *cb = new WidgetCallback<LCDMonitor>(this,&LCDMonitor::networkConfigDHCP);
			midDHCP=protocolM->insertItem("DHCP...",cb);
			MenuItem *mi = protocolM->itemAt(midStaticIP4);
			if (mi != NULL) mi->setChecked(networkProtocol==DHCP);
			
			cb = new WidgetCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIP4);
			midStaticIP4=protocolM->insertItem("Static IPv4...",cb);
			mi = protocolM->itemAt(midStaticIP4);
			if (mi != NULL) mi->setChecked(networkProtocol==StaticIPV4);
			
		cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::LCDConfig);
		setupM->insertItem("LCD settings...",cb);
		
		displayModeM = new Menu("Display mode...");
		setupM->insertItem(displayModeM);
			
			cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::setGPSDisplayMode);
		  midGPSDisplayMode =  displayModeM ->insertItem("GPS",cb);
			mi = displayModeM->itemAt(midGPSDisplayMode);
			Dout(dc::trace,"midGPSDisplayMode = " << midGPSDisplayMode);
			if (mi != NULL) mi->setChecked(displayMode==GPS);
			
			cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::setNTPDisplayMode);
		  midNTPDisplayMode = displayModeM ->insertItem("NTP",cb);
			mi = displayModeM->itemAt(midNTPDisplayMode);
			if (mi != NULL) mi->setChecked(displayMode==NTP);
			
	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::showAlarms);
	menu->insertItem("Show alarms",cb);
	
	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::showSysInfo);
	menu->insertItem("Show system info",cb);
	
	Menu *restartM = new Menu("Restart...");
	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::restartGPS);
	restartM->insertItem("Restart GPS",cb);
	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::restartNtpd);
	restartM->insertItem("Restart NTPD",cb);
	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::reboot);
	restartM->insertItem("Reboot",cb);
	cb = new WidgetCallback<LCDMonitor>(this, &LCDMonitor::poweroff);
	restartM->insertItem("Power down",cb);
	menu->insertItem(restartM);
}
	
void LCDMonitor::getResponse()
{
	int timed_out =1;
	for(int k=0;k<=100;k++)
	{
		usleep(10000);
		if(packetReceived())
		{
			
			ShowReceivedPacket();
			timed_out = 0; 
			break;
		}
	}
	if(timed_out)
	{
		Dout(dc::trace,"LCDMonitor::getResponse() Timed out waiting for a response");
		log("I/O timeout");
		
		Uninit_Serial();
		
		if(Serial_Init(PORT,BAUD))
		{
			Dout(dc::trace,"Could not open port " << PORT << " at " << BAUD << " baud.");
			exit(EXIT_FAILURE);
		}
		else
		{
   	 Dout(dc::trace,PORT << " opened at "<< BAUD <<" baud");
		}
	
		/* Clear the buffer */
		while(BytesAvail())
    	GetByte();
	}
}

void LCDMonitor::showCursor(bool on)
{
	outgoing_response.command = 12;
	if (on)
		outgoing_response.data[0]=4;
	else
		outgoing_response.data[0]=0;
	outgoing_response.data_length =1;
	send_packet();
	getResponse();
}

void LCDMonitor::updateLine(int row,std::string buf)
{
	// a bit of optimization to cut down on I/O and flicker
	if (buf==status[row]) return;
	status[row]=buf;
	std::string tmp=buf;
	if (buf.length() < 20) // pad out to clear the line
		tmp+=std::string(20-buf.length(),' ');
	
	outgoing_response.command = 31;
	outgoing_response.data[0]=0; //col
	outgoing_response.data[1]=row; //row
	int nch = tmp.length();
	if (nch > 20) nch=20;
	memcpy(&outgoing_response.data[2],tmp.c_str(),nch);
	outgoing_response.data_length =2+nch;
	send_packet();
	getResponse();
}

void LCDMonitor::updateStatusLED(int row,LEDState s)
{
	if (statusLED[row] == s) return;// nothing to do
	statusLED[row] = s;
	
	int idx = 12 - row*2;
	int rstate,gstate;
	switch (s)
	{
		case Off: rstate=gstate=0;break;
		case RedOn: rstate=100;gstate=0;break;
		case GreenOn: rstate=0;gstate=100;break;
		case Unknown:return;
	}
	
	outgoing_response.command = 34;
	outgoing_response.data[0]=idx;
	outgoing_response.data[1]=rstate;
	outgoing_response.data_length =2;
	send_packet();
	getResponse();
	
	outgoing_response.command = 34;
	outgoing_response.data[0]=idx-1;
	outgoing_response.data[1]=gstate;
	outgoing_response.data_length =2;
	send_packet();
	getResponse();
}

void LCDMonitor::statusLEDsOff()
{
	for (int i=0;i<4;i++)
		updateStatusLED(i,Off);
}

void LCDMonitor::updateCursor(int row,int col)
{
	outgoing_response.command = 11;
	outgoing_response.data[0]=col; // row
	outgoing_response.data[1]=row; //col
	outgoing_response.data_length =2;
	send_packet();
	getResponse();
}

void LCDMonitor::repaintWidget(Widget *w,std::vector<std::string> &display,bool forcePaint)
{
	
	if (w->dirty() || forcePaint)
	{
		w->paint(display);
		
		for (int i=0;i<4;i++)
		{
			if (display.at(i).size()>0)
					updateLine(i,display.at(i));
		}
	}
}

bool LCDMonitor::checkAlarms()
{

	alarms.clear();

	glob_t aglob;
	int globret=glob(alarmPath.c_str(),0,0,&aglob);
	if (globret == GLOB_NOMATCH)
	{
		globfree(&aglob);
		return true;
	} 
	else if (globret == 0)
	{
		
		Dout(dc::trace,"LCDMonitor::checkAlarms()");
		for (unsigned int i=0;i<aglob.gl_pathc;i++)
		{
			// have to strip path
			std::string msg(aglob.gl_pathv[i]);
			size_t pos = msg.find_last_of('/');
			if (pos != string::npos)
				msg = msg.substr(pos+1,string::npos);
			alarms.push_back(msg);
			Dout(dc::trace,msg);
		}
		globfree(&aglob);
		return false;
	}
	
	return false;
}

bool LCDMonitor::checkGPS(int *nsats,std::string &prns,bool *unexpectedEOF)
{
	
	*unexpectedEOF=false;
	bool ret = checkFile(GPSStatusFile.c_str());
	if (!ret)
	{
		Dout(dc::trace,"LCDMonitor::checkGPS() stale file");
		return false; // don't display stale information
	}

	// status file is current so extract useful stuff
	std::ifstream fin(GPSStatusFile.c_str());
	if (!fin.good()) // not really going to happen
	{
		Dout(dc::trace,"LCDMonitor::checkGPS() stream error");
		return false;
	}

	std::string tmp;
	bool gotSats=false;

	while (!fin.eof())
	{
		getline(fin,tmp);
		if (string::npos != tmp.find("sats"))
		{	
			gotSats=true;
			std::string sbuf;
			parseConfigEntry(tmp,sbuf,'=');
			int ntmp=-1;
			ntmp=atoi(sbuf.c_str());
			if (ntmp >=0)
			{
				gotSats=true;
				*nsats=ntmp;
			}
		}
		else if (string::npos != tmp.find("prns"))
		{	
			parseConfigEntry(tmp,prns,'=');
		}
	}
	fin.close();
	*unexpectedEOF = !(gotSats || (!prns.empty()));
	Dout(dc::trace,"LCDMonitor::checkGPS() done");
	return ret;
}

bool LCDMonitor::detectNTPVersion()
{
	// NTP versioning
	// 
	// pre 4-2.2.
	// 	NTP uses A.B.C. - style release numbers.
	// 
	// The third (C) part of the version number can be:
	// 
	//    0-69 for releases on the A.B.C series.
	//    70-79 for alpha releases of the A.B+1.0 series.
	//    80+ for beta releases of the A.B+1.0 series.
	// 
	// At the moment:
	// 
	//    A is 4, for NTP version 4,
	//    B is the minor release number.
	//    C is the patch/bugfix number, and may have extra cruft in it.
	// 
	// Any extra cruft in the C portion of the number indicates an "interim" release. 
	// post 4.2.2
	// 	The syntax of a name is: Version[Point][Special][ReleaseCandidate]
	// 
	// where Version is A.B.C, and:
	// 
	//     * A is the protocol version (currently 4).
	//     * B is the major version number.
	//     * C is the minor version number. Even numbers are -stable releases, and odd numbers are -dev releases. 
	// 
	// Point is the letter p followed by an increasing number.
	// 
	// Special is currently only used for interim projects, and will generally be neither seen nor used by public releases.
	// 
	// ReleaseCandidate is the string -RC.
	 
	char buf[1024];
	bool ret=false;
	FILE *fp=popen("/usr/local/bin/ntpq -c version","r"); 
	while (fgets(buf,1023,fp) != NULL)
	{
		Dout(dc::trace,"LCDMonitor::detectNTPVersion() " << buf);
		boost::regex re("^ntpq\\s+(\\d+)\\.(\\d+)\\.(\\d+).*");
		boost::cmatch matches;
		if (boost::regex_match(buf,matches,re))
		{
			NTPProtocolVersion=boost::lexical_cast<int>(matches[1]);
			NTPMajorVersion=boost::lexical_cast<int>(matches[2]);
			NTPMinorVersion=boost::lexical_cast<int>(matches[3]);
			Dout(dc::trace,"LCDMonitor::detectNTPVersion() ver=" << NTPProtocolVersion << 
				",major=" << NTPMajorVersion << ",minor=" << NTPMinorVersion << endl);
		}
	}
	pclose(fp);
	return ret;
}

void LCDMonitor::getNTPstats(int *oldpkts,int *newpkts,int *badpkts)
{
	
	char buf[1024];
	
	FILE *fp=popen("/usr/local/bin/ntpdc -c sysstats","r");
	while (fgets(buf,1023,fp) != NULL)
	{
		Dout(dc::trace,buf);
	
		if (NTPProtocolVersion == 4){
		
			if (strstr(buf,currPacketsTag.c_str()))
			{
				char* sep = strchr(buf,':');
				if (sep!=NULL)
				{
					if (strlen(sep) > 1)
					{
						sep++;
						*newpkts=atoi(sep);
					}
				}
			}
			else if(strstr(buf,oldPacketsTag.c_str()))
			{
				char* sep = strchr(buf,':');
				if (sep!=NULL)
				{
					if (strlen(sep) > 1)
					{
						sep++;
						*oldpkts=atoi(sep);
					}
				}
			}
			else if(strstr(buf,badPacketsTag.c_str()))
			{
				char* sep = strchr(buf,':');
				if (sep!=NULL)
				{
					if (strlen(sep) > 1)
					{
						sep++;
						*badpkts=atoi(sep);
					}
				}
			}
		}
	}
	pclose(fp);
	Dout(dc::trace,"getNTPstats() " << *oldpkts << " " << *newpkts << " " << *badpkts);
}

bool LCDMonitor::checkFile(const char *fname)
{
	struct stat statbuf;
	time_t ttime;
	struct sysinfo info;
	int retval=0;
	
	time(&ttime);
	sysinfo(&info);
	retval = stat(fname,&statbuf);
	
	//Debug();
	//fprintf(stderr,"squealer_check_file(): %s modified %i rebooted %i\n",fname,
	//		(int) statbuf.st_mtime,(int) (ttime - info.uptime));
	
	if (retval == 0) /* file exists */
	{
		/* Was it created since the last boot */
		if (statbuf.st_mtime > ttime - info.uptime)
		{
			/* Is it current ? */
			if (ttime - statbuf.st_mtime < MAX_FILE_AGE)
			{
				Dout(dc::trace,"LCDMonitor::checkFile(): " << fname << " ok");
				return true;
			}
			else
			{
				Dout(dc::trace,"LCDMonitor::checkFile(): " << fname <<" too old");
				return false;
			}
		}
		else
		{
			Dout(dc::trace,"LCDMonitor::checkFile(): " << fname <<" predates boot");
			return false; /* predates boot */
		}
		
	}
	else /* file doesn't exist */
	{
		/* Have we just booted ? OK if we have */
		Dout(dc::trace,"LCDMonitor::checkFile(): " << fname << "doesn't exist");
		return (info.uptime < BOOT_GRACE_PERIOD);
	}
	
	return (retval != 0);	
}

bool LCDMonitor::serviceEnabled(const char *service)
{
	struct stat statbuf;
	std::string sname = std::string("/etc/rc.d/rc3.d/") + service;
	return (0 == stat(sname.c_str(),&statbuf));
}

bool LCDMonitor::runSystemCommand(std::string cmd,std::string okmsg,std::string failmsg)
{
	int sysret = system(cmd.c_str());
	Dout(dc::trace, cmd << " returns " << sysret);
	if (sysret == 0)
	{
		log(okmsg);
		updateLine(2,okmsg);
	}
	else
	{
		log(failmsg);
		updateLine(2,failmsg);
	}
	sleep(2);
	return (sysret==0);
}

void LCDMonitor::parseConfigEntry(std::string &entry,std::string &val,char delim)
{
	size_t pos = entry.find(delim);
	if (pos != string::npos)
	{
		val = entry.substr(pos+1);//FIXME needs checking
	}
}

void LCDMonitor::parseNetworkConfig()
{
	Dout(dc::trace,"LCDMonitor::parseNetworkConfig()");

	//
	// Set some defaults
	//
	
	ipv4gw = "192.168.1.1";
	ipv4nm = "255.255.255.0";
	ipv4ns = "192.168.1.1";
	ipv4addr="192.168.1.10";
	
	std::ifstream fin(networkConf.c_str());
	if (!fin.good())
	{
		string msg = "Couldn't open " + networkConf;
		log(msg);
		return;
	}
	string tmp;
	while (!fin.eof())
	{
		getline(fin,tmp);
		if (fin.eof())
			break;
		if (string::npos != tmp.find("GATEWAY"))
			parseConfigEntry(tmp,ipv4gw,'=');
	}
	fin.close();
	
	std::ifstream fin2(eth0Conf.c_str());
	if (!fin2.good())
	{
		string msg = "Couldn't open " + eth0Conf;
		log(msg);
		return;
	}
	
	while (!fin2.eof())
	{
		getline(fin2,tmp);
		if (fin2.eof())
			break;
		if (string::npos != tmp.find("BOOTPROTO"))
		{
			parseConfigEntry(tmp,bootProtocol,'=');
			if (bootProtocol=="dhcp")
				networkProtocol=DHCP;
			else if (bootProtocol=="static")
				networkProtocol=StaticIPV4;
		}
		else if (string::npos != tmp.find("IPADDR"))
			parseConfigEntry(tmp,ipv4addr,'=');
		else if (string::npos != tmp.find("NETMASK"))
			parseConfigEntry(tmp,ipv4nm,'=');
	}
	fin2.close();
	
	// !!! Different format 
	std::ifstream fin3(DNSconf.c_str());
	if (!fin3.good())
	{
		string msg = "Couldn't open " + DNSconf;
		log(msg);
		return;
	}
	std::string val;
	fin3 >> tmp >> val;
	while (!fin3.eof())
	{
		if (string::npos != tmp.find("nameserver"))
		{
			ipv4ns = val;
			break;
		}
		fin3 >> tmp >> val;
	}
	fin.close();
}


