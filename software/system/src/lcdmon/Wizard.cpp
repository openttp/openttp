//
//
//

#include "Wizard.h"
#include "KeyEvent.h"

Wizard::Wizard(Widget *parent):Dialog(parent)
{
	currentPage=0;
}

Wizard::~Wizard()
{
	for (unsigned int i=0;i<pages.size();i++)
		delete pages[i];
}
	
bool Wizard::keyEvent(KeyEvent &ke)
{
	int ev=ke.event();
	
	if (currentPage == pages.size()) // confirmation page
	{
		// movement is constrained to Y/N fields
		if (ev & KeyEvent::KeyLeft || ev & KeyEvent::KeyRight) // can't move
		{
			return true;
		}
		else if (ev & KeyEvent::KeyDown)
		{
			if (ke.y() == 2) return true;
		}
		else if (ev & KeyEvent::KeyUp)
		{
			if (ke.y()==1) return true;
		}
		else if (ev & KeyEvent::KeyEnter)
		{
			if (ke.y()==1)
				ok();
			else 
				cancel();
			return true;
		}
	}
	else
	{
		if (pages[currentPage]->keyEvent(ke))
			return true;
			
		if (ev & KeyEvent::KeyLeft)
		{
			if (currentPage > 0)
			{
				currentPage--;
				setDirty(true);
			}
			return true;
		}
		else if (ev & KeyEvent::KeyRight)
		{
			if (currentPage < pages.size())
			{
				currentPage++;
				setDirty(true);
			}
			return true;
		}
	}
	
	return false;
}

void Wizard::paint(std::vector<std::string> &display)
{
	
	if (currentPage == pages.size()) // confirmation page
	{
		display[0]="Apply network settings";
		display[1]="  Yes";
		display[2]="  No";
		display[3]=" ";
	}
	else 
	{
		if (currentPage==0) // 
		{
			display[0]=labels[currentPage];
			display[1]=" ";
			display[2]=" ";
			display[3]="       Next >       ";
			
		}
		else
		{
			display[0]=labels[currentPage];
			display[1]=" ";
			display[2]=" ";
			display[3]="   < Prev  Next >   ";
		}
		pages[currentPage]->paint(display);
	}
}

void  Wizard::focus(int *x,int *y)
{
	if (currentPage == pages.size()) // confirmation page
	{
		*x=2;
		*y=1;
	}
	else
	{
		pages[currentPage]->focus(x,y);
	}
}

Widget * Wizard::addPage(std::string l)
{
	Widget *w = new Widget();
	pages.push_back(w);
	labels.push_back(l);
	return w;
}
