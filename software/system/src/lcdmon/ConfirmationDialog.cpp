//
//
//

#include "Sys.h"
#include "Debug.h"

#include "Button.h"
#include "ConfirmationDialog.h"
#include "KeyEvent.h"
#include "Label.h"
#include "WidgetCallback.h"

ConfirmationDialog::ConfirmationDialog(std::string msg,Widget *parent):Dialog(parent)
{
	ConfirmationDialog::msg = msg;
	
	Label *l = new Label(msg,this);
	l->setGeometry(0,0,20,1);
	
	WidgetCallback<ConfirmationDialog> *cb = new WidgetCallback<ConfirmationDialog>(this,&ConfirmationDialog::ok);
	Button *b = new Button("Yes",cb,this);
	b->setGeometry(3,1,3,1);
	b->setFocusWidget(true);
	
	cb = new WidgetCallback<ConfirmationDialog>(this,&ConfirmationDialog::cancel);
	b = new Button("No",cb,this);
	b->setGeometry(3,2,2,1);
	
}




