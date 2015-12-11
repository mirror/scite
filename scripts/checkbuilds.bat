rem Script to build SciTE for Windows with all the different
rem compilers and exercise all the projects and makefiles.
rem Current directory must be scite\scripts before running.
rem Contains references to local install directories on Neil's
rem machine so must be modified for other installations.
rem Assumes environment set up so gcc, MSVC amd cppcheck can be called.
rem
cd ..\..
rem
rem ************************************************************
rem Target 1: basic unit tests with gcc
call scite\scripts\clearboth
pushd scintilla\test\unit
mingw32-make -j
if ERRORLEVEL 2 goto ERROR
.\unitTest
if ERRORLEVEL 2 goto ERROR
popd
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
pushd scintilla\win32
cl
nmake -f scintilla.mak QUIET=1
if ERRORLEVEL 2 goto ERROR
popd
pushd scite\win32
nmake -f scite.mak QUIET=1
if ERRORLEVEL 2 goto ERROR
popd
rem
rem ************************************************************
rem Target 4: Visual C++ using scintilla\win32\SciLexer.vcxproj and scite\win32\SciTE.vcxproj
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
pushd scintilla\gtk
mingw32-make -j CXXFLAGS=-Wno-long-long
if ERRORLEVEL 2 goto ERROR
popd ..\..
rem
rem ************************************************************
rem Target 6: Visual C++ 64 bit
call scite\scripts\clearboth
pushd scintilla\win32
msbuild /verbosity:minimal /p:Platform=x64 /p:Configuration=Release SciLexer.vcxproj
if ERRORLEVEL 2 goto ERROR
popd
call scite\scripts\clearboth
pushd scite\win32
msbuild /verbosity:minimal /p:Platform=x64 /p:Configuration=Release SciTE.vcxproj
if ERRORLEVEL 2 goto ERROR
popd
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
cppcheck -j 8 --enable=all --suppressions-list=scintilla/cppcheck.suppress --max-configs=100 -I scintilla/src -I scintilla/include -I scintilla/lexlib -I scintilla/qt/ScintillaEditBase --template=gcc --quiet scintilla
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
