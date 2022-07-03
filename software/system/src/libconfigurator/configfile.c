/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Michael J. Wouters
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
 * Modification history
 * 28-05-2002 MJW First version.
 * 29-05-2002 MJW Extra whitespace removed from tokens
 * 06-06-2002 MJW Extraction of section names to aid building 
 *                gui from config file
 * 18-07-2002 MJW Rearranged into a static library. One library to rule them all
 *                Better handling of missing token/value
 * 25-11-2003 MJW Added configfile_isasection()
 * 08-03-2006 MJW Remove error messages
 * 10-08-2015 MJW Silly bug in parsing into a list - comments not ignored !								
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <regex.h>
#include <stdlib.h>
#include <unistd.h>

#include "configurator.h"

#define TRUE 1
#define FALSE 0

static int configfile_iscomment(char *buf);
static int configfile_issection(char *buf,const char *s);
static int configfile_istoken(char *buf);
static int configfile_split(char delim,char *buf,char *t,char *v);
static int configfile_get_section_name(char *buf,char **name);
static int configfile_isasection(char *buf);

int lastError;


void 
lowercase(
	char *s)
{
	int i=0;
	while (s[i] !=0)
	{
		s[i]= tolower(s[i]);
		i++;
	}
}

int 
config_file_get_last_error(
	char *buf,
	int buflen)
{
	if (buf != NULL)
		errtostr(lastError,buf,buflen);
	return lastError;
}

int 
	configfile_update(
	const char *section,
	const char *token,
	const char *value,
	const char *filename)
{
	/* XML would be sooo ... much neater */
	/* What we do is open the config file and write it line by line to a temporary file 
	 * and then copy it across 
	 */
	
	#define BUFSIZE 1024
	char buf[BUFSIZE],tmp[BUFSIZE],atoken[BUFSIZE],avalue[BUFSIZE],lctoken[BUFSIZE],lcatoken[BUFSIZE];
	 FILE *fin,*fout;	
	#ifdef DEBUG
	fprintf(stderr,"configfile_update() section =%s,token=%s,value=%s,file=%s\n",section,token,value,filename);
	#endif
	
	strncpy(lctoken,token,BUFSIZE-1);
	lowercase(lctoken);
	 if (!(fin = fopen(filename,"r")))
	 {
		 lastError = FileNotFound;
		 return FALSE;
	 }
	
	 strncpy(tmp,filename,BUFSIZE-1);
	 strncat(tmp,".tmp",BUFSIZE-1-strlen(tmp));
	 
	 if (!(fout = fopen(tmp,"w")))
	 {
		 lastError = FileNotFound;
		 return FALSE;
	 }
	 
	 /* Search for the section name */
	
	while (fgets(buf,BUFSIZE,fin) != NULL )
	{

		if (configfile_issection(buf,section)) /* Got the section we want */
		{
			fprintf(fout,"%s",buf); 
			while (fgets(buf,BUFSIZE,fin) != NULL ) /* keep on reading */
			{ 
				if (configfile_istoken(buf))
				{
					if (configfile_split('=',buf,atoken,avalue))
					{
						strncpy(lcatoken,atoken,BUFSIZE-1);
						lowercase(lcatoken);
						if (0==strcmp(lcatoken,lctoken))
						{
							fprintf(fout,"%s=%s\n",atoken,value); /* preserve case of original */
						}
						else
							fprintf(fout,"%s",buf); 
					}
					else
						fprintf(fout,"%s",buf);  /* munged ? just echo it */
				}
				else if (configfile_isasection(buf))
				{
					fprintf(fout,"%s",buf); 
					break; /* Reached the end of the section */
				}
				else
					fprintf(fout,"%s",buf); /* comment/whitespace, I suppose */
			}
		}
		else
			fprintf(fout,"%s",buf);
	}
	
	fclose(fin);
	fclose(fout);
	 
	unlink(filename);
	rename(tmp,filename);
	
	return TRUE;
	 #undef BUFSIZE
}

