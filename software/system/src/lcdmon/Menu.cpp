//
//
//
// Modification history
//

//
//
//

#include "Functor.h"
#include "Menu.h"

static int menuid=1;

MenuItem::MenuItem()
{
	ismenu=false;
	callback=NULL;
}

MenuItem::MenuItem(std::string txt,Functor *cb)
{
	MenuItem::txt = txt;
	ismenu=false;
	callback=cb;
}

MenuItem::~MenuItem()
{
}

void MenuItem::exec()
{
	if (!callback) return;
	callback->Call();
}
//
//
//

Menu::Menu():MenuItem()
{
	setIsMenu(true);
}

Menu::Menu(std::string txt):MenuItem(txt,NULL)
{
	setIsMenu(true);
}

Menu::~Menu()
{
	for (unsigned int i=0;i<items.size();i++)
		delete items.at(i);
}
		
MenuItem *Menu::itemAt(unsigned int idx)
{
	if (idx < 0 || idx >= items.size()) return NULL;
	return items.at(idx);
}
		
int Menu::insertItem(std::string txt,Functor *cb)
{
	items.push_back(new MenuItem(txt,cb));
	menuid++;
	return menuid;
}

int Menu::insertItem(Menu *m)
{
	items.push_back(m);
	menuid++;
	return menuid;
}

