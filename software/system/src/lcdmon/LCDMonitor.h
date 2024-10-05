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

#ifndef __LCD_MONITOR_H_
#define __LCD_MONITOR_H_

#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include <sstream>
#include <string>
#include <vector>

#include "CFA635.h"

class Menu;
class Dialog;
class Widget;

class NetworkInterface
{
	public:
		NetworkInterface(){};
		~NetworkInterface(){};
		
		enum NetworkProtocol {IPV4,IPV6};
		
		bool DHCP;
		
		int networkProtocol;
		std::string name;
		std::string address;
		std::string netmask;
		std::string gateway;
		
		std::vector<std::string> nameservers;
};


class LCDMonitor:public CFA635
{
	public:
	
		LCDMonitor(int,char **);
		~LCDMonitor();
	
		void run();
		void showStatus();
		void execMenu();
		bool execDialog(Dialog *);
		
		void sendCommand(COMMAND_PACKET &);
		 
		// callbacks for whatever - public so menu can access
		
		void showAlarms();
		void showSysInfo();
		void showIP();
		
		void networkConfigDHCP(int);
		void networkConfigStaticIP4(int);
	
		void LCDConfig();
		void LCDBacklightTimeout();
		void setGPSDisplayMode();
		void setNTPDisplayMode();
		void setRefDisplayMode();
#ifdef MULTIRX
		void setGLOBDDisplayMode();
#endif	
		void restartRx();
		void restartNtpd();
		void reboot();
		void poweroff();
		
	private:
	
		enum LEDState {Off,RedOn,GreenOn,Unknown};
		enum DisplayMode {GPS,NTP,REF,GLOBD};
		enum AddressAssignment {DHCP,Static};
		enum Reference {LCXO,ULN1100,Furuno};
		enum NTPDaemon {NTPD,CHRONYD};
		
		void getNetworkInterfaces(std::string &, std::string &,std::string &);
		
		static void signalHandler(int);
		void startTimer(long secs=10);
		void stopTimer();
		struct sigaction sa;
		static bool timeout;
		
		void init();
		void configure();
		void updateConfig(std::string,std::string,std::string);
		void log(std::string );
		void touchLock();

		void showHelp();
		void showVersion();
		
		void makeMenu();
	
		void getResponse();
		void clearDisplay();
		void showCursor(bool);
		void updateLine(int,std::string);
		void updateStatusLED(int,LEDState);
		void statusLEDsOff();
		void updateCursor(int,int);
		void repaintWidget(Widget *,std::vector<std::string> &,bool forcePaint=false);
		
		bool checkAlarms();
		bool checkRx(int *,std::string &,bool *);
		//              Status        ffe           EFC%          health
		bool checkRef(std::string &,std::string &,std::string &,std::string &,bool *);
		bool detectNTPVersion();
		void getNTPstats(int *,int *,int *);
		
		bool checkFile(const char *);
		bool serviceEnabled(const char *);
		bool restartNetworking();
		bool runSystemCommand(std::string,std::string,std::string);
		bool runCommand(std::string,std::vector<std::string> &);
		
		std::string relativeToAbsolutePath(std::string,std::string);
		std::string  prefix2netmask(std::string);
		std::string  netmask2prefix(std::string);
		std::string quote(std::string);
		
		time_t lastLazyCheck;
		
		void networkConfigDHCPLAN0();
		void networkConfigStaticIP4LAN0();
		void networkConfigDHCPLAN1();
		void networkConfigStaticIP4LAN1();
		
		void parseNetworkConfig_IfConfig();
		void parseNetworkConfig_NetPlan();
		void parseConfigEntry(std::string &,std::string &,char );

		bool writeNetPlanConfig(int);
		bool writeIfConfig(int);
		
		std::string poweroffCommand;
		std::string rebootCommand;
		std::string ntpdRestartCommand;
		std::string gpsRxRestartCommand;
		std::string gpsLoggerLockFile;

		std::vector<NetworkInterface *> nets;
		std::string NPrenderer,NPversion; // Netplan
		int addressAssignmentLAN[2]; // Static or DHCP 
		int idxLAN[2];  // index in nets
		std::string LANIFname[2],netCfg;
		
#ifndef NETPLAN
		std::string DNSconf,networkConf;
#endif
		int NTPDaemon;
		int NTPProtocolVersion,NTPCLIMajorVersion,NTPCLIMinorVersion; 
		std::string currPacketsTag,oldPacketsTag,badPacketsTag;
		
		std::string status[4];
		LEDState    statusLED[4];
		std::vector<std::string> alarms;
		
		bool showPRNs;

		Menu *menu,*displayModeM,*networkM,*LAN0M,*LAN1M,*lcdSetup;
		int midGPSDisplayMode,midNTPDisplayMode,midGPSDODisplayMode; // some menu items we want to track
		int midDHCP0,midStaticIP40,midDHCP1,midStaticIP41;
		
		int reference;
		
		std::string logFile;
		std::string lockFile;
		std::string alarmPath;
		std::string sysInfoConf;
		
		std::string NTPuser,GPSCVuser;
		std::string cvgpsHome,ntpadminHome;
		
		
		std::string receiverName;
		std::string refStatusFile,rxStatusFile;
		
#ifdef MULTIRX
		std::string GLONASSStatusFile,BeidouStatusFile;
		int midGLOBDDisplayMode;
		//              GLOsats       BDsats
		bool checkGLOBD(std::string &,std::string &,bool *);
		bool showGLOBD;
#endif

		// some settings
		int intensity;
		int contrast;
		int displayMode;
		int displaytimeout;
		bool displaybacklightoff;
		
		
		//
		struct timeval lastNTPtrafficPoll,currNTPtrafficPoll;
		int    lastNTPPacketCount,currNTPPacketCount;
		
		std::string configFile;
		
};

#endif
