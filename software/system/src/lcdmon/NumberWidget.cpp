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
// Copied off IPWidget.h 2017-08-28 Louis Marais

#include "Sys.h"
#include "Debug.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>

#include "NumberWidget.h"
#include "KeyEvent.h"

NumberWidget::NumberWidget(int value,Widget *parent):Widget(parent)
{
	num_val = value;
	std::ostringstream oss;
	oss << std::setfill('0') << std::setw(4) << value;
	str_val = oss.str();
}

NumberWidget::~NumberWidget()
{
	Dout(dc::trace,"NumberWidget::~NumberWidget() " << num_val);
}

bool NumberWidget::keyEvent(KeyEvent &ke)
{
	// Is it even in our space
	Dout(dc::trace,"NumberWidget::keyEvent at " << ke.x() << " " << ke.y());
	
	if (!contains(ke.x(),ke.y())) return false;
	setFocusWidget(true);
	
	int ev=ke.event();
	if ((ev & KeyEvent::KeyRelease))
	{
		if ((ev & KeyEvent::KeyUp) || (ev & KeyEvent::KeyDown))
		{
			int col=ke.x()-x();
			int row=ke.y()-y();
			if ((col >= 0 ) && (col < 4)) // tweak the timeout digit
			{
				int sgn=-1;
				if (ev & KeyEvent::KeyUp) sgn=1;
				int pos = col;
				int pwr = 3 - pos;
				int incr = sgn;
				for (int i=0;i<pwr;i++)
					incr*=10;
				std::string strs = str_val;
				std::istringstream strss(strs);
				int istrs;
				strss >> istrs;
				istrs += incr;
				// Range is 0..9999 for timeout
				if (istrs < 0 || istrs > 9999) istrs -= incr;
				std::ostringstream oss;
				oss << std::setfill('0') << std::setw(4) << istrs;
				str_val.replace(0,4,oss.str());
				setDirty(true);
				return true;
			}
		}
		else if ((ev & KeyEvent::KeyRight))
		{
			if (ke.x()-x() < 3)
			{
				setFocus(ke.x()+1,ke.y());
				Dout(dc::trace,ke.x() + 1);
				return true;
			}
			else
			{
				setFocusWidget(false);
				setFocus(x(),y());
				return false;
			}
		}
		else if ((ev & KeyEvent::KeyLeft))
		{
			if (ke.x() - x() > 0)
			{
				setFocus(ke.x()-1,ke.y());
				return true;
			}
			else
			{
				setFocusWidget(false);
				setFocus(x(),y());
				return false;
			}
		}
	}
	return false;
}

void NumberWidget::paint(std::vector<std::string> &display)
{
	std::string s = str_val;
	display.at(y()).replace(x(),s.size(),s); 
}

int NumberWidget::value()
{
	std::istringstream(str_val) >> num_val;
	return num_val;
}