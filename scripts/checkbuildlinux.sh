# Script to build Scintilla and SciTE for Linux with most supported build files.
# Both gcc and clang are used for both GTK+ 2 and 3 and the clang static analyzer
# run for GTK+2.
# All three Qt libraries are built with gcc.
# Current directory should be scite\scripts before running.

# Pre-requisite packages on Ubuntu:
# sudo apt-get install --yes g++
# sudo apt-get install --yes libgtk-3-dev
# sudo apt-get install --yes libgtk2.0-dev
# sudo apt-get install --yes clang
# sudo apt-get install --yes qtcreator
# sudo apt-get install --yes python-dev
# sudo apt-get install --yes libshiboken-dev
# sudo apt-get install --yes shiboken
# sudo apt-get install --yes libpyside-dev
# sudo apt-get install --yes python-pyside

# Pre-requisite packages on Fedora:
# sudo yum install -y gcc-c++
# sudo yum install -y gtk3-devel
# sudo yum install -y gtk2-devel
# sudo yum install -y clang
# sudo yum install -y qt-creator
# sudo yum install -y python-devel
# sudo yum install -y shiboken-devel
# sudo yum install -y python-pyside-devel

# On Fedora 17, qmake is called qmake-qt4 so sepbuild.py should probe for correct name.
# There are also problems with clang failing in the g++ 4.7 headers.

# Run up to 2 commands in parallel
set JOBS=-j 2

cd ../..

# ************************************************************
# Target 1: gcc build for GTK+ 2
cd scintilla/gtk
make clean
make $JOBS CHECK_DEPRECATED=1
cd ../..

cd scite/gtk
make clean
# Don't bother with CHECK_DEPRECATED on SciTE as the GTK+ 3.x code path fixes the deprecations
make $JOBS
cd ../..

# ************************************************************
# Target 2: gcc build for GTK+ 3
cd scintilla/gtk
make clean
make $JOBS GTK3=1 CHECK_DEPRECATED=1
cd ../..

cd scite/gtk
make clean
make $JOBS GTK3=1 CHECK_DEPRECATED=1
cd ../..

# ************************************************************
# Target 3: Qt builds
# Requires Qt development libraries and qmake to be installed
cd scintilla/qt
cd ScintillaEditBase
qmake
make clean
make $JOBS
cd ..

cd ScintillaEdit
python WidgetGen.py
qmake
make clean
make $JOBS
cd ..

cd ScintillaEditPy
python sepbuild.py
cd ..
cd ../..

# ************************************************************
# Target 4: clang build for GTK+ 2
cd scintilla/gtk
make clean
make $JOBS CLANG=1 CHECK_DEPRECATED=1
cd ../..

cd scite/gtk
make clean
# Don't bother with CHECK_DEPRECATED on SciTE as the GTK+ 3.x code path fixes the deprecations
make $JOBS CLANG=1
cd ../..

# ************************************************************
# Target 5: clang build for GTK+ 3
cd scintilla/gtk
make clean
make $JOBS CLANG=1 GTK3=1 CHECK_DEPRECATED=1
cd ../..

cd scite/gtk
make clean
make $JOBS CLANG=1 GTK3=1 CHECK_DEPRECATED=1
cd ../..

# ************************************************************
# Target 6: clang analyze for GTK+ 2
# Disabled as there are warnings from librarian, linker and Lua code
#~ cd scintilla/gtk
#~ make clean
#~ make $JOBS CLANG=1 CLANG_ANALYZE=1 CHECK_DEPRECATED=1
#~ cd ../..

#~ # Need scintilla.a or else make of SciTE dies early
#~ cd scintilla/gtk
#~ make clean
#~ make $JOBS CLANG=1 GTK3=1 CHECK_DEPRECATED=1
#~ cd ../..
#~ # There are several warnings from clang analyze for Lua code
#~ cd scite/gtk
#~ make clean
#~ make $JOBS CLANG=1 CLANG_ANALYZE=1
#~ cd ../..
