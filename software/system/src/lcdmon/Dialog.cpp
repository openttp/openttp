
//
//
//

#include "Dialog.h"
#include "KeyEvent.h"

Dialog::Dialog(Widget *parent):Widget(parent)
{
	exitCode_=OK;
}

Dialog::~Dialog()
{
}

bool Dialog::keyEvent(KeyEvent &ke)
{
	
	int ev=ke.event();
	if (ev & KeyEvent::KeyEnter)
	{
		//  this would be where we validated any changes
		return true;
	}
	
	Widget::keyEvent(ke);
	
	return false;
}

int Dialog::exitCode()
{
	return exitCode_;
}

void Dialog::ok()
{
	exitCode_=OK;
}

void Dialog::cancel()
{
	exitCode_=Cancel;
}
