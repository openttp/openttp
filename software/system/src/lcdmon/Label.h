#ifndef __LABEL_H_
#define __LABEL_H_

#include <string>
#include <vector>

#include "Widget.h"

class Label:public Widget
{
	public:
	
		Label(std::string t,Widget *parent=NULL);
		virtual ~Label();
		
		virtual void paint(std::vector<std::string> &);
		
		void setText(std::string t){txt=t;}
		
		std::string & text(){return txt;}
		
	private:
	
		std::string txt;
};

#endif


