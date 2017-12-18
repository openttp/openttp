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


#include "Button.h"
#include "ConfirmationDialog.h"
#include "KeyEvent.h"
#include "Label.h"
#include "WidgetCallback.h"

ConfirmationDialog::ConfirmationDialog(std::string msg,Widget *parent):Dialog(parent)
{
	ConfirmationDialog::msg = msg;
	
	Label *l = new Label(msg,this);
	l->setGeometry(0,0,20,1);
	
	WidgetCallback<ConfirmationDialog> *cb = new WidgetCallback<ConfirmationDialog>(this,&ConfirmationDialog::ok);
	Button *b = new Button("Yes",cb,this);
	b->setGeometry(3,1,3,1);
	b->setFocusWidget(true);
	
	cb = new WidgetCallback<ConfirmationDialog>(this,&ConfirmationDialog::cancel);
	b = new Button("No",cb,this);
	b->setGeometry(3,2,2,1);
	
}




