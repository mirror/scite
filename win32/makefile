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

LDFLAGS = -lkernel32 -lgdi32 -luser32 -lcomdlg32 -lcomctl32 -limm32 -lole32 -luuid
#CXXFLAGS = -W -Wall
# Add -MMD to get dependencies
#CXXFLAGS = -g -pg -pedantic -Os -fno-exceptions -fvtable-thunks -fno-rtti
INCLUDEDIRS=-I ../../scintilla/include -I ../../scintilla/win32 -I ../src 
RCINCLUDEDIRS=--include-dir ../../scintilla/win32 --include-dir ../src 
CXXBASEFLAGS = -W -Wall -pedantic $(INCLUDEDIRS) -Os -fno-exceptions -fvtable-thunks -fno-rtti

ifdef DEBUG
CXXFLAGS=-DDEBUG $(CXXBASEFLAGS)
else
CXXFLAGS=-DNDEBUG $(CXXBASEFLAGS)
endif

.cxx.o:
	$(CC) $(CXXFLAGS) -c $< -o $@

LEXOBJS	=  LexCPP.o LexHTML.o LexLua.o LexOthers.o LexPerl.o LexPython.o \
LexSQL.o LexVB.o LexConf.o
OBJS	= SciTEBase.o SciTEBuffers.o SciTEIO.o SciTEProps.o \
SciTEWin.o SciTEWinBar.o SciTEWinDlg.o WindowAccessor.o \
PropSet.o PlatWin.o UniConversion.o SciTERes.o

DLLS=..\bin\Scintilla.dll ..\bin\SciLexer.dll

PROPS=..\bin\SciTEGlobal.properties ..\bin\cpp.properties ..\bin\html.properties \
..\bin\latex.properties ..\bin\lua.properties ..\bin\others.properties \
..\bin\perl.properties ..\bin\python.properties ..\bin\sql.properties ..\bin\vb.properties \
..\bin\conf.properties

LEXPROPS=..\bin\cpp.properties ..\bin\html.properties \
..\bin\latex.properties ..\bin\lua.properties ..\bin\others.properties \
..\bin\perl.properties ..\bin\python.properties ..\bin\sql.properties ..\bin\vb.properties \
..\bin\conf.properties

ALL:	$(PROG) $(PROGSTATIC) $(DLLS) $(PROPS)

clean:
	del /q *.exe *.o *.obj *.dll *.res *.map

..\bin\Scintilla.dll:	..\..\scintilla\bin\Scintilla.dll
	copy ..\..\scintilla\bin\Scintilla.dll $@

..\bin\SciLexer.dll:	..\..\scintilla\bin\SciLexer.dll
	copy ..\..\scintilla\bin\SciLexer.dll $@
	
..\bin\SciTEGlobal.properties:	..\src\SciTEGlobal.properties
	copy $^ $@
..\bin\cpp.properties:	..\src\cpp.properties
	copy $^ $@
..\bin\html.properties:	..\src\html.properties
	copy $^ $@
..\bin\latex.properties:	..\src\latex.properties
	copy $^ $@
..\bin\lua.properties:	..\src\lua.properties
	copy $^ $@
..\bin\others.properties:	..\src\others.properties
	copy $^ $@
..\bin\perl.properties:	..\src\perl.properties
	copy $^ $@
..\bin\python.properties:	..\src\python.properties
	copy $^ $@
..\bin\sql.properties:	..\src\sql.properties
	copy $^ $@
..\bin\vb.properties:	..\src\vb.properties
	copy $^ $@
..\bin\conf.properties:	..\src\conf.properties
	copy $^ $@

# Normally distributed rather than built as may not have grep on all machines
# Copy all non-comment lines from all the properties files into one combined file
..\src\Embedded.properties: $(LEXPROPS)
	fgrep -G -v -h "^#" $(LEXPROPS) >..\src\Embedded.properties

$(PROG): $(OBJS)
	$(CC) "-Wl,-s" -Xlinker --subsystem -Xlinker windows -o  $@ $^ $(LDFLAGS)

OBJSSTATIC = Sc1.o SciTEWinBar.o SciTEWinDlg.o SciTEBase.o SciTEBuffers.o \
	SciTEIO.o SciTEProps.o ScintillaWinL.o ScintillaBaseL.o Editor.o \
	WindowAccessor.o DocumentAccessor.o KeyWords.o $(LEXOBJS) PropSet.o \
	ContractionState.o Document.o CellBuffer.o CallTip.o Sc1Res.o PlatWin.o UniConversion.o \
	KeyMap.o Indicator.o LineMarker.o Style.o ViewStyle.o AutoComplete.o

$(PROGSTATIC): $(OBJSSTATIC)
	$(CC) "-Wl,-s" -Xlinker --subsystem -Xlinker windows -o $@ $^ $(LDFLAGS)

SciTEBase.o: SciTEBase.cxx Platform.h SciTE.h PropSet.h Accessor.h WindowAccessor.h \
 KeyWords.h Scintilla.h SciLexer.h SciTEBase.h Extender.h
SciTEBuffers.o:	SciTEBuffers.cxx Platform.h SciTE.h PropSet.h Accessor.h WindowAccessor.h \
 KeyWords.h Scintilla.h SciLexer.h SciTEBase.h Extender.h
SciTEIO.o:	SciTEIO.cxx Platform.h SciTE.h PropSet.h Accessor.h WindowAccessor.h \
 KeyWords.h Scintilla.h SciLexer.h SciTEBase.h Extender.h
SciTEProps.o:	SciTEProps.cxx Platform.h SciTE.h PropSet.h Accessor.h WindowAccessor.h \
 KeyWords.h Scintilla.h SciLexer.h SciTEBase.h Extender.h
SciTEWin.o: SciTEWin.cxx SciTEWin.h Platform.h SciTE.h PropSet.h Accessor.h \
 WindowAccessor.h KeyWords.h Scintilla.h SciTEBase.h Extender.h
SciTEWinBar.o: SciTEWinBar.cxx SciTEWin.h Platform.h SciTE.h PropSet.h Accessor.h \
 WindowAccessor.h KeyWords.h Scintilla.h SciTEBase.h Extender.h
SciTEWinDlg.o: SciTEWinDlg.cxx SciTEWin.h Platform.h SciTE.h PropSet.h Accessor.h \
 WindowAccessor.h KeyWords.h Scintilla.h SciTEBase.h Extender.h

#	$(CC) -Xlinker --subsystem -Xlinker windows -o $(PROGSTATIC) $(OBJSSTATIC) $(LDFLAGS)

Sc1.o: SciTEWin.cxx SciTEWin.h SciTE.h Accessor.h WindowAccessor.h KeyWords.h PropSet.h Scintilla.h Extender.h
	$(CC) $(CXXFLAGS) -D STATIC_BUILD -c $< -o $@

SciTERes.o:	SciTERes.rc SciTE.h PlatformRes.h
	windres $(RCINCLUDEDIRS) SciTERes.rc $@

# Also depends on ../src/Embedded.properties but may not want to build everywhere
# so must explicitly ask to build it.
Sc1Res.o:	SciTERes.rc SciTE.h PlatformRes.h
	windres $(RCINCLUDEDIRS) SciTERes.rc --define STATIC_BUILD $@
	
