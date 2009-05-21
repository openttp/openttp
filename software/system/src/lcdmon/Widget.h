#ifndef __WIDGET_H_
#define __WIDGET_H_

#include <string>
#include <vector>

class KeyEvent;

class Widget
{
	public:
	
		Widget(Widget *parent=NULL);
		virtual ~Widget();
		
		virtual bool keyEvent(KeyEvent &);
		virtual void paint(std::vector<std::string> &);
		
		void setGeometry(int,int,int w=-1,int h=-1);
		int x(){return x_;}
		int y(){return y_;}
		int width(){return width_;}
		int height(){return height_;}
		
		virtual void  focus(int *,int *); // sets initial focus
		bool isFocusWidget(){return focus_;}
		void setFocus(bool f){focus_=f;}
		bool contains(int,int);
		
		bool dirty(); // repaint required
		
	protected:
	
		std::vector<Widget *> children;
		void setDirty(bool);
		
	private:
	
		Widget *parent_;
		int x_,y_,width_,height_; // coordinates are all absolute
		bool focus_;
		
		bool dirty_;
};

#endif
