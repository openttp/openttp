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

#include "Label.h"
#include "MessageBox.h"
#include "KeyEvent.h"


MessageBox::MessageBox(std::string line1,std::string line2,std::string line3,std::string line4,Widget *parent):Dialog(parent)
{
	
	MessageBox::line1 = new Label(line1,this);
	MessageBox::line1->setGeometry(0,0,20,1);
	
	MessageBox::line2 = new Label(line2,this);
	MessageBox::line2->setGeometry(0,1,20,1);
	
	MessageBox::line3 = new Label(line3,this);
	MessageBox::line3->setGeometry(0,2,20,1);
	
	MessageBox::line4 = new Label(line4,this);
	MessageBox::line4->setGeometry(0,3,20,1);
	
}

void MessageBox::setLine(int l,std::string  txt)
{
	if (l==0)
		line1->setText(txt);
	else if (l==1)
		line2->setText(txt);
	else if (l==2)
		line3->setText(txt);
	else if (l==3)
		line4->setText(txt);
}


bool MessageBox::keyEvent(KeyEvent &)
{
	// any key is bye bye
	ok();
	return true;
}


