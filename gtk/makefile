# Make file for SciTE on Linux or compatible OS
# Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
# The License.txt file describes the conditions under which this software may be distributed.
# This makefile assumes GCC 2.95.2 is used and changes will be needed to use other compilers.
# GNU make does not like \r\n line endings so should be saved to CVS in binary form.

.SUFFIXES: .cxx .o .h .a
CC = g++
AR = ar

# If explicit setting of GTK1 or GTK2 then use that else look for
# pkg-config which is an OK indication that GTK2 is available
ifndef GTK1
ifndef GTK2
ifneq (,$(findstring /,$(shell whereis -b pkg-config)))
GTK2=1
else
GTK1=1
endif
endif
endif

# For the Gnome desktop stuff to work, prefix must point to where Gnome thinks it is.
# "h@h@" is printed here when gnome-config unavailable. Seems harmless.
ifdef GTK2
CONFIGFLAGS=pkg-config --cflags gtk+-2.0
CONFIGLIB=pkg-config --libs gtk+-2.0 gthread-2.0
gnomeprefix:=$(shell pkg-config --variable=prefix gtk+-2.0 2>/dev/null)
else
CONFIGFLAGS=gtk-config --cflags
CONFIGLIB=gtk-config --libs
gnomeprefix:=$(shell gnome-config --prefix 2>/dev/null)
endif
ifdef gnomeprefix
  prefix=$(gnomeprefix)
else
  prefix=/usr
endif
datadir=$(prefix)/share
pixmapdir=$(datadir)/pixmaps
bindir=$(prefix)/bin
SYSCONF_PATH=$(prefix)/share/scite

PROG	= ../bin/SciTE

vpath %.h ../src ../../scintilla/include
vpath %.cxx ../src

#CXXFLAGS= -g -DGTK -DSCI_LEXER -DSYSCONF_PATH=\"$(SYSCONF_PATH)\" -Wwrite-strings
INCLUDEDIRS=-I ../../scintilla/include -I ../src
CXXBASEFLAGS=-W -Wall -DGTK -DSCI_LEXER -DSYSCONF_PATH=\"$(SYSCONF_PATH)\" $(INCLUDEDIRS)

ifdef DEBUG
CXXFLAGS=-DDEBUG -g $(CXXBASEFLAGS)
else
CXXFLAGS=-DNDEBUG -Os $(CXXBASEFLAGS)
endif

ifeq ($(shell uname), Darwin)
CXXFLAGS:=$(CXXFLAGS) -Dunix
endif

.cxx.o:
	$(CC) `$(CONFIGFLAGS)` $(CXXFLAGS) -c $< -o $@

all: $(PROG)

clean:
	rm -f *.o $(PROG)

deps:
	$(CC) -MM `$(CONFIGFLAGS)` $(CXXFLAGS) *.cxx ../src/*.cxx >deps.mak

# make should be run in ../../scintilla/gtk to compile all the lexers.
COMPLIB=../../scintilla/bin/scintilla.a

$(PROG): SciTEGTK.o SciTEBase.o SciTEBuffers.o SciTEIO.o Exporters.o \
MultiplexExtension.o DirectorExtension.o SciTEProps.o Utf8_16.o $(COMPLIB)
	$(CC) `$(CONFIGLIB)` `glib-config --libs gthread` -DGTK $^ -o $@

# Automatically generate header dependencies with "make deps"
include deps.mak

# The two last install commands will fail if Gnome is not installed or is not at $(prefix).
# This is OK - just means no SciTE in the Gnome Applications menu
# Dead:	install -D SciTEGTK.properties $(SYSCONF_PATH)/SciTEGlobal.properties
install:
	install -s $(PROG) $(bindir)
	install -d $(SYSCONF_PATH)
	install ../src/*.properties $(SYSCONF_PATH)
	install ../doc/*.html $(SYSCONF_PATH)
	install ../doc/SciTEIco.png $(SYSCONF_PATH)
	install ../doc/PrintHi.png $(SYSCONF_PATH)
ifdef gnomeprefix
ifndef GTK2
	install SciTE.desktop $(datadir)/gnome/apps/Applications
endif
	install Sci48M.png $(pixmapdir)
endif

uninstall:
	rm -f $(bindir)/SciTE
	rm -rf $(SYSCONF_PATH)
ifdef gnomeprefix
	rm -f $(datadir)/gnome/apps/Applications/SciTE.desktop
	rm -f $(pixmapdir)/Sci48M.png
endif
