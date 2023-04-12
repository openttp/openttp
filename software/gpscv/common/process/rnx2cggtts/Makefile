PROGRAM = rnx2cggtts
CXX = g++
INCLUDE = -I/usr/local/include 
LDFLAGS= 
LIBS= -lconfigurator -lboost_regex -lgsl -lgslcblas
CXXFLAGS= -Wall -Wno-unused-variable -DDEBUG -g
CFGFLAGS= 
OBJECTS = Application.o  CGGTTS.o CGGTTSOutput.o GNSSSystem.o  GPS.o Main.o Measurements.o RINEXFile.o RINEXNavFile.o RINEXObsFile.o Timer.o Utility.o

all: $(PROGRAM)

Application.o: Application.cpp  Application.h Debug.h CGGTTS.h CGGTTSOutput.h GNSSSystem.h\
	RINEXNavFile.h RINEXObsFile.h Timer.h Utility.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Application.cpp

CGGTTS.o: CGGTTS.cpp CGGTTS.h GNSSSystem.h RINEXFile.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c CGGTTS.cpp

CGGTTSOutput.o: CGGTTSOutput.cpp CGGTTSOutput.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c CGGTTSOutput.cpp

GPS.o: GPS.cpp GPS.h Application.h Debug.h GNSSSystem.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c GPS.cpp
	
GNSSSystem.o: GNSSSystem.cpp GNSSSystem.h Application.h Debug.h RINEXFile.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c GNSSSystem.cpp
	
Main.o: Main.cpp Debug.h Application.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Main.cpp

Measurements.o: Measurements.cpp Measurements.h 
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Measurements.cpp
	
RINEXFile.o: RINEXFile.cpp RINEXFile.h Application.h Debug.h 
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c RINEXFile.cpp

RINEXObsFile.o: RINEXObsFile.cpp  RINEXObsFile.h Application.h Debug.h GPS.h Measurements.h RINEXFile.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c RINEXObsFile.cpp
	
RINEXNavFile.o: RINEXNavFile.cpp  RINEXNavFile.h Application.h Debug.h GPS.h RINEXFile.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c RINEXNavFile.cpp
	
Timer.o: Timer.cpp Timer.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Timer.cpp

Utility.o: Utility.cpp Utility.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Utility.cpp

$(PROGRAM): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

clean:
	rm -f *.o $(PROGRAM)
	
