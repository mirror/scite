# Fixer.py
# Take a C++ file run astyle and remove any badness done by astyle
import sys
import string
import os
import glob
import stat

tempname = "FixStyle.tmp"
recurse = 1

def fixCode(code):
	#code = string.replace(code, "if(", "if (")
	return code
	
def fixLine(line, inComment):
	line = string.rstrip(line)
	if inComment:
		if string.find(line, "*/") != -1:
			inComment = 0
	else:
		if string.find(line, "#include") == 0:
			line = string.replace(line, " / ", "/")
			line = string.replace(line, "< ", "<")
			line = string.replace(line, " >", ">")
		elif string.find(line, "/*") != -1:
			inComment = 1
		elif string.find(line, "//") != -1:
			pos = string.find(line, "//")
			code = line[:pos]
			comment = line[pos:]
			line = fixCode(code) + comment
		else:
			line = fixCode(line)
	return line, inComment
	
def fixFile(filename):
	os.system("astyle -tapO %s" % filename)
	out = open(tempname, "wt")
	cfile = open(filename, "rt")
	lastLine = 1
	#~ print "processing", filename
	inComment = 0
	for line in cfile.readlines():
		line, inComment = fixLine(line, inComment)
		if line or lastLine:
			out.write(line)
			out.write("\n")
		lastLine = line
	out.close()
	cfile.close()
	os.unlink(filename)
	os.rename(tempname, filename)

def fixDir(dir, extensions):
	print "dir", dir
	for filename in os.listdir(dir):
		for ext in extensions:
			if not filename.count(".orig") and filename.count(ext):
				fixFile(dir + os.sep + filename)
	if recurse:
		for filename in os.listdir(dir):
			dirname =  dir + os.sep + filename
			#print ":", dirname
			if stat.S_ISDIR(os.stat(dirname)[stat.ST_MODE]):
				fixDir(dirname, extensions)

#os.chdir("\\os\\Updates\\SciTE-1.36+pl01")
if len(sys.argv) > 1:
	fixFile(sys.argv[1])
else:
	fixDir(os.getcwd(), [".cxx", ".h"])
