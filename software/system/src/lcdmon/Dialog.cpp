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


#include "Sys.h"
#include "Debug.h"

#include "Dialog.h"
#include "KeyEvent.h"

Dialog::Dialog(Widget *parent):Widget(parent)
{
	exitCode_=OK;
	finished_=false;
}

Dialog::~Dialog()
{
}

// bool Dialog::keyEvent(KeyEvent &ke)
// {
// 	
// 	int ev=ke.event();
// 	if (ev & KeyEvent::KeyEnter)
// 	{
// 		//  this would be where we validated any changes
// 		return true;
// 	}
// 	
// 	Widget::keyEvent(ke);
// 	
// 	return false;
// }

int Dialog::exitCode()
{
	return exitCode_;
}

void Dialog::ok()
{
	Dout(dc::trace,"Dialog::ok()");
	exitCode_=OK;
	finished_=true;
}

void Dialog::cancel()
{
	Dout(dc::trace,"Dialog::cancel()");
	exitCode_=Cancel;
	finished_=true;
}
