PROGRAM = okcounterd
CXX = g++
INCLUDE = 
LDFLAGS= 
LIBS= -lpthread -lokFrontPanel -ldl
CXXFLAGS= -Wall -DDEBUG
OBJECTS = OKCounterD.o Client.o Main.o Server.o

.SUFFIXES: .o .cpp

all: $(PROGRAM)

$(OBJECTS): %.o:%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c $<

$(PROGRAM): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

clean:
	rm -f *.o $(PROGRAM)

