# CheckMentioned.py
# Find all the properties used in SciTE source files and check if they 
# are mentioned in scite/doc/SciTEDoc.html.

import os
import string
import stat

srcRoot = "../../scite"
docFileName = srcRoot + "/doc/SciTEDoc.html"
propsFileName = srcRoot + "/src/SciTEGlobal.properties"
identCharacters = "_*." + string.letters + string.digits

# Convert all punctuation characters except '_', '*', and '.' into spaces.
def depunctuate(s):
	d = ""
	for ch in s:
		if ch in identCharacters:
			d = d + ch
		else:
			d = d + " "
	return d

srcPaths = []
propertiesPaths = []
for filename in os.listdir(srcRoot):
	dirname =  srcRoot + os.sep + filename
	if stat.S_ISDIR(os.stat(dirname)[stat.ST_MODE]):
		for src in os.listdir(dirname):
			if src.count(".cxx"):
				srcPaths.append(dirname + os.sep + src)

propertyNames = {}
#print srcPaths
for srcPath in srcPaths:
	srcFile = open(srcPath)
	for line in srcFile.readlines():
		line = line.strip()
		if line.count("props") and line.count("Get"):
			propsPos = line.find("props")
			getPos = line.find("Get")
			if propsPos < getPos:
				parts = line[getPos:].split('\"')
				#print parts
				if len(parts) > 1:
					propertyName = parts[1]
					if propertyName:
						propertyNames[propertyName] = 0
						#print propertyName
	srcFile.close()

docFile = open(docFileName, "rt")
for line in docFile.readlines():
	for word in depunctuate(line).split():
		if word in propertyNames.keys():
			propertyNames[word] = 1
docFile.close()

print "# Not mentioned in", docFileName
identifiersSorted = propertyNames.keys()
identifiersSorted.sort()
for identifier in identifiersSorted:
	if not propertyNames[identifier]:
		print identifier

# Rest flags for searching properties file
for identifier in identifiersSorted:
	propertyNames[identifier] = 0

def keyOfLine(line):
	if '=' in line:
		line = line.strip()
		if line[0] == "#":
			line = line[1:]
		line = line[:line.find("=")]
		line = line.strip()
		return line
	else:
		return None

propsFile = open(propsFileName, "rt")
for line in propsFile.readlines():
	if line:
		key = keyOfLine(line)
		if key:
			if key in propertyNames.keys():
				propertyNames[key] = 1
propsFile.close()

print "# Not mentioned in", propsFileName
for identifier in identifiersSorted:
	if not propertyNames[identifier]:
		if "." != identifier[-1:]:
			print identifier

# This is a test to see whether properties are defined in more than one file.
# It doesn't understand the if directive so yields too many false positives to run often.
print "# Duplicate mentions"
"""
fileOfProp = {}
notRealProperties = ["abbrev.properties", "SciTE.properties", "Embedded.properties"]
for filename in os.listdir(srcRoot + os.sep + "src"):
	if filename.count(".properties") and filename not in notRealProperties:
		propsFile = open(srcRoot + os.sep + "src" + os.sep + filename, "rt")
		for line in propsFile.readlines():
			if line:
				key = keyOfLine(line)
				if key:
					if fileOfProp.has_key(key):
						print "Clash for", key, fileOfProp[key], filename
					else:
						fileOfProp[key] =filename
		propsFile.close()
"""