# Make file for SciTE on Linux or compatible OS
# Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
# The License.txt file describes the conditions under which this software may be distributed.
# This makefile assumes GCC 2.95.2 is used and changes will be needed to use other compilers.
# GNU make does not like \r\n line endings so should be saved to CVS in binary form.

.SUFFIXES: .cxx .o .h .a
CC = g++
AR = ar

# For the Gnome desktop stuff to work, prefix must point to where Gnome thinks it is.
# "h@h@" is printed here when gnome-config unavailable. Seems harmless.
gnomeprefix:=$(shell gnome-config --prefix 2>/dev/null)
ifdef gnomeprefix
  prefix=$(gnomeprefix)
  datadir=$(gnomeprefix)/share
  pixmapdir=$(datadir)/pixmaps
else
  prefix=/usr/local
endif
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
CXXFLAGS=-DNDEBUG $(CXXBASEFLAGS)
endif

.cxx.o:
	$(CC) `gtk-config --cflags` $(CXXFLAGS) -c $< -o $@

all: $(PROG)

clean:
	rm -f *.o $(PROG)

deps:
	$(CC) -MM `gtk-config --cflags` $(CXXFLAGS) *.cxx ../src/*.cxx >deps.mak

# make should be run in ../../scintilla/gtk to compile all the lexers.
LEXEROBJS=$(wildcard ../../scintilla/gtk/Lex*.o)

$(PROG): SciTEGTK.o SciTEBase.o SciTEBuffers.o SciTEIO.o Exporters.o DirectorExtension.o \
SciTEProps.o ../../scintilla/bin/scintilla.a $(LEXEROBJS)
	$(CC) `gtk-config --libs` -DGTK $^ -o $@

# SciTE-static no longer builds. Kept here in case of later need.
SciTE-static: SciTEGTK.o SciTEBase.o SciTEBuffers.o SciTEIO.o Exporters.o SciTEProps.o \
KeyWords.o Document.o Scintilla.o PropSet.o DirectorExtension.o
	$(CC) -L/usr/X11R6/lib -lXext -lX11 -lm -DGTK -DSCI_LEXER SciTE.o KeyWords.o Document.o Scintilla.o PropSet.o /usr/local/lib/libgtk.a /usr/local/lib/libgdk.a /usr/local/lib/libgmodule.a /usr/local/lib/libglib.a -o SciTE-static

# Automatically generate header dependencies with "make deps"
include deps.mak

# The two last install commands will fail if Gnome is not installed or is not at $(prefix).
# This is OK - just means no SciTE in the Gnome Applications menu
# Dead:	install -D SciTEGTK.properties $(SYSCONF_PATH)/SciTEGlobal.properties
install:
	install -s $(PROG) $(bindir)
	install -d $(SYSCONF_PATH)
	install -D ../src/*.properties $(SYSCONF_PATH)
ifdef gnomeprefix
	install SciTE.desktop $(datadir)/gnome/apps/Applications
	install Sci48M.png $(pixmapdir)
endif

uninstall:
	rm -f $(bindir)/SciTE
	rm -rf $(SYSCONF_PATH)
ifdef gnomeprefix
	rm -f $(datadir)/gnome/apps/Applications/SciTE.desktop
	rm -f $(pixmapdir)/Sci48M.png
endif
