/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Michael J. Wouters
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

#include "processlock.h"

#define BUFLEN 512

char buf[BUFLEN+1];

int 
process_lock_create(
	const char *lockfile,
	const char *procname)
{
	/* Returns 1 if successful, 0 if not */
	
	FILE *flock;
	pid_t pid;
	
	if (NULL == lockfile || NULL == procname){
		return 0;
	}
	
	if (process_lock_test(lockfile,procname)){
		flock = fopen(lockfile,"w");
		if (flock != NULL){ /* maybe we don't have write permission */
			pid = getpid();
			fprintf(flock,"%i %s\n",(int) pid,procname);
			fclose(flock);
			return 1;
		}
	}
	
	return 0;
}

/* Returns 1 if a lock can be created */
int
process_lock_test(
	const char *lockfile,
	const char *procname)
{
	
	struct sysinfo sinfo;
	int error;
	struct stat statbuf;
	time_t tnow;
	int pid,oldpid;
	FILE *flock;
	char oldprocname[BUFLEN+1];
	
	/* First, does the lock file exist ? */
  if (stat(lockfile, &statbuf) < 0){ /* TESTED */
		return 1;
	}
	
	/* Was the lock file created before the last reboot ?*/
	error = sysinfo(&sinfo);
	if (error == 0){ /* TESTED */
		tnow = time(NULL);
		if (statbuf.st_mtim.tv_sec < tnow - sinfo.uptime){ /* the lock file is stale */
			return 1;
		}
	}
	/* If sysinfo() fails, this is not fatal */
	
	/* Time to peek inside the lock file */ 
	flock=fopen(lockfile,"r");
	if (flock!= NULL){
		pid=getpid();
		fgets(buf,BUFLEN,flock);
		sscanf(buf,"%i %s",&oldpid,oldprocname);
		printf("%i %s\n",(int) oldpid,oldprocname);
		if (oldpid != pid &&
		return 1;
	}	
	
	return 0;
}

int
process_lock_remove(
	const char *lockfile)
{
	/* Returns 1 if successful, 0 if not */
	return 0;
}

