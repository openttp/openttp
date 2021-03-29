//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Michael J. Wouters
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Modification history
//

#include "Debug.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>

using namespace std;

extern ostream *debugStream;

#include "IPWidget.h"
#include "KeyEvent.h"

IPWidget::IPWidget(std::string ipaddr,IPV ipver,Widget *parent):Widget(parent)
{
	
	ipv=ipver;
	
	if (ipv==IPV4)
		setGeometry(0,0,15,1);
	else
		setGeometry(0,0,15,2);
		
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
		
		std::istringstream iss(bs);
		int b;
		iss >>b;
		pos=posdelim+1;
		
		oss << std::setfill('0') << std::setw(3) << b;
		if (i<quadcnt-1) oss << '.';
		
	}
	ip=oss.str();
}

IPWidget::~IPWidget()
{
	DBGMSG(debugStream,TRACE, ip);
}

bool IPWidget::keyEvent(KeyEvent &ke)
{
	// So we accept up/down when we are over a number field
	//XXXX:NNN.NNN.NNN.NNN
	// Is it even in our space
	
	DBGMSG(debugStream,TRACE, "(x,y)=" << ke.x() << " " << ke.y());
	
	if (!contains(ke.x(),ke.y())) return false;
	setFocusWidget(true);
	
	int ev=ke.event();
	if ((ev & KeyEvent::KeyRelease))
	{
		if ((ev & KeyEvent::KeyUp) || (ev & KeyEvent::KeyDown))
		{
			int col=ke.x()-x();
			int row=ke.y()-y();
			if ((col != 3) && (col!= 7) && (col!= 11)) // tweak a digit
			{
				int sgn=-1;
				if (ev & KeyEvent::KeyUp) sgn=1;
				int pos = col;
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
				//fprintf(stderr,"%i %s %i\n",incr,quad.c_str(),iquad);
				iquad += incr;
				// Range is 0..255 for a quad
				// What we do is first get the current quad value, the amount we are
				// incrementing by and cycle through, clamping to valid values
				if (iquad <0 || iquad> 255) iquad-=incr;
				std::ostringstream oss;
				oss << std::setfill('0') << std::setw(3) << iquad;
			
				ip.replace((pos/4)*4,3,oss.str());
				//fprintf(stderr,"%i %s %s\n",iquad,oss.str().c_str(),ip.c_str());
				setDirty(true);
				return true;
			}
		}
		else if ((ev & KeyEvent::KeyRight))
		{
			if (ke.x()-x() < 14)
			{
				setFocus(ke.x()+1,ke.y());
				DBGMSG(debugStream,TRACE, "x->" << ke.x() + 1);
				return true;
			}
			else
			{
				setFocusWidget(false);
				setFocus(x(),y());
				return false;
			}
		}
		else if ((ev & KeyEvent::KeyLeft))
		{
			if (ke.x() - x() > 0)
			{
				setFocus(ke.x()-1,ke.y());
				return true;
			}
			else
			{
				setFocusWidget(false);
				setFocus(x(),y());
				return false;
			}
		}
	}
	
	return false;
}

void IPWidget::paint(std::vector<std::string> &display)
{
	if (ipv==IPV4)
	{
		std::string s =ip;
		display.at(y()).replace(x(),ip.size(),ip); 
	}
	else
	{
		// need to split this into two lines
		display.at(y()).replace(x(),15,ip.substr(0,15)); 
		display.at(y()+1).replace(x(),15,ip.substr(16,15)); 
	}
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
		std::istringstream iss(bs);
		int b;
		iss >>b;
		pos=posdelim+1;
		
		oss << b;
		if (i<quadcnt-1) oss << '.';
	}
	return oss.str();
}

