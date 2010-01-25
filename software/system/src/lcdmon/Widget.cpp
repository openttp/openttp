//
//
//

#include "Sys.h"
#include "Debug.h"

#include "KeyEvent.h"
#include "Widget.h"

	
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
	
	for (unsigned int i=0;i<children.size();i++) // check children next
	{
		if (children.at(i)->keyEvent(ke))
			return true;
	}
	
	
	// key movement may require a change of focus child widget

	Dout(dc::trace,"Here !!!!");
	
	if (ke.event() & KeyEvent::KeyRelease )
	{
		if (ke.event() & KeyEvent::KeyLeft && xfocus_ > 0)
		{
			xfocus_--; // this just moves us around
		}
		else if (ke.event() & KeyEvent::KeyRight && xfocus_ < 19)
		{
			xfocus_++;
		}
		else if (ke.event() & KeyEvent::KeyUp && yfocus_ >0)
		{
			//yfocus_--;
			Dout(dc::trace,"Before !!!! " << (currentFocusWidget != NULL));
			if (children.size() > 0)
			{
				if (currentFocusWidget)
				{
					Widget *prevWidget;
					if (prevWidget = focusWidgetBefore(currentFocusWidget))
						prevWidget->setFocusWidget(true);
					else
						currentFocusWidget->setFocusWidget(true);
				}
			}	
		}
		else if (ke.event() & KeyEvent::KeyDown  && yfocus_ < 3)
		{
			//yfocus_++;
			Dout(dc::trace,"After !!!! " << (currentFocusWidget != NULL));
			if (children.size() > 0)
			{
				if (currentFocusWidget)
				{
					Widget *nextWidget;
					if (nextWidget = focusWidgetAfter(currentFocusWidget))
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
	unsigned int nlines=children.size();
	if (nlines > 4) nlines=4;
	for (unsigned int i=0;i<nlines;i++)
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
	
	// first child has focus by default
	
	children.at(0)->focus(fx,fy); 
	children.at(0)->setFocusWidget(true);
	
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

//
// FIXME assumes one widget per line
//

Widget *Widget::focusWidgetBefore(Widget *w)
{
	int wx = w->x();
	int wy = w->y();
	int wht = w->height();
	Dout(dc::trace," Widget::focusWidgetBefore() " <<wx << " " << wy << " " << wht);
	if (wy > 0)
	{
		for (int nwy = wy-1;nwy>=0;nwy--)
		{
			for (unsigned int i=0;i<children.size();i++)
			{
				Widget *nw =children.at(i);
				if ( nwy >= nw->y()  && nw->y()+nw->height()-1 >= nwy && children.at(i)->acceptsFocus())
				{
					Dout(dc::trace," Widget::focusWidgetBefore() found at " << children.at(i)->y());
					return children.at(i);
				}
			}
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
	if (wy + wht<= 3)
	{
		for (int nwy = wy+wht ;nwy<=3;nwy++)
		{
			for (unsigned int i=0;i<children.size();i++)
			{
				if (children.at(i)->y()==nwy && children.at(i)->acceptsFocus())
				{
					Dout(dc::trace," Widget::focusWidgetAfter() found at " << children.at(i)->y());
					return children.at(i);
				}
			}
		}
	}
	return NULL;
}
		