int 
configfile_parse(
	HashEntry *tab[],
	const char *sectionname,
	const char *filename)
{
	#define BUFSIZE 1024
	char buf[BUFSIZE],token[BUFSIZE],value[BUFSIZE];
	
	FILE *fin;
	
	lastError = NoError;
	
	if (!(fin = fopen(filename,"r")))
	{
		lastError = FileNotFound;
		return FALSE;
	}
	/* Search for the section name */
	
	while (!feof(fin))
	{
		fgets(buf,BUFSIZE,fin);
		if (configfile_iscomment(buf))
		{
			/* do nothing */	
		}
		else if (configfile_issection(buf,sectionname))
			break;
	}
	
	if (feof(fin)) /* got to end */
	{
		lastError=SectionNotFound;
		return FALSE;
	}
	
	/* Now read in (token,value) pairs */
	while (!feof(fin) && fgets(buf,BUFSIZE,fin))
	{
		
		if (configfile_iscomment(buf))
		{
			/* do nothing */	
		}
		else if (configfile_istoken(buf))
		{
			if (configfile_split('=',buf,token,value))
				hash_addentry(tab,token,value);
		}
		else if (configfile_isasection(buf))
			break; /* Reached the end of the section */
	}
	
	fclose(fin);
	return TRUE;
	#undef BUFSIZE
}

int 
configfile_getsections(
	char ***sections,
	int *nsections,
	const char *filename)
{
	/* Get strings labelling sections */
	#define BUFSIZE 1024
	char buf[BUFSIZE];
	char **sbuf,*s;
	FILE *fin;
	*nsections=0;
	sbuf = (char **) malloc(sizeof(char *)*128);
	

	if (!(fin = fopen(filename,"r")))
	{
		lastError = FileNotFound;
		return FALSE;
	}
	/* Search for the section name */
	
	while (!feof(fin))
	{
		fgets(buf,BUFSIZE,fin);
		if (configfile_iscomment(buf))
		{
			/* do nothing */	
		}
		else if (configfile_issection(buf,".*"))
		{
			configfile_get_section_name(buf,&s);
			sbuf[*nsections]=strdup(s);
			(*nsections)++;
		}
	}
	fclose(fin);
	*sections = sbuf;
	
	return TRUE;
	#undef BUFSIZE
}

int
errtostr(
	int errnum,
	char *buf,
	int bufsize)
{
	int retval=1;
	switch (errnum)
	{
		case NoError:
			strncpy(buf,"no errors",bufsize);
			break;
		case FileNotFound:
			strncpy(buf,"file not found",bufsize);
			break;
		case SectionNotFound:
			strncpy(buf,"section not found",bufsize);
			break;
		case InternalError:
			strncpy(buf,"internal error",bufsize);
			break;
		case ParseFailed:
			strncpy(buf,"parsing failure",bufsize);
			break;
		case TokenNotFound:
			strncpy(buf,"token not found",bufsize);
			break;
		default:
			retval=0;
	}
	return retval;
}

/*
 * Private functions
 */
 
int 
configfile_iscomment(
	char *buf
	)
{
	char *ch;
	/* ignore any leading whitespace */
	ch = &(buf[0]);
	while (isspace(*ch) && (*ch != '\0')) ch++;
	return (*ch == '#');
}


int
configfile_issection(
	char *buf,
	const char *s)
{
	#define BUFSIZE 1024
	regex_t preg;
	char rbuf[BUFSIZE+1+12];
	char lcbuf[BUFSIZE+1];
	char lcs[BUFSIZE+1];
	int res;
	/* Sections are of the form [SectionName] */
	/* Case insensitive ! */
	strncpy(lcbuf,buf,BUFSIZE);
	strncpy(lcs,s,BUFSIZE);
	lowercase(lcbuf);
	lowercase(lcs);
	snprintf(rbuf,BUFSIZE+12,"^ *\\[ *%s *\\]",lcs); 
	if ( 0 != (res = regcomp(&preg,rbuf,REG_EXTENDED | REG_NOSUB)))
		lastError = InternalError;
	res = regexec(&preg,lcbuf,0,NULL,0);
	regfree(&preg);
	if (REG_NOMATCH == res)
		return FALSE;
	else
	{
		#ifdef DEBUG
		fprintf(stderr,"configfile_issection() (%s) %s",s,buf);
		#endif
		return TRUE;
	}
	#undef BUFSIZE 
}

int
configfile_isasection(
	char *buf)
{
	/* Finds a section identifier */
	#define BUFSIZE 1024
	regex_t preg;
	char rbuf[BUFSIZE];
	int res;
	/* Sections are of the form [SectionName] */
	sprintf(rbuf,"^ *\\[.*\\]"); 
	if ( 0 != (res = regcomp(&preg,rbuf,REG_EXTENDED | REG_NOSUB)))
		lastError = InternalError;
	res = regexec(&preg,buf,0,NULL,0);
	regfree(&preg);
	if (REG_NOMATCH == res)
		return FALSE;
	else
	{
		#ifdef DEBUG
		fprintf(stderr,"configfile_isasection() %s",buf);
		#endif
		return TRUE;
	}
	#undef BUFSIZE 
}

