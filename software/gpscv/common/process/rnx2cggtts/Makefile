PROGRAM = rnx2cggtts
CXX = g++
INCLUDE = -I/usr/local/include 
LDFLAGS= 
LIBS= -lconfigurator -lboost_regex -lgsl -lgslcblas
CXXFLAGS= -Wall -Wno-unused-variable -DDEBUG -g
CFGFLAGS= 
OBJECTS = Application.o  CGGTTS.o CGGTTSOutput.o GNSSSystem.o  Main.o RINEXFile.o RINEXNavFile.o Timer.o Utility.o

all: $(PROGRAM)

Application.o: Application.cpp  Application.h Debug.h CGGTTS.h CGGTTSOutput.h GNSSSystem.h Timer.h Utility.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Application.cpp

CGGTTS.o: CGGTTS.cpp CGGTTS.h GNSSSystem.h RINEXFile.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c CGGTTS.cpp

CGGTTSOutput.o: CGGTTSOutput.cpp CGGTTSOutput.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c CGGTTSOutput.cpp
	
GNSSSystem.o: GNSSSystem.cpp GNSSSystem.h RINEXFile.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c GNSSSystem.cpp
	
Main.o: Main.cpp Debug.h Application.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Main.cpp

RINEXFile.o: RINEXFile.cpp RINEXFile.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c RINEXFile.cpp

RINEXNavFile.o: RINEXNavFile.cpp  RINEXNavFile.h RINEXFile.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c RINEXNavFile.cpp
	
Timer.o: Timer.cpp Timer.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Timer.cpp

Utility.o: Utility.cpp Utility.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Utility.cpp

$(PROGRAM): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

clean:
	rm -f *.o $(PROGRAM)
	
