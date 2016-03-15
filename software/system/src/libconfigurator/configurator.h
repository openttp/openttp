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

#ifndef __CONFIGURATOR_H_
#define __CONFIGURATOR_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ALPHABETIC_HASH_SIZE 26


enum Errors {NoError,
						 FileNotFound,
						 SectionNotFound,
						 InternalError,
						 ParseFailed,
						 TokenNotFound};

/* Hash table data structure */  
typedef struct _HashEntry
{
	char *token;
	char *value;
	struct _HashEntry *next;
}HashEntry;


void 				hash_init(HashEntry *tab[],int tabsize);
void 				hash_clear(HashEntry *tab[],int tabsize);
HashEntry * hash_addentry(HashEntry *tab[],const char *token,const char *value);
char *			hash_lookup(HashEntry *tab[],const char * token);

int					hash_get_int(HashEntry *tab[],const char * token,int *value);
int					hash_get_float(HashEntry *tab[],const char * token,float *value);
int					hash_get_double(HashEntry *tab[],const char * token,double *value);
int					hash_get_string(HashEntry *tab[],const char * token,char **value);


/* Deprecated - retain for compatibility */
int					hash_get_intvalue(HashEntry *tab[],const char * token,int *value);
int					hash_get_floatvalue(HashEntry *tab[],const char * token,float *value);
int					hash_get_doublevalue(HashEntry *tab[],const char * token,double *value);
int					hash_get_stringvalue(HashEntry *tab[],const char * token,char **value);
/* End of deprecated functions */

int configfile_parse(HashEntry *tab[],const char *sectionname,const char *filename);
int configfile_getsections(char ***sections,int *nsections,const char *filename);

/* Flat list  data structure */
typedef struct _ListEntry
{
	char *section;
	char *token;
	char *value;
	struct _ListEntry *next;
}ListEntry;


void 				list_clear(ListEntry *first);
ListEntry * list_add_entry(ListEntry *first,const char *section,const char *token,const char *value);
char *			list_lookup(ListEntry *first,const char *section,const char * token);

int					list_get_int(ListEntry *first,const char *section,const char * token,int *value);
int					list_get_float(ListEntry *first,const char *section,const char * token,float *value);
int					list_get_double(ListEntry *first,const char *section,const char * token,double *value);
int					list_get_string(ListEntry *first,const char *section,const char * token,char **value);

/* Deprecated - retain for compatibility */
int					list_get_int_value(ListEntry *first,const char *section,const char * token,int *value);
int					list_get_float_value(ListEntry *first,const char *section,const char * token,float *value);
int					list_get_double_value(ListEntry *first,const char *section,const char * token,double *value);
int					list_get_string_value(ListEntry *first,const char *section,const char * token,char **value);
/* End of deprecated functions */

int configfile_parse_as_list(ListEntry **first,const char *filename);
int config_file_get_last_error(char *buf,int buflen);

/* Generic bits */
int  configfile_update(const char *section,const char *token,const char *value,const char *filename);
void lowercase(char *s);
int  errtostr(int errnum,char *buf,int bufsize);

#ifdef __cplusplus
}
#endif

#endif
