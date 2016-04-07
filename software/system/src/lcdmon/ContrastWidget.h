#ifndef __CONTRAST_WIDGET_H_
#define __CONTRAST_WIDGET_H_

#include "SliderWidget.h"

class ContrastWidget:public SliderWidget
{
	public:
	
		ContrastWidget(std::string,int,Widget *parent=NULL);
	
		virtual void updateSetting();
		
	private:
		
		
};

#endif
