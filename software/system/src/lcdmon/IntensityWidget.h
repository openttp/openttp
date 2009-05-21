#ifndef __INTENSITY_WIDGET_H_
#define __INTENSITY_WIDGET_H_

#include "SliderWidget.h"

class IntensityWidget:public SliderWidget
{
	public:
	
		IntensityWidget(std::string,int,Widget *parent=NULL);
	
		virtual void updateSetting();
		
	private:
		
		
};

#endif
