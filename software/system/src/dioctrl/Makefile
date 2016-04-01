PROGRAM = dioctrl
CC = gcc
INCLUDE = 
LDFLAGS= 
LIBS= 
#CFLAGS= -Wall -O -DUSE_PARALLEL_PORT
CFLAGS= -Wall -O -DUSE_SIO8186x
OBJECTS = dioctrl.o

.SUFFIXES: .o .c

all: $(PROGRAM)

$(OBJECTS): %.o:%.c
	$(CC) $(CFLAGS) $(INCLUDE)  -c $<

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

clean:
	rm -f *.o $(PROGRAM)

install:
	cp $(PROGRAM) /usr/local/bin
