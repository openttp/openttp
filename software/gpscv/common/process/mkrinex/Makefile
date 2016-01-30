PROGRAM = mkrinex
CXX = g++
INCLUDE = -I/usr/local/include 
LDFLAGS= 
LIBS= -lconfigurator
CXXFLAGS= -Wall -DDEBUG -g
CFGFLAGS= 
OBJECTS = Antenna.o Counter.o GPS.o HexBin.o Main.o Receiver.o RIN2CGGTTS.o MakeRINEX.o ReceiverMeasurement.o \
	CGGTTS.o RINEX.o \
	Javad.o NVS.o TrimbleResolution.o \
	Utility.o

all: $(PROGRAM)

Antenna.o: Antenna.cpp Antenna.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Antenna.cpp
	
Counter.o: Counter.cpp Counter.h CounterMeasurement.h Debug.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Counter.cpp

CGGTTS.o : CGGTTS.cpp CGGTTS.h  Debug.h Antenna.h Counter.h Receiver.h MakeRINEX.h MeasurementPair.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c CGGTTS.cpp
	
GPS.o: GPS.cpp  Debug.h GPS.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c GPS.cpp

HexBin.o: HexBin.cpp HexBin.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c HexBin.cpp

Javad.o: Javad.cpp Antenna.h Debug.h GPS.h HexBin.h Receiver.h ReceiverMeasurement.h SVMeasurement.h Javad.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Javad.cpp

NVS.o: NVS.cpp Antenna.h Debug.h GPS.h HexBin.h Receiver.h ReceiverMeasurement.h SVMeasurement.h NVS.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c NVS.cpp
	
Main.o: Main.cpp Debug.h MakeRINEX.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Main.cpp

Receiver.o: Receiver.cpp Antenna.h Debug.h Receiver.h ReceiverMeasurement.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Receiver.cpp

ReceiverMeasurement.o: ReceiverMeasurement.cpp ReceiverMeasurement.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c ReceiverMeasurement.cpp

RIN2CGGTTS.o: RIN2CGGTTS.cpp RIN2CGGTTS.h Utility.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c RIN2CGGTTS.cpp

MakeRINEX.o: MakeRINEX.cpp MakeRINEX.h Antenna.h Counter.h Debug.h Receiver.h MeasurementPair.h SVMeasurement.h Javad.h NVS.h \
	ReceiverMeasurement.h RINEX.h TrimbleResolution.h Utility.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c MakeRINEX.cpp

RINEX.o: RINEX.cpp RINEX.h Debug.h Antenna.h Counter.h Receiver.h MakeRINEX.h MeasurementPair.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c RINEX.cpp

TrimbleResolution.o: TrimbleResolution.cpp Antenna.h Debug.h GPS.h HexBin.h Receiver.h ReceiverMeasurement.h SVMeasurement.h TrimbleResolution.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c TrimbleResolution.cpp

Utility.o: Utility.cpp Utility.h
	$(CXX) $(CXXFLAGS) $(CFGFLAGS) $(INCLUDE)  -c Utility.cpp
	
$(PROGRAM): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

clean:
	rm -f *.o $(PROGRAM)
 
	