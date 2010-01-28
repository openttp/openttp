//
//
//

#include "Sys.h"
#include "Debug.h"

#include <cmath>

#include "SliderWidget.h"
#include "KeyEvent.h"

SliderWidget::SliderWidget(std::string label,int initialValue,Widget *parent):Widget(parent)
{
	// labels can only be 9 characters so truncate as required
	SliderWidget::label = label.substr(0,9);
	val = initialValue;
	minVal=0;
	maxVal=100;
	nsteps=10;
	stepSize= (maxVal-minVal)/nsteps;
}

		
bool SliderWidget::keyEvent(KeyEvent &ke)
{
	Dout(dc::trace,"SliderWidget::keyEvent() for " << label);
	
	if (!contains(ke.x(),ke.y())) return false;
	
	int ev=ke.event();
	if (ev & KeyEvent::KeyRelease)
	{
		if  ((ev & KeyEvent::KeyLeft) || (ev & KeyEvent::KeyRight))
		{
			if (ev & KeyEvent::KeyLeft)
				val -= stepSize;
			else
				val += stepSize;
			if (val <minVal)   val=minVal;
			if (val >maxVal) val=maxVal;
			Dout(dc::trace,"SliderWidget::keyEvent value = " << val);
			setDirty(true);
			updateSetting();
			setFocusWidget(true); // shoudl be true anyway
			return true;
		}
		else if ((ev & KeyEvent::KeyUp) || (ev & KeyEvent::KeyDown))
		{
			setFocusWidget(false);
			return false;
		}
	}
	return false;
}

void SliderWidget::paint(std::vector<std::string> &display)
{
	Dout(dc::trace,"SliderWidget::paint()");
	std::string txt=label;
	// pad with whitespace out to 10 characters
	for (unsigned int i=label.size()+1;i<=10;i++)
		txt += ' ';
	// now add the slider bar - characters are 5 pixels wide
	// character 208 = 5 wide &c 212 = 1 wide
	int n208 = (int) (trunc((float)val/(float)(maxVal-minVal) * 10.0));
	for (int i=0;i<n208;i++)
		txt += 208;
	int rem = trunc(((float)val/(float)(maxVal-minVal) * 100.0 - n208*10)/2);
	if (rem > 0)
		txt += 213 - rem;
	display.at(y())=txt;
	setDirty(false);
}

int SliderWidget::value()
{
	return val;
}

void SliderWidget::setRange(int min,int max)
{
	minVal=min;
	maxVal=max;
	stepSize= (maxVal-minVal)/nsteps;
}

void SliderWidget::setNumSteps(int n)
{
	nsteps=n;
	stepSize= (maxVal-minVal)/nsteps;
}


