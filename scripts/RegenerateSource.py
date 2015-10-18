#!/usr/bin/env python
# RegenerateSource.py - implemented 2013 by Neil Hodgson neilh@scintilla.org
# Released to the public domain.

# Regenerate the SciTE source files that list all the lexers and all the
# properties files.
# Should be run whenever a new lexer is added or removed.
# Requires Python 2.5 or later
# Most files are regenerated in place with templates stored in comments.
# The VS .NET project file is generated into a different file as the
# VS .NET environment will not retain comments when modifying the file.
# The format of generation comments is documented in FileGenerator.py.

# Regenerates Scintilla files by calling LexGen.RegenerateAll

import glob, os, sys

srcRoot = "../.."

sys.path.append(srcRoot + "/scintilla/scripts")

from FileGenerator import Generate, Regenerate, UpdateLineInFile, ReplaceREInFile
import ScintillaData
import LexGen
import IFaceTableGen
import commandsdoc

def UpdateVersionNumbers(sci, root):
    UpdateLineInFile(root + "scite/src/SciTE.h", "#define VERSION_SCITE",
        "#define VERSION_SCITE \"" + sci.versionDotted + "\"")
    UpdateLineInFile(root + "scite/src/SciTE.h", "#define VERSION_WORDS",
        "#define VERSION_WORDS " + sci.versionCommad)
    UpdateLineInFile(root + "scite/src/SciTE.h", "#define COPYRIGHT_DATES",
        '#define COPYRIGHT_DATES "December 1998-' + sci.myModified + '"')
    UpdateLineInFile(root + "scite/src/SciTE.h", "#define COPYRIGHT_YEARS",
        '#define COPYRIGHT_YEARS "1998-' + sci.yearModified + '"')
    UpdateLineInFile(root + "scite/doc/SciTEDownload.html", "       Release",
        "       Release " + sci.versionDotted)
    ReplaceREInFile(root + "scite/doc/SciTEDownload.html",
        r"/www.scintilla.org/([a-zA-Z]+)\d\d\d",
        r"/www.scintilla.org/\g<1>" +  sci.version)
    UpdateLineInFile(root + "scite/doc/SciTE.html",
        '          <font color="#FFCC99" size="3"> Release version',
        '          <font color="#FFCC99" size="3"> Release version ' + \
        sci.versionDotted + '<br />')
    UpdateLineInFile(root + "scite/doc/SciTE.html",
        '           Site last modified',
        '           Site last modified ' + sci.mdyModified + '</font>')
    UpdateLineInFile(root + "scite/doc/SciTE.html",
        '    <meta name="Date.Modified"',
        '    <meta name="Date.Modified" content="' + sci.dateModified + '" />')

def OctalEscape(s):
    result = []
    for char in s:
        try:
            ordChar = ord(char)
            # Python 2.x, s is a string so bytes
            if ordChar < 128:
                result.append(char)
            else:
                result.append("\%o" % ordChar)
        except TypeError:
            # Python 3.x, s is a byte string
            if char < 128:
                result.append(chr(char))
            else:
                result.append("\%o" % char)
    return ''.join(result)

def UpdateEmbedded(root, propFiles):
    propFilesSpecial = ["SciTEGlobal.properties", "abbrev.properties"]
    propFilesAll = propFilesSpecial + propFiles
    linesEmbedded = []
    for pf in propFilesAll:
        fullPath = os.path.join(root, "scite", "src", pf)
        with open(fullPath) as fi:
            fileBase = pf.split(".")[0]
            if pf not in propFilesSpecial:
                linesEmbedded.append("\nmodule " + fileBase + "\n")
            for line in fi:
                if not line.startswith("#"):
                    linesEmbedded.append(line)
            if not linesEmbedded[-1].endswith("\n"):
                linesEmbedded[-1] += "\n"
    textEmbedded = "".join(linesEmbedded)
    pathEmbedded = os.path.join(root, "scite", "src", "Embedded.properties")
    with open(pathEmbedded) as fileEmbedded:
        original = fileEmbedded.read()
    if textEmbedded != original:
        with open(pathEmbedded, "w") as fileOutEmbedded:
            fileOutEmbedded.write(textEmbedded)
            print("Changed %s" % pathEmbedded)

def RegenerateAll():
    root="../../"

    sci = ScintillaData.ScintillaData(root + "scintilla/")

    # Generate HTML to document each property
    # This is done because tags can not be safely put inside comments in HTML
    documentProperties = list(sci.propertyDocuments.keys())
    ScintillaData.SortListInsensitive(documentProperties)
    propertiesHTML = []
    for k in documentProperties:
        propertiesHTML.append("\t<tr id='property-%s'>\n\t<td>%s</td>\n\t<td>%s</td>\n\t</tr>" %
            (k, k, sci.propertyDocuments[k]))

    # Find all the SciTE properties files
    otherProps = [
        "abbrev.properties",
        "Embedded.properties",
        "SciTEGlobal.properties",
        "SciTE.properties"]
    propFilePaths = glob.glob(root + "scite/src/*.properties")
    ScintillaData.SortListInsensitive(propFilePaths)
    propFiles = [os.path.basename(f) for f in propFilePaths if os.path.basename(f) not in otherProps]
    ScintillaData.SortListInsensitive(propFiles)

    UpdateEmbedded(root, propFiles)
    Regenerate(root + "scite/win32/makefile", "#", propFiles)
    Regenerate(root + "scite/win32/scite.mak", "#", propFiles)
    Regenerate(root + "scite/src/SciTEProps.cxx", "//", sci.lexerProperties)
    Regenerate(root + "scite/doc/SciTEDoc.html", "<!--", propertiesHTML)
    credits = [OctalEscape(c.encode("utf-8")) for c in sci.credits]
    Regenerate(root + "scite/src/Credits.cxx", "//", credits)

    UpdateVersionNumbers(sci, root)

LexGen.RegenerateAll("../../scintilla/")
RegenerateAll()
IFaceTableGen.RegenerateAll()
commandsdoc.RegenerateAll()
