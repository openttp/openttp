#ifndef __WIZARD_H_
#define __WIZARD_H_

#include "Dialog.h"

class Wizard:public Dialog
{
	public:
	
		Wizard(Widget *parent=NULL);
		~Wizard();
		
		virtual bool keyEvent(KeyEvent &);
		virtual void paint(std::vector<std::string> &);
		
		virtual void  focus(int *,int *);
		
		Widget *addPage(std::string);
		
	private:
	
		std::vector<Widget *> pages;
		std::vector<std::string> labels;
		unsigned int currentPage;
};

#endif

