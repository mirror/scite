# Upload.py
# Upload files to scintilla.org.

import sys
import glob
import os
from ftplib import FTP

def upload(ftp, file):
	f = open(file, "rb")
	f.seek(0,2)
	lenfile = f.tell()
	print "Uploading", file, "as", os.path.basename(file), "length is", lenfile
	f.seek(0,0)
	ftp.storbinary("STOR " + os.path.basename(file), f)
	f.close()

# connect to host, default port
connection = FTP("ftp.scintilla.org")   
connection.login("scintilla", os.getenv("SCIPASS"))
print connection.getwelcome() 
connection.cwd("www")
#print connection.retrlines('LIST')

if len(sys.argv) > 1:
	for file in glob.glob(sys.argv[1]):
		upload(connection, file)
	
#print connection.retrlines('LIST')
connection.quit()
print "Completed"