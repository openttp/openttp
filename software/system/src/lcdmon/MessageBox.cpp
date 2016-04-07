//
//
//

#include "Sys.h"
#include "Debug.h"

#include "Label.h"
#include "MessageBox.h"
#include "KeyEvent.h"


MessageBox::MessageBox(std::string line1,std::string line2,std::string line3,std::string line4,Widget *parent):Dialog(parent)
{
	
	MessageBox::line1 = new Label(line1,this);
	MessageBox::line1->setGeometry(0,0,20,1);
	
	MessageBox::line2 = new Label(line2,this);
	MessageBox::line2->setGeometry(0,1,20,1);
	
	MessageBox::line3 = new Label(line3,this);
	MessageBox::line3->setGeometry(0,2,20,1);
	
	MessageBox::line4 = new Label(line4,this);
	MessageBox::line4->setGeometry(0,3,20,1);
	
}

void MessageBox::setLine(int l,std::string  txt)
{
	if (l==0)
		line1->setText(txt);
	else if (l==1)
		line2->setText(txt);
	else if (l==2)
		line3->setText(txt);
	else if (l==3)
		line4->setText(txt);
}


bool MessageBox::keyEvent(KeyEvent &)
{
	// any key is bye bye
	ok();
	return true;
}


