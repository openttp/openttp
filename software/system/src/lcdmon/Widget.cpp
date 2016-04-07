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

#include "KeyEvent.h"
#include "Widget.h"
#include "WidgetCallback.h"
	
Widget::Widget(Widget *parent)
{
	parent_=parent;
	if (parent_) parent_->children.push_back(this);
	x_=xfocus_=0;
	y_=yfocus_=0;
	width_=20;
	height_=4;
	focus_=false;
	acceptsFocus_=true;
	dirty_=false;
}

Widget::~Widget()
{
	for (unsigned int i=0;i<children.size();i++)
		delete children.at(i);
	for (unsigned int i=0;i<callbacks.size();i++)
		delete callbacks[i];	
}

bool Widget::keyEvent(KeyEvent &ke)
{
	// return value indicates acceptance of the event
	
	Dout(dc::trace,"Widget::keyEvent()");
	
	if (!contains(ke.x(),ke.y()))
	{ 
		setFocusWidget(false);
		return false;
	}
	
	Widget *currentFocusWidget=NULL;
	for (unsigned int i=0;i<children.size();i++) 
	{
		if (children.at(i)->isFocusWidget())
		{
			
			currentFocusWidget=children.at(i);
			Dout(dc::trace,"Widget::keyEvent() current focus widget at " << currentFocusWidget->x() << " " << currentFocusWidget->y());
			//break; // FIXME to help with debugging!
		}
	}
	
	if (currentFocusWidget) // check the focus widget first (should always be one)
	{
		if (currentFocusWidget->keyEvent(ke))
			return true;
	}
	 
	for (unsigned int i=0;i<children.size();i++) // check all children next
	{
		if (children.at(i)->keyEvent(ke))
			return true;
	}
	
	
	// key movement may require a change of focus child widget

	
	if (ke.event() & KeyEvent::KeyRelease )
	{
		if (ke.event() & KeyEvent::KeyLeft)
		{
			if (xfocus_ > 0) xfocus_--; // this just moves us around by default
	
			if (children.size() > 0)
			{
				if (currentFocusWidget)
				{
					Widget *prevWidget;
					if ((prevWidget = focusWidgetBefore(currentFocusWidget)))
						prevWidget->setFocusWidget(true);
					else
						currentFocusWidget->setFocusWidget(true);
				}
			}	
		}
		else if (ke.event() & KeyEvent::KeyRight)
		{
			if (xfocus_ < 19) xfocus_++;
			
			if (children.size() > 0)
			{
				if (currentFocusWidget)
				{
					Widget *nextWidget;
					if ((nextWidget = focusWidgetAfter(currentFocusWidget)))
						nextWidget->setFocusWidget(true);
					else
						currentFocusWidget->setFocusWidget(true);
				}
			}	
		}
		else if (ke.event() & KeyEvent::KeyUp)
		{
			if (yfocus_ > 0) yfocus_--;
			
			if (children.size() > 0)
			{
				if (currentFocusWidget)
				{
					Widget *prevWidget;
					if ((prevWidget = focusWidgetBefore(currentFocusWidget)))
						prevWidget->setFocusWidget(true);
					else
						currentFocusWidget->setFocusWidget(true);
				}
			}	
		}
		else if (ke.event() & KeyEvent::KeyDown)
		{
			if (yfocus_< 19) yfocus_++;
			
			
			if (children.size() > 0)
			{
				if (currentFocusWidget)
				{
					Widget *nextWidget;
					if ((nextWidget = focusWidgetAfter(currentFocusWidget)))
						nextWidget->setFocusWidget(true);
					else
						currentFocusWidget->setFocusWidget(true);
				}
			}	
		}
	}
	
	return true;
}

void Widget::paint(std::vector<std::string> &display)
{
	// We paint into the string ...
	//unsigned int nlines=children.size();
	//if (nlines > 4) nlines=4;
	for (unsigned int i=0;i<children.size();i++)
	{
		children.at(i)->paint(display);
	}
}

