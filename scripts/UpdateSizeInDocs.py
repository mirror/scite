#!/usr/bin/env python3
# UpdateSizeInDocs.py
# Update the documented sizes of downloads to match the current release.
# Uses local build directory (../../../arc/upload<version>) on Neil's machine
# so must be modified for other installations.
# Implemented 2019 by Neil Hodgson neilh@scintilla.org
# Requires Python 3.6 or later

import glob, os

sciteRoot = ".."
scintillaRoot = os.path.join("..", "..", "scintilla")
releaseRoot = os.path.join("..", "..", "..", "arc")

uploadDocs = [
    os.path.join(sciteRoot, "doc", "SciTEDownload.html"),
    os.path.join(scintillaRoot, "doc", "ScintillaDownload.html")
]

downloadHome = "https://www.scintilla.org/"

def ExtractFileName(s):
    pre, quote, rest = s.partition('"')
    url, quote, rest = rest.partition('"')
    domain, slash, name = url.rpartition('/')
    return name

def FileSizeInMB(filePath):
    size = os.stat(filePath).st_size
    sizeInM = size / 1024 / 1024
    roundToNearest = round(sizeInM * 10) / 10
    return str(roundToNearest) + "M"

def FileSizesInDirectory(baseDirectory):
    fileSizes = {}
    for filePath in glob.glob(os.path.join(baseDirectory, "*")):
        fileBase = os.path.basename(filePath)
        fileSizes[fileBase] = FileSizeInMB(filePath)
    return fileSizes

def UpdateFileSizes():
    with open(os.path.join(scintillaRoot, "version.txt")) as f:
        version = f.read().strip()
    releaseDir = os.path.join(releaseRoot, "upload" + version)
    fileSizes = FileSizesInDirectory(releaseDir)
    if not fileSizes:
        print("No files in", releaseDir)

    for docFileName in uploadDocs:
        outLines = []
        changes = False
        with open(docFileName, "rt") as docFile:
            for line in docFile:
                if downloadHome in line and '(' in line and ')' in line:
                    pre, bracket, rest = line.partition('(')
                    size, rbracket, end = rest.partition(')')
                    fileName = ExtractFileName(line)
                    if fileName in fileSizes:
                        new = pre + bracket + fileSizes[fileName] + rbracket + end
                        if size != fileSizes[fileName]:
                            changes = True
                            print(f"{size} -> {fileSizes[fileName]} {fileName}")
                        line = new
                outLines.append(line)
        if changes:
            print("Updating", docFileName)
            with open(docFileName, "wt") as docFile:
                docFile.write("".join(outLines))

if __name__=="__main__":
    UpdateFileSizes()
