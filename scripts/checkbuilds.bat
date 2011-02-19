rem Script to build SciTE for Windows with all the different
rem compilers and exercise all the projects and makefiles.
rem Current directory must be scite\scripts before running.
rem Contains references to local install directories on Neil's
rem machine so must be modified for other installations.
rem Assumes environment set up so gcc and MSVC can be called.
rem
cd ..\..
set
set BORLAND_BASE=C:\Borland\bcc55
set MSDEV_BASE=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98\Bin
set MSDEV71_BASE=C:\Program Files\Microsoft Visual Studio .NET 2003\Common7\Tools
rem
rem ************************************************************
rem Target 1: basic unit tests with gcc
call scite\scripts\clearboth
cd scintilla\test\unit
mingw32-make
if ERRORLEVEL 2 goto ERROR
.\unitTest
if ERRORLEVEL 2 goto ERROR
cd ..\..\..
rem
rem ************************************************************
rem Target 2: Normal gcc build
call scite\scripts\clearboth
cd scintilla\win32
mingw32-make
if ERRORLEVEL 2 goto ERROR
cd ..\test
pythonw simpleTests.py
pythonw lexTests.py
pythonw performanceTests.py
cd ..\..\scite\win32
mingw32-make
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem
rem ************************************************************
rem Target 3: Microsoft VC++ build
call scite\scripts\clearboth
cd scintilla\win32
cl
nmake -f scintilla.mak QUIET=1
if ERRORLEVEL 2 goto ERROR
cd ..\..\scite\win32
nmake -f scite.mak QUIET=1
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem
rem Set path for VC 6
path %MSDEV_BASE%;%path%
rem
rem ************************************************************
rem Target 4: Visual C++ .NET using scite\boundscheck\SciTE.sln
call "%MSDEV71_BASE%\vsvars32.bat"
call scite\scripts\clearboth
cd scite\boundscheck
devenv scite.sln /rebuild release
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem
rem ************************************************************
rem Target 5: GTK+ version using gcc on scintilla\gtk\makefile
call scite\scripts\clearboth
cd scintilla\gtk
mingw32-make
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem Visual C++ builds
call "%MSDEV_BASE%\..\..\..\VC98\bin\vcvars32.bat"
echo on
rem
rem ************************************************************
rem Target 6: Visual C++ 98 using scintilla\win32\scintilla_vc6.mak
call scite\scripts\clearboth
cd scintilla\win32
nmake -f scintilla_vc6.mak QUIET=1
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem
rem ************************************************************
rem Removed: Target 7
rem
rem ************************************************************
rem Removed: Target 8
rem
rem ************************************************************
rem Target 9: Visual C++ using scite\boundscheck\SciTE.dsp
call scite\scripts\clearboth
cd scite\boundscheck
msdev SciTE.dsp /MAKE "SciTE - Win32 Release" /REBUILD
if ERRORLEVEL 2 goto ERROR
cd ..\..
call scite\scripts\clearboth
goto CLEANUP
:ERROR
@echo checkbuilds.bat:1: Failed %ERRORLEVEL%
:CLEANUP
set SAVE_PATH=
set SAVE_INCLUDE=
set BORLAND_BASE=
set MSDEV_BASE=
set