void  Widget::focus(int *fx,int *fy)
{
	if (children.empty())
	{ 
		*fx=xfocus_;
		*fy=yfocus_;
		return;
	}
	
	
	// check if anyone claims focus now
	for (unsigned int i=0;i<children.size();i++)
	{
		if (children.at(i)->isFocusWidget())
		{
			
			children.at(i)->focus(fx,fy);
			Dout(dc::trace,"Widget::focus() claimed by child at " << *fx << " " << *fy);
			return;
		}
	}
	
	// no winners so pick the first one
	std::vector<Widget *> ordered;
	orderChildren(ordered);
	for (unsigned int i=0;i<ordered.size();i++)
	{
		if (ordered.at(i)->acceptsFocus())
		{
			ordered.at(i)->focus(fx,fy); 
			ordered.at(i)->setFocusWidget(true);
		}
	}
	
}

void Widget::setGeometry(int x,int y,int w,int h)
{
	x_=xfocus_=x;
	y_=yfocus_=y;
	if (w > 0) width_=w;
	if (h > 0) height_=h;
}

bool Widget::contains(int xp,int yp)
{
	return ((xp >= x_) && (xp <=x_+width_ -1) && (yp>=y_) && (yp <=y_+height_-1)); 
}

bool Widget::dirty()
{
	bool ret=dirty_;
	for (unsigned int i=0;i<children.size();i++)
		ret = ret || children.at(i)->dirty();
	return ret;
}

//
//
//

void Widget::setDirty(bool d)
{
	dirty_ = d;
}

void Widget::addCallback(Functor *cb)
{
	callbacks.push_back(cb);
}
//
// FIXME assumes one widget per line
//

Widget *Widget::focusWidgetBefore(Widget *w)
{
	int wx = w->x();
	int wy = w->y();
	int wht = w->height();
	Dout(dc::trace," Widget::focusWidgetBefore() " <<wx << " " << wy << " " << wht);
	
	std::vector<Widget *> ordered;
	orderChildren(ordered);
	for ( int i=0;i<(int) ordered.size();i++)
	{
		Widget *c =ordered.at(i);
		if (c==w)
		{
			Dout(dc::trace," Widget::focusWidgetBefore() match index = " << i);
			int indx;
			if ((i-1) >= 0)
				indx=i-1;
			else //(i-1 < 0)
				indx=ordered.size()-1; //.go to last
			Dout(dc::trace," Widget::focusWidgetBefore() focus at " << ordered.at(indx)->x() << " " << ordered.at(indx)->y());
			return ordered.at(indx);
		}
	}
	return NULL;
}

Widget *Widget::focusWidgetAfter(Widget *w)
{
	int wx = w->x();
	int wy = w->y();
	int wht = w->height();
	Dout(dc::trace," Widget::focusWidgetAfter() " <<wx << " " << wy << " " << wht);
	
	std::vector<Widget *> ordered;
	orderChildren(ordered);
	
	for (int i=0;i<(int)ordered.size();i++)
	{
		Widget *c =ordered.at(i);
		if (c==w)
		{
			int indx;
			if (i+1 <(int) ordered.size())
				indx=i+1;
			else //i+1 >= ordered.size() 
				indx=0; // go to first
			Dout(dc::trace," Widget::focusWidgetBefore() focus at " << ordered.at(indx)->x() << " " << ordered.at(indx)->y());
			return ordered.at(indx);
		}
	}
	
	return NULL;
}


void Widget::orderChildren(std::vector<Widget *> & ordered)
{
	// spatially sorts children so that next focus widget can be determined
	
	
	// for each line build a list of widgets on that line
	// order that list by 'x' and then copy to the ordered list
	
	std::vector<Widget *> tmp;
	Dout(dc::trace,"Widget::orderChildren() children before =" << children.size());
	for (int j=0;j<=3;j++)
	{
		tmp.clear();
		for (unsigned int w=0;w<children.size();w++)
		{
			if (children.at(w)->y() == j && children.at(w)->acceptsFocus())
				tmp.push_back(children.at(w));
		}
		// order that list by 'x' and then copy to the ordered list
	
		for (int i=0;i<20;i++) // small search space so brute force
		{
			for (unsigned int w=0;w<tmp.size();w++)
			{
				if (tmp.at(w)->x()==i)
					ordered.push_back(tmp.at(w));
			}
		}
	
	}

	Dout(dc::trace,"Widget::orderChildren() children after =" << ordered.size());
}



