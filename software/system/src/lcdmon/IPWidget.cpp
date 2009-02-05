
#include <iostream>
#include <iomanip>
#include <sstream>

#include "IPWidget.h"
#include "KeyEvent.h"

IPWidget::IPWidget(std::string label,std::string ipaddr,IPV ipver,Widget *parent):Widget(parent)
{
	
	ipv=ipver;
	
	if (ipv==IPV4)
		setGeometry(0,0,20,1);
	else
		setGeometry(0,0,20,2);
		
	IPWidget::label=label;
	// pad the ip
	// break the ip into the four bytes and then zero pad
	int pos=0;
	std::ostringstream oss;
	int quadcnt=4;
	if (ipv==IPV6) quadcnt=8; 
	for (int i=0;i<quadcnt;i++)
	{
		unsigned int posdelim=ipaddr.find('.',pos);
		if (posdelim == std::string::npos) posdelim=ipaddr.length();
		std::string bs =ipaddr.substr(pos,posdelim-pos);
		std::cout << bs << std::endl;
		std::istringstream iss(bs);
		int b;
		iss >>b;
		pos=posdelim+1;
		
		oss << std::setfill('0') << std::setw(3) << b;
		if (i<quadcnt-1) oss << '.';
		std::cout << b << " " << oss.str() << std::endl;
	}
	ip=oss.str();
}

		
bool IPWidget::keyEvent(KeyEvent &ke)
{
	// So we accept up/down when we are over a number field
	//XXXX:NNN.NNN.NNN.NNN
	// Is it even in our space
	
	if (!contains(ke.x(),ke.y())) return false;
	
	int ev=ke.event();
	if ((ev & KeyEvent::KeyReleaseEvent) && 
		((ev & KeyEvent::KeyUp) || (ev & KeyEvent::KeyDown)))
	{
		int col=ke.x();
		int row=ke.y()-y();
		if ((col>4) && (col != 8) && (col!= 12) && (col!= 16)) // tweak a digit
		{
			int sgn=-1;
			if (ev & KeyEvent::KeyUp) sgn=1;
			// first digit in column 5 so subtract 5 from the col
			int pos = col-5;
			if (ipv==IPV6 && row ==1)
				pos += 16;
			int pwr = 2 - (pos % 4);
			int incr = sgn;
			for (int i=0;i<pwr;i++)
				incr*=10;
			std::string quad=ip.substr((pos/4)*4,3);
			std::istringstream iss(quad);
			int iquad;
			iss>>iquad;
			fprintf(stderr,"%i %s %i\n",incr,quad.c_str(),iquad);
			iquad += incr;
			// Range is 0..255 for a quad
			// What we do is first get the current quad value, the amount we are
			// incrementing by and cycle through, clamping to valid values
			if (iquad <0 || iquad> 255) iquad-=incr;
			std::ostringstream oss;
			oss << std::setfill('0') << std::setw(3) << iquad;
			
			ip.replace((pos/4)*4,3,oss.str());
			fprintf(stderr,"%i %s %s\n",iquad,oss.str().c_str(),ip.c_str());
			
			return true;
		}
	}
	else if (ev & KeyEvent::KeyEnter)
	{
		return false;
	}
	return false;
}

void IPWidget::paint(std::vector<std::string> &display)
{
	if (ipv==IPV4)
	{
		std::string s =label+":"+ip;
		display.at(y())=s; // fixme
	}
	else
	{
		// need to split this into two lines
		std::string s1=label + ":"+ ip.substr(0,15);
		display.at(y())=s1;
		std::string s2="     "    + ip.substr(16,15);
		display.at(y()+1)=s2;
	}
}

void IPWidget::focus(int *fx,int *fy)
{
	*fx=5+x();
	*fy=y();
}

std::string IPWidget::ipAddress()
{
	// depad the value
	
	int pos=0;
	std::ostringstream oss;
	int quadcnt=4;
	if (ipv==IPV6) quadcnt=8; 
	for (int i=0;i<quadcnt;i++)
	{
		unsigned int posdelim=ip.find('.',pos);
		if (posdelim == std::string::npos) posdelim=ip.length();
		std::string bs =ip.substr(pos,posdelim-pos);
		std::cout << bs << std::endl;
		std::istringstream iss(bs);
		int b;
		iss >>b;
		pos=posdelim+1;
		
		oss << b;
		if (i<quadcnt-1) oss << '.';
		std::cout << b << " " << oss.str() << std::endl;
	}
	return oss.str();
}

