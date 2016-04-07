//
//
//

#include "Sys.h"
#include "Debug.h"

#include "LCDMonitor.h"
#include "ContrastWidget.h"

extern LCDMonitor *app;

ContrastWidget::ContrastWidget(std::string l,int ival,Widget*parent):
	SliderWidget(l,ival,parent)
{
}

	
void ContrastWidget::updateSetting()
{
	COMMAND_PACKET cmd;
	cmd.command=13;
	cmd.data[0]=value();
	cmd.data_length=1;
	app->sendCommand(cmd);
}

