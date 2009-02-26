//
//
// Modification history
//

#include "Sys.h"
#include "Debug.h"

#include <stdlib.h>
#include <unistd.h>

#include "LCDMonitor.h"

LCDMonitor *app;

int main(int argc,char **argv)
{

	Debug(lcdmonitor::debug::init());

	LCDMonitor *lcd = new LCDMonitor(argc,argv);
	app = lcd;

	lcd->run();
	
	Dout(dc::trace,"main()");

	return(EXIT_SUCCESS);
}
