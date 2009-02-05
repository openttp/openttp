//
//
//

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/timex.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
 
#include <iostream>
#include <stack>
#include <fstream>
#include <sstream>

#include "configurator.h"

#include "ConfirmationDialog.h"
#include "Dialog.h"
#include "IPWidget.h"
#include "KeyEvent.h"
#include "Label.h"
#include "LCDMonitor.h"
#include "MenuCallback.h"
#include "Menu.h"
#include "Widget.h"

#define BAUD 115200
#define PORT "/dev/ttyUSB0"
#define DEFAULT_LOG_FILE        "/logs/lcdmonitor.log"
#define DEFAULT_LOCK_FILE       "/logs/lcdmonitor.lock"
#define PRETTIFIER "*********************************************"

#define BOOT_GRACE_PERIOD 300 // in seconds
#define MAX_FILE_AGE 300


using namespace std;

bool LCDMonitor::timeout=false;

LCDMonitor::LCDMonitor(int argc,char **argv)
{

	char c;
	
	while ((c=getopt(argc,argv,"hvd")) != EOF)
	{
		switch(c)
  	{
			case 'h':showHelp(); exit(EXIT_SUCCESS);
			case 'v':showVersion();exit(EXIT_SUCCESS);
			case 'd':debugOn=true;break; 
		}
	}
	
	init();
	
	makeMenu();
	
}

LCDMonitor::~LCDMonitor()
{
	Uninit_Serial();
}


void LCDMonitor::showAlarms()
{
	clearDisplay();
	Dialog *dlg = new Dialog();
	dlg->setGeometry(0,0,20,4);
	// quick and dirty here ...
	unsigned int nalarms=alarms.size();
	if (nalarms > 4) nalarms=4;
	std::cerr << nalarms << endl;
	for (unsigned int i=0;i<nalarms;i++)
	{
		Label *l = new Label(alarms[i],dlg);
		l->setGeometry(0,i,20,1);
		std::cerr << alarms[i] << endl;
	}
	
	execDialog(dlg);
	delete dlg;
}

