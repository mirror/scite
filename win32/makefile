# Make file for SciTE on Windows
# Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
# The License.txt file describes the conditions under which this software may be distributed.
# This makefile assumes the mingw32 version of GCC 2.95.2 is used and changes will
# be needed to use other compilers.

.SUFFIXES: .cxx
CC = g++
DLLWRAP = dllwrap

PROG	= ../bin/SciTE.exe
PROGSTATIC = ../bin/Sc1.exe

vpath %.h ../src ../../scintilla/include ../../scintilla/win32
vpath %.cxx ../src
vpath %.o ../../scintilla/win32

LDFLAGS = -lkernel32 -lgdi32 -luser32 -lwinmm -lcomdlg32 -lcomctl32 -limm32 -lole32 -luuid
#CXXFLAGS = -W -Wall
# Add -MMD to get dependencies
#CXXFLAGS = -g -pg -pedantic -Os -fno-exceptions -fvtable-thunks -fno-rtti
INCLUDEDIRS=-I ../../scintilla/include -I ../../scintilla/win32 -I ../src 
RCINCLUDEDIRS=--include-dir ../../scintilla/win32 --include-dir ../src 
CXXFLAGS = -W -Wall -pedantic $(INCLUDEDIRS) -Os -fno-exceptions -fvtable-thunks -fno-rtti

.cxx.o:
	$(CC) $(CXXFLAGS) -c $< -o $@

LEXOBJS	=  LexCPP.o LexHTML.o LexOthers.o LexPerl.o LexPython.o LexSQL.o LexVB.o
OBJS	= SciTEBase.o SciTEWin.o WindowAccessor.o PropSet.o PlatWin.o UniConversion.o SciTERes.o

DLLS=..\bin\Scintilla.dll ..\bin\SciLexer.dll

ALL:	$(PROG) $(PROGSTATIC) $(DLLS) ..\bin\SciTEGlobal.properties

clean:
	del /q *.exe *.o *.obj *.dll *.res *.map

..\bin\Scintilla.dll:	..\..\scintilla\bin\Scintilla.dll
	copy ..\..\scintilla\bin\Scintilla.dll ..\bin

..\bin\SciLexer.dll:	..\..\scintilla\bin\SciLexer.dll
	copy ..\..\scintilla\bin\SciLexer.dll ..\bin
	
..\bin\SciTEGlobal.properties:	..\src\SciTEGlobal.properties
	copy ..\src\SciTEGlobal.properties ..\bin

$(PROG): $(OBJS)
	$(CC) "-Wl,-s" -Xlinker --subsystem -Xlinker windows -o $(PROG) $^ $(LDFLAGS)

OBJSSTATIC = Sc1.o SciTEBase.o ScintillaWinL.o ScintillaBaseL.o Editor.o \
	WindowAccessor.o DocumentAccessor.o KeyWords.o $(LEXOBJS) PropSet.o \
	ContractionState.o Document.o CellBuffer.o CallTip.o Sc1Res.o PlatWin.o UniConversion.o \
	KeyMap.o Indicator.o LineMarker.o Style.o ViewStyle.o AutoComplete.o

$(PROGSTATIC): $(OBJSSTATIC)
	$(CC) "-Wl,-s" -Xlinker --subsystem -Xlinker windows -o $(PROGSTATIC) $^ $(LDFLAGS)

SciTEBase.o: SciTEBase.cxx Platform.h SciTE.h PropSet.h Accessor.h WindowAccessor.h \
 KeyWords.h Scintilla.h SciLexer.h SciTEBase.h
SciTEWin.o: SciTEWin.cxx Platform.h SciTE.h PropSet.h Accessor.h WindowAccessor.h \
 KeyWords.h Scintilla.h SciTEBase.h

#	$(CC) -Xlinker --subsystem -Xlinker windows -o $(PROGSTATIC) $(OBJSSTATIC) $(LDFLAGS)

Sc1.o: SciTEWin.cxx SciTE.h Accessor.h WindowAccessor.h KeyWords.h PropSet.h Scintilla.h
	$(CC) $(CXXFLAGS) -D STATIC_BUILD -c $< -o Sc1.o

SciTERes.o:	SciTERes.rc SciTE.h PlatformRes.h
	windres $(RCINCLUDEDIRS) SciTERes.rc SciTERes.o

Sc1Res.o:	SciTERes.rc SciTE.h PlatformRes.h
	windres $(RCINCLUDEDIRS) SciTERes.rc --define STATIC_BUILD Sc1Res.o
