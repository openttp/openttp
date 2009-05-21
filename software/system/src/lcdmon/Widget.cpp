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
	x_=0;
	y_=0;
	width_=20;
	height_=4;
	focus_=false;
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
	// children first
	
	for (unsigned int i=0;i<children.size();i++)
	{
		if (children.at(i)->keyEvent(ke))
		{
			return true;
		}
	}
	return false;
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
	if (children.empty()) return;
	children.at(0)->focus(fx,fy); // first child gets focus by default
}

void Widget::setGeometry(int x,int y,int w,int h)
{
	x_=x;y_=y;
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



