#ifndef __CONFIRMATION_DIALOG_H_
#define __CONFIRMATION_DIALOG_H_

#include "Dialog.h"

class ConfirmationDialog:public Dialog
{
	public:
	
		ConfirmationDialog(std::string,Widget *parent=NULL);
		
		virtual bool keyEvent(KeyEvent &);
		virtual void paint(std::vector<std::string> &);
		
		virtual void  focus(int *,int *);
		
	private:
	
		std::string msg;
};

#endif

