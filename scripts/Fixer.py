# Fixer.py
# Take a C++ file run astyle and remove any badness done by astyle
import sys
import string
import os
import glob

tempname = "FixStyle.tmp"
argname = sys.argv[1]
for filename in glob.glob(argname):
	os.system("astyle -tapO %s" % filename)
	out = open(tempname, "wt")
	cfile = open(filename, "rt")
	lastLine = 1
	print "processing", filename
	for line in cfile.readlines():
		line = string.rstrip(line)
		if string.find(line, "#include") == 0:
			line = string.replace(line, " / ", "/")
			line = string.replace(line, "< ", "<")
			line = string.replace(line, " >", ">")
		if line or lastLine:
			out.write(line)
			out.write("\n")
		lastLine = line
	out.close()
	cfile.close()
	os.unlink(filename)
	os.rename(tempname, filename)

