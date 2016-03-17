PROGRAM = okbfloader
CXX = g++
INCLUDE = -I/usr/local/include
LDFLAGS= -L/usr/local/lib
LIBS= -lokFrontPanel -ldl 
OBJECTS = BFLoader.o

.SUFFIXES: .o .cpp

all: $(PROGRAM)

$(OBJECTS): %.o:%.cpp
	$(CXX) $(INCLUDE)  -c $<

$(PROGRAM): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

clean:
	rm -f *.o $(PROGRAM)

