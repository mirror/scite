# Make file for SciTE on Windows
# Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
# The License.txt file describes the conditions under which this software may be distributed.
# This makefile assumes the mingw32 version of GCC 4.x is used and changes will
# be needed to use other compilers.

.SUFFIXES: .cxx .properties
WINDRES ?= windres
ifeq (\,$(shell echo \\))
	SHELL_TYPE = sh
endif

ifeq ($(OS),Windows_NT)
	CC = gcc
	DEL = $(if $(wildcard $(dir $(SHELL))rm.exe), $(dir $(SHELL))rm.exe -f, del)
	COPY = $(if $(wildcard $(dir $(SHELL))cp.exe), $(dir $(SHELL))cp.exe -a, xcopy /Y)
	VER = cmd /c ver
	# Discover Windows version by running 'VER' command and parsing output
	# For Windows 2000 looks like:Microsoft Windows 2000 [Version 5.00.2195]
	WINVERWORDS:=$(wordlist 1,2,$(subst ., ,$(lastword $(shell $(VER)))))
	WINVER:=$(firstword $(WINVERWORDS)).$(lastword $(WINVERWORDS))
	# Windows NT 4 and Windows 2000 do not support themes so turn off
ifeq '$(WINVER)' '4.0'
	VERSION_DEFINES:=-DDISABLE_THEMES -DWIN_TARGET=0x0400
else
ifeq '$(WINVER)' '5.00'
	VERSION_DEFINES:=-DDISABLE_THEMES
else
	UXLIB:=-luxtheme
endif
endif
else
	DEL = rm -f
	COPY = cp -a
	UXLIB:=-luxtheme
endif

PROG	= ../bin/SciTE.exe
PROGSTATIC = ../bin/Sc1.exe
WIDEFLAGS=-DUNICODE -D_UNICODE

vpath %.h ../src ../../scintilla/include
vpath %.cxx ../src
vpath %.o ../../scintilla/win32
vpath %.a ../../scintilla/win32

ifndef NO_LUA
LUA_CORE_OBJS = lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o \
		lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o \
		ltable.o ltm.o lundump.o lvm.o lzio.o

LUA_LIB_OBJS =	lauxlib.o lbaselib.o lbitlib.o lcorolib.o ldblib.o liolib.o lmathlib.o ltablib.o \
		lstrlib.o loadlib.o loslib.o linit.o lutf8lib.o

LUA_OBJS = LuaExtension.o $(LUA_CORE_OBJS) $(LUA_LIB_OBJS)

vpath %.c ../lua/src ../lua/src/lib

LUA_INCLUDES = -I ../lua/src
#LUA_SCRIPTS = ..\bin\SciTEExtension.lua ..\bin\SciTEStartup.lua
LUA_DEFINES = -DLUA_USER_H=\"scite_lua_win.h\"
else
LUA_DEFINES = -DNO_LUA
endif

INCLUDEDIRS=-I ../../scintilla/include -I ../src $(LUA_INCLUDES)
RCINCLUDEDIRS=--include-dir ../src

CBASEFLAGS = -W -Wall $(INCLUDEDIRS) $(LUA_DEFINES) $(WIDEFLAGS) $(VERSION_DEFINES)

ifdef DEBUG
CFLAGS=-DDEBUG -g $(CBASEFLAGS)
else
CFLAGS=-DNDEBUG -Os $(CBASEFLAGS)
STRIPFLAG="-Wl,-s"
endif

CXXFLAGS = $(CFLAGS) -pedantic --std=gnu++17

LDFLAGS=-mwindows -lmsimg32 -lcomctl32 -limm32 -lole32 -luuid -loleaut32 $(UXLIB) $(LUA_LDFLAGS)

.cxx.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

SHAREDOBJS=\
	Cookie.o \
	DirectorExtension.o \
	ExportHTML.o \
	ExportPDF.o \
	ExportRTF.o \
	ExportTEX.o \
	ExportXML.o \
	FilePath.o \
	FileWorker.o \
	GUIWin.o \
	IFaceTable.o \
	JobQueue.o \
	MatchMarker.o \
	MultiplexExtension.o \
	PropSetFile.o \
	SciTEBase.o \
	SciTEBuffers.o \
	SciTEIO.o \
	SciTEProps.o \
	SciTEWinBar.o \
	SciTEWinDlg.o \
	StringHelpers.o \
	StringList.o \
	Strips.o \
	StyleDefinition.o \
	StyleWriter.o \
	UniqueInstance.o \
	Utf8_16.o \
	WinMutex.o

