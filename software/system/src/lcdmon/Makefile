INSTDIR=/usr/local
BINDIR = $(INSTDIR)/bin
ETCDIR = $(INSTDIR)/etc
GROUP = suck
CC = g++
DEBUGFLAG = -g
LIBS = -L/usr/local/lib -lconfigurator -lstdc++ -lboost_regex
DEBUGFLAG = -g
CFLAGS = -Wall -Wno-unused $(DEBUGFLAG) -I/usr/local/include
LDFLAGS = $(DEBUGFLAG)

LCDMONOBJS = Button.o CFA635.o Debug.o ConfirmationDialog.o Dialog.o Label.o IPWidget.o LCDMonitor.o Menu.o Main.o Widget.o \
	SliderWidget.o IntensityWidget.o ContrastWidget.o Wizard.o MessageBox.o

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
	
Button.o: Button.h Widget.h Functor.h

CFA635.o: CFA635.h

ConfirmationDialog.o: ConfirmationDialog.h Dialog.h Widget.h KeyEvent.h

ContrastWidget.o: Widget.h SliderWidget.h KeyEvent.h ContrastWidget.h

Debug.o: Debug.h

Dialog.o: Dialog.h Widget.h KeyEvent.h

IPWidget.o: Widget.h IPWidget.h KeyEvent.h

IntensityWidget.o: Widget.h SliderWidget.h KeyEvent.h IntensityWidget.h

Label.o: Widget.h Label.h

LCDMonitor.o: CFA635.h Dialog.h IPWidget.h LCDMonitor.h WidgetCallback.h KeyEvent.h Functor.h \
	Version.h SliderWidget.h IntensityWidget.h ContrastWidget.h Wizard.h

LCDConfigure.o: CFA635.h LCDConfigure.h Version.h

MessageBox.o: Widget.h Dialog.h Label.h KeyEvent.h

Wizard.o: Wizard.h Dialog.h Widget.h KeyEvent.h Button.h WidgetCallback.h

Main.o: LCDMonitor.h

Menu.o: Menu.h

SliderWidget.o: Widget.h SliderWidget.h KeyEvent.h

Widget.o: KeyEvent.h Widget.h

install: mkversion lcdmonitor lcdconfig
	-/bin/cp ./lcdmonitor $(BINDIR)
	-/bin/cp ./lcdconfig  $(BINDIR)
	-/bin/cp ./lcdmonitor.conf $(ETCDIR)
	-/bin/cp ./sysinfo.conf $(ETCDIR)
	chown root. $(BINDIR)/lcdmonitor
	chmod og-x  $(BINDIR)/lcdmonitor
	chown root. $(BINDIR)/lcdconfig
	chmod og-x  $(BINDIR)/lcdconfig
	chown root. $(ETCDIR)/lcdmonitor.conf
	chmod og-w  $(ETCDIR)/lcdmonitor.conf
	chown root. $(ETCDIR)/sysinfo.conf
	chmod og-w  $(ETCDIR)/sysinfo.conf

clean:
	rm -f $(LCDMONOBJS) $(LCDCONFIGOBJS) lcdmonitor lcdconfig

distribution:	clean
	tar -C .. -cvzf ../lcdmonitor.tgz lcdmon
