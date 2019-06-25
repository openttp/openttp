PROGRAM = ppsdevlog
SHELL=/bin/bash
INSTDIR=/usr/local
BINDIR = $(INSTDIR)/bin
ETCDIR = /home/ntpadmin/etc
CC = gcc
INCLUDE =
LDFLAGS= 
LIBS= -L/usr/local/lib -lconfigurator
CFLAGS= -Wall -O 
OBJECTS = ppsdevlog.o

.SUFFIXES: .o .c

all: $(PROGRAM)

$(OBJECTS): %.o:%.c
	$(CC) $(CFLAGS) $(INCLUDE)  -c $<

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

clean: 
	rm -f *.o $(PROGRAM)

install: $(PROGRAM)
	@ install -o root -g root -m 0744 $(PROGRAM)  $(BINDIR)
	@ install -o root -g root -m 0644 $(PROGRAM).conf $(ETCDIR)
	@ if [[ `systemctl` =~ -\.mount ]]; then \
		echo "systemd detected"; \
		systemctl stop $(PROGRAM).service; \
		cp $(PROGRAM).service /lib/systemd/system; \
		systemctl enable $(PROGRAM).service; \
	else\
		echo "unsupported init system";\
	fi
