#!/usr/bin/env python3
# CheckMentioned.py
# Find all the symbols in scintilla/include/Scintilla.h and check if they
# are mentioned in scintilla/doc/ScintillaDoc.html.

import string

uninteresting = {
	"SCINTILLA_H", "SCI_START", "SCI_LEXER_START", "SCI_OPTIONAL_START",
	# These archaic names are #defined to the Sci_ prefixed modern equivalents.
	# They are not documented so they are not used in new code.
	"CharacterRange", "TextRange", "TextToFind", "RangeToFormat",
}
srcRoot = "../.."
incFileName = srcRoot + "/scintilla/include/Scintilla.h"
docFileName = srcRoot + "/scintilla/doc/ScintillaDoc.html"
try:	# Old Python
	identCharacters = "_" + string.letters + string.digits
except AttributeError:	# Python 3.x
	identCharacters = "_" + string.ascii_letters + string.digits

# Convert all punctuation characters except '_' into spaces.
def depunctuate(s):
	d = ""
	for ch in s:
		if ch in identCharacters:
			d = d + ch
		else:
			d = d + " "
	return d

symbols = {}
incFile = open(incFileName, "rt")
for line in incFile.readlines():
	if line.startswith("#define"):
		identifier = line.split()[1]
		symbols[identifier] = 0
incFile.close()

docFile = open(docFileName, "rt")
for line in docFile.readlines():
	for word in depunctuate(line).split():
		if word in symbols.keys():
			symbols[word] = 1
docFile.close()

identifiersSorted = list(symbols.keys())
identifiersSorted.sort()
for identifier in identifiersSorted:
	if not symbols[identifier] and identifier not in uninteresting:
		print(identifier)
