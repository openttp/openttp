PROGRAM = ntploadtest
CC = gcc
INCLUDE = 
LDFLAGS= 
LIBS= -lm
CFLAGS= -Wall -O
OBJECTS = ntploadtest.o

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
