#ifndef __WIDGET_CALLBACK_H_
#define __WIDGET_CALLBACK_H_

#include "Functor.h"

template <class TClass> class WidgetCallback : public Functor
{
   private:
      void (TClass::*fpt)();   // pointer to member function
      TClass* pt2Object;                  // pointer to object

   public:

      // constructor - takes pointer to an object and pointer to a member and stores
      // them in two private variables
      WidgetCallback(TClass* _pt2Object, void(TClass::*_fpt)())
         { pt2Object = _pt2Object;  fpt=_fpt; };
			virtual ~WidgetCallback(){;}

      // override operator "()"
      virtual void operator()()
       { (*pt2Object.*fpt)();};              // execute member function

      // override function "Call"
      virtual void Call()
        { (*pt2Object.*fpt)();};             // execute member function
};

#endif
