#ifndef _KEY_EVENT_H_
#define _KEY_EVENT_H_

class KeyEvent
{
	public:
	
		enum Event
		{
			KeyPressEvent=0x01,
			KeyReleaseEvent=0x02,
			KeyUp=0x04,
			KeyDown=0x08,
			KeyLeft=0x10,
			KeyRight=0x20,
			KeyEnter=0x40,
			KeyEsc=0x80
		};
	
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

