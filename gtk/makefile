# Make file for SciTE on Linux or compatible OS
# Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
# The License.txt file describes the conditions under which this software may be distributed.
# This makefile assumes GCC 2.95.2 is used and changes will be needed to use other compilers.
# GNU make does not like \r\n line endings so should be saved to CVS in binary form.

.SUFFIXES: .cxx .o .h .a
CC = g++
AR = ar

# For the Gnome desktop stuff to work, prefix must point to where Gnome thinks it is.
prefix=/usr/local
bindir=$(prefix)/bin
datadir=$(prefix)/share
pixmapdir=$(datadir)/pixmaps
SYSCONF_PATH=$(prefix)/share/scite

PROG	= ../bin/SciTE

vpath %.h ../src ../../scintilla/include
vpath %.cxx ../src

#CXXFLAGS= -g -DGTK -DSCI_LEXER -DSYSCONF_PATH=\"$(SYSCONF_PATH)\" -Wwrite-strings 
INCLUDEDIRS=-I ../../scintilla/include -I ../src 
CXXFLAGS= -DGTK -DSCI_LEXER -DSYSCONF_PATH=\"$(SYSCONF_PATH)\" -W -Wall

.cxx.o:
	$(CC) `gtk-config --cflags` $(INCLUDEDIRS) $(CXXFLAGS) -c $< -o $@

all: $(PROG)

LEXEROBJS=\
	../../scintilla/gtk/LexCPP.o \
	../../scintilla/gtk/LexPython.o \
	../../scintilla/gtk/LexHTML.o \
	../../scintilla/gtk/LexLua.o \
	../../scintilla/gtk/LexSQL.o \
	../../scintilla/gtk/LexOthers.o \
	../../scintilla/gtk/LexVB.o \
	../../scintilla/gtk/LexPerl.o

$(PROG): SciTEGTK.o SciTEBase.o ../../scintilla/bin/scintilla.a $(LEXEROBJS)
	$(CC) `gtk-config --libs` -DGTK $^ -o $@

# SciTE-static no longer builds. Kept here in case of later need.
SciTE-static: SciTEGTK.o SciTEBase.o KeyWords.o Document.o Scintilla.o PropSet.o
	$(CC) -L/usr/X11R6/lib -lXext -lX11 -lm -DGTK -DSCI_LEXER SciTE.o KeyWords.o Document.o Scintilla.o PropSet.o /usr/local/lib/libgtk.a /usr/local/lib/libgdk.a /usr/local/lib/libgmodule.a /usr/local/lib/libglib.a -o SciTE-static

SciTEBase.o: SciTEBase.cxx Platform.h WinDefs.h SciTE.h PropSet.h \
 Accessor.h WindowAccessor.h KeyWords.h Scintilla.h SciLexer.h SciTEBase.h
SciTEGTK.o: SciTEGTK.cxx Platform.h WinDefs.h SciTE.h PropSet.h \
 Accessor.h WindowAccessor.h KeyWords.h Scintilla.h ScintillaWidget.h SciTEBase.h

clean: 
	rm -f *.o $(PROG)
	
# The two last install commands will fail if Gnome is not installed or is not at $(prefix).
# This is OK - just means no SciTE in the Gnome Applications menu
install:
	install -s $(PROG) $(bindir)
	install -D SciTEGTK.properties $(SYSCONF_PATH)/SciTEGlobal.properties
	install SciTE.desktop $(datadir)/gnome/apps/Applications
	install Sci48M.png $(pixmapdir)

uninstall:
	rm -f $(bindir)/SciTE
	rm -rf $(SYSCONF_PATH)
	rm -f $(datadir)/gnome/apps/Applications/SciTE.desktop
	rm -f $(pixmapdir)/Sci48M.png
