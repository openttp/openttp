//
//
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
#include <boost/algorithm/string.hpp>

#include "configurator.h"

#include "ConfirmationDialog.h"
#include "ContrastWidget.h"
#include "Dialog.h"
#include "IntensityWidget.h"
#include "IPWidget.h"
#include "KeyEvent.h"
#include "Label.h"
#include "LCDMonitor.h"
#include "MenuCallback.h"
#include "Menu.h"
#include "SliderWidget.h"
#include "Widget.h"
#include "Version.h"

#define LCDMONITOR_VERSION "1.0"

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
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	// quick and dirty here ...
	
	unsigned int nalarms=alarms.size();
	if (nalarms > 4) nalarms=4;
	if (nalarms == 0)
	{
		Label *l = new Label("     No alarms",dlg);
		l->setGeometry(0,1,20,1);
	}
	else
	{
		for (unsigned int i=0;i<nalarms;i++)
		{
			Label *l = new Label(alarms[i],dlg);
			l->setGeometry(0,i,20,1);
		}
	}
	
	execDialog(dlg);
	delete dlg;
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
	updateLine(0,"Config for DHCP");
	sleep(1);
	updateLine(1,"Done");
	sleep(1);
	log("DHCP configured");
}

void LCDMonitor::networkConfigApply()
{
	clearDisplay();
	
	// make temporary files and copy across
	
	// network
	string netcfg("/etc/sysconfig/network");
	ifstream fin(netcfg.c_str());
	string tmp;
	getline(fin,tmp);
	string ftmp("/tmp/network.tmp");
	ofstream fout(ftmp.c_str());
	while (!fin.eof())
	{
		if (string::npos != tmp.find("GATEWAY"))
			fout << "GATEWAY=" << ipv4gw << endl;
		else
			fout << tmp << endl;
			
		getline(fin,tmp);
	}
	fin.close();
	fout.close();
	
	// ifcfg-eth0
	string ifcfg("/etc/sysconfig/network-scripts/ifcfg-eth0");
	ifstream fin2(ifcfg.c_str());
	getline(fin2,tmp);
	ftmp="/tmp/ifcfg-eth0.tmp";
	ofstream fout2(ftmp.c_str());
	while (!fin2.eof())
	{
		if (string::npos != tmp.find("IPADDR"))
			fout2 << "IPADDR=" << ipv4addr << endl;
		else if (string::npos != tmp.find("NETMASK"))
			fout2 << "NETMASK=" << ipv4nm << endl;
		else
			fout2 << tmp << endl;
			
		getline(fin2,tmp);
	}
	fin2.close();
	fout2.close();
	
	updateLine(0,"Restarting network ...");
	sleep(1);
	updateLine(1,"Done");
	sleep(1);
	log("network config changed");
}

void LCDMonitor::networkConfigStaticIPv4Address()
{
	clearDisplay();
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	IPWidget *ipw = new IPWidget("ADDR",ipv4addr,IPWidget::IPV4,dlg);
	bool ret = execDialog(dlg);
	if (ret)
	{
			ipv4addr = ipw->ipAddress();
			log("Network address changed");
	}
	delete dlg;
}

void LCDMonitor::networkConfigStaticIPv4NetMask()
{
	clearDisplay();
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	IPWidget *ipw = new IPWidget("NM  ",ipv4nm,IPWidget::IPV4,dlg);
	bool ret = execDialog(dlg);
	if (ret)
	{
			ipv4nm = ipw->ipAddress();
			log("Network netmask changed");
	}
	delete dlg;
}

void LCDMonitor::networkConfigStaticIPv4Gateway()
{
	clearDisplay();
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	IPWidget *ipw = new IPWidget("GW  ",ipv4gw,IPWidget::IPV4,dlg);
	bool ret = execDialog(dlg);
	if (ret)
	{
			ipv4gw = ipw->ipAddress();
			log("Network gateway changed");
	}
	delete dlg;
}

void LCDMonitor::networkConfigStaticIPv4NameServer()
{
	clearDisplay();
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	IPWidget *ipw = new IPWidget("NS  ",ipv4ns,IPWidget::IPV4,dlg);
	bool ret = execDialog(dlg);
	if (ret)
	{
			ipv4ns = ipw->ipAddress();
			log("Network nameserver changed");
	}
	delete dlg;
}
		
void LCDMonitor::networkConfigStaticIPv6Address()
{
	clearDisplay();
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	IPWidget *ipw = new IPWidget("ADDR","192.168.1.2.32.99.255.1",IPWidget::IPV6,dlg);
	bool ret = execDialog(dlg);
	if (ret)
	{
			//ipv6addr = ipw->ipAddress();
			log("Network address changed");
	}
	delete dlg;
}

void LCDMonitor::networkConfigStaticIPv6NetMask()
{
}

void LCDMonitor::networkConfigStaticIPv6Gateway()
{
}

void LCDMonitor::networkConfigStaticIPv6NameServer()
{
}

