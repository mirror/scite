# Microsoft Developer Studio Project File - Name="SciTE" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=SciTE - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "SciTE.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "SciTE.mak" CFG="SciTE - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "SciTE - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "SciTE - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SciTE - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /Ox /I "..\src" /I "..\..\scintilla\include" /I "..\..\scintilla\src" /I "..\..\scintilla\lexlib" /I "..\lua\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "UNICODE" /D "_UNICODE" /D "STATIC_BUILD" /D "SCI_LEXER" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0xc09 /i "..\src" /i "..\..\Scintilla\Win32" /d "NDEBUG" /d "STATIC_BUILD"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib msimg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib msimg32.lib advapi32.lib shell32.lib ole32.lib imm32.lib comctl32.lib /nologo /machine:I386 /opt:nowin98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "SciTE - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\src" /I "..\..\scintilla\include" /I "..\..\scintilla\src" /I "..\..\scintilla\lexlib" /I "..\lua\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "UNICODE" /D "_UNICODE" /D "STATIC_BUILD" /D "SCI_LEXER" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0xc09 /i "..\src" /i "..\..\Scintilla\Win32" /d "_DEBUG" /d "STATIC_BUILD"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib msimg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib msimg32.lib advapi32.lib shell32.lib ole32.lib imm32.lib comctl32.lib /nologo /debug /machine:I386

!ENDIF

# Begin Target

# Name "SciTE - Win32 Release"
# Name "SciTE - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\scintilla\lexlib\Accessor.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\AutoComplete.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\CallTip.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\Catalogue.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\CellBuffer.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexlib\CharacterSet.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\CharClassify.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\ContractionState.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\Decoration.cxx
# End Source File
# Begin Source File

SOURCE=..\win32\DirectorExtension.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\Document.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\Editor.cxx
# End Source File
# Begin Source File

SOURCE=..\src\Exporters.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\ExternalLexer.cxx
# End Source File
# Begin Source File

SOURCE=..\src\FilePath.cxx
# End Source File
# Begin Source File

SOURCE=..\win32\GUIWin.cxx
# End Source File
# Begin Source File

SOURCE=..\src\IFaceTable.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\Indicator.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\KeyMap.cxx
# End Source File
# Begin Source File

SOURCE=..\lua\src\lapi.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lib\lauxlib.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lib\lbaselib.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lcode.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lib\ldblib.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\ldebug.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\ldo.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\ldump.c
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexAbaqus.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexAda.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexAPDL.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexAsm.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexAsn1.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexASY.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexAU3.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexAVE.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexBaan.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexBash.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexBasic.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexBullant.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexCaml.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexCLW.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexCmake.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexCOBOL.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexConf.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexCPP.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexCrontab.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexCsound.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexCSS.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexD.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexEiffel.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexlib\LexerBase.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexErlang.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexlib\LexerModule.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexlib\LexerNoExceptions.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexlib\LexerSimple.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexEScript.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexFlagship.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexForth.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexFortran.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexGAP.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexGui4Cli.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexHaskell.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexHTML.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexInno.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexKix.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexLisp.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexLout.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexLua.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexMagik.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexMarkdown.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexMatlab.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexMetapost.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexMMIXAL.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexMPT.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexMSSQL.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexMySQL.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexNimrod.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexNsis.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexOpal.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexOthers.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexPascal.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexPB.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexPerl.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexPLM.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexPOV.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexPowerPro.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexPowerShell.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexProgress.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexPS.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexPython.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexR.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexRebol.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexRuby.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexScriptol.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexSmalltalk.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexSML.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexSorcus.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexSpecman.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexSpice.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexSQL.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexTACL.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexTADS3.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexTAL.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexTCL.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexTeX.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexTxt2tags.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexVB.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexVerilog.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexVHDL.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexers\LexYAML.cxx
# End Source File
# Begin Source File

SOURCE=..\lua\src\lfunc.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lgc.c
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\LineMarker.cxx
# End Source File
# Begin Source File

SOURCE=..\lua\src\lib\linit.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lib\liolib.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\llex.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lib\lmathlib.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lmem.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lib\loadlib.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lobject.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lopcodes.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lib\loslib.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lparser.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lstate.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lstring.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lib\lstrlib.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\ltable.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lib\ltablib.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\ltm.c
# End Source File
# Begin Source File

SOURCE=..\src\LuaExtension.cxx
# End Source File
# Begin Source File

SOURCE=..\lua\src\lundump.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lvm.c
# End Source File
# Begin Source File

SOURCE=..\lua\src\lzio.c
# End Source File
# Begin Source File

SOURCE=..\src\MultiplexExtension.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\PerLine.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\Win32\PlatWin.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\Src\PositionCache.cxx
# End Source File
# Begin Source File

SOURCE=..\Src\PropSetFile.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexlib\PropSetSimple.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\RESearch.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\RunStyles.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\ScintillaBase.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\win32\ScintillaWin.cxx
# End Source File
# Begin Source File

SOURCE=..\Src\SciTEBase.cxx
# End Source File
# Begin Source File

SOURCE=..\src\SciTEBuffers.cxx
# End Source File
# Begin Source File

SOURCE=..\src\SciTEIO.cxx
# End Source File
# Begin Source File

SOURCE=..\src\SciTEProps.cxx
# End Source File
# Begin Source File

SOURCE=..\Win32\SciTERes.rc
# End Source File
# Begin Source File

SOURCE=..\Win32\SciTEWin.cxx
# End Source File
# Begin Source File

SOURCE=..\win32\SciTEWinBar.cxx
# End Source File
# Begin Source File

SOURCE=..\win32\SciTEWinDlg.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\Selection.cxx
# End Source File
# Begin Source File

SOURCE=..\win32\SingleThreadExtension.cxx
# End Source File
# Begin Source File

SOURCE=..\src\StringHelpers.cxx
# End Source File
# Begin Source File

SOURCE=..\src\StringList.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\Style.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexlib\StyleContext.cxx
# End Source File
# Begin Source File

SOURCE=..\src\StyleWriter.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\Src\UniConversion.cxx
# End Source File
# Begin Source File

SOURCE=..\win32\UniqueInstance.cxx
# End Source File
# Begin Source File

SOURCE=..\src\Utf8_16.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\ViewStyle.cxx
# End Source File
# Begin Source File

SOURCE=..\win32\WinMutex.cxx
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\src\XPM.cxx
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\scintilla\Include\Accessor.h
# End Source File
# Begin Source File

SOURCE=..\src\GUI.h
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\Include\Platform.h
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\Win32\PlatformRes.h
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\Include\SciLexer.h
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\Include\Scintilla.h
# End Source File
# Begin Source File

SOURCE=..\Src\SciTE.h
# End Source File
# Begin Source File

SOURCE=..\Src\SciTEBase.h
# End Source File
# Begin Source File

SOURCE=..\src\StyleWriter.h
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\Include\WinDefs.h
# End Source File
# Begin Source File

SOURCE=..\..\scintilla\lexlib\WordList.cxx
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\Win32\SciBall.ico
# End Source File
# End Group
# End Target
# End Project
