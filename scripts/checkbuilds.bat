rem Script to build SciTE for Windows with all the different 
rem compilers and exercise all the projects and makefiles.
rem Current directory must be scite\scripts before running.
rem Contains references to local install directories on Neil's 
rem machine so must be modified for other installations.
rem Assumes environment set up so gcc and MSVC can be called.
rem 
cd ..\..
set
set BORLAND_BASE=G:\Borland\bcc55
rem 
rem Target 1: Normal gcc build
call scite\scripts\clearboth
cd scintilla\win32
make
if ERRORLEVEL 2 goto ERROR
cd ..\..\scite\win32
make
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem 
rem Target 2: Microsoft VC++ build
call scite\scripts\clearboth
cd scintilla\win32
nmake -f scintilla.mak
if ERRORLEVEL 2 goto ERROR
cd ..\..\scite\win32
nmake -f scite.mak
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem 
rem Target 3: Borland C++ build
call scite\scripts\clearboth
cd scintilla\win32
set SAVE_PATH=%path%
set SAVE_INCLUDE=%INCLUDE%
path %BORLAND_BASE%\Bin;%path%
set libpath=%BORLAND_BASE%\lib
set INCLUDE=%BORLAND_BASE%\include
make -f scintilla.mak
if ERRORLEVEL 2 goto ERROR
cd ..\..\scite\win32
make -f scite.mak
if ERRORLEVEL 2 goto ERROR
cd ..\..
path %SAVE_PATH%
set INCLUDE=%SAVE_INCLUDE%
rem 
rem Target 4: Visual C++ using scite\vcbuild\scite.dsp
call scite\scripts\clearboth
cd scintilla\vcbuild
msdev SciLexer.dsp /MAKE "SciLexer - Win32 Release" /REBUILD
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem 
rem Target 5: Visual C++ using scintilla\vcbuild\SciTE.dsp
call scite\scripts\clearboth
cd scite\vcbuild
msdev SciTE.dsp /MAKE "SciTE - Win32 Release" /REBUILD
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem 
rem Target 6: Visual C++ using scite\boundscheck\SciTE.dsp
call scite\scripts\clearboth
cd scite\boundscheck
msdev SciTE.dsp /MAKE "SciTE - Win32 Release" /REBUILD
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem 
rem Target 7: GTK+ version using Visual C++ on scintilla\gtk\scintilla.mak
call scite\scripts\clearboth
cd scintilla\gtk
nmake -f scintilla.mak
if ERRORLEVEL 2 goto ERROR
cd ..\..
goto CLEANUP
:ERROR
@echo checkbuilds.bat:1: Failed %ERRORLEVEL%
:CLEANUP
set SAVE_PATH=
set SAVE_INCLUDE=
set BORLAND_BASE=
set
