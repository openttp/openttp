BINDIR = $(HOME)/bin
ETCDIR = $(HOME)/etc
GROUP = suck
CC = g++
DEBUGFLAG = -g
LIBS = -L/usr/local/lib -lcwd -lconfigurator -lstdc++
DEBUGFLAG = -g
CFLAGS = -DCWDEBUG -Wall $(DEBUGFLAG) -I/usr/local/include
LDFLAGS = $(DEBUGFLAG)

LCDMONOBJS = CFA635.o Debug.o ConfirmationDialog.o Dialog.o Label.o IPWidget.o LCDMonitor.o Menu.o Main.o Widget.o

LCDCONFIGOBJS=CFA635.o Debug.o LCDConfigure.o

all: mkversion lcdmonitor lcdconfig

.cpp.o:
	$(CC) $(CFLAGS) $(DEFINES) -c  $*.cpp

mkversion:
	./mkversion.pl
	
lcdmonitor:	$(LCDMONOBJS)
	$(CC) $(CFLAGS) -o lcdmonitor $(LCDMONOBJS) $(LIBS)

lcdconfig:	$(LCDCONFIGOBJS)
	$(CC) $(CFLAGS) -o lcdconfig $(LCDCONFIGOBJS) $(LIBS)
	
CFA635.o: CFA635.h

ConfirmationDialog.o: ConfirmationDialog.h Dialog.h Widget.h KeyEvent.h

Debug.o: Debug.h

Dialog.o: Dialog.h Widget.h KeyEvent.h

IPWidget.o: Widget.h IPWidget.h KeyEvent.h

Label.o: Widget.h Label.h

LCDMonitor.o: CFA635.h Dialog.h IPWidget.h LCDMonitor.h MenuCallback.h KeyEvent.h Functor.h \
	Version.h

LCDConfigure.o: CFA635.h LCDConfigure.h Version.h

Main.o: LCDMonitor.h

Menu.o: Menu.h

Widget.o: KeyEvent.h Widget.h

install:
	-/bin/cp ./lcdmonitor $(BINDIR)
	-/bin/cp ./lcdconfig $(BINDIR)
	-/bin/cp ./lcdmonitor.conf $(ETCDIR)
clean:
	rm -f $(OBJS) $(TARGET)

distribution:	clean
	tar -C .. -cvzf ../lcdmonitor.tgz lcdmon
