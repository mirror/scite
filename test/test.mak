# Build all the unit tests with Microsoft Visual C++ using nmake
# Tested with Visual C++ 2022

DEL = del /q
EXE = unitTest.exe

INCLUDEDIRS = /I../src

CXXFLAGS = /MP /EHsc /std:c++20 $(OPTIMIZATION) /nologo /D_HAS_AUTO_PTR_ETC=1 /wd 4805 $(INCLUDEDIRS)

# Files in this directory containing tests
TESTSRC=test*.cxx
# Files being tested from scintilla/src directory
TESTEDSRC=\
 ../src/Cookie.cxx \
 ../src/StringHelpers.cxx \
 ../src/Utf8_16.cxx

TESTS=$(EXE)

all: $(TESTS)

test: $(TESTS)
	$(EXE)

clean:
	$(DEL) $(TESTS) *.o *.obj *.exe

$(EXE): $(TESTSRC) $(TESTEDSRC) $(@B).obj
	$(CXX) $(CXXFLAGS) /Fe$@ $**
