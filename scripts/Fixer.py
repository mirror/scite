# Fixer.py
# Take a C++ file run astyle and remove any badness done by astyle
import sys
import string
import os
import glob

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

tempname = "FixStyle.tmp"
argname = sys.argv[1]
for filename in glob.glob(argname):
	os.system("astyle -tapO %s" % filename)
	out = open(tempname, "wt")
	cfile = open(filename, "rt")
	lastLine = 1
	print "processing", filename
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

