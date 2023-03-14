#!/usr/bin/env python3
# TagRelease.py - implemented 2023 by Neil Hodgson neilh@scintilla.org
# Released to the public domain.

# Tag Lexilla, Scintilla, and SciTE with version numbers.
# Requires Python 3.6 or later

import os, pathlib, subprocess, sys

sciteBase = pathlib.Path(__file__).resolve().parent.parent
baseDirectory = sciteBase.parent
sciDirectory = baseDirectory / "scintilla"
lexDirectory = baseDirectory / "lexilla"

sys.path.append(str(sciDirectory / "scripts"))
sys.path.append(str(lexDirectory / "scripts"))

import ScintillaData
import LexillaData

def DashedVersion(version):
    return version[0:-2] + '-' + version[-2] + '-' + version[-1]

def tagOne(project, version):
    os.chdir(project)
    releaseTag = "rel-" + DashedVersion(version)
    if project == "lexilla":
        command = f"git tag {releaseTag}"
    else:
        command = f"hg tag {releaseTag}"
    print(command)
    subprocess.call(command, shell=True)
    os.chdir("..")

def TagAll():
    os.chdir(baseDirectory)
    sci = ScintillaData.ScintillaData(sciDirectory)
    lex = LexillaData.LexillaData(lexDirectory)
    sciteVersion = (sciteBase / "version.txt").read_text().strip()

    tagOne("lexilla", lex.version)
    tagOne("scintilla", sci.version)
    tagOne("scite", sciteVersion)

TagAll()
