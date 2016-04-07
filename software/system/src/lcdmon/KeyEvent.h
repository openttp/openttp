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

#ifndef _KEY_EVENT_H_
#define _KEY_EVENT_H_

class KeyEvent
{
	public:
	
		enum Event
		{
			KeyPress=0x01,
			KeyRelease=0x02,
			KeyUp=0x04,
			KeyDown=0x08,
			KeyLeft=0x10,
			KeyRight=0x20,
			KeyEnter=0x40,
			KeyEsc=0x80
		};
	
		KeyEvent(){;}
		KeyEvent(KeyEvent &ke){*this=ke;}
		KeyEvent(int ev,int xpos,int ypos){KeyEvent::ev = ev;KeyEvent::xpos=xpos;KeyEvent::ypos=ypos;}
		
		int event(){return ev;}
		int x(){return xpos;}
		int y(){return ypos;}
		
		void setxy(int newx,int newy){xpos=newx;ypos=newy;}
		
	private:
	
		int ev;
		int xpos,ypos;
};
#endif

