#ifndef __SLIDER_WIDGET_H_
#define __SLIDER_WIDGET_H_

#include "Widget.h"

class SliderWidget:public Widget
{
	public:
	
		
		SliderWidget(std::string,int,Widget *parent=NULL);
		
		virtual bool keyEvent(KeyEvent &);
		virtual void paint(std::vector<std::string> &);
		
		virtual int value();
		void setRange(int,int);
		void setNumSteps(int);
		
		virtual void updateSetting(){;}
		
	private:
		
		std::string label;
		int val;
		int minVal,maxVal;
		int nsteps;
		int stepSize;
};

#endif
