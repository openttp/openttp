//
//
//

#ifndef __MENU_H_
#define __MENU_H_

#include <string>
#include <vector>

class Functor;

class MenuItem
{
	public:
		
		MenuItem();
		MenuItem(std::string,Functor *);
		virtual ~MenuItem();
		
		std::string text(){return txt;}
		void setText(std::string txt){MenuItem::txt = txt;}
		
		bool isMenu(){return ismenu;} // to avoid casts
		virtual void exec();
		
	protected:
	
		void setIsMenu(bool ismenu){MenuItem::ismenu=ismenu;}
		
	private:
	
		std::string txt;
		bool ismenu;
		Functor *callback;
		
};

class Menu:public MenuItem
{
	public:
	
		Menu();
		Menu(std::string);
		~Menu();
		
		MenuItem *itemAt(unsigned int);
		int numItems(){return items.size();}
		
		int insertItem(std::string,Functor *);
		int insertItem(Menu *);
		
	private:
	
		std::vector<MenuItem *> items;
		
};

#endif