int
configfile_istoken(
	char *buf
	)
{
	regex_t preg;
	int res;
	
	/* Token value pairs are of the form string = string */
	if ( 0 != (res = regcomp(&preg,"^.*=.*",REG_EXTENDED | REG_NOSUB)))
		lastError = InternalError;
	res = regexec(&preg,buf,0,NULL,0);
	regfree(&preg);
	if (REG_NOMATCH == res)
		return FALSE;
	else
	{
		#ifdef DEBUG
		fprintf(stderr,"configfile_istoken() %s",buf);
		#endif
		return TRUE;
	}
}

int
configfile_split(
	char delim,
	char *buf,
	char *t,
	char *v)
{
	/* Splits a string into two parts around the first occurrence of delim
	 * removing leading and trailing white space from the two parts
	 */
	char *pdelim,*pchstart,*pchend,*pch;
	int i;
	
	/* Test for existence of delimiter  */
	if (!(pdelim=strchr(buf,delim))) return FALSE;
	
	/* Get the token */
	pchstart = &(buf[0]);
	while (isspace(*pchstart) && (pchstart < pdelim)) pchstart++; /* eat leading white space */
	/* Test whether the token is missing */
	if (pchstart == pdelim) return FALSE;
	/* Now work backwards from delim to find the first non-whitespace character */
	pchend = --pdelim; /* safe because of initial tests on pdelim */
	while (isspace(*pchend) && pchend > &(buf[0]) ) pchend--;
	/* Eat any extra whitespace in the token (eg separating words) */
	pch=pchstart;
	i=0;
	while (pch <= pchend)
	{
		if (isspace(*pch))
		{
			/* Could have used a tab etc rather than a single space */
			if (*pch == ' ')
				t[i]=*pch; /* want the first space */
			else
				t[i]= ' '; /* replace with a space */
			i++;
			pch++;
			while (isspace(*pch)) /* eat any following whitespace */
				pch++;
		}
		else
		{
			t[i]=*pch;
			i++;
			pch++;
		}
	}
	t[i]='\0';
	
	/* Get the value */
	pdelim++; /* undo previous decrement */
	pchstart = pdelim;
	pchstart++;
	
	while (isspace(*pchstart)) pchstart++; /* eat my space */
	if (*pchstart == 0) return FALSE; /* missing value */
	pchend=&(buf[strlen(buf)-1]);
	while (isspace(*pchend) && (pchend >= pchstart) ) 
		pchend--;
	strncpy(v,pchstart,pchend-pchstart+1);
	v[pchend-pchstart+1]='\0';

	#ifdef DEBUG
		fprintf(stderr,"configfile_split(): <%s> <%s>\n",t,v);
	#endif
	
	return TRUE;
}

static int
configfile_get_section_name(
	char *buf,
	char **name)
{
	/* Extract a section name from buf, stripping white space from ends etc */
	char *pchstart,*pchend;
	
	pchstart=&(buf[0]);
	while ((*pchstart)!='[') pchstart++;
	pchstart++;
	while (isspace(*pchstart)) pchstart++;
	
	pchend = &buf[strlen(buf)-1];
	while ((*pchend)!=']') pchend--;
	pchend--;
	while (isspace(*pchend)) pchend--;
	
	*name = (char *) malloc(sizeof(char)*(pchend-pchstart+2));
	strncpy(*name,pchstart,pchend-pchstart+1);
	(*name)[pchend-pchstart+1]='\0';
	
	return TRUE;
}

int 
configfile_parse_as_list(
	ListEntry **first,
	const char *filename)
{
	#define BUFSIZE 1024
	char buf[BUFSIZE],token[BUFSIZE],value[BUFSIZE];
	char emptysection[1]="";
	char *currsection=emptysection;
	char *s;
	FILE *fin;
	ListEntry *curr=NULL;

	#ifdef DEBUG
	fprintf(stderr,"configfile_parse_as_list %s\n",filename);
	#endif
	
	
	lastError = NoError;
	
	if (!(fin = fopen(filename,"r")))
	{
		lastError = FileNotFound;
		return FALSE;
	}
	/* Search for the section name */
	
	while (fgets(buf,BUFSIZE-1,fin) != NULL )
	{
		if (configfile_iscomment(buf)){
			/* do nothing */
		}
		else if (configfile_isasection(buf))
		{
			if (configfile_get_section_name(buf,&s))
				currsection=s;
		}
		else if (configfile_istoken(buf))
		{
			if (configfile_split('=',buf,token,value))
				curr=list_add_entry(curr,currsection,token,value);
		}
	}

	fclose(fin);
	*first=curr; /* actually returns last */
	return TRUE;
	#undef BUFSIZE
}

#undef TRUE 
#undef FALSE