OTHER_OBJS = $(SHAREDOBJS) $(LUA_OBJS) SciTERes.o SciTEWin.o

OBJS = Credits.o $(OTHER_OBJS)

DLLS=../bin/Scintilla.dll ../bin/SciLexer.dll

NOTBINPROPS=SciTE.properties Embedded.properties
PROPS=$(addprefix ../bin/,$(filter-out $(NOTBINPROPS), $(notdir $(wildcard ../src/*.properties))))

ALL:	$(PROG) $(PROGSTATIC) $(DLLS) $(PROPS) $(LUA_SCRIPTS)

clean:
	$(DEL) *.exe *.o *.obj *.dll *.res *.map *.plist

analyze:
	clang --analyze $(CXXFLAGS) *.cxx ../src/*.cxx

deps:
	$(CXX) -MM $(CXXFLAGS) *.cxx ../src/*.cxx ../lua/src/*.c >deps.mak

../bin/Scintilla.dll:	../../scintilla/bin/Scintilla.dll
ifneq (sh,$(SHELL_TYPE))
	$(COPY) $(subst /,\, $< $(@D))
else
	$(COPY) $^ $(@D)
endif

../bin/SciLexer.dll:	../../scintilla/bin/SciLexer.dll
ifneq (sh,$(SHELL_TYPE))
	$(COPY) $(subst /,\, $< $(@D))
else
	$(COPY) $^ $(@D)
endif

# Using '/' rather than '\' as that allows make to match the next rule
../bin/%.properties:	../src/%.properties
ifneq (sh,$(SHELL_TYPE))
	$(COPY) $(subst /,\, $< $(@D))
else
	$(COPY) $^ $(@D)
endif

$(PROG): $(OBJS)
	$(CXX) $(STRIPFLAG) -Xlinker --subsystem -Xlinker windows -o  $@ $^ $(LDFLAGS)

OBJSSTATIC = \
	$(SHAREDOBJS) \
	Credits.o \
	Sc1.o \
	Sc1Res.o \
	Accessor.o \
	AutoComplete.o \
	CallTip.o \
	CaseConvert.o \
	CaseFolder.o \
	Catalogue.o \
	CellBuffer.o \
	CharacterCategory.o \
	CharacterSet.o \
	CharClassify.o \
	ContractionState.o \
	DBCS.o \
	Decoration.o \
	DefaultLexer.o \
	Document.o \
	EditModel.o \
	Editor.o \
	EditView.o \
	ExternalLexer.o \
	HanjaDic.o \
	Indicator.o \
	KeyMap.o \
	LexerBase.o \
	LexerModule.o \
	LexerSimple.o \
	LineMarker.o \
	MarginView.o \
	PerLine.o \
	PlatWin.o \
	PositionCache.o \
	PropSetSimple.o \
	RESearch.o \
	RunStyles.o \
	ScintillaBaseL.o \
	ScintillaWinL.o \
	Selection.o \
	Style.o \
	StyleContext.o \
	UniConversion.o \
	ViewStyle.o \
	WordList.o \
	XPM.o \
	Lexers.a \
	$(LUA_OBJS)

$(PROGSTATIC): $(OBJSSTATIC)
	$(CXX) $(STRIPFLAG) -static -Xlinker --subsystem -Xlinker windows -o $@ $^ $(LDFLAGS)

# Automatically generate dependencies for most files with "make deps"
include deps.mak

Sc1.o: SciTEWin.cxx SciTEWin.h SciTE.h PropSetFile.h \
 Scintilla.h Extender.h FilePath.h SciTEBase.h FileWorker.h JobQueue.h
	$(CXX) $(CXXFLAGS) -D STATIC_BUILD -c $< -o $@

SciTERes.o:	SciTERes.rc SciTE.h SciTE.exe.manifest
	$(WINDRES) $(RCINCLUDEDIRS) SciTERes.rc $@

# Also depends on ../src/Embedded.properties but may not want to build everywhere
# so must explicitly ask to build it.
Sc1Res.o:	SciTERes.rc SciTE.h SciTE.exe.manifest
	$(WINDRES) $(RCINCLUDEDIRS) SciTERes.rc --define STATIC_BUILD $@

# Make sure Credits gets rebuilt (so its about box gets a new
# date stamp) when any of the other objects are updated.
Credits.o: $(OTHER_OBJS)
