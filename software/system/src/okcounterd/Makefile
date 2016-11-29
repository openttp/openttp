PROGRAM = okcounterd
CXX = g++
INCLUDE = 
LDFLAGS= 
LIBS= -lpthread -lokFrontPanel -ldl
CXXFLAGS= -Wall 
DEFINES= -DDEBUG -DOKFRONTPANEL
OBJECTS = OKCounterD.o Client.o Main.o Server.o

.SUFFIXES: .o .cpp

all: $(PROGRAM)

$(OBJECTS): %.o:%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(DEFINES)  -c $<

$(PROGRAM): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS)

install: $(PROGRAM)
	@ echo Installing $(PROGRAM)
	@ cp $(PROGRAM) /usr/local/bin
	@ if [[ `systemctl` =~ -\.mount ]]; then \
		echo "systemd detected"; \
		cp okcounterd.service /lib/systemd/system; \
 		systemctl enable okcounterd.service; \
		systemctl load   okcounterd.service; \
	  elif [[ `/sbin/init --version` =~ upstart ]]; then\
		echo "upstart detected";\
		cp okcounterd.upstart.conf /etc/init/okcounterd.conf;\
	  else\
		echo "unsupported init system";\
	  fi
	
clean:
	rm -f *.o $(PROGRAM)
