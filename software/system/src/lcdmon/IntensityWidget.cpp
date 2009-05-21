//
//
//

#include "Sys.h"
#include "Debug.h"

#include "LCDMonitor.h"
#include "IntensityWidget.h"

extern LCDMonitor *app;

IntensityWidget::IntensityWidget(std::string l,int ival,Widget*parent):
	SliderWidget(l,ival,parent)
{
}

	
void IntensityWidget::updateSetting()
{
	COMMAND_PACKET cmd;
	cmd.command=14;
	cmd.data[0]=value();
	cmd.data_length=1;
	app->sendCommand(cmd);
}