void LCDMonitor::surveyAntenna()
{
	clearDisplay();
	updateLine(0,"Getting new coords..");
	sleep(1);
	updateLine(1,"Done");
	sleep(1);
	log("antenna coordinates updated");
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
		Debug(tmp);
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
					
void LCDMonitor::restartGPS()
{
	log("GPS receiver restarted");
}

void LCDMonitor::restartNtpd()
{
	log("ntpd restarted");
}

void LCDMonitor::reboot()
{
	
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm reboot");
	bool ret = execDialog(dlg);
	if (ret)
	{
		log("reboot");
	}
	delete dlg;
	Debug("Deleted");
}

void LCDMonitor::poweroff()
{
	
	clearDisplay();
	ConfirmationDialog *dlg = new ConfirmationDialog("Confirm poweroff");
	bool ret = execDialog(dlg);
	if (ret)
	{
		log("poweroff");
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
	
	bool GPSOK=false;
	sprintf(buf,"GPS sats=%d",0);
	updateLine(1,buf);
	if (GPSOK)
		updateStatusLED(1,GreenOn);
	else
		updateStatusLED(1,RedOn);

	// only run these checks every 30s or so
	if (tnow - lastLazyCheck > 30)
	{
		
		clearAlarms();
		
		bool ntpOK=checkNTP();
		bool prs10OK=checkPRS10();

		updateLine(2,buf);
		if (GPSOK && ntpOK && prs10OK)
		{
			updateLine(2,"SYSTEM OK");
			updateStatusLED(2,GreenOn);
		}
		else
		{
			updateLine(2,"SYSTEM ALARM");
			updateStatusLED(2,RedOn);
		}
		lastLazyCheck=tnow;
	}
	
	// leap second status
	struct timex tx;
	tx.modes=0;
	int clkstate=adjtimex(&tx);
	switch (clkstate)
	{
		case TIME_OK:
			snprintf(buf,20,"TIME OK");
			break;
		case TIME_INS:
			snprintf(buf,20,"LS INS TODAY");
			break;
		case TIME_DEL:
			snprintf(buf,20,"LS DEL TODAY");
			break;
		case TIME_OOP:
			snprintf(buf,20,"LS IN PROGRESS");
			break;
		case TIME_WAIT:
			snprintf(buf,20,"LS OCCURRED");
			break;
		case TIME_BAD:
			snprintf(buf,20,"UNSYNCHRONIZED");
			break;	
	}
	updateLine(3,buf);
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
	
	startTimer();
	while (showMenu && !LCDMonitor::timeout)
	{
		// show the menu
		clearDisplay();
		int numItems=currMenu->numItems();
		for (int i=0;i<numItems;i++)
			updateLine(i,">"+currMenu->itemAt(i)->text());
		
		updateCursor(currRow,0);
		while (!LCDMonitor::timeout)
		{
			usleep(10000);
			if(packetReceived())
			{
				stopTimer();
				startTimer();
				if(incoming_command.command==0x80)
				{
					if (incoming_command.data[0]==11 )// ENTER
					{
						if (debugOn) fprintf(stderr,"Selected %i\n",currRow);
						MenuItem *currItem = currMenu->itemAt(currRow);
						if (currItem)
						{
							if (currItem->isMenu())
							{
								if (debugOn) fprintf(stderr,"Selected menu %i (menu)\n",currRow);
								currMenu = (Menu *) currItem;
								menus.push(currMenu);
								currRow=0;
							}
							else
							{
								stopTimer();
								currItem->exec();
								startTimer();
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
	
	Debug("LCDMonitor::execDialog()");
	startTimer();
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
				startTimer();
				
				if(incoming_command.command==0x80)
				{
					if (incoming_command.data[0]==11 )// ENTER
					{	
						KeyEvent ke(KeyEvent::KeyEnter,currCol,currRow);
						if (dlg->keyEvent(ke))
						{
							retval = dlg->exitCode()==Dialog::OK;
							exec=false;
							Debug("Got there");
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
					}
					else if (incoming_command.data[0]==10) // RIGHT RELEASE
					{
						KeyEvent ke(KeyEvent::KeyReleaseEvent|KeyEvent::KeyRight,currCol,currRow);
						if (!(dlg->keyEvent(ke)))
						{
							if (currCol<19) currCol++;
							updateCursor(currRow,currCol);
						}
					}
					else if (incoming_command.data[0]==12) // EXIT_RELEASE
					{
						exec=false;
						retval=false;
						Debug("Dialog cancelled");
						break;
					}
				}
			}
		}
	}
	Debug("Dialog finished");
	stopTimer();
	return retval;
}

//
// private
//

void LCDMonitor::signalHandler(int sig)
{
	switch (sig)
	{
		case SIGALRM: fprintf(stderr,"SIGALRM\n");LCDMonitor::timeout=true;break;// can't call Debug()
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
	string  homedir;
	char   *hd;
	struct stat statbuf;

	FILE *fd;
	pid_t oldpid;
	
	
	if ((hd=getenv("HOME"))==NULL) /* get our home directory */
	{
		cerr << "Couldn't get $HOME" << endl;
		exit(EXIT_FAILURE);
	}
	
	logFile=hd;
	logFile+=DEFAULT_LOG_FILE;
	
	lockFile = hd;
	lockFile += DEFAULT_LOCK_FILE;
	
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
	//snprintf(sbuf,BUF_LEN-1,"announcer v%s, last modified %s",
	//	ANNOUNCER_VERSION,LAST_MODIFIED);
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
	

	configure();
	
	if(Serial_Init(PORT,BAUD))
	{
		if (debugOn) fprintf(stderr,"Could not open port \"%s\" at \"%d\" baud.\n",PORT,BAUD);
		exit(EXIT_FAILURE);
	}
	else
	{
    if (debugOn) fprintf(stderr,"\"%s\" opened at \"%d\" baud.\n\n",PORT,BAUD);
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

	lastLazyCheck=0; // trigger an immediate check
	
	statusLEDsOff();
}


void LCDMonitor::configure()
{
	#define BUF_LEN 1024
	char *hd;
	HashEntry *htab[ALPHABETIC_HASH_SIZE];
	char buf[BUF_LEN];
	char **sections,*stmp;
	int nsections;

	// set some sensible defaults
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
	rbStatusFile="/home/cvgps/logs/rb.status";
	GPSStatusFile="home/cvgps/logs/rcvr.status";

	string squealerConfig("/home/cvgps/etc/squealer.conf");
	string cctfConfig("home/cvgps/etc/cctf.setup");
	
	if ((hd=getenv("HOME"))==NULL) /* get our home directory */
	{
		cerr << "Couldn't get $HOME" << endl;
		exit(EXIT_FAILURE);
	}
	
	string config(hd);
	config += "/etc/lcdmonitor.conf";
	
	if (!configfile_getsections(&sections,&nsections,config.c_str()))
	{
		ostringstream msg;
		msg << "failed to read " << config;
		log(msg.str());
		exit(EXIT_FAILURE);
	}

	hash_init(htab,ALPHABETIC_HASH_SIZE);
	
	for (int i=0;i<nsections;i++)
	{
		
		if (!configfile_parse(htab,sections[i],config.c_str()))
		{
			ostringstream msg;
			msg << "failed to parse section " << sections[i] << " in " << config;
			log(msg.str());
			exit(EXIT_FAILURE);
		}
		
		if (strstr("General",sections[i]))
		{
			if (hash_get_stringvalue(htab,"NTP user",&stmp))
				NTPuser=stmp;
			else
				log("NTP user not found in config file");
				
			if (hash_get_stringvalue(htab,"GPSCV user",&stmp))
				GPSCVuser=stmp;
			else
				log("GPSCV user not found in config file");
				
			if (hash_get_stringvalue(htab,"Squealer config",&stmp))
				squealerConfig=stmp;
			else
				log("Squealer config not found in config file");
				
			if (hash_get_stringvalue(htab,"CCTF config",&stmp))
				cctfConfig=stmp;
			else
				log("CCTF config not found in config file");
			
		}
		if (strstr("Network",sections[i]))
		{
			if (hash_get_stringvalue(htab,"DNS",&stmp))
				DNSconf=stmp;
			else
				log("DNS not found in config file");
			if (hash_get_stringvalue(htab,"Network",&stmp))
				networkConf=stmp;
			else
				log("Network not found in config file");
			if (hash_get_stringvalue(htab,"Eth0",&stmp))
				eth0Conf=stmp;
			else
				log("Eth0 not found in config file");
			
		}
		
		hash_clear(htab,ALPHABETIC_HASH_SIZE);
	}
	
	
	// read cctf.setup
	
	if (!configfile_getsections(&sections,&nsections,cctfConfig.c_str()))
	{
		ostringstream msg;
		msg << "failed to read " << config;
		log(msg.str());
		exit(EXIT_FAILURE);
	}
	
	for (int i=0;i<nsections;i++)
	{
		
		if (!configfile_parse(htab,sections[i],cctfConfig.c_str()))
		{
			ostringstream msg;
			msg << "failed to parse section " << sections[i] << " in " << cctfConfig;
			log(msg.str());
			exit(EXIT_FAILURE);
		}
		
		if (strstr("Main",sections[i]))
		{
			if (hash_get_stringvalue(htab,"receiver name",&stmp))
				receiverName=stmp;
			else
				log("receiver name not found in config file");
			
		}
		
		hash_clear(htab,ALPHABETIC_HASH_SIZE);
	}
	
	
	// read squealer.conf
	
	if (!configfile_getsections(&sections,&nsections,squealerConfig.c_str()))
	{
		ostringstream msg;
		msg << "failed to read " << squealerConfig;
		log(msg.str());
		exit(EXIT_FAILURE);
	}
	
	for (int i=0;i<nsections;i++)
	{
		
		if (!configfile_parse(htab,sections[i],squealerConfig.c_str()))
		{
			ostringstream msg;
			msg << "failed to parse section " << sections[i] << " in " << squealerConfig;
			log(msg.str());
			exit(EXIT_FAILURE);
		}
		
		if (strstr("General",sections[i]))
		{
			if (hash_get_stringvalue(htab,"Rb Status File",&stmp))
				rbStatusFile=stmp;
			else
				log("Rb Status File not found in config file");
			
			if (hash_get_stringvalue(htab,"GPS Status File",&stmp))
				GPSStatusFile=stmp;
			else
				log("Rb Status File not found in config file");
				
		}
		
		hash_clear(htab,ALPHABETIC_HASH_SIZE);
	}
	
	#undef BUF_LEN
}

void LCDMonitor::log(std::string msg)
{
	FILE *fd;
	
	Debug(msg);
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
	cout << "Usage: lcdmonitor -dhv" << endl;
	cout << "  -d debugging on" << endl;
	cout << "  -h show this help" << endl;
	cout << "  -v show version" << endl;
}

void LCDMonitor::showVersion()
{
	cout << "lcdmonitor " << endl;
	cout << "This ain't no stinkin' Perl script!" << endl;
}

		
void LCDMonitor::makeMenu()
{
	menu = new Menu("Main menu");
	
	Menu *setupM = new Menu("Setup...");
	menu->insertItem(setupM);
	
		MenuCallback<LCDMonitor> *cb = new MenuCallback<LCDMonitor>(this, &LCDMonitor::surveyAntenna);
		setupM->insertItem("Survey antenna",cb);
	
		Menu *networkM = new Menu("Network...");
		setupM->insertItem(networkM);
			cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigDHCP);
			networkM->insertItem("Set up DHCP",cb);
	
			Menu* staticIPv4 = new Menu("Set up static IPv4");
			networkM->insertItem(staticIPv4);
					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv4Address);
					staticIPv4 ->insertItem("Address",cb);
					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv4NetMask);
					staticIPv4 ->insertItem("Netmask",cb);
					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv4Gateway);
					staticIPv4 ->insertItem("Gateway",cb);
					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv4NameServer);
					staticIPv4 ->insertItem("Nameserver",cb);
			Menu *staticIPV6 = new Menu("Set up static IPv6");
			networkM->insertItem(staticIPV6);
					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv6Address);
					staticIPV6->insertItem("Address",cb);
					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv6NetMask);
					staticIPV6->insertItem("Netmask",cb);
					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv6Gateway);
					staticIPV6->insertItem("Gateway",cb);
					cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigStaticIPv6NameServer);
					staticIPV6->insertItem("Nameserver",cb);				
			cb = new MenuCallback<LCDMonitor>(this,&LCDMonitor::networkConfigApply);
			networkM->insertItem("Apply changes",cb);
	
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
		Debug("Timed out waiting for a response");
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
	// a bit of optimization to cuto down on I/O and flicker
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

void LCDMonitor::clearAlarms()
{
	alarms.clear();
}

void LCDMonitor::addAlarm(std::string a)
{
	alarms.push_back(a);
}

bool LCDMonitor::checkNTP()
{
	struct timex tx;
	adjtimex(&tx);
	std::string alarm="NTPD:";
	bool ok=true;
	if (tx.status == TIME_BAD)
	{
		ok=false;
		alarm +="unsynchronized";
	}
	return ok;
}

bool LCDMonitor::checkPRS10()
{
	
	// is it being logged ?
	bool ret = checkFile(rbStatusFile.c_str());
	if (!ret)
	{
		addAlarm("PRS10:No logging");
		return false;
	}
	// status file is current so check if Rb is locked 
	return ret;
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
				Debug("LCDMonitor::checkFile(): file ok");
				return true;
			}
			else
			{
				Debug("LCDMonitor::checkFile(): file too old");
				return false;
			}
		}
		else
		{
			Debug("LCDMonitor::checkFile(): file predates boot");
			return false; /* predates boot */
		}
		
	}
	else /* file doesn't exist */
	{
		/* Have we just booted ? OK if we have */
		Debug("LCDMonitor::checkFile(): file doesn't exist");
		return (info.uptime < BOOT_GRACE_PERIOD);
	}
	
	return (retval != 0);	
}


void LCDMonitor::parseConfigEntry(std::string &entry,std::string &val,char delim)
{
	size_t pos = entry.find(delim);
	if (pos != string::npos)
	{
		val = entry.substr(pos+1);
		Debug(val);
	}
}

void LCDMonitor::parseNetworkConfig()
{
	
	
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
		Debug(tmp);
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
		
		Debug(tmp);
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


