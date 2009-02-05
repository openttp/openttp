//
//
// Modification history
//

#include <stdlib.h>
#include <unistd.h>

#include "LCDMonitor.h"

int main(int argc,char **argv)
{
	LCDMonitor lcd(argc,argv);

	lcd.run();
	
	return(EXIT_SUCCESS);
}
