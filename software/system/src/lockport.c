/*
 * Creates a UUCP style lock file in /var/lock
 * At the very least, the lock file prevents minicom and kermit
 * from trying to use the port.
 *
 * Usage
 *   Create a lock
 * 	   [-d lockfile directory] port process name
 *   The default lock directory is /var/lock 
 *
 *   Remove a lock
 *  -r [-d lockfile directory] port
 *
 * 
 * Example:
 *     
 *		lockport /dev/ttyC3 nurse_piggy.pl
 *
 * Returns 
 *    0 = fail
 *    1 = OK
 *
 */

/*
 * Modification history
 * 10.02.2000 MJW First version
 *  6.06.2001 RBW Command line option to pass in PID
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>

#define VERSION "lockport 1.0"
#define DEFLOCKFILEDIR "/var/lock/" /* default directory for the lock file */
#define LOCKPREFIX  "LCK.."
#define MAXBUF 1024 /* Maximum number of characters in path name */

extern char *optarg; /* globals available for use with getopt() */
extern int optind;

/* Set a sane search path for safety's sake */
char *env[]={"PATH=/bin:/usr/bin:/usr/sbin/",NULL};

int 
main(int argc,
char *argv[])
{

	pid_t pid,oldpid;
	uid_t uid;
	
	struct passwd *pw;
	struct stat statbuf;
	
	FILE *fout;
	FILE *fin;
	
	char fName[MAXBUF],buf[MAXBUF],procName[MAXBUF];
	int removeLock=0;
	char c, *pChar;
	
	/* Get stuff about the calling process */
	pid=getpgrp(); /* process ID */
	uid=getuid();
	pw= getpwuid(uid);
	
  setuid(0); /* become superdude */
	
 	strcpy(fName,DEFLOCKFILEDIR);
	
	/* Process options with getopt */
	
	while ((c=getopt(argc,argv,"rhd:p:")) != EOF){
		switch(c){
			case 'd': /* directory for lock file */
				strcpy(fName,optarg);
				break;
			case 'p': /* set PID */
			  pid=atoi(optarg);
				break;
			case 'r':	/* remove lock file */
				removeLock=1;
				break;
			case 'h': /* print help */
				fprintf(stdout,"%s\n\n",VERSION);
				fprintf(stdout,"usage : lockport [-r] [-d directory_name] [-p PID] port [process_name]\n");
				fprintf(stdout,"-r  : remove a lock\n");
				fprintf(stdout,"-d  : directory for lock file\n");
				fprintf(stdout,"-p  : set PID to be written to file\n");
				fprintf(stdout,"-h  : print help\n");
				fprintf(stdout,"Note: process name required for creating a lock only\n");
				return 0;
				break;
		}
	}
	
	if (removeLock){
	  
		if (optind == argc - 1){ /* Should be one more argument */
			/* Test for trailing / in the directory name  - if it ain't there, add it */
			if (fName[strlen(fName)-1] != '/') strcat(fName,"/");
			strcat(fName,LOCKPREFIX);
			
			if ((pChar = (char *) strstr(argv[argc-1],"/dev/")) != NULL)/* Strip /dev/ from the port name */
				pChar += 5;
			else
				pChar=argv[argc-1];
			strcat(fName,pChar);
			if (unlink(fName)==0){
				fprintf(stdout,"1\n");
				return 0;
			}
			else{
				fprintf(stdout,"0\n"); /* failed to remove the lock */
				return 0;
			}
		}
		else{
			fprintf(stderr,"lockport(): wrong number of arguments\n");
			fprintf(stdout,"0\n");
			return 0;
		}
	}
	else{
		if (optind==argc-2){/* Should be two more arguments */
			if (fName[strlen(fName)-1] != '/') strcat(fName,"/");
			strcat(fName,LOCKPREFIX);
			if ((pChar = (char *) strstr(argv[argc-2],"/dev/")) != NULL)/* Strip /dev/ from the port name */
				pChar += 5;
			else
				pChar = argv[argc-2];
			strcat(fName,pChar);
			strcpy(procName,argv[argc-1]);
		}
		else{
			fprintf(stderr,"lockport(): wrong number of arguments\n");
			fprintf(stdout,"0\n");
			return 0;
		}
	}
	
  /* If we got here then we want to make a new lock file */
 
	/* First, does the lock file exist ? */
	
  if (stat(fName, &statbuf) == 0){ /* lock file exists */
		/* Check whether the process that created the lock is still alive*/
	  fin=fopen(fName,"r");
		if (fin != NULL){
			fgets(buf,MAXBUF,fin);
			sscanf(buf,"%i %*s %*s",&oldpid);
			if ((oldpid > 0) && (kill(oldpid,0) <0) && (errno == ESRCH) ){ /* Stale lock */
				unlink(fName);
			}
			else{ /* Back off, man ! */
				fprintf(stdout,"0\n");
				return 0;
			}
		}
		else{ /* Failed to open the lock file - shouldn't happen but ... */
			fprintf(stdout,"0\n");
			return 0;
		}
	}
	
	/* If we get here then it's OK to make the lock file */
	fout=fopen(fName,"w");
	if (fout != NULL){
		fprintf(fout,"%i %s %s",pid,procName,pw->pw_name);
		fclose(fout);
		fprintf(stdout,"1\n");
	}
	else
		fprintf(stdout,"0\n"); /* We failed for whatever reason */

	
	return 0;
}
