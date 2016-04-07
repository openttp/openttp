#ifndef __DIALOG_H_
#define __DIALOG_H_

#include "Widget.h"

class Dialog:public Widget
{
	public:
	
		enum ExitCode {OK,Cancel};
		
		Dialog(Widget *parent=NULL);
		virtual ~Dialog();
		
		int exitCode();
		bool finished(){return finished_;}
		
		virtual void ok();
		virtual void cancel();
		
		
	protected:
	
		void setFinished(bool f){finished_=f;}
		
	private:
	
		int exitCode_;
		bool finished_;
};

#endif
