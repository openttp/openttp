
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
	if (Widget::keyEvent(ke)) return true; // check children
	int ev=ke.event();
	if (ev & KeyEvent::KeyEnter)
	{
		return true;
	}
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

