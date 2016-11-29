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
		
		void networkDisable();
		void networkConfigDHCP();
		void networkConfigStaticIP4();
	
		void LCDConfig();
		void setGPSDisplayMode();
		void setNTPDisplayMode();
		void setGPSDODisplayMode();
		
		void restartGPS();
		void restartNtpd();
		void reboot();
		void poweroff();
		
	private:
	
		enum LEDState {Off,RedOn,GreenOn,Unknown};
		enum DisplayMode {GPS,NTP,GPSDO};
		enum NetworkProtocol {DHCP,StaticIPV4,StaticIPV6};
		
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
		bool checkGPS(int *,std::string &,bool *);
		//              Status        ffe           EFC%          health
		bool checkGPSDO(std::string &,std::string &,std::string &,std::string &,bool *);
		bool detectNTPVersion();
		void getNTPstats(int *,int *,int *);
		
		bool checkFile(const char *);
		bool serviceEnabled(const char *);
		void restartNetworking();
		bool runSystemCommand(std::string,std::string,std::string);
		
		std::string makeAbsoluteFilePath(std::string);
		
		time_t lastLazyCheck;
		
		
		void parseNetworkConfig();
		void parseConfigEntry(std::string &,std::string &,char );

		std::string poweroffCommand;
		std::string rebootCommand;
		std::string ntpdRestartCommand;
		std::string gpsRxRestartCommand;
		std::string gpsLoggerLockFile;

		std::string bootProtocol;
		std::string ipv4addr,ipv4nm,ipv4gw,ipv4ns;
		
		int NTPProtocolVersion,NTPMajorVersion,NTPMinorVersion; 
		std::string currPacketsTag,oldPacketsTag,badPacketsTag;
		
		std::string status[4];
		LEDState    statusLED[4];
		std::vector<std::string> alarms;
		
		bool showPRNs;

		Menu *menu,*displayModeM,*protocolM;
		int midGPSDisplayMode,midNTPDisplayMode,midGPSDODisplayMode; // some menu items we want to track
		int midDHCP,midStaticIP4,midStaticIP6;
		
		std::string logFile;
		std::string lockFile;
		std::string alarmPath;
		std::string sysInfoConf;
		
		std::string NTPuser,GPSCVuser;
		
		std::string DNSconf,networkConf,eth0Conf;
		
		std::string receiverName;
		std::string refStatusFile,GPSStatusFile,GPSDOStatusFile; 
		
		// some settings
		int intensity;
		int contrast;
		int displayMode;
		int networkProtocol;
		// 
		struct timeval lastNTPtrafficPoll,currNTPtrafficPoll;
		int    lastNTPPacketCount,currNTPPacketCount;
		
};

#endif
