#!/usr/bin/env python
# DepGen.py - produce a make dependencies file for SciTE
# Copyright 2019 by Neil Hodgson <neilh@scintilla.org>
# The License.txt file describes the conditions under which this software may be distributed.
# Requires Python 2.7 or later

import sys

srcRoot = "../.."

sys.path.append(srcRoot + "/scintilla")

import scripts.Dependencies as Dependencies

topComment = "# Created by DepGen.py. To recreate, run 'python DepGen.py'.\n"

def Generate():
	sciteSources = ["../src/*.cxx", "../lua/src/*.c"]
	sciteIncludes = ["../../scintilla/include", "../src", "../lua/src"]

	deps = Dependencies.FindDependencies(["../gtk/*.cxx"] + sciteSources,  ["../gtk"] + sciteIncludes, ".o", "../gtk/")
	Dependencies.UpdateDependencies("../gtk/deps.mak", deps, topComment)

if __name__ == "__main__":
	Generate()