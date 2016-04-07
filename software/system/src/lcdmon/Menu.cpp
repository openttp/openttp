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

#include "Functor.h"
#include "Menu.h"

MenuItem::MenuItem()
{
	ismenu=false;
	callback=NULL;
	checked_=false;
}

MenuItem::MenuItem(std::string txt,Functor *cb)
{
	MenuItem::txt = txt;
	ismenu=false;
	callback=cb;
	checked_=false;
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
	return items.size()-1;
}

int Menu::insertItem(Menu *m)
{
	items.push_back(m);
	return items.size()-1;
}

