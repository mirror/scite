# Make file for SciTE on Windows Visual C++ and Borland C++ version
# Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
# The License.txt file describes the conditions under which this software may be distributed.
# This makefile is for using Visual C++ with nmake or Borland C++ with make depending on 
# the setting of the VENDOR macro. If no VENDOR is defined n the command line then
# the tool used is automatically detected.
# Usage for Microsoft:
#     nmake -f scite.mak
# Usage for Borland:
#     make -f scite.mak
# For debug versions define DEBUG on the command line.
# The main makefile uses mingw32 gcc and may be more current than this file.

.SUFFIXES: .cxx

DIR_BIN=..\bin

PROG=$(DIR_BIN)\SciTE.exe
PROGSTATIC=$(DIR_BIN)\Sc1.exe
DLLS=$(DIR_BIN)\Scintilla.dll $(DIR_BIN)\SciLexer.dll

!IFNDEF VENDOR
!IFDEF _NMAKE_VER
#Microsoft nmake so make default VENDOR MICROSOFT
VENDOR=MICROSOFT
!ELSE
VENDOR=BORLAND
!ENDIF
!ENDIF

!IF "$(VENDOR)"=="MICROSOFT"

CC=cl
RC=rc
LD=link

INCLUDEDIRS=-I ../../scintilla/include -I ../../scintilla/win32 -I ../src 
CXXFLAGS=/TP /W4 
# For something scary:/Wp64
CXXDEBUG=/Zi /Od /MDd -DDEBUG
CXXNDEBUG=/Ox /MD -DNDEBUG
NAMEFLAG=-Fo
LDFLAGS=/opt:nowin98 /NODEFAULTLIB:LIBC
LDDEBUG=/DEBUG
LIBS=KERNEL32.lib USER32.lib GDI32.lib COMDLG32.lib COMCTL32.lib ADVAPI32.lib IMM32.lib SHELL32.LIB OLE32.LIB

!ELSE
# BORLAND

CC=bcc32
RC=brcc32 -r
LD=ilink32

INCLUDEDIRS=-I../../scintilla/include -I../../scintilla/win32 -I../src 
CXXFLAGS =-v
CXXFLAGS=-P -tWM -w -w-prc -w-inl -RT- -x-
# Above turns off warnings for clarfying parentheses and inlines with for not expanded
CXXDEBUG=-v -DDEBUG
CXXNDEBUG=-O1 -DNDEBUG
NAMEFLAG=-o
LDFLAGS=
LDDEBUG=-v
LIBS=import32 cw32mt

!ENDIF

!IFDEF DEBUG
CXXFLAGS=$(CXXFLAGS) $(CXXDEBUG)
LDFLAGS=$(LDDEBUG) $(LDFLAGS)
!ELSE
CXXFLAGS=$(CXXFLAGS) $(CXXNDEBUG)
!ENDIF

ALL: $(PROG) $(PROGSTATIC) $(DLLS) $(DIR_BIN)\SciTEGlobal.properties

clean:
	del /q $(DIR_BIN)\*.exe *.o *.obj $(DIR_BIN)\*.dll *.res *.map

OBJS=\
	SciTEBase.obj \
	SciTEWin.obj \
	..\..\scintilla\win32\WindowAccessor.obj \
	..\..\scintilla\win32\PropSet.obj \
	..\..\scintilla\win32\PlatWin.obj \
	..\..\scintilla\win32\UniConversion.obj

OBJSSTATIC=\
	Sc1.obj \
	SciTEBase.obj \
	..\..\scintilla\win32\KeyWords.obj \
	..\..\scintilla\win32\LexCPP.obj \
	..\..\scintilla\win32\LexHTML.obj \
	..\..\scintilla\win32\LexLua.obj \
	..\..\scintilla\win32\LexOthers.obj \
	..\..\scintilla\win32\LexPerl.obj \
	..\..\scintilla\win32\LexPython.obj \
	..\..\scintilla\win32\LexSQL.obj \
	..\..\scintilla\win32\LexVB.obj \
	..\..\scintilla\win32\WindowAccessor.obj \
	..\..\scintilla\win32\DocumentAccessor.obj \
	..\..\scintilla\win32\PropSet.obj \
	..\..\scintilla\win32\ScintillaWinL.obj \
	..\..\scintilla\win32\ScintillaBaseL.obj \
	..\..\scintilla\win32\Editor.obj \
	..\..\scintilla\win32\Document.obj \
	..\..\scintilla\win32\CellBuffer.obj \
	..\..\scintilla\win32\ContractionState.obj \
	..\..\scintilla\win32\CallTip.obj \
	..\..\scintilla\win32\PlatWin.obj \
	..\..\scintilla\win32\UniConversion.obj \
	..\..\scintilla\win32\KeyMap.obj \
	..\..\scintilla\win32\Indicator.obj \
	..\..\scintilla\win32\LineMarker.obj \
	..\..\scintilla\win32\Style.obj \
	..\..\scintilla\win32\ViewStyle.obj \
	..\..\scintilla\win32\AutoComplete.obj

