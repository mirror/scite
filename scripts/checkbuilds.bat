@rem Script to build SciTE for Windows with all the different
@rem compilers and exercise all the projects and makefiles.
@rem Current directory must be scite\scripts before running.
@rem Contains references to local install directories on Neil's
@rem machine so must be modified for other installations.
@rem Assumes environment set up so gcc, MSVC amd cppcheck can be called.
@rem
@cd ..\..
@rem
rem ************************************************************
rem Target 1: basic unit tests with gcc
@call scite\scripts\clearboth
@pushd scintilla\test\unit
mingw32-make -j
@if ERRORLEVEL 2 goto ERROR
.\unitTest
@if ERRORLEVEL 2 goto ERROR
@popd
@rem
rem ************************************************************
rem Target 2: Build and check Lexilla
@call scite\scripts\clearboth
@pushd scintilla\lexilla\src
mingw32-make clean
mingw32-make -j
@if ERRORLEVEL 2 goto ERROR
@popd
@pushd scintilla\lexilla\test
mingw32-make clean
mingw32-make test
@popd
@rem
rem ************************************************************
rem Target 3: Normal gcc build
@call scite\scripts\clearboth
@pushd scintilla\win32
mingw32-make -j
@if ERRORLEVEL 2 goto ERROR
@popd
@pushd scintilla\test
pythonw simpleTests.py
pythonw lexTests.py
pythonw performanceTests.py
pythonw win32Tests.py
pythonw simpleTests.py -large
@popd
@pushd scite\win32
mingw32-make -j
@if ERRORLEVEL 2 goto ERROR
@popd
@rem
rem ************************************************************
rem Target 4: Microsoft VC++ build
@call scite\scripts\clearboth
@pushd scintilla\win32
cl
nmake -f scintilla.mak QUIET=1
@if ERRORLEVEL 2 goto ERROR
@popd
@pushd scintilla\test
pythonw simpleTests.py
pythonw lexTests.py
pythonw performanceTests.py
pythonw win32Tests.py
@popd
@pushd scite\win32
nmake -f scite.mak QUIET=1
@if ERRORLEVEL 2 goto ERROR
@popd
@rem
rem ************************************************************
rem Target 5: Visual C++ using scintilla\win32\SciLexer.vcxproj and scite\win32\SciTE.vcxproj
@echo on
@call scite\scripts\clearboth
@pushd scintilla\win32
msbuild /verbosity:minimal /p:Platform=Win32 /p:Configuration=Release SciLexer.vcxproj
@if ERRORLEVEL 2 goto ERROR
@popd
@call scite\scripts\clearboth
@pushd scite\win32
msbuild /verbosity:minimal /p:Platform=Win32 /p:Configuration=Release SciTE.vcxproj
@if ERRORLEVEL 2 goto ERROR
@popd
@rem
rem ************************************************************
rem Target 6: GTK+ version using gcc on scintilla\gtk\makefile
@call scite\scripts\clearboth
@pushd scintilla\gtk
rem -Wno-parentheses is temporary for GTK+ header gtkfilechooserbutton.h
mingw32-make -j CXXFLAGS=-Wno-parentheses
@if ERRORLEVEL 2 goto ERROR
@popd ..\..
@rem
rem ************************************************************
rem Target 7: Visual C++ 64 bit
@call scite\scripts\clearboth
@pushd scintilla\win32
msbuild /verbosity:minimal /p:Platform=x64 /p:Configuration=Release SciLexer.vcxproj
@if ERRORLEVEL 2 goto ERROR
@popd
@call scite\scripts\clearboth
@pushd scite\win32
msbuild /verbosity:minimal /p:Platform=x64 /p:Configuration=Release SciTE.vcxproj
@if ERRORLEVEL 2 goto ERROR
@popd
@rem
rem ************************************************************
rem Target 8: Clang analyze
REM ~ call scite\scripts\clearboth
REM ~ set PATH=c:\mingw32-dw2\bin;%PATH%
REM ~ cd scintilla\win32
REM ~ mingw32-make CLANG=1 analyze
REM ~ if ERRORLEVEL 2 goto ERROR
REM ~ cd ..\..\scite\win32
REM ~ mingw32-make CLANG=1 analyze
REM ~ if ERRORLEVEL 2 goto ERROR
REM ~ cd ..\..
@rem
rem ************************************************************
rem Target 9: Clang build
@call scite\scripts\clearboth
@pushd scintilla\win32
mingw32-make CLANG=1 -j
@if ERRORLEVEL 2 goto ERROR
@popd
@pushd scintilla\test
pythonw simpleTests.py
pythonw lexTests.py
pythonw performanceTests.py
pythonw win32Tests.py
@popd
@pushd scite\win32
mingw32-make CLANG=1 -j
@if ERRORLEVEL 2 goto ERROR
@popd
@rem
rem ************************************************************
rem Target 10: qt with msvc
@call scite\scripts\clearboth
@set QBIN=D:\Qt\Qt5.12.0\5.12.0\msvc2017_64\bin
@pushd scintilla\qt\ScintillaEditBase
%QBIN%\qmake
nmake
nmake distclean
@if ERRORLEVEL 2 goto ERROR
@popd
@pushd scintilla\qt\ScintillaEdit
python WidgetGen.py
%QBIN%\qmake
nmake
nmake distclean
@if ERRORLEVEL 2 goto ERROR
@popd
@rem
rem ************************************************************
rem Target 11: cppcheck
@call scite\scripts\clearboth
cppcheck -j 8 --enable=all --suppressions-list=scintilla/cppcheck.suppress --max-configs=100 -I scintilla/src -I scintilla/include -I scintilla/lexlib -I scintilla/qt/ScintillaEditBase "-DSTDMETHODIMP_(type) type STDMETHODCALLTYPE" --template=gcc --quiet scintilla
cppcheck -j 8 --enable=all --suppressions-list=scite/cppcheck.suppress --max-configs=100 -I scite/src -I scintilla/include -I scite/lua/src -Ulua_assert -DPATH_MAX=260 --template=gcc --quiet scite
@rem
rem ************************************************************
rem Target 12: header order check
@pushd scintilla\scripts
python HeaderCheck.py
@popd
@rem
rem Finished
@call scite\scripts\clearboth
goto CLEANUP
:ERROR
@echo checkbuilds.bat:1: Failed %ERRORLEVEL%
:CLEANUP
set SAVE_PATH=
set SAVE_INCLUDE=
