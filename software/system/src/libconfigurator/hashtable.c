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

/* 
 * hashtable.c
 *
 * Modification history
 * 28-05-2002 MJW First version. Cribbed from hostmon.c.
 * 18-07-2002 MJW Rearranged into a static library
 * 17-06-2004 MJW Fixed hash_lookup() bug. Incorrect matches to a
 *								substring.
 * 24-09-2009 MJW Token matching is no longer case-sensitive
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "configurator.h"

extern int lastError;

static int hash_makehash(const char *token);


void hash_init(
	HashEntry *tab[],int tabsize)
{
	int i;
	for (i=0;i<tabsize;i++)
		tab[i]=NULL;
}

void hash_clear(
	HashEntry *tab[],
	int tabsize)
{
	int i;
	HashEntry* curr,*next;
	#ifdef DEBUG
	fprintf(stderr,"hash_clear()\n");
	#endif
	for (i=0;i<tabsize;i++)
	{
		curr=tab[i];
		while (curr != NULL)
		{
			next=curr->next;
			free(curr->token);
			free(curr->value);
			free(curr);
			curr=next;
		}
		tab[i]=NULL;
	}
}

char *
hash_lookup(
	HashEntry *tab[],
	const char *token
	)
{
	
	HashEntry *h;
	char *temp;
	temp=strdup(token);
	lowercase(temp);
	#ifdef DEBUG
		fprintf(stderr,"hash_lookup(): looking up %s\n",temp);
	#endif
	for (h=tab[hash_makehash(temp)];h!=NULL;h=h->next)
	{
		if ( strcmp(h->token,temp) == 0) /* exact match */
		{ 
				free(temp);
				return h->value; /*found */
		}
	}
	#ifdef DEBUG
	fprintf(stderr,"hash_lookup():failed\n");
	#endif
	free(temp);
	return NULL; /* not found */
}



HashEntry *
hash_addentry(
	HashEntry *tab[],
	const char *token,
	const char *value)
{
	/* FIXME doesn't attempt to deal with duplicate entries */
	HashEntry *h;
	int hval;
	
	h= (HashEntry *) malloc(sizeof(*h));
	if (NULL==h) return NULL;
	hval=hash_makehash(token);
	h->next=tab[hval];
	tab[hval]=h;
	h->token = strdup(token);
	lowercase(h->token);
	h->value = strdup(value);
	#ifdef DEBUG
		fprintf(stderr,"hash_addentry(): %i %s %s\n",
			hval,h->token,h->value);
	#endif
	return h;
}

/* Some convenience functions for getting values  and doing appropriate error 
 * handling */

/* Deprecated - retain for compatibility */
int hash_get_intvalue(HashEntry *tab[],const char * token,int *value)
{
	return hash_get_int(tab,token,value);
}

int hash_get_floatvalue(HashEntry *tab[],const char * token,float *value){
	return hash_get_float(tab,token,value);
}

int hash_get_doublevalue(HashEntry *tab[],const char * token,double *value){
	return hash_get_double(tab,token,value);
}

int	hash_get_stringvalue(HashEntry *tab[],const char * token,char **value){
	return hash_get_string(tab,token,value);
}
/* End of deprecated functions */

int	
hash_get_int(
	HashEntry *tab[],
	const char * token,
	int *value)
{
	char *sval;
	char *endptr;
	
	if ((sval=hash_lookup(tab,token)))
	{
		*value = strtol(sval,&endptr,0);
		if (endptr == sval) /* conversion failed */
		{
			lastError=ParseFailed;
			return 0;
		}
		return 1;
	}
	else
	{
		lastError=TokenNotFound;
	}
	return 0;
}

int	
hash_get_float(
	HashEntry *tab[],
	const char * token,
	float *value)
{
	double dval;
	int ret = hash_get_double(tab,token,&dval);
	if (ret==1)
		*value = (float) dval;
		
	return ret;
}

int	
hash_get_double(
	HashEntry *tab[],
	const char * token,
	double *value)
{
	char *sval;
	char *endptr;
	
	if ((sval=hash_lookup(tab,token)))
	{
		*value = strtod(sval,&endptr);
		if (endptr == sval) /* conversion failed */
		{
			lastError=ParseFailed;
			return 0;
		}
		return 1;
	}
	else
	{
		lastError=TokenNotFound;
	}
	return 0;
}

int	hash_get_string(
	HashEntry *tab[],
	const char * token,
	char **value)
{
	char *sval;
	if ((sval=hash_lookup(tab,token)))
	{
		*value=sval;
		return 1;
	}
	else
	{
		lastError=TokenNotFound;
	}
	return 0;
}

int
hash_makehash(
	const char *token)
{
	/* Uses an alphabetic hash on the first character of the token */
	int res=toupper(token[0])-'A';
	return res;
}
