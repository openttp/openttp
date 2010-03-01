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
		
		void restartGPS();
		void restartNtpd();
		void reboot();
		void poweroff();
		
	private:
	
		enum LEDState {Off,RedOn,GreenOn,Unknown};
		enum DisplayMode {GPS,NTP};
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
		bool detectNTPVersion();
		void getNTPstats(int *,int *,int *);
		
		bool checkFile(const char *);
		void restartNetworking();
		bool runSystemCommand(std::string,std::string,std::string);
		
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
		int midGPSDisplayMode,midNTPDisplayMode; // some menu items we want to track
		int midDHCP,midStaticIP4,midStaticIP6;
		
		std::string logFile;
		std::string lockFile;
		std::string alarmPath;
		std::string sysInfoConf;
		
		std::string NTPuser,GPSCVuser;
		
		std::string DNSconf,networkConf,eth0Conf;
		
		std::string receiverName;
		std::string rbStatusFile,GPSStatusFile; 
		
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
