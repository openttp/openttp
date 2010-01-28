#ifndef __IP_WIDGET_H_
#define __IP_WIDGET_H_

#include "Widget.h"

class IPWidget:public Widget
{
	public:
	
		enum IPV { IPV4,IPV6};
		
		IPWidget(std::string,IPV,Widget *parent=NULL);
		~IPWidget();
		
		virtual bool keyEvent(KeyEvent &);
		virtual void paint(std::vector<std::string> &);
		
		
		std::string ipAddress();
		
	private:
		
		std::string ip;
		IPV ipv;
		
};

#endif
