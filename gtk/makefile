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

#Comment / Uncomment the next line out if you want GTK single instance support
SINGLEINSTANCE= -DSINGLE_INSTANCE

#CXXFLAGS= -g -DGTK -DSCI_LEXER -DSYSCONF_PATH=\"$(SYSCONF_PATH)\" -Wwrite-strings 
INCLUDEDIRS=-I ../../scintilla/include -I ../src 
CXXBASEFLAGS= -DGTK -DSCI_LEXER -DSYSCONF_PATH=\"$(SYSCONF_PATH)\" $(SINGLEINSTANCE) -W -Wall

ifdef DEBUG
CXXFLAGS=-DDEBUG $(CXXBASEFLAGS)
else
CXXFLAGS=-DNDEBUG $(CXXBASEFLAGS)
endif

.cxx.o:
	$(CC) `gtk-config --cflags` $(INCLUDEDIRS) $(CXXFLAGS) -c $< -o $@

all: $(PROG)

LEXEROBJS=\
	../../scintilla/gtk/LexConf.o \
	../../scintilla/gtk/LexCPP.o \
	../../scintilla/gtk/LexHTML.o \
	../../scintilla/gtk/LexLua.o \
	../../scintilla/gtk/LexOthers.o \
	../../scintilla/gtk/LexPascal.o \
	../../scintilla/gtk/LexPerl.o \
	../../scintilla/gtk/LexPython.o \
	../../scintilla/gtk/LexSQL.o \
	../../scintilla/gtk/LexVB.o

$(PROG): SciTEGTK.o SciTEBase.o SciTEBuffers.o SciTEIO.o Exporters.o \
SciTEProps.o ../../scintilla/bin/scintilla.a $(LEXEROBJS)
	$(CC) `gtk-config --libs` -DGTK $^ -o $@

# SciTE-static no longer builds. Kept here in case of later need.
SciTE-static: SciTEGTK.o SciTEBase.o SciTEBuffers.o SciTEIO.o Exporters.o SciTEProps.o \
KeyWords.o Document.o Scintilla.o PropSet.o
	$(CC) -L/usr/X11R6/lib -lXext -lX11 -lm -DGTK -DSCI_LEXER SciTE.o KeyWords.o Document.o Scintilla.o PropSet.o /usr/local/lib/libgtk.a /usr/local/lib/libgdk.a /usr/local/lib/libgmodule.a /usr/local/lib/libglib.a -o SciTE-static

SciTEBase.o: SciTEBase.cxx Platform.h WinDefs.h SciTE.h PropSet.h SString.h \
 Accessor.h WindowAccessor.h KeyWords.h Scintilla.h SciLexer.h SciTEBase.h
SciTEBuffers.o: SciTEBuffers.cxx Platform.h WinDefs.h SciTE.h PropSet.h SString.h \
 Accessor.h WindowAccessor.h KeyWords.h Scintilla.h SciLexer.h SciTEBase.h
SciTEIO.o: SciTEIO.cxx Platform.h WinDefs.h SciTE.h PropSet.h SString.h \
 Accessor.h WindowAccessor.h KeyWords.h Scintilla.h SciLexer.h SciTEBase.h
Exporters.o: Exporters.cxx Platform.h WinDefs.h SciTE.h PropSet.h SString.h \
 Accessor.h WindowAccessor.h KeyWords.h Scintilla.h SciLexer.h SciTEBase.h
SciTEProps.o: SciTEProps.cxx Platform.h WinDefs.h SciTE.h PropSet.h SString.h \
 Accessor.h WindowAccessor.h KeyWords.h Scintilla.h SciLexer.h SciTEBase.h
SciTEGTK.o: SciTEGTK.cxx Platform.h WinDefs.h SciTE.h PropSet.h SString.h \
 Accessor.h WindowAccessor.h KeyWords.h Scintilla.h ScintillaWidget.h SciTEBase.h

clean: 
	rm -f *.o $(PROG)
	
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
