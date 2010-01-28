
//
//
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
