# Build all the unit tests using GNU make and either g++ or clang
# Should be run using mingw32-make on Windows, not nmake
# On Windows g++ is used, on macOS clang, and on Linux G++ is used by default
# but clang can be used by defining CLANG when invoking make
# clang works only with libc++, not libstdc++
# Tested with clang 17 and g++ 11.2

CXXSTD=c++20

ifndef windir
ifeq ($(shell uname),Darwin)
# On macOS (detected with Darwin uname) always use clang as g++ is old version
CLANG = 1
USELIBCPP = 1
endif
endif

CXXFLAGS += $(OPTIMIZATION)
CXXFLAGS += --std=$(CXXSTD)

ifdef CLANG
CXX = clang++
ifdef USELIBCPP
# macOS, use libc++ but don't have sanitizers
CXXFLAGS += --stdlib=libc++
else
ifndef windir
# Linux, have sanitizers
SANITIZE = -fsanitize=address,undefined
CXXFLAGS += $(SANITIZE)
endif
endif
else
CXX = g++
endif

ifdef windir
DEL = del /q
EXE = unitTest.exe
else
DEL = rm -f
EXE = unitTest
endif

vpath %.cxx ../src

INCLUDEDIRS = -I ../src

CPPFLAGS += $(INCLUDEDIRS)
CXXFLAGS += -Wall -Wextra

# Files in this directory containing tests
TESTSRC=$(wildcard test*.cxx)
TESTOBJ=$(TESTSRC:.cxx=.o)

# Files being tested from scintilla/src directory
TESTEDOBJ=\
Cookie.o \
StringHelpers.o \
Utf8_16.o

TESTS=$(EXE)

all: $(TESTS)

test: $(TESTS)
	./$(EXE)

clean:
	$(DEL) $(TESTS) *.o *.obj *.exe

%.o: %.cxx
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(EXE): $(TESTOBJ) $(TESTEDOBJ) unitTest.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LINKFLAGS) $^ -o $@
