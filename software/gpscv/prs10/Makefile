CPP = g++
PROF = 
INCLUDES = -I/usr/local/include
#DEFINES = -DDEBUG
DEFINES = 
C_FLAGS = -Wall $(PROF) $(INCLUDES)
L_FLAGS = $(PROF)
L_POSTFLAGS = -lm
PROGNAME = prs10c

O_FILES = prs10.o prs10c.o

$(PROGNAME): $(O_FILES)
	rm -f $(PROGNAME)
	$(CPP) $(L_FLAGS) -o $(PROGNAME) $(O_FILES) $(L_POSTFLAGS)
     
.cpp.o:
	$(CPP) -c $(C_FLAGS) $(DEFINES) $<

clean:
	rm -f core *.o  $(PROGNAME)

install:
	cp $(PROGNAME) $(HOME)/bin
	cp prs10.setup  $(HOME)/etc
	
