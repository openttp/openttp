CC=gcc
CFLAGS= -Wall
LIBS = -lm
INCLUDE=
TARGETS= mjd  lockport 
INSTALLS = ${TARGETS}
DEFINES =
INSTALLDIR=/usr/local/bin
SHELL=/bin/bash

all:	$(TARGETS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) $(DEFINES) -c  $*.c

mjd: mjd.o
	$(CC) $(CFLAGS) $(INCLUDE) -o mjd mjd.o $(LIBS)

lockport: lockport.o
	$(CC) $(CFLAGS) -o lockport lockport.o 

mjd.o: mjd.c 

lockport.o: lockport.c

install: ${TARGETS}
	@ echo "Installing to ${INSTALLDIR}"
	@ cp ${INSTALLS} ${INSTALLDIR}
	@ chown root. ${INSTALLDIR}/${INSTALLS}
	
	@ cat /etc/issue | grep Red; if [ $$? -eq 0 ]; then \
		chown root.lock ${INSTALLDIR}/lockport; \
		chmod g+s ${INSTALLDIR}/lockport; \
	fi
		
clean:
	rm -f *.o core.* $(TARGETS) 
