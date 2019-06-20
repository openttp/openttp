PROGRAM = ppsdevlog
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
	cp $(PROGRAM) /usr/local/bin