$(DIR_BIN)\Scintilla.dll: ..\..\scintilla\bin\Scintilla.dll
	copy ..\..\scintilla\bin\Scintilla.dll $@

$(DIR_BIN)\SciLexer.dll: ..\..\scintilla\bin\SciLexer.dll
	copy ..\..\scintilla\bin\SciLexer.dll $@

$(DIR_BIN)\SciTEGlobal.properties: ..\src\SciTEGlobal.properties
	copy ..\src\SciTEGlobal.properties $@

# A custom rule for .obj files built by scintilla:
..\..\scintilla\win32\PlatWin.obj: 	..\..\scintilla\win32\PlatWin.cxx
	echo You must run the Scintilla makefile to build $*.obj
	fail_the_build # Is there an official way to do this?

!IF "$(VENDOR)"=="MICROSOFT"

$(PROG): $(OBJS) SciTERes.res
	$(LD) $(LDFLAGS) /OUT:$@ $(OBJS) SciTERes.res $(LIBS)

SciTERes.res: SciTERes.rc ..\src\SciTE.h ..\..\scintilla\win32\PlatformRes.h
	$(RC) $(INCLUDEDIRS) -fo$@ SciTERes.rc

$(PROGSTATIC): $(OBJSSTATIC) Sc1Res.res
	$(LD) $(LDFLAGS) /OUT:$@ $(OBJSSTATIC) Sc1Res.res $(LIBS)

Sc1Res.res: SciTERes.rc ..\src\SciTE.h ..\..\scintilla\win32\PlatformRes.h
	$(RC) $(INCLUDEDIRS) -dSTATIC_BUILD -fo$@ SciTERes.rc

!ELSE

$(PROG): $(OBJS) SciTERes.res
	$(LD) $(LDFLAGS) -Tpe -aa -Gn -x c0w32 $(OBJS), $@, ,$(LIBS), , SciTERes.res

SciTERes.res: SciTERes.rc ..\src\SciTE.h ..\..\scintilla\win32\PlatformRes.h
	$(RC) $(INCLUDEDIRS) -fo$@ SciTERes.rc

$(PROGSTATIC): $(OBJSSTATIC) Sc1Res.res
	$(LD) $(LDFLAGS) -Tpe -aa -Gn -x c0w32 $(OBJSSTATIC), $@, ,$(LIBS), , Sc1Res.res

Sc1Res.res: SciTERes.rc ..\src\SciTE.h ..\..\scintilla\win32\PlatformRes.h
	$(RC) $(INCLUDEDIRS) -dSTATIC_BUILD -fo$@ SciTERes.rc

!ENDIF

# Define how to build all the objects and what they depend on
# Some source files are compiled into more than one object because of different conditional compilation

SciTEBase.obj: ..\src\SciTEBase.cxx
	$(CC) $(INCLUDEDIRS) $(CXXFLAGS) -c $(NAMEFLAG)$@ ..\src\SciTEBase.cxx

SciTEWin.obj: SciTEWin.cxx
	$(CC) $(INCLUDEDIRS) $(CXXFLAGS) -c $(NAMEFLAG)$@ SciTEWin.cxx

Sc1.obj: SciTEWin.cxx
	$(CC) $(INCLUDEDIRS) $(CXXFLAGS) -DSTATIC_BUILD -c $(NAMEFLAG)$@ SciTEWin.cxx

# Dependencies
SSciTEBase.obj:\
	..\src\SciTEBase.cxx \
	..\src\SciTEBase.h \
	..\src\SciTE.h \
	..\src\Extender.h \
	..\..\scintilla\include\Platform.h \
	..\..\scintilla\include\PropSet.h \
	..\..\scintilla\include\Accessor.h \
	..\..\scintilla\include\KeyWords.h \
	..\..\scintilla\include\Scintilla.h \
	..\..\scintilla\include\SciLexer.h

SciTEWin.obj:\
	SciTEWin.cxx \
	..\src\SciTEBase.h \
	..\src\SciTE.h \
	..\src\Extender.h \
	..\..\scintilla\include\Platform.h \
	..\..\scintilla\include\PropSet.h \
	..\..\scintilla\include\Accessor.h \
	..\..\scintilla\include\KeyWords.h \
	..\..\scintilla\include\Scintilla.h \
	..\..\scintilla\include\SciLexer.h

Sc1.obj:\
	SciTEWin.cxx \
	..\src\SciTEBase.h \
	..\src\SciTE.h \
	..\src\Extender.h \
	..\..\scintilla\include\Platform.h \
	..\..\scintilla\include\PropSet.h \
	..\..\scintilla\include\Accessor.h \
	..\..\scintilla\include\KeyWords.h \
	..\..\scintilla\include\Scintilla.h
