rem ba.bat - download and build all of scintilla and scite
rd /s/q scintilla scite
cvs co scintilla scite
cd scintilla
call delbin
del/q bin\*.a
call delcvs
call zipsrc
cd win32
make
cd ..
cd ..
cd scite
del/q bin\*.properties
del/q bin\SciTE
call delbin
call delcvs
call zipsrc
cd win32
make
cd ..
call upxsc1
call zipwscite
call delbin
cd ..
cd scintilla
call delbin
cd ..
