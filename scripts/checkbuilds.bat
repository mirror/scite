rem Script to build SciTE for Windows with all the different
rem compilers and exercise all the projects and makefiles.
rem Current directory must be scite\scripts before running.
rem Contains references to local install directories on Neil's
rem machine so must be modified for other installations.
rem Assumes environment set up so gcc, MSVC amd cppcheck can be called.
rem
cd ..\..
set MSDEV_BASE=C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC
set WINSDK_BASE=C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin
rem
rem ************************************************************
rem Target 1: basic unit tests with gcc
call scite\scripts\clearboth
cd scintilla\test\unit
mingw32-make -j
if ERRORLEVEL 2 goto ERROR
.\unitTest
if ERRORLEVEL 2 goto ERROR
cd ..\..\..
rem
rem ************************************************************
rem Target 2: Normal gcc build
call scite\scripts\clearboth
cd scintilla\win32
mingw32-make -j
if ERRORLEVEL 2 goto ERROR
cd ..\test
pythonw simpleTests.py
pythonw lexTests.py
pythonw performanceTests.py
cd ..\..\scite\win32
mingw32-make -j
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
rem Target 4: Visual C++ Express using scintilla\win32\SciLexer.vcxproj and scite\win32\SciTE.vcxproj
call "%MSDEV_BASE%\vcvarsall"
@echo on
call scite\scripts\clearboth
pushd scintilla\win32
msbuild /verbosity:minimal /p:Platform=Win32 /p:Configuration=Release SciLexer.vcxproj
if ERRORLEVEL 2 goto ERROR
popd
call scite\scripts\clearboth
pushd scite\win32
msbuild /verbosity:minimal /p:Platform=Win32 /p:Configuration=Release SciTE.vcxproj
if ERRORLEVEL 2 goto ERROR
popd
rem
rem ************************************************************
rem Target 5: GTK+ version using gcc on scintilla\gtk\makefile
call scite\scripts\clearboth
cd scintilla\gtk
mingw32-make -j CXXFLAGS=-Wno-long-long
if ERRORLEVEL 2 goto ERROR
cd ..\..
rem
rem ************************************************************
rem Target 6: SDK 64 bit compiler
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
rem Target 7: Clang analyze
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
rem Target 8: cppcheck
call scite\scripts\clearboth
cppcheck -j 8 --enable=all --suppressions scintilla/cppcheck.suppress --max-configs=100 -I scintilla/src -I scintilla/include -I scintilla/lexlib -I scintilla/qt/ScintillaEditBase --template=gcc --quiet scintilla
cppcheck -j 8 --enable=all --max-configs=100 -I scite/src -I scintilla/include -I scite/lua/include --template=gcc --quiet scite
rem
rem Finished
call scite\scripts\clearboth
goto CLEANUP
:ERROR
@echo checkbuilds.bat:1: Failed %ERRORLEVEL%
:CLEANUP
set SAVE_PATH=
set SAVE_INCLUDE=
