cd ..
del/q scite.zip
zip scite.zip scintilla\*.* scintilla\*\*.* scite\*.* scite\*\*.* -x *.o -x *.obj -x *.lib -x *.dll -x *.exe -x *.pdb -x *.res -x *.exp
cd scite
