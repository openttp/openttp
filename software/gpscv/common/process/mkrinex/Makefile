PROGRAM = mkrinex
CXX = g++
INCLUDE = -I/usr/local/include 
LDFLAGS= 
LIBS= -lconfigurator
CXXFLAGS= -Wall -DDEBUG
OBJECTS = Antenna.o Counter.o GPS.o HexBin.o Main.o Receiver.o RIN2CGGTTS.o MakeRINEX.o Javad.o TrimbleResolution.o Utility.o

all: $(PROGRAM)

Antenna.o: Antenna.cpp Antenna.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c Antenna.cpp
	
Counter.o: Counter.cpp Counter.h CounterMeasurement.h Debug.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c Counter.cpp

GPS.o: GPS.cpp  Debug.h GPS.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c GPS.cpp

HexBin.o: HexBin.cpp HexBin.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c HexBin.cpp

Javad.o: Javad.cpp Antenna.h Debug.h GPS.h HexBin.h Receiver.h ReceiverMeasurement.h SVMeasurement.h Javad.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c Javad.cpp
	
Main.o: Main.cpp Debug.h MakeRINEX.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c Main.cpp

Receiver.o: Receiver.cpp Antenna.h Debug.h Receiver.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c Receiver.cpp
	
RIN2CGGTTS.o: RIN2CGGTTS.cpp RIN2CGGTTS.h Utility.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c RIN2CGGTTS.cpp

MakeRINEX.o: MakeRINEX.cpp MakeRINEX.h Antenna.h Counter.h Receiver.h SVMeasurement.h Javad.h TrimbleResolution.h Utility.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c MakeRINEX.cpp

TrimbleResolution.o: TrimbleResolution.cpp Antenna.h Debug.h GPS.h HexBin.h Receiver.h ReceiverMeasurement.h SVMeasurement.h TrimbleResolution.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c TrimbleResolution.cpp

Utility.o: Utility.cpp Utility.h
	$(CXX) $(CXXFLAGS) $(INCLUDE)  -c Utility.cpp
	
$(PROGRAM): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

clean:
	rm -f *.o $(PROGRAM)
 
	