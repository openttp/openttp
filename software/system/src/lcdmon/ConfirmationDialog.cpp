
#include "ConfirmationDialog.h"
#include "KeyEvent.h"

ConfirmationDialog::ConfirmationDialog(std::string msg,Widget *parent):Dialog(parent)
{
	ConfirmationDialog::msg = msg;
	setFocus(2,1);
}

		
bool ConfirmationDialog::keyEvent(KeyEvent &ke)
{
	int ev=ke.event();
	// movement is constrained to Y/N fields
	if (ev & KeyEvent::KeyLeft || ev & KeyEvent::KeyRight) // can't move
	{
		return true;
	}
	else if (ev & KeyEvent::KeyUp)
	{
		if (ke.y() == 2) 
		{
			setFocus(2,1);
			return true;
		}
	}
	else if (ev & KeyEvent::KeyDown)
	{
		if (ke.y()==1)
		{ 
			setFocus(2,2);
			return true;
		}
	}
	else if (ev & KeyEvent::KeyEnter)
	{
		if (ke.y()==1)
			ok();
		else 
			cancel();
		return true;
	}
	return false;
}

void ConfirmationDialog::paint(std::vector<std::string> &display)
{
	// Construct by hand
	display[0]=msg;
	display[1]="  Yes";
	display[2]="  No";
}

