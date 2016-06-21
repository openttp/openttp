CC = gcc
AR = ar clqv
RM = rm -f
RANLIB = ranlib
#DEFINES= -DDEBUG
DEFINES=
SRCS= configfile.c hashtable.c list.c

OBJS= configfile.o hashtable.o list.o

all: libconfigurator.a $(OBJS)

.c.o:
	$(CC) $(DEFINES) -Wall -c  -g $*.c

libconfigurator.a: $(OBJS)
	$(AR) $@ $(OBJS)
	-$(RANLIB) $@

configfile.o: configurator.h

hashtable.o:configurator.h

list.o: configurator.h

install: libconfigurator.a
	cp libconfigurator.a /usr/local/lib
	cp configurator.h /usr/local/include
	chmod a+r /usr/local/lib/libconfigurator.a
	chmod a+r /usr/local/include/configurator.h

clean:
	$(RM) *.o libconfigurator.a
