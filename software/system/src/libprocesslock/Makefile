CC = gcc
AR = ar rcs
RM = rm -f
RANLIB = ranlib
#DEFINES= -DDEBUG
DEFINES=
SRCS= processlock.c test.c

OBJS= processlock.o test.o

all: test libprocesslock.a $(OBJS)

.c.o:
	$(CC) $(DEFINES) -Wall -c  -g $*.c

libprocesslock.a: processlock.o
	$(AR) $@ processlock.o
	-$(RANLIB) $@

test: test.o libprocesslock.a
	$(CC) -o test test.o -L./ -lprocesslock
	
processlock.o: processlock.h

test.o: processlock.h 

install: processlock.a
	cp libprocesslock.a /usr/local/lib
	cp processlock.h /usr/local/include
	chmod a+r /usr/local/lib/libprocesslock.a
	chmod a+r /usr/local/include/processlock.h

clean:
	$(RM) *.o libprocesslock.a processlock.a test
