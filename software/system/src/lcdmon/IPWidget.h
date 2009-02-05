#ifndef __IP_WIDGET_H_
#define __IP_WIDGET_H_

#include "Widget.h"

class IPWidget:public Widget
{
	public:
	
		enum IPV { IPV4,IPV6};
		
		IPWidget(std::string,std::string,IPV,Widget *parent=NULL);
		
		virtual bool keyEvent(KeyEvent &);
		virtual void paint(std::vector<std::string> &);
		
		virtual void  focus(int *,int *);
		
		std::string ipAddress();
		
	private:
		
		std::string label;
		std::string ip;
		IPV ipv;
		
};

#endif
