/** @file unitTest.cxx
 ** Unit Tests for SciTE internal data structures
 **/

/*
*/

#include <cstdio>
#include <cstdarg>

#include <string_view>
#include <vector>
#include <optional>
#include <memory>

#if defined(__GNUC__)
// Want to avoid misleading indentation warnings in catch.hpp but the pragma
// may not be available so protect by turning off pragma warnings
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wpragmas"
#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#endif

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
