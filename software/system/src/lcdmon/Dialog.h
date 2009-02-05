#ifndef __DIALOG_H_
#define __DIALOG_H_

#include "Widget.h"

class Dialog:public Widget
{
	public:
	
		enum ExitCode {OK,Cancel};
		
		Dialog(Widget *parent=NULL);
		virtual ~Dialog();
		
		virtual bool keyEvent(KeyEvent &);
		
		int exitCode();
		
		virtual void ok();
		virtual void cancel();
		
		
	protected:
	
		
	private:
	
		int exitCode_;
		
};

#endif
