###############################################
# select a system:

# Where we will install Zed
PREFIX=/usr/local

# Pentium Linux
CFLAGS=-pipe -Wall -malign-jumps=2 -malign-functions=2 -fomit-frame-pointer -m486 -O2

# i486 Linux
#CFLAGS=-pipe -Wall -fomit-frame-pointer -m486 -O2

# HPUX
#ILIBS=-lm
#IXLIBS=-lm
#CFLAGS=-pipe -Wall -mpa-risc-1-1 -O2

# RS6000 AIX
#CFLAGS=-pipe -Wall -O2

# Sun
#ILIBS=-lm
#IXLIBS=-lm
#CFLAGS=-pipe -Wall -O2 -msupersparc

COMPILER=gcc
# someone needs this:
#COMPILER=g++

LINKER=gcc

###############################################

#CFLAGS=-pipe -Wall -m486 -g
#CFLAGS=-pipe -Wall -mpa-risc-1-1 -ggdb
#CFLAGS=-pipe -Wall -malign-jumps=2 -malign-functions=2 -m486 -ggdb
#CFLAGS=-pipe -Wall -ggdb

###############################################
# comment this lines if you don't have XPM (used only for the icon)

XPMLIB=-lXpm
XPMOPT=-DXPM_ICON -I/usr/X11R6/include/X11
#XPMOPT=-DXPM_ICON -I/usr/X11/include/X11

###############################################

OBJS=config.o editor.o lowl.o main.o varie.o edvis.o # hpux.o
LIBS=$(ILIBS)
OPTIONS=$(IOPTIONS)

XOBJS=config.xo editor.xo lowl.xo main.xo varie.xo x11part.xo edvis.xo # hpux.xo
XLIBS=-L/usr/X11R6/lib -lX11 $(XPMLIB) $(IXLIBS)
XOPTIONS=$(XPMOPT) $(IXOPTIONS)

all: zed zedx

default:
	 @echo
	 @echo "Type one of the following:"
	 @echo "  make zed      ansi/vt100 version"
	 @echo "  make zedx     X11 version"
	 @echo "  make install"
	 @echo "  make clean"
	 @echo

zed : $(OBJS)
	 $(LINKER) -o zed $(AFLAGS) $(OBJS) $(LIBS)

zedx : $(XOBJS)
	 $(LINKER) -o zedx $(AFLAGS) $(XOBJS) $(XLIBS)

clean:
	 rm -rf core *.o *.xo *~

install: zed zedx
	 install -m 755 -d $(PREFIX)/bin
	 install -m 755 -d $(PREFIX)/etc
	 install -m 755 -d $(PREFIX)/lib/zed
	 install -s -m 755 zed $(PREFIX)/bin/zed
	 install -s -m 755 zedx $(PREFIX)/bin/zedx
	 install -m 644 cfg/zedxrc $(PREFIX)/etc/zedxrc
	 install -m 644 cfg/zedrc $(PREFIX)/etc/zedrc
	 install -m 644 cfg/zedrc.xterm $(PREFIX)/etc/zedrc.xterm
	 install -m 644 zed.doc $(PREFIX)/lib/zed/zed.doc
	 install -m 644 zedico.xpm $(PREFIX)/lib/zed/zedico.xpm
	 install -m 644 README $(PREFIX)/lib/zed/README
	 install -m 644 zed.png $(PREFIX)/lib/zed/zed.png
	 install -m 644 zedlogo.png $(PREFIX)/lib/zed/zedlogo.png
	 @echo
	 @echo "Zed is now installed in your system."
	 @echo
	 @echo "Please READ at least the README file. It contain important information."
	 @echo

.SUFFIXES:

%.xo : %.cc
	 $(COMPILER) $(CFLAGS) $(AFLAGS) -DX11 $(XOPTIONS) -o $@ -c $<

%.o  : %.cc
	 $(COMPILER) $(CFLAGS) $(AFLAGS) $(OPTIONS) -o $@ -c $<

### Dependencies:
config.o: config.cc zed.h
editor.o: editor.cc zed.h
edvis.o: edvis.cc zed.h
lowl.o: lowl.cc zed.h
main.o: main.cc zed.h
varie.o: varie.cc zed.h
config.xo: config.cc zed.h
editor.xo: editor.cc zed.h
lowl.xo: lowl.cc zed.h
main.xo: main.cc zed.h
varie.xo: varie.cc zed.h
x11part.xo: x11part.cc zedico.xpm zed.h

