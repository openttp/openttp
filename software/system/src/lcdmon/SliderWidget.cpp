//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Michael J. Wouters
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Modification history
//


#include "Debug.h"

#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>

using namespace std;

extern ostream *debugStream;

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
	DBGMSG(debugStream,TRACE, "for " << label);
	
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
			DBGMSG(debugStream,TRACE," value = " << val);
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
	DBGMSG(debugStream,TRACE,"");
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


