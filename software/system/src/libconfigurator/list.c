/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015  Michael J. Wouters
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "configurator.h"

extern int lastError;

#define TRUE 1
#define FALSE 0

void
list_clear(
	ListEntry *first)
{
	ListEntry *curr,*next;
	#ifdef DEBUG
	fprintf(stderr,"list_clear()\n");
	#endif
	
	curr=first;
	while (curr != NULL)
	{
		next=curr->next;
		free(curr->section);
		free(curr->token);
		free(curr->value);
		free(curr);
		curr=next;
	}
}

ListEntry *
list_add_entry(
	ListEntry *first,
	const char *section,
	const char *token,
	const char *value)
{
	ListEntry *l;
	l = (ListEntry *) malloc(sizeof(*l));
	if (NULL==l) return NULL;
	l->next=first;
	l->section=strdup(section);
	lowercase(l->section);
	l->token=strdup(token);
	lowercase(l->token);
	l->value=strdup(value);
	#ifdef DEBUG
	fprintf(stderr,"list_add_entry() %s %s %s\n",l->section,l->token,l->value);
	#endif
	return l;
}

char *
list_lookup(
	ListEntry *first,
	const char *section,
	const char * token)
{
	ListEntry *h;
	
	char *lctoken,*lcsection;
	
	#ifdef DEBUG
	fprintf(stderr,"list_lookup looking up %s %s\n",section,token);
	#endif
	
	lctoken=strdup(token);
	lowercase(lctoken);
	lcsection=strdup(section);
	lowercase(lcsection);
	h=first;
	while (h!=NULL)
	{
		if (0==strcmp(lcsection,h->section))
		{
			if (0==strcmp(lctoken,h->token))
			{
				free(lctoken);
				free(lcsection);
				#ifdef DEBUG
				fprintf(stderr,"list_lookup got %s %s : %s\n",section,token,h->value);
				#endif
				return h->value;
			}
		}
		h=h->next;
	}
	#ifdef DEBUG
	fprintf(stderr,"list_lookup %s %s failed\n",section,token);
	#endif
	free(lctoken);
	free(lcsection);
	return NULL;
}

/* Deprecated - retain for compatibility */
int list_get_int_value(ListEntry *first,const char *section,const char * token,int *value)
{
	return list_get_int(first,section,token,value);
}
int list_get_float_value(ListEntry *first,const char *section,const char * token,float *value)
{
	return list_get_float(first,section,token,value);
}

int list_get_double_value(ListEntry *first,const char *section,const char * token,double *value)
{
	return list_get_double(first,section,token,value);
}

int list_get_string_value(ListEntry *first,const char *section,const char * token,char **value)
{
	return list_get_string(first,section,token,value);
}

/* End of deprecated functions */

int	
	list_get_int(
	ListEntry *first,
	const char *section,
	const char * token,
	int *value)
{
	char *sval;
	char *endptr;

	if ((sval = list_lookup(first,section,token)))
	{
		*value = strtol(sval,&endptr,0);
		if (endptr == sval) /* conversion failed */
		{
			lastError=ParseFailed;
			return FALSE;
		}
		#ifdef DEBUG
		fprintf(stderr,"list_get_int() %s %s got %i\n",section,token,*value);
		#endif
		return TRUE;
	}
	
	#ifdef DEBUG
	fprintf(stderr,"list_get_int() %s %s failed\n",section,token);
	#endif
	lastError=TokenNotFound;
	return FALSE;
}

int	
list_get_float(
	ListEntry *first,
	const char *section,
	const char * token,
	float *value)
{
	double dval;
	int ret = list_get_double(first,section,token,&dval);
	if (ret)
		*value = dval;
	return ret;
}

int	
list_get_double(
	ListEntry *first,
	const char *section,
	const char * token,
	double *value)
{
	char *sval;
	char *endptr;

	if ((sval = list_lookup(first,section,token)))
	{
		*value = strtod(sval,&endptr);
		if (endptr == sval) /* conversion failed */
		{
			lastError=ParseFailed;
			return FALSE;
		}
		#ifdef DEBUG
		fprintf(stderr,"list_get_double() %s %s got %g\n",section,token,*value);
		#endif
		return TRUE;
	}
	
	#ifdef DEBUG
	fprintf(stderr,"list_get_double() %s %s failed\n",section,token);
	#endif
	lastError=TokenNotFound;
	return FALSE;
}

int	
list_get_string(
	ListEntry *first,
	const char *section,
	const char * token,
	char **value)
{
	char *v = list_lookup(first,section,token);
	if (v!= NULL)
	{
		*value=v;
		#ifdef DEBUG
		fprintf(stderr,"list_get_string() %s %s got %s\n",section,token,v);
		#endif
		return TRUE;
	}
	
	#ifdef DEBUG
	fprintf(stderr,"list_get_string() %s %s failed\n",section,token);
	#endif
	lastError=TokenNotFound;
	return FALSE;
	
}

#undef TRUE 
#undef FALSE
