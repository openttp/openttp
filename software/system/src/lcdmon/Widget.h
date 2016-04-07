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

#ifndef __WIDGET_H_
#define __WIDGET_H_

#include <string>
#include <vector>

class Functor;
class KeyEvent;

class Widget
{
	public:
	
		Widget(Widget *parent=NULL);
		virtual ~Widget();
		
		virtual bool keyEvent(KeyEvent &);
		virtual void paint(std::vector<std::string> &);
		
		void setGeometry(int,int,int w=-1,int h=-1);
		int x(){return x_;}
		int y(){return y_;}
		int width(){return width_;}
		int height(){return height_;}
		
		virtual void  focus(int *,int *); // sets initial focus
		
		bool acceptsFocus(){return acceptsFocus_;}
		void setAcceptsFocus(bool f){acceptsFocus_=f;}
		bool isFocusWidget(){return (acceptsFocus_ && focus_);}
		void setFocusWidget(bool f){focus_=f;}
		bool contains(int,int);
		
		virtual bool dirty(); // repaint required
		
	protected:
	
		std::vector<Widget *> children; // FIXME private 
		
		
		void setDirty(bool);
		void setFocus(int xfocus,int yfocus){xfocus_=xfocus;yfocus_=yfocus;}
		
		void addChild(Widget *);
		void addCallback(Functor *);
		
	private:
	
		Widget *focusWidgetBefore(Widget *);
		Widget *focusWidgetAfter(Widget *);
		
		void orderChildren(std::vector<Widget *> &);
		
		Widget *parent_;
		int x_,y_,width_,height_; // coordinates are all absolute
		bool focus_;
		bool acceptsFocus_;
		int xfocus_,yfocus_;
		
		bool dirty_;
	
		std::vector<Functor *> callbacks;
			
};

#endif
