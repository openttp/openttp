
#include "Sys.h"
#include "Debug.h"

#include "Label.h"

Label::Label(std::string t,Widget *parent):Widget(parent)
{
	txt=t;
	setAcceptsFocus(false);
}

Label::~Label()
{
	Dout(dc::trace,"Label::~Label() " << txt);
}

void Label::setText(std::string t)
{
	txt=t;
	setDirty(true);
}
	
void Label::paint(std::vector<std::string> &d)
{
	d.at(y()).replace(x(),txt.size(),txt);
	setDirty(false);
}

