CC=gcc
CFLAGS= -Wall
LIBS = -lm
INCLUDE=
TARGETS= mjd  lockport 
INSTALLS = ${TARGETS} kickstart.pl
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

install:
	@ echo "Installing to ${INSTALLDIR}"
	@ cp ${TARGETS} ${INSTALLDIR}
	@ chown root. ${INSTALLDIR}/${TARGETS}
	
	@ cat /etc/issue | grep Red; if [ $$? -eq 0 ]; then \
		chown root.lock ${INSTALLDIR}/lockport; \
		chmod g+s ${INSTALLDIR}/lockport; \
	fi
	
	@ # perl -e "print qq(@INC)"
	@ if [ -d /usr/local/lib/site_perl ]; then \
		echo "Installing Perl libraries to /usr/local/lib/site_perl"; \
		cp TFLibrary.pm /usr/local/lib/site_perl; \
		chown root. /usr/local/lib/site_perl/TFLibrary.pm; \
	elif [ -d /usr/lib/perl5/site_perl ]; then \
		echo "Installing Perl libraries to /usr/lib/perl5/site_perl"; \
		cp TFLibrary.pm /usr/lib/perl5/site_perl; \
		chown root. /usr/lib/perl5/site_perl/TFLibrary.pm; \
	else \
		echo "Not sure where to install Perl libraries"; \
	fi
		
clean:
	rm -f *.o core.* $(TARGETS) 
