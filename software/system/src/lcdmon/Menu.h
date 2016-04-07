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
		
		bool checked(){return checked_;}
		void setChecked(bool c){checked_=c;}
		
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
	  bool checked_;
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
