#ifndef __CONFIRMATION_DIALOG_H_
#define __CONFIRMATION_DIALOG_H_

#include "Dialog.h"

class ConfirmationDialog:public Dialog
{
	public:
	
		ConfirmationDialog(std::string,Widget *parent=NULL);
		
		
	private:
	
		std::string msg;
};

#endif

