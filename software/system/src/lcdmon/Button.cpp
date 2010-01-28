//
//
//
//

#include "Sys.h"
#include "Debug.h"

#include "Button.h"
#include "Functor.h"
#include "KeyEvent.h"

Button::Button(std::string t,Functor *cb,Widget *parent):Widget(parent)
{
	txt=t;
	callback=cb;
}

Button::~Button()
{
	Dout(dc::trace,"Button::~Button() " << txt);
}

bool Button::keyEvent(KeyEvent &ke)
{
	int ev=ke.event();
	if  (ev & KeyEvent::KeyEnter)
	{
		Dout(dc::trace,"Button::keyEvent -> Enter " << txt);
		if (callback)
			callback->Call();
		return true;
	}
	else if ((ev & KeyEvent::KeyRelease) && ((ev & KeyEvent::KeyUp) || (ev & KeyEvent::KeyDown) || (ev & KeyEvent::KeyLeft) || (ev & KeyEvent::KeyRight)))
	{
			setFocusWidget(false);
			return false;
	}
	return false;
}
	
void Button::paint(std::vector<std::string> &d)
{
	d.at(y()).replace(x(),txt.size(),txt);
}

