#ifndef __BUTTON_H_
#define __BUTTON_H_

#include <string>
#include <vector>

#include "Widget.h"

class Functor;

class Button:public Widget
{
	public:
	
		Button(std::string t,Functor *,Widget *parent=NULL);
		virtual ~Button();
		
		virtual bool keyEvent(KeyEvent &);
		virtual void paint(std::vector<std::string> &);
		
		void setText(std::string t){txt=t;}
		
		std::string & text(){return txt;}
		
	private:
	
		std::string txt;
		Functor *callback;
};

#endif


