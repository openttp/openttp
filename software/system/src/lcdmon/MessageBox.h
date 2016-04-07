#ifndef __MESSAGE_BOX_H_
#define __MESSAGE_BOX_H_

#include "Dialog.h"

class Label;

class MessageBox:public Dialog
{
	public:
	
		MessageBox(std::string,std::string,std::string,std::string,Widget *parent=NULL);
		
		virtual bool keyEvent(KeyEvent &);
		
		void setLine(int,std::string);
		
	private:
	
		Label *line1,*line2,*line3,*line4;
};

#endif

