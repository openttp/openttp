#ifndef __LCD_MONITOR_H_
#define __LCD_MONITOR_H_

#include <signal.h>
#include <time.h>

#include <sstream>
#include <string>
#include <vector>

#include "CFA635.h"

class Menu;
class Dialog;

class LCDConfigure:public CFA635
{
	public:
	
		LCDConfigure(int,char **);
		~LCDConfigure();
	
		void run();
		
	private:
	
		enum LEDState {Off,RedOn,GreenOn};
		
		static void signalHandler(int);
		void startTimer(long secs=10);
		void stopTimer();
		struct sigaction sa;
		static bool timeout;
		
		void init();
		void log(std::string);
		void configure();
		void touchLock();

		void showHelp();
		void showVersion();
		
		void getResponse();
		void clearDisplay();
		void storeState();
		void updateLine(int,std::string);
		void updateStatusLED(int,LEDState);
		void statusLEDsOn();
		
		
		std::string user;
		
		std::string status[4];
		LEDState    statusLED[4];
		std::vector<std::string> alarms;
		
		std::string lockFile;
		
	
};

#endif
