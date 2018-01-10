PROGRAM = ppsd
CC = gcc
INCLUDE = 
LDFLAGS= 
LIBS= 
CFLAGS= -Wall -O -DUSE_SIO8186x
OBJECTS = ppsd.o

.SUFFIXES: .o .c

all: $(PROGRAM)

$(OBJECTS): %.o:%.c
	$(CC) $(CFLAGS) $(INCLUDE)  -c $<

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

install: $(PROGRAM)
	cp ppsd.conf /usr/local/etc
	cp $(PROGRAM) /usr/local/sbin
	
clean:
	rm -f *.o $(PROGRAM)