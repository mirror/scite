cd ..
del/q wscite.zip
mkdir wscite
copy scite\license.txt wscite 
copy scite\bin\SciTE.exe wscite 
copy scite\bin\SciLexer.dll wscite 
copy scite\src\SciTEGlobal.properties wscite 
copy scite\doc\*.html wscite 
copy scite\doc\*.png wscite 
copy scite\doc\*.jpg wscite 
zip wscite.zip wscite\*.*
del/q wscite\*.*
rmdir wscite
cd scite
