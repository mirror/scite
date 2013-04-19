rem Script to build SciTE for Windows with all the different
rem compilers and exercise all the projects and makefiles.
rem Current directory must be scite\scripts before running.
rem Contains references to local install directories on Neil's
rem machine so must be modified for other installations.
rem Assumes environment set up so gcc and MSVC can be called.
rem
cd ..\..
set
set MSDEV_BASE=C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98\Bin
set MSDEV71_BASE=C:\Program Files\Microsoft Visual Studio .NET 2003\Common7\Tools
set WINSDK_BASE=C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin
rem
rem ************************************************************
rem Target 1: basic unit tests with gcc
call scite\scripts\clearboth
cd scintilla\test\unit
make -j
if ERRORLEVEL 2 goto ERROR
.\unitTest
if ERRORLEVEL 2 goto ERROR
cd ..\..\..
rem
rem ************************************************************
rem Target 2: Normal gcc build
call scite\scripts\clearboth
cd scintilla\win32
make -j
if ERRORLEVEL 2 goto ERROR
cd ..\test
pythonw simpleTests.py
pythonw lexTests.py
pythonw performanceTests.py
cd ..\..\scite\win32
make -j
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
rem ************************************************************
rem Target 4: Visual C++ Express using scite\boundscheck\SciTE.sln
REM ~ call scite\scripts\clearboth
REM ~ cd scite\boundscheck
vcexpress scite.sln /rebuild release
REM ~ if ERRORLEVEL 2 goto ERROR
REM ~ cd ..\..
rem
rem ************************************************************
rem Target 5: GTK+ version using gcc on scintilla\gtk\makefile
call scite\scripts\clearboth
cd scintilla\gtk
make -j CXXFLAGS=-Wno-long-long
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem Visual C++ builds
REM ~ call "%MSDEV_BASE%\..\..\..\VC98\bin\vcvars32.bat"
REM ~ echo on
rem
rem ************************************************************
rem Target 6: Visual C++ 98 using scintilla\win32\scintilla_vc6.mak
REM ~ call scite\scripts\clearboth
REM ~ cd scintilla\win32
REM ~ nmake -f scintilla_vc6.mak QUIET=1
REM ~ if ERRORLEVEL 2 goto ERROR
REM ~ cd ..\..
rem
rem ************************************************************
rem Removed: Target 7
rem
rem ************************************************************
rem Removed: Target 8
rem
rem ************************************************************
rem Target 9: Visual C++ using scite\boundscheck\SciTE.dsp
REM ~ call scite\scripts\clearboth
REM ~ cd scite\boundscheck
REM ~ msdev SciTE.dsp /MAKE "SciTE - Win32 Release" /REBUILD
REM ~ if ERRORLEVEL 2 goto ERROR
REM ~ cd ..\..
rem
rem ************************************************************
rem Target 10: SDK 64 bit compiler
call scite\scripts\clearboth
call "%WINSDK_BASE%\SetEnv.Cmd" /Release /x64 /vista
cd scintilla\win32
nmake -f scintilla.mak
if ERRORLEVEL 2 goto ERROR
cd ..\..\scite\win32
nmake -f scite.mak
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem
rem ************************************************************
rem Target 11: Clang analyze
REM ~ call scite\scripts\clearboth
REM ~ set PATH=c:\mingw32-dw2\bin;%PATH%
REM ~ cd scintilla\win32
REM ~ mingw32-make analyze
REM ~ if ERRORLEVEL 2 goto ERROR
REM ~ cd ..\..\scite\win32
REM ~ mingw32-make analyze
REM ~ if ERRORLEVEL 2 goto ERROR
REM ~ cd ..\..
rem
rem ************************************************************
rem Target 12: cppcheck
REM ~ call scite\scripts\clearboth
REM ~ cppcheck -j 8 --enable=all --max-configs=100 -I scintilla/src -I scintilla/include -I scintilla/lexlib -I scintilla/qt/ScintillaEditBase --template=gcc --quiet scintilla
REM ~ cppcheck -j 8 --enable=all --max-configs=100 -I scite/src -I scintilla/include -I scite/lua/include --template=gcc --quiet scite
rem
rem Finished
call scite\scripts\clearboth
goto CLEANUP
:ERROR
@echo checkbuilds.bat:1: Failed %ERRORLEVEL%
:CLEANUP
set SAVE_PATH=
set SAVE_INCLUDE=
set MSDEV_BASE=
set