void LCDMonitor::LCDConfig()
{
	clearDisplay();
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	IntensityWidget *iw = new IntensityWidget("Intensity",intensity,dlg);
	iw->setGeometry(0,0,20,1);
	ContrastWidget *cw = new ContrastWidget("Contrast",contrast,dlg);
	cw->setRange(0,255);
	cw->setNumSteps(10);
	cw->setGeometry(0,1,20,1);
	std::string help = "  Adjust with ";
	help += 225;
	help += " ";
	help += 223;
	Label *l= new Label(help,dlg);
	l->setGeometry(0,3,20,1);
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
		int sysret = system(ntpdRestartCommand.c_str());
		Dout(dc::trace,"LCDMonitor::restartNtpd() system() returns " << sysret);
		if (sysret == 0)
		{
			log("ntpd restarted");
			updateLine(2,"  Done");
		}
		else
		{
			log("ntpd restart failed");
			updateLine(2,"  Restart failed");
		}
		sleep(2);
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
		int sysret = system(rebootCommand.c_str());
		Dout(dc::trace,"LCDMonitor::reboot() system() returns " << sysret);
		if (sysret == 0)
		{
			log("Rebooted");
			updateLine(2,"  Done");
		}
		else
		{
			log("reboot failed");
			updateLine(2,"  Reboot failed");
		}
		sleep(2);
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
		int sysret = system(poweroffCommand.c_str());
		Dout(dc::trace,"LCDMonitor::poweroff() system() returns " << sysret);
		if (sysret == 0)
		{
			log("Powering off");
			updateLine(2,"  Done");
			
		}
		else
		{
			log("poweroff failed");
			updateLine(2,"  Poweroff failed");
		}
		sleep(2);
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
			
				int oldpkts=-1,newpkts=-1;
				getNTPstats(&oldpkts,&newpkts);
				if (oldpkts >=0 && newpkts >=0)
				{
					
					currNTPPacketCount=oldpkts+newpkts;
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
	
	parseNetworkConfig();
	
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
					if (incoming_command.data[0]==11 )// ENTER
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
					else if (incoming_command.data[0]==7) // UP RELEASE
					{
						if (currRow>0) currRow--;
						updateCursor(currRow,0);
					}
					else if (incoming_command.data[0]==8) // DOWN RELEASE
					{
						if (currRow<numItems-1) currRow++;
						updateCursor(currRow,0);
					}
					else if (incoming_command.data[0]==12) // EXIT_RELEASE
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
	
	display.push_back("");display.push_back("");display.push_back("");display.push_back("");
	
	bool retval = false;
	
	Dout(dc::trace,"LCDMonitor::execDialog()");
	startTimer(300);
	while (exec && !LCDMonitor::timeout)
	{
	
		dlg->paint(display);
		
		for (int i=0;i<4;i++)
		{
			if (display.at(i).size()>0)
				updateLine(i,display.at(i));
		}
		
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
					if (incoming_command.data[0]==11 )// ENTER
					{	
						KeyEvent ke(KeyEvent::KeyEnter,currCol,currRow);
						if (dlg->keyEvent(ke))
						{
							retval = dlg->exitCode()==Dialog::OK;
							exec=false;
						}
						break;
					}
					else if (incoming_command.data[0]==7) // UP RELEASE
					{
						KeyEvent ke(KeyEvent::KeyReleaseEvent|KeyEvent::KeyUp,currCol,currRow);
						if (!(dlg->keyEvent(ke)))
						{
							if (currRow>0) currRow--;
							updateCursor(currRow,currCol);
						}
						else
							break;
					}
					else if (incoming_command.data[0]==8) // DOWN RELEASE
					{
						KeyEvent ke(KeyEvent::KeyReleaseEvent|KeyEvent::KeyDown,currCol,currRow);
						if (!(dlg->keyEvent(ke)))
						{
							if (currRow<3) currRow++;
							updateCursor(currRow,currCol);
						}
						else
							break;
					}
					else if (incoming_command.data[0]==9) // LEFT RELEASE
					{
						KeyEvent ke(KeyEvent::KeyReleaseEvent|KeyEvent::KeyLeft,currCol,currRow);
						if (!(dlg->keyEvent(ke)))
						{
							if (currCol > 0) currCol--;
							updateCursor(currRow,currCol);
						}
						else
						{
							if (dlg->dirty())
							{
								dlg->paint(display);
		
								for (int i=0;i<4;i++)
								{
									if (display.at(i).size()>0)
									updateLine(i,display.at(i));
								}
							}
						}
					}
					else if (incoming_command.data[0]==10) // RIGHT RELEASE
					{
						KeyEvent ke(KeyEvent::KeyReleaseEvent|KeyEvent::KeyRight,currCol,currRow);
						if (!(dlg->keyEvent(ke)))
						{
							if (currCol<19) currCol++;
							updateCursor(currRow,currCol);
						}
						else
						{
							if (dlg->dirty())
							{
								dlg->paint(display);
		
								for (int i=0;i<4;i++)
								{
									if (display.at(i).size()>0)
									updateLine(i,display.at(i));
								}
							}
						}
					}
					else if (incoming_command.data[0]==12) // EXIT_RELEASE
					{
						exec=false;
						retval=false;
						Dout(dc::trace,"Dialog cancelled");
						break;
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
	
// 		Menu *networkM = new Menu("Network...");
// 		setupM->insertItem(networkM);
// 			MenuCallback<LCDMonitor> *cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigDHCP);
// 			networkM->insertItem("Set up DHCP",cb);
// 	
// 			Menu* staticIPv4 = new Menu("Set up static IPv4");
// 			networkM->insertItem(staticIPv4);
// 					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv4Address);
// 					staticIPv4 ->insertItem("Address",cb);
// 					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv4NetMask);
// 					staticIPv4 ->insertItem("Netmask",cb);
// 					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv4Gateway);
// 					staticIPv4 ->insertItem("Gateway",cb);
// 					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv4NameServer);
// 					staticIPv4 ->insertItem("Nameserver",cb);
// 			Menu *staticIPV6 = new Menu("Set up static IPv6");
// 			networkM->insertItem(staticIPV6);
// 					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv6Address);
// 					staticIPV6->insertItem("Address",cb);
// 					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv6NetMask);
// 					staticIPV6->insertItem("Netmask",cb);
// 					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv6Gateway);
// 					staticIPV6->insertItem("Gateway",cb);
// 					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv6NameServer);
// 					staticIPV6->insertItem("Nameserver",cb);				
// 			cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigApply);
// 			networkM->insertItem("Apply changes",cb);
	
		MenuCallback<LCDMonitor> *cb = new MenuCallback<LCDMonitor>(this, &LCDMonitor::LCDConfig);
		setupM->insertItem("LCD settings...",cb);
		
		displayModeM = new Menu("Display mode...");
		setupM->insertItem(displayModeM);
			
			cb = new MenuCallback<LCDMonitor>(this, &LCDMonitor::setGPSDisplayMode);
		  midGPSDisplayMode =  displayModeM ->insertItem("GPS",cb);
			MenuItem *mi = displayModeM->itemAt(midGPSDisplayMode);
			Dout(dc::trace,"midGPSDisplayMode = " << midGPSDisplayMode);
			if (mi != NULL) mi->setChecked(displayMode==GPS);
			
			cb = new MenuCallback<LCDMonitor>(this, &LCDMonitor::setNTPDisplayMode);
		  midNTPDisplayMode = displayModeM ->insertItem("NTP",cb);
			mi = displayModeM->itemAt(midNTPDisplayMode);
			if (mi != NULL) mi->setChecked(displayMode==NTP);
			
	cb = new MenuCallback<LCDMonitor>(this, &LCDMonitor::showAlarms);
	menu->insertItem("Show alarms",cb);
	
	Menu *restartM = new Menu("Restart...");
	cb = new MenuCallback<LCDMonitor>(this, &LCDMonitor::restartGPS);
	restartM->insertItem("Restart GPS",cb);
	cb = new MenuCallback<LCDMonitor>(this, &LCDMonitor::restartNtpd);
	restartM->insertItem("Restart NTPD",cb);
	cb = new MenuCallback<LCDMonitor>(this, &LCDMonitor::reboot);
	restartM->insertItem("Reboot",cb);
	cb = new MenuCallback<LCDMonitor>(this, &LCDMonitor::poweroff);
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

void LCDMonitor::getNTPstats(int *oldpkts,int *newpkts)
{
	
	char buf[1024];
	
	FILE *fp=popen("ntpdc -c sysstats","r");
	while (fgets(buf,1023,fp) != NULL)
	{
		Dout(dc::trace,buf);
	
		if (strstr(buf,"new version packets"))
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
		else if(strstr(buf,"old version packets"))
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
		
	}
	pclose(fp);
	Dout(dc::trace,"getNTPstats() " << *oldpkts << " " << *newpkts);
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

	std::ifstream fin(networkConf.c_str());
	if (!fin.good())
	{
		string msg = "Couldn't open " + networkConf;
		log(msg);
		return;
	}
	std::string tmp;
	fin >> tmp;
	while (!fin.eof())
	{
		if (string::npos != tmp.find("GATEWAY"))
			parseConfigEntry(tmp,ipv4gw,'=');
		fin >> tmp;
	}
	fin.close();
	
	std::ifstream fin2(eth0Conf.c_str());
	if (!fin2.good())
	{
		string msg = "Couldn't open " + eth0Conf;
		log(msg);
		return;
	}
	
	fin2 >> tmp;
	while (!fin2.eof())
	{
		
		if (string::npos != tmp.find("BOOTPROTO"))
			parseConfigEntry(tmp,bootProtocol,'=');
		else if (string::npos != tmp.find("IPADDR"))
			parseConfigEntry(tmp,ipv4addr,'=');
		else if (string::npos != tmp.find("NETMASK"))
			parseConfigEntry(tmp,ipv4nm,'=');
		fin2 >> tmp;
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


