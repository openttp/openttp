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
		virtual bool dirty();
		
		Widget *addPage(std::string);
		
		void nextPage();
		void prevPage();
		
	private:
	
		std::vector<Widget *> pages;
		
		unsigned int currentPage;
		bool forcePaint;
};

#endif

