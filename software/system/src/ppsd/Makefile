SHELL=/bin/bash
PROGRAM = ppsd
CC = gcc
INCLUDE = 
LDFLAGS= 
LIBS= 
CFLAGS= -Wall -O -DUSE_PARALLEL_PORT
OBJECTS = ppsd.o

.SUFFIXES: .o .c

all: $(PROGRAM)

$(OBJECTS): %.o:%.c
	$(CC) $(CFLAGS) $(INCLUDE)  -c $<

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

install: $(PROGRAM)
	cp ppsd.conf /usr/local/etc
	@ if [[ `systemctl` =~ -\.mount ]]; then \
		echo "systemd detected"; \
		systemctl stop ppsd.service; \
		cp $(PROGRAM) /usr/local/sbin; \
		cp ppsd.service /lib/systemd/system; \
 		systemctl enable ppsd.service; \
	  elif [[ `/sbin/init --version` =~ upstart ]]; then\
		echo "upstart detected";\
		stop ppsd ;\
		cp $(PROGRAM) /usr/local/sbin; \
		cp ppsd.upstart.conf /etc/init/ppsd.conf; \
	  else\
		echo "unsupported init system- installing executable anyway";\
		cp $(PROGRAM) /usr/local/sbin; \
	  fi

clean:
	rm -f *.o $(PROGRAM)
