#ifndef __FUNCTOR_H_
#define __FUNCTOR_H_

class Functor
{
	public:

	// two possible functions to call member function. virtual cause derived
	// classes will use a pointer to an object and a pointer to a member function
	// to make the function call
	virtual void operator()()=0;  // call using operator
	virtual void Call()=0;        // call using function
};

#endif

