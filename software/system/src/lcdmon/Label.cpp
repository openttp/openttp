
#include "Label.h"

Label::Label(std::string t,Widget *parent):Widget(parent)
{
	txt=t;
}

Label::~Label()
{
}
		
void Label::paint(std::vector<std::string> &d)
{
	d.at(y()) = txt;
}

