// SciTE - Scintilla based Text Editor
// SciTEBase.cxx - platform independent base class of editor
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

#include "Platform.h"

#if PLAT_GTK

#include <unistd.h>
#include "WinDefs.h"

#endif

#if PLAT_WIN

#ifdef _MSC_VER
#include <direct.h>
#endif
#ifdef __BORLANDC__
#include <dir.h>
#endif

#endif

#include "SciTE.h"
#include "PropSet.h"
#include "Accessor.h"
#include "WindowAccessor.h"
#include "KeyWords.h"
#include "Scintilla.h"
#include "SciLexer.h"
#include "SciTEBase.h"

#define SciTE_MARKER_BOOKMARK 1

const char propFileName[] = "SciTE.properties";

const char *contributors[] = {
	"Atsuo Ishimoto",
	"Mark Hammond",
	"Francois Le Coguiec",
	"Dale Nagata",
	"Ralf Reinhardt",
	"Philippe Lhoste",
	"Andrew McKinlay",
	"Stephan R. A. Deibel",
	"Hans Eckardt",
	"Vassili Bourdo",
	"Maksim Lin",
	"Robin Dunn",
	"John Ehresman",
	"Steffen Goeldner",
	"Deepak S.",
	"DevelopMentor http://www.develop.com",
	"Yann Gaillard",
	"Aubin Paul",
	"Jason Diamond",
	"Ahmad Baitalmal",
	"Paul Winwood",
	"Maxim Baranov",
};

// AddStyledText only called from About so static size buffer is OK
void AddStyledText(WindowID hwnd, const char *s, int attr) {
	char buf[1000];
	int len = strlen(s);
	for (int i = 0; i < len; i++) {
		buf[i*2] = s[i];
		buf[i*2 + 1] = static_cast<char>(attr);
	}
	Platform::SendScintilla(hwnd, SCI_ADDSTYLEDTEXT, len*2,
	            reinterpret_cast<LPARAM>(static_cast<char *>(buf)));
}

void SetAboutStyle(WindowID wsci, int style, Colour fore) {
	Platform::SendScintilla(wsci, SCI_STYLESETFORE, style, fore.AsLong());
}

static void HackColour(int &n) {
	n += (rand() % 100) - 50;
	if (n < 0x40)
		n = 0xC0;
	if (n > 0xFF)
		n = 0xC0;
}

void SetAboutMessage(WindowID wsci, const char *appTitle) {
	if (wsci) {
		Platform::SendScintilla(wsci, SCI_STYLERESETDEFAULT, 0, 0);
		int fontSize = 15;
#if PLAT_GTK
		// On GTK+, new century schoolbook looks better in large sizes than default font
		Platform::SendScintilla(wsci, SCI_STYLESETFONT, STYLE_DEFAULT, 
			reinterpret_cast<unsigned int>("new century schoolbook"));
		fontSize = 16;
#endif
		Platform::SendScintilla(wsci, SCI_STYLESETSIZE, STYLE_DEFAULT, fontSize);
		Platform::SendScintilla(wsci, SCI_STYLESETBACK, STYLE_DEFAULT, Colour(0, 0, 0).AsLong());
		Platform::SendScintilla(wsci, SCI_STYLECLEARALL, 0, 0);
		
		SetAboutStyle(wsci, 0, Colour(0xff, 0xff, 0xff));
		Platform::SendScintilla(wsci, SCI_STYLESETSIZE, 0, 24);
		Platform::SendScintilla(wsci, SCI_STYLESETBACK, 0, Colour(0, 0, 0x80).AsLong());
		AddStyledText(wsci, appTitle, 0);
		AddStyledText(wsci, "\n", 0);
		SetAboutStyle(wsci, 1, Colour(0xff, 0xff, 0xff));
		AddStyledText(wsci, "Version 1.25\n", 1);
		SetAboutStyle(wsci, 2, Colour(0xff, 0xff, 0xff));
		Platform::SendScintilla(wsci, SCI_STYLESETITALIC, 2, 1);
		AddStyledText(wsci, "by Neil Hodgson.\n", 2);
		SetAboutStyle(wsci, 3, Colour(0xff, 0xff, 0xff));
		AddStyledText(wsci, "December 1998-May 2000.\n", 3);
		SetAboutStyle(wsci, 4, Colour(0, 0xff, 0xff));
		AddStyledText(wsci, "http://www.scintilla.org\n", 4);
		AddStyledText(wsci, "Contributors:\n", 1);
		srand(static_cast<unsigned>(time(0)));
		int r=rand()%256;
		int g=rand()%256;
		int b=rand()%256;
		for (unsigned int co=0;co<(sizeof(contributors)/sizeof(contributors[0]));co++) {
			HackColour(r);
			HackColour(g);
			HackColour(b);
			SetAboutStyle(wsci, 5+co, Colour(r, g, b));
			AddStyledText(wsci, "    ", 5+co);
			AddStyledText(wsci, contributors[co], 5+co);
			AddStyledText(wsci, "\n", 5+co);
		}
		Platform::SendScintilla(wsci, EM_SETREADONLY, 1, 0);
	}
}

Job::Job() {
	Clear();
}

void Job::Clear() {
	command = "";
	directory = "";
	jobType = jobCLI;
}

SciTEBase::SciTEBase() : apis(true) {
	codePage = 0;
	language = "java";
	lexLanguage = SCLEX_CPP;
	functionDefinition = 0;
	indentSize = 8;
	indentOpening = true;
	indentClosing = true;

	tbVisible = false;
	sbVisible = false;
	visHeightTools = 0;
	visHeightStatus = 0;
	visHeightEditor = 1;
	heightBar = 7;
	dialogsOnScreen = 0;

	heightOutput = 0;

	allowMenuActions = true;
	isDirty = false;
	isBuilding = false;
	isBuilt = false;
	executing = false;
	commandCurrent = 0;
	cancelFlag = 0L;

	ptStartDrag.x = 0;
	ptStartDrag.y = 0;
	capturedMouse = false;
	firstPropertiesRead = true;
	splitVertical = false;
	bufferedDraw = true;
	bracesCheck = true;
	bracesSloppy = false;
	bracesStyle = 0;
	braceCount = 0;
	
	margin = false;
	marginWidth = marginWidthDefault;
	foldMargin = true;
	foldMarginWidth = foldMarginWidthDefault;
	lineNumbers = false;
	lineNumbersWidth = lineNumbersWidthDefault;
	usePalette = false;

	clearBeforeExecute = false;
	findWhat[0] = '\0';
	replaceWhat[0] = '\0';
	replacing = false;
	havefound = false;
	matchCase = false;
	wholeWord = false;
	reverseFind = false;

	windowName[0] = '\0';
	fullPath[0] = '\0';
	fileName[0] = '\0';
	fileExt[0] = '\0';
	dirName[0] = '\0';
	dirNameAtExecute[0] = '\0';
	fileModTime = 0;

	propsBase.superPS = &propsEmbed;
	propsUser.superPS = &propsBase;
	props.superPS = &propsUser;
}

SciTEBase::~SciTEBase() {
}

void SciTEBase::ReadGlobalPropFile() {
	char propfile[MAX_PATH + 20];
	if (GetDefaultPropertiesFileName(propfile, sizeof(propfile))) {
		propsBase.Read(propfile);
	}
	if (GetUserPropertiesFileName(propfile, sizeof(propfile))) {
		propsUser.Read(propfile);
	}
}

void SciTEBase::ReadLocalPropFile() {
	char propfile[MAX_PATH + 20];
	if (dirName[0])
		strcpy(propfile, dirName);
	else
		getcwd(propfile, sizeof(propfile));
	strcat(propfile, pathSepString);
	strcat(propfile, propFileName);
	props.Read(propfile);
	//Platform::DebugPrintf("Reading local properties from %s\n", propfile);

	// TODO: Grab these from Platform and update when environment says to
	props.Set("Chrome", "#C0C0C0");
	props.Set("ChromeHighlight", "#FFFFFF");
}

LRESULT SciTEBase::SendEditor(UINT msg, WPARAM wParam, LPARAM lParam) {
	return Platform::SendScintilla(wEditor.GetID(), msg, wParam, lParam);
}

LRESULT SciTEBase::SendEditorString(UINT msg, WPARAM wParam, const char *s) {
	return SendEditor(msg, wParam, reinterpret_cast<LPARAM>(s));
}

LRESULT SciTEBase::SendOutput(UINT msg, WPARAM wParam, LPARAM lParam) {
	return Platform::SendScintilla(wOutput.GetID(), msg, wParam, lParam);
}

void SciTEBase::SendChildren(UINT msg, WPARAM wParam, LPARAM lParam) {
	SendEditor(msg, wParam, lParam);
	SendOutput(msg, wParam, lParam);
}

LRESULT SciTEBase::SendFocused(UINT msg, WPARAM wParam, LPARAM lParam) {
	if (wEditor.HasFocus())
		return SendEditor(msg, wParam, lParam);
	else
		return SendOutput(msg, wParam, lParam);
}

void SciTEBase::DeleteFileStackMenu() {
	for (int stackPos = 1; stackPos < fileStackMax; stackPos++) {
		DestroyMenuItem(0, fileStackCmdID + stackPos);
	}
	DestroyMenuItem(0, IDM_MRU_SEP);
}

void SciTEBase::SetFileStackMenu() {
	int menuStart = 6;
	if (recentFileStack[1].fileName[0]) {
		SetMenuItem(0, menuStart, IDM_MRU_SEP, "");
		for (int stackPos = 1; stackPos < fileStackMax; stackPos++) {
			int itemID = fileStackCmdID + stackPos;
			if (recentFileStack[stackPos].fileName[0]) {
				char entry[MAX_PATH + 20];
				entry[0] = '\0';
#if PLAT_WIN
				sprintf(entry, "&%d ", stackPos);
#endif
				strcat(entry, recentFileStack[stackPos].fileName);
				SetMenuItem(0, menuStart + stackPos, itemID, entry);
			}
		}
	}
}

void SciTEBase::DropFileStackTop() {
	DeleteFileStackMenu();
	for (int stackPos = 0; stackPos < fileStackMax - 1; stackPos++)
		recentFileStack[stackPos] = recentFileStack[stackPos + 1];
	strcpy(recentFileStack[fileStackMax - 1].fileName, "");
	recentFileStack[fileStackMax - 1].lineNumber = -1;
	SetFileStackMenu();
}

void SciTEBase::AddFileToStack(const char *file, int line, int scrollPos) {
	DeleteFileStackMenu();
	// Only stack non-empty names
	if ((file[0]) && (file[strlen(file) - 1] != pathSepChar)) {
		int stackPos;
		int eqPos = fileStackMax - 1;
		for (stackPos = 0; stackPos < fileStackMax; stackPos++)
			if (strcmp(recentFileStack[stackPos].fileName, file) == 0)
				eqPos = stackPos;
		for (stackPos = eqPos; stackPos > 0; stackPos--)
			recentFileStack[stackPos] = recentFileStack[stackPos - 1];
		strcpy(recentFileStack[0].fileName, file);
		recentFileStack[0].lineNumber = line;
		recentFileStack[0].scrollPosition = scrollPos;
	}
	SetFileStackMenu();
}

void SciTEBase::RememberLineNumberStack(const char *file, int line, int scrollPos) {
	for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
		if (strcmp(recentFileStack[stackPos].fileName, file) == 0) {
			recentFileStack[stackPos].lineNumber = line;
			recentFileStack[stackPos].scrollPosition = scrollPos;
		}
	}
}

void SciTEBase::DisplayAround(int scrollPosition, int lineNumber) {
	if (lineNumber != -1) {
		SendEditor(SCI_ENSUREVISIBLE, lineNumber);
		int lineTop = SendEditor(SCI_VISIBLEFROMDOCLINE, scrollPosition);
		SendEditor(EM_LINESCROLL, 0, lineTop);
		SendEditor(SCI_GOTOLINE, lineNumber);
	}
}

// Next and Prev file comments - can't decide which way round these should be?
// Decided that "Prev" file should mean the file you had opened last
// This means "Next" file means the file you opened longest ago.
void SciTEBase::StackMenuNext() {
	DeleteFileStackMenu();
	for (int stackPos = fileStackMax-1; stackPos>=0;stackPos--) {
		if (recentFileStack[stackPos].fileName[0]!='\0') {
			SetFileStackMenu();
			StackMenu(stackPos);
			return;
		}
	}
	SetFileStackMenu();
}

void SciTEBase::StackMenuPrev() {
	StackMenu(1);
	// And rotate the one we just closed to the end.
	DeleteFileStackMenu();
	RecentFile temp = recentFileStack[1];
	int stackPos = 2;
	for (; stackPos < fileStackMax-1; stackPos++) {
		if (recentFileStack[stackPos].fileName[0]=='\0') {
			stackPos--;
			break;
		}
		recentFileStack[stackPos-1] = recentFileStack[stackPos];
	}
	recentFileStack[stackPos] = temp;
	SetFileStackMenu();
}

void SciTEBase::StackMenu(int pos) {
	//Platform::DebugPrintf("Stack menu %d\n", pos);
	if (pos == -1) {
		int lastPos = -1;
		for (int stackPos = 1; stackPos < fileStackMax; stackPos++) {
			if (recentFileStack[stackPos].fileName[0])
				lastPos = stackPos;
		}
		if (lastPos > 0) {
			//Platform::DebugPrintf("Opening lpos %d %s\n",recentFileStack[lastPos].lineNumber,recentFileStack[lastPos].fileName);
			int line = recentFileStack[lastPos].lineNumber;
			int scrollPosition = recentFileStack[lastPos].scrollPosition;
			SString fileNameCopy = recentFileStack[lastPos].fileName;
			Open(fileNameCopy.c_str());
			DisplayAround(scrollPosition, line);
		}
	} else {
		if ((pos == 0) && (recentFileStack[pos].fileName[0] == '\0')) {	// Empty
			New();
			ReadProperties();
		} else if (recentFileStack[pos].fileName[0] != '\0') {
			int line = recentFileStack[pos].lineNumber;
			int scrollPosition = recentFileStack[pos].scrollPosition;
			//Platform::DebugPrintf("Opening pos %d %s\n",recentFileStack[pos].lineNumber,recentFileStack[pos].fileName);
			SString fileNameCopy = recentFileStack[pos].fileName;
			Open(fileNameCopy.c_str());
			DisplayAround(scrollPosition, line);
		}
	}
}

void SciTEBase::RemoveToolsMenu() {
	for (int pos = 0; pos < toolMax; pos++) {
		DestroyMenuItem(2, IDM_TOOLS + pos);
	}
}

void SciTEBase::SetToolsMenu() {
	//command.name.0.*.py=Edit in PythonWin
	//command.0.*.py="c:\program files\python\pythonwin\pythonwin" /edit c:\coloreditor.py
	RemoveToolsMenu();
	const int menuPos = 3;
	for (int item = 0; item < toolMax; item++) {
		int itemID = IDM_TOOLS + item;
		SString prefix = "command.name.";
		prefix += SString(item).c_str();
		prefix += ".";
		SString commandName = props.GetNewExpand(prefix.c_str(), fileName);
		if (commandName.length()) {
			SString sMenuItem = commandName;
			SString sMnemonic = "Ctrl+";
			sMnemonic += SString(item).c_str();
			SetMenuItem(2, menuPos + item, itemID, sMenuItem.c_str(), sMnemonic.c_str());
		}
	}
}

JobSubsystem SciTEBase::SubsystemType(const char *cmd, int item) {
	JobSubsystem jobType = jobCLI;
	SString subsysprefix = cmd;
	if (item >= 0) {
		subsysprefix += SString(item).c_str();
		subsysprefix += ".";
	}
	SString subsystem = props.GetNewExpand(subsysprefix.c_str(), fileName);
	if (subsystem[0] == '1')
		jobType = jobGUI;
	else if (subsystem[0] == '2')
		jobType = jobShell;
	return jobType;
}

void SciTEBase::ToolsMenu(int item) {
	SelectionIntoProperties();

	SString prefix = "command.";
	prefix += SString(item).c_str();
	prefix += ".";
	SString command = props.GetNewExpand(prefix.c_str(), fileName);
	if (command.length()) {
		if (SaveIfUnsure() != IDCANCEL) {
			SString isfilter = "command.is.filter.";
			isfilter += SString(item).c_str();
			isfilter += ".";
			SString filter = props.GetNewExpand(isfilter.c_str(), fileName);
			if (filter[0] == '1')
				fileModTime -= 1;

			JobSubsystem jobType = SubsystemType("command.subsystem.", item);

			AddCommand(command, "", jobType);
			if (commandCurrent > 0)
				Execute();
		}
	}
}

static int IntFromHexDigit(const char ch) {
	if (isdigit(ch))
		return ch - '0';
	else if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	else if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	else
		return 0;
}

static Colour ColourFromString(const char *val) {
	int r = IntFromHexDigit(val[1]) * 16 + IntFromHexDigit(val[2]);
	int g = IntFromHexDigit(val[3]) * 16 + IntFromHexDigit(val[4]);
	int b = IntFromHexDigit(val[5]) * 16 + IntFromHexDigit(val[6]);
	return Colour(r, g, b);
}

void SciTEBase::SetOneStyle(Window &win, int style, const char *s) {
	char *val = StringDup(s);
	//Platform::DebugPrintf("Style %d is [%s]\n", style, val);
	char *opt = val;
	while (opt) {
		char *cpComma = strchr(opt, ',');
		if (cpComma)
			*cpComma = '\0';
		char *colon = strchr(opt, ':');
		if (colon)
			*colon++ = '\0';
		if (0 == strcmp(opt, "italics"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETITALIC, style, 1);
		if (0 == strcmp(opt, "notitalics"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETITALIC, style, 0);
		if (0 == strcmp(opt, "bold"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETBOLD, style, 1);
		if (0 == strcmp(opt, "notbold"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETBOLD, style, 0);
		if (0 == strcmp(opt, "font"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETFONT, style, reinterpret_cast<LPARAM>(colon));
		if (0 == strcmp(opt, "fore"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETFORE, style, ColourFromString(colon).AsLong());
		if (0 == strcmp(opt, "back"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETBACK, style, ColourFromString(colon).AsLong());
		if (0 == strcmp(opt, "size"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETSIZE, style, atoi(colon));
		if (0 == strcmp(opt, "eolfilled"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETEOLFILLED, style, 1);
		if (0 == strcmp(opt, "noteolfilled"))
			Platform::SendScintilla(win.GetID(), SCI_STYLESETEOLFILLED, style, 0);
		if (cpComma)
			opt = cpComma + 1;
		else
			opt = 0;
	}
	if (val)
		delete []val;
}

void SciTEBase::SetStyleFor(Window &win, const char *lang) {
	for (int style = 0; style <= STYLE_MAX; style++) {
		if (style != STYLE_DEFAULT) {
			char key[200];
			sprintf(key, "style.%s.%0d", lang, style);
			SString sval = props.GetNewExpand(key, "");
			SetOneStyle(win, style, sval.c_str());
		}
	}
}

// Properties that are interactively modifiable are only read from the properties file once.
void SciTEBase::ReadPropertiesInitial() {
	splitVertical = props.GetInt("split.vertical");
	SendEditor(SCI_SETVIEWWS, props.GetInt("view.whitespace"));
	SendEditor(SCI_SETVIEWEOL, props.GetInt("view.eol"));

	sbVisible = props.GetInt("statusbar.visible");
	tbVisible = props.GetInt("toolbar.visible");
	
	lineNumbersWidth = 0;
	SString linenums = props.Get("line.numbers");
	if (linenums.length())
		lineNumbersWidth = linenums.value();
	lineNumbers = lineNumbersWidth;
	if (lineNumbersWidth == 0)
		lineNumbersWidth = lineNumbersWidthDefault;

	marginWidth = 0;
	SString margwidth = props.Get("margin.width");
	if (margwidth.length())
		marginWidth = margwidth.value();
	margin = marginWidth;
	if (marginWidth == 0)
		marginWidth = marginWidthDefault;

	foldMarginWidth = props.GetInt("fold.margin.width", foldMarginWidthDefault);
	foldMargin = foldMarginWidth;
	if (foldMarginWidth == 0)
		foldMarginWidth = foldMarginWidthDefault;
}

StyleAndWords SciTEBase::GetStyleAndWords(const char *base) {
	StyleAndWords sw;
	SString fileNameForExtension = ExtensionFileName();
	SString sAndW = props.GetNewExpand(base, fileNameForExtension.c_str());
	sw.styleNumber = sAndW.value();
	const char *space = strchr(sAndW.c_str(), ' ');
	if (space)
		sw.words = space + 1;
	return sw;
}

SString SciTEBase::ExtensionFileName() {
	if (fileName[0]) 
		return fileName;
	else 
		return props.Get("default.file.ext");
}

void SciTEBase::ReadProperties() {
//DWORD dwStart = timeGetTime();
	SString fileNameForExtension = ExtensionFileName();

	language = props.GetNewExpand("lexer.", fileNameForExtension.c_str());
	
	if (language == "python") {
		lexLanguage = SCLEX_PYTHON;
	} else if (language == "cpp") {
		lexLanguage = SCLEX_CPP;
	} else if (language == "hypertext") {
		lexLanguage = SCLEX_HTML;
	} else if (language == "xml") {
		lexLanguage = SCLEX_XML;
	} else if (language == "perl") {
		lexLanguage = SCLEX_PERL;
	} else if (language == "sql") {
		lexLanguage = SCLEX_SQL;
	} else if (language == "vb") {
		lexLanguage = SCLEX_VB;
	} else if (language == "props") {
		lexLanguage = SCLEX_PROPERTIES;
	} else if (language == "errorlist") {
		lexLanguage = SCLEX_ERRORLIST;
	} else if (language == "makefile") {
		lexLanguage = SCLEX_MAKEFILE;
	} else if (language == "batch") {
		lexLanguage = SCLEX_BATCH;
	} else {
		lexLanguage = SCLEX_NULL;
	}

	if ((lexLanguage == SCLEX_HTML) || (lexLanguage == SCLEX_XML))
		SendEditor(SCI_SETSTYLEBITS, 7);
	else
		SendEditor(SCI_SETSTYLEBITS, 5);
		
	SendEditor(SCI_SETLEXER, lexLanguage);
	SendOutput(SCI_SETLEXER, SCLEX_ERRORLIST);

	apis.Clear();

	SString kw0 = props.GetNewExpand("keywords.", fileNameForExtension.c_str());
	SendEditorString(SCI_SETKEYWORDS, 0, kw0.c_str());
	SString kw1 = props.GetNewExpand("keywords2.", fileNameForExtension.c_str());
	SendEditorString(SCI_SETKEYWORDS, 1, kw1.c_str());
	SString kw2 = props.GetNewExpand("keywords3.", fileNameForExtension.c_str());
	SendEditorString(SCI_SETKEYWORDS, 2, kw2.c_str());
	SString kw3 = props.GetNewExpand("keywords4.", fileNameForExtension.c_str());
	SendEditorString(SCI_SETKEYWORDS, 3, kw3.c_str());

	SString fold = props.Get("fold");
	SendEditorString(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("fold"), 
		fold.c_str());
	SString ttwl = props.Get("tab.timmy.whinge.level");
	SendEditorString(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("tab.timmy.whinge.level"), 
		ttwl.c_str());

	SString apifilename = props.GetNewExpand("api.", fileNameForExtension.c_str());
	if (apifilename.length()) {
		FILE *fp = fopen(apifilename.c_str(), "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			int len = ftell(fp);
			char *buffer = apis.Allocate(len);
			if (buffer) {
				fseek(fp, 0, SEEK_SET);
				fread(buffer, 1, len, fp);
				apis.SetFromAllocated();
			}
			fclose(fp);
			//Platform::DebugPrintf("Finished api file %d\n", len);
		}
	}

	SString eol_mode = props.Get("eol.mode");
	if (eol_mode=="LF") {
		SendEditor(SCI_SETEOLMODE, SC_EOL_LF);
	} else if (eol_mode=="CR") {
		SendEditor(SCI_SETEOLMODE, SC_EOL_CR);
	} else if (eol_mode=="CRLF") {
		SendEditor(SCI_SETEOLMODE, SC_EOL_CRLF);
	}

	codePage = props.GetInt("code.page");
	SendEditor(SCI_SETCODEPAGE, codePage);

	SString colour;
	colour = props.Get("caret.fore");
	if (colour.length()) {
		SendEditor(SCI_SETCARETFORE, ColourFromString(colour.c_str()).AsLong());
	}

	colour = props.Get("calltip.back");
	if (colour.length()) {
		SendEditor(SCI_CALLTIPSETBACK, ColourFromString(colour.c_str()).AsLong());
	}

	SString caretPeriod = props.Get("caret.period");
	if (caretPeriod.length()) {
		SendEditor(SCI_SETCARETPERIOD, caretPeriod.value());
		SendOutput(SCI_SETCARETPERIOD, caretPeriod.value());
	}

	SendEditor(SCI_SETEDGECOLUMN, props.GetInt("edge.column", 0));
	SendEditor(SCI_SETEDGEMODE, props.GetInt("edge.mode", EDGE_NONE));
	colour = props.Get("edge.colour");
	if (colour.length()) {
		SendEditor(SCI_SETEDGECOLOUR, ColourFromString(colour.c_str()).AsLong());
	}

	SString selfore = props.Get("selection.fore");
	if (selfore.length()) {
		SendChildren(SCI_SETSELFORE, 1, ColourFromString(selfore.c_str()).AsLong());
	} else {
		SendChildren(SCI_SETSELFORE, 0, 0);
	}
	colour = props.Get("selection.back");
	if (colour.length()) {
		SendChildren(SCI_SETSELBACK, 1, ColourFromString(colour.c_str()).AsLong());
	} else {
		if (selfore.length())
			SendChildren(SCI_SETSELBACK, 0, 0);
		else	// Have to show selection somehow
			SendChildren(SCI_SETSELBACK, 1, Colour(0xC0, 0xC0, 0xC0).AsLong());
	}
	
	char bracesStyleKey[200];
	sprintf(bracesStyleKey, "braces.%s.style", language.c_str());
	bracesStyle = props.GetInt(bracesStyleKey, 0);

	char key[200];
	SString sval;

	// Set styles
	// For each window set the global default style, then the language default style, then the other global styles, then the other language styles

	SendEditor(SCI_STYLERESETDEFAULT, 0, 0);
	SendOutput(SCI_STYLERESETDEFAULT, 0, 0);

	sprintf(key, "style.%s.%0d", "*", STYLE_DEFAULT);
	sval = props.GetNewExpand(key, "");
	SetOneStyle(wEditor, STYLE_DEFAULT, sval.c_str());
	SetOneStyle(wOutput, STYLE_DEFAULT, sval.c_str());

	sprintf(key, "style.%s.%0d", language.c_str(), STYLE_DEFAULT);
	sval = props.GetNewExpand(key, "");
	SetOneStyle(wEditor, STYLE_DEFAULT, sval.c_str());

	SendEditor(SCI_STYLECLEARALL, 0, 0);

	SetStyleFor(wEditor, "*");
	SetStyleFor(wEditor, language.c_str());

	SendOutput(SCI_STYLECLEARALL, 0, 0);

	sprintf(key, "style.%s.%0d", "errorlist", STYLE_DEFAULT);
	sval = props.GetNewExpand(key, "");
	SetOneStyle(wOutput, STYLE_DEFAULT, sval.c_str());

	SendOutput(SCI_STYLECLEARALL, 0, 0);

	SetStyleFor(wOutput, "*");
	SetStyleFor(wOutput, "errorlist");

	if (firstPropertiesRead) {
		ReadPropertiesInitial();
	}

	usePalette = props.GetInt("use.palette");
	SendEditor(SCI_SETUSEPALETTE, usePalette);

	clearBeforeExecute = props.GetInt("clear.before.execute");
	SendEditor(SCI_SETUSEPALETTE, usePalette);

	int blankMarginLeft = props.GetInt("blank.margin.left", 1);
	int blankMarginRight = props.GetInt("blank.margin.right", 1);
	long marginCombined = MAKELONG(blankMarginLeft, blankMarginRight);
	SendEditor(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, marginCombined);
	SendOutput(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, marginCombined);

	SendEditor(SCI_SETMARGINWIDTHN, 1, margin ? marginWidth : 0);
	SendEditor(SCI_SETMARGINWIDTHN, 0, lineNumbers ? lineNumbersWidth : 0);

	bufferedDraw = props.GetInt("buffered.draw", 1);
	SendEditor(SCI_SETBUFFEREDDRAW, bufferedDraw);

	bracesCheck = props.GetInt("braces.check");
	bracesSloppy = props.GetInt("braces.sloppy");

	SString wordCharacters = props.GetNewExpand("word.characters.", fileNameForExtension.c_str());
	if (wordCharacters.length()) {
		SendEditorString(SCI_SETWORDCHARS, 0, wordCharacters.c_str());
	} else {
		SendEditor(SCI_SETWORDCHARS, 0, 0);
	}

	SendEditor(SCI_MARKERDELETEALL, static_cast<WPARAM>(-1));

	int tabSize = props.GetInt("tabsize");
	if (tabSize) {
		SendEditor(SCI_SETTABWIDTH, tabSize);
	}
	indentSize = props.GetInt("indent.size");
	SendEditor(SCI_SETINDENT, indentSize);
	indentOpening = props.GetInt("indent.opening");
	indentClosing = props.GetInt("indent.closing");
	statementIndent = GetStyleAndWords("statement.indent.");
	statementEnd = GetStyleAndWords("statement.end.");
	blockStart = GetStyleAndWords("block.start.");
	blockEnd = GetStyleAndWords("block.end.");
	
	SendEditor(SCI_SETUSETABS, props.GetInt("use.tabs", 1));
	SendEditor(SCI_SETHSCROLLBAR, props.GetInt("horizontal.scrollbar", 1));

	SetToolsMenu();

	SendEditor(SCI_SETFOLDFLAGS, props.GetInt("fold.flags"));
	
	// To put the folder markers in the line number region
	//SendEditor(SCI_SETMARGINMASKN, 0, SC_MASK_FOLDERS);
	
	SendEditor(SCI_SETMODEVENTMASK, SC_MOD_CHANGEFOLD);

	// Create a margin column for the folding symbols
	SendEditor(SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
	SendEditor(SCI_SETMARGINWIDTHN, 2, foldMarginWidth);
	SendEditor(SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
	SendEditor(SCI_SETMARGINSENSITIVEN, 2, 1);
	
	if (props.GetInt("fold.use.plus")) {
		SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_MINUS);
		SendEditor(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, Colour(0xff, 0xff, 0xff).AsLong());
		SendEditor(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, Colour(0, 0, 0).AsLong());
		SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_PLUS);
		SendEditor(SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, Colour(0xff, 0xff, 0xff).AsLong());
		SendEditor(SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, Colour(0, 0, 0).AsLong());
	} else {
		SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_ARROWDOWN);
		SendEditor(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, Colour(0, 0, 0).AsLong());
		SendEditor(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, Colour(0, 0, 0).AsLong());
		SendEditor(SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_ARROW);
		SendEditor(SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, Colour(0, 0, 0).AsLong());
		SendEditor(SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, Colour(0, 0, 0).AsLong());
	}
	
	firstPropertiesRead = false;
//DWORD dwEnd = timeGetTime();
//Platform::DebugPrintf("Properrties read took %d\n", dwEnd - dwStart);
}

int SciTEBase::LengthDocument() {
	return SendEditor(SCI_GETLENGTH);
}

int SciTEBase::GetLine(char *text, int sizeText, int line) {
	if (line == -1) {
		return SendEditor(SCI_GETCURLINE, sizeText, reinterpret_cast<LPARAM>(text));
	} else {
		WORD buflen = static_cast<WORD>(sizeText);
		memcpy(text, &buflen, sizeof(buflen));
		return SendEditor(EM_GETLINE, line, reinterpret_cast<LPARAM>(text));
	}
}

void SciTEBase::GetRange(Window &win, int start, int end, char *text) {
	TEXTRANGE tr;
	tr.chrg.cpMin = start;
	tr.chrg.cpMax = end;
	tr.lpstrText = text;
	Platform::SendScintilla(win.GetID(), EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));
}

#ifdef OLD_CODE
void SciTEBase::Colourise(int start, int end, bool editor) {
// Colourisation is now performed by the SciLexer DLL
	//DWORD dwStart = timeGetTime();
	Window &win = editor ? wEditor : wOutput;
	int lengthDoc = Platform::SendScintilla(win.GetID(), SCI_GETLENGTH, 0, 0);
	if (end == -1)
		end = lengthDoc;
	int len = end - start;

	StylingContext styler(win.GetID(), props);

	int styleStart = 0;
	if (start > 0)
		styleStart = styler.StyleAt(start - 1);
	styler.SetCodePage(codePage);
	
	if (editor) {
		LexerModule::Colourise(start, len, styleStart, lexLanguage, keyWordLists, styler);
	} else {
		LexerModule::Colourise(start, len, 0, SCLEX_ERRORLIST, 0, styler);
	}
	styler.Flush();
	//DWORD dwEnd = timeGetTime();
	//Platform::DebugPrintf("end colourise %d\n", dwEnd - dwStart);
}
#endif

void SciTEBase::FindMatchingBracePosition(bool editor, int &braceAtCaret, int &braceOpposite) {
	Window &win = editor ? wEditor : wOutput;
	int bracesStyleCheck = editor ? bracesStyle : 0;
	int caretPos = Platform::SendScintilla(win.GetID(), SCI_GETCURRENTPOS, 0, 0);
	braceAtCaret = -1;
	braceOpposite = -1;
	char charBefore = '\0';
	char styleBefore = '\0';
	WindowAccessor acc(win.GetID(), props);
	if (caretPos > 0) {
		charBefore = acc[caretPos - 1];
		styleBefore = static_cast<char>(acc.StyleAt(caretPos - 1) & 31);
	}
	// Priority goes to character before caret
	if (charBefore && strchr("[](){}", charBefore) && (styleBefore == bracesStyleCheck)) {
		braceAtCaret = caretPos - 1;
	}
	if (bracesSloppy && (braceAtCaret < 0)) {
		// No brace found so check other side
		char charAfter = acc[caretPos];
		char styleAfter = static_cast<char>(acc.StyleAt(caretPos) & 31);
		if (charAfter && strchr("[](){}", charAfter) && (styleAfter == bracesStyleCheck)) {
			braceAtCaret = caretPos;
		}
	}
	if (braceAtCaret >= 0) {
		braceOpposite =
		    Platform::SendScintilla(win.GetID(), SCI_BRACEMATCH, braceAtCaret, 0);
	}
}

void SciTEBase::BraceMatch(bool editor) {
	if (!bracesCheck)
		return;
	int braceAtCaret = -1;
	int braceOpposite = -1;
	FindMatchingBracePosition(editor, braceAtCaret, braceOpposite);
	Window &win = editor ? wEditor : wOutput;
	if ((braceAtCaret != -1) && (braceOpposite == -1))
		Platform::SendScintilla(win.GetID(), SCI_BRACEBADLIGHT, braceAtCaret, 0);
	else
		Platform::SendScintilla(win.GetID(), SCI_BRACEHIGHLIGHT, braceAtCaret, braceOpposite);
}

void SciTEBase::SetWindowName() {
	if (fileName[0] == '\0')
		strcpy(windowName, "(Untitled)");
	else if (props.GetInt("title.full.path"))
		strcpy(windowName, fullPath);
	else
		strcpy(windowName, fileName);
	if (isDirty)
		strcat(windowName, " * ");
	else
		strcat(windowName, " - ");
	strcat(windowName, appName);
	wSciTE.SetTitle(windowName);
}

void SciTEBase::FixFilePath() {
	// Only used on Windows to fix the case of file names
}

void SciTEBase::SetFileName(const char *openName) {
	char pathCopy[MAX_PATH + 1];
	pathCopy[0] = '\0';

	if (openName[0] == '\"') {
		strncpy(pathCopy, openName + 1, MAX_PATH);
		pathCopy[MAX_PATH] = '\0';
		if (pathCopy[strlen(pathCopy) - 1] == '\"')
			pathCopy[strlen(pathCopy) - 1] = '\0';
		AbsolutePath(fullPath, pathCopy, MAX_PATH);
	} else if (openName[0]) {
		AbsolutePath(fullPath, openName, MAX_PATH);
	} else {
		fullPath[0] = '\0';
	}

	char *cpDirEnd = strrchr(fullPath, pathSepChar);
	if (cpDirEnd) {
		strcpy(fileName, cpDirEnd + 1);
		strcpy(dirName, fullPath);
		dirName[cpDirEnd - fullPath] = '\0';
	} else {
		strcpy(fileName, fullPath);
		getcwd(dirName, sizeof(dirName));
		//Platform::DebugPrintf("Working directory: <%s>\n", dirName);
		strcpy(fullPath, dirName);
		strcat(fullPath, pathSepString);
		strcat(fullPath, fileName);
	}
	FixFilePath();
	char fileBase[MAX_PATH];
	strcpy(fileBase, fileName);
	char *cpExt = strrchr(fileBase, '.');
	if (cpExt) {
		*cpExt = '\0';
		strcpy(fileExt, cpExt + 1);
	} else { // No extension
		fileExt[0] = '\0';
	}

	chdir(dirName);

	ReadLocalPropFile();

	props.Set("FilePath", fullPath);
	props.Set("FileDir", dirName);
	props.Set("FileName", fileBase);
	props.Set("FileExt", fileExt);
	props.Set("FileNameExt", fileName);

	SetWindowName();
}

void SciTEBase::New() {
	SendEditor(SCI_CLEARALL);
	fullPath[0] = '\0';
	fileName[0] = '\0';
	fileExt[0] = '\0';
	dirName[0] = '\0';
	SetWindowName();
	isDirty = false;
	isBuilding = false;
	isBuilt = false;
	SendEditor(EM_EMPTYUNDOBUFFER);
	SendEditor(SCI_SETSAVEPOINT);
}

bool SciTEBase::Exists(const char *dir, const char *path, char *testPath) {
	if (dir) {
		strcpy(testPath, path);
	} else {
		if ((strlen(dir) + strlen(pathSepString) + strlen(path) + 1) > MAX_PATH)
			return false;
		strcpy(testPath, dir);
		strcat(testPath, pathSepString);
		strcat(testPath, path);
	}
	FILE *fp = fopen(testPath, "rb");
	if (!fp)
		return false;
	fclose(fp);
	return true;
}

time_t GetModTime(const char *fullPath) {
	struct stat statusFile;
	if (stat(fullPath, &statusFile) != -1)
		return statusFile.st_mtime;
	else
		return 0;
}

void SciTEBase::Open(const char *file, bool initialCmdLine) {
	RememberLineNumberStack(fullPath, GetCurrentLineNumber(),
		GetCurrentScrollPosition());

	if (file) {
		New();
		//Platform::DebugPrintf("Opening %s\n", file);
		SetFileName(file);
		ReadProperties();
		if (fileName[0]) {
			SendEditor(SCI_CANCEL);
			SendEditor(SCI_SETUNDOCOLLECTION, 0);

			fileModTime = GetModTime(fullPath);

			AddFileToStack(fullPath);
			FILE *fp = fopen(fullPath, "rb");
			if (fp || initialCmdLine) {
				if (fp) {
					char data[blockSize];
					int lenFile = fread(data, 1, sizeof(data), fp);
					while (lenFile > 0) {
						SendEditorString(SCI_ADDTEXT, lenFile, data);
						lenFile = fread(data, 1, sizeof(data), fp);
					}
					fclose(fp);
				}

			} else {
				char msg[MAX_PATH + 100];
				strcpy(msg, "Could not open file \"");
				strcat(msg, fullPath);
				strcat(msg, "\".");
				MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
			}
			SendEditor(SCI_SETUNDOCOLLECTION, 1);
			// Flick focus to the output window and back to
			// ensure palette realised correctly.
			SetFocus(wOutput.GetID());
			SetFocus(wEditor.GetID());
			SendEditor(EM_EMPTYUNDOBUFFER);
			SendEditor(SCI_SETSAVEPOINT);
			if (props.GetInt("fold.on.open") > 0) {
				FoldAll();
			}
			SendEditor(SCI_GOTOPOS, 0);
		}
		Redraw();
	} else {
		if (!OpenDialog()) {
			return;
		}
	}
	AddFileToStack(fullPath);
	SetWindowName();
}

int SciTEBase::SaveIfUnsure(bool forceQuestion) {
	if (isDirty) {
		if (props.GetInt("are.you.sure", 1) || forceQuestion) {
			char msg[MAX_PATH + 100];
			strcpy(msg, "Save changes to \"");
			strcat(msg, fullPath);
			strcat(msg, "\"?");
			dialogsOnScreen++;
			int decision = MessageBox(wSciTE.GetID(), msg, appName, MB_YESNOCANCEL);
			dialogsOnScreen--;
			if (decision == IDYES) {
				if (!Save())
					decision = IDCANCEL;
			}
			return decision;
		} else {
			if (!Save())
				return IDCANCEL;
		}
	}
	return IDYES;
}

int SciTEBase::SaveIfUnsureForBuilt() {
	if (isDirty) {
		if (props.GetInt("are.you.sure.for.build", 0))
			return SaveIfUnsure(true);

		Save();
	}
	return IDYES;
}

int StripTrailingSpaces(char *data, int ds, bool lastBlock) {
	int lastRead = 0;
	char *w = data;
	
	for (int i=0; i<ds; i++) {
		char ch = data[i];
		if ((ch == ' ') || (ch == '\t')) {
			// Swallow those spaces
		} else if ((ch == '\r') || (ch == '\n')) {
			*w++ = ch;
			lastRead = i+1;
		} else {
			while (lastRead < i) {
				*w++ = data[lastRead++];
			}
			*w++ = ch;
			lastRead=i+1;
		}
	}
	// If a non-final block, then preserve whitespace at end of block as it may be significant.
	if (!lastBlock) {
		while (lastRead < ds) {
			*w++ = data[lastRead++];
		}
	}
	return w - data;
}

// Returns false only if cancelled
bool SciTEBase::Save() {
	if (fileName[0]) {
		//Platform::DebugPrintf("Saving <%s><%s>\n", fileName, fullPath);
		//DWORD dwStart = timeGetTime();
		FILE *fp = fopen(fullPath, "wb");
		if (fp) {
			char data[blockSize + 1];
			AddFileToStack(fullPath, GetCurrentLineNumber(), 
				GetCurrentScrollPosition());
			int lengthDoc = LengthDocument();
			for (int i = 0; i < lengthDoc; i += blockSize) {
				int grabSize = lengthDoc - i;
				if (grabSize > blockSize)
					grabSize = blockSize;
				GetRange(wEditor, i, i + grabSize, data);
 				if (props.GetInt("strip.trailing.spaces"))
 					grabSize = StripTrailingSpaces(
						data, grabSize, grabSize != blockSize);
				fwrite(data, grabSize, 1, fp);
			}
			fclose(fp);
			//DWORD dwEnd = timeGetTime();
			//Platform::DebugPrintf("Saved file=%d\n", dwEnd - dwStart);
			fileModTime = GetModTime(fullPath);
			SendEditor(SCI_SETSAVEPOINT);
			if ((EqualCaseInsensitive(fileName, propFileName)) ||
			        (EqualCaseInsensitive(fileName, propGlobalFileName)) || 
				(EqualCaseInsensitive(fileName, propUserFileName))) {
				ReadGlobalPropFile();
				ReadLocalPropFile();
				ReadProperties();
				SetWindowName();
				Redraw();
			}
		} else {
			char msg[200];
			strcpy(msg, "Could not save file \"");
			strcat(msg, fullPath);
			strcat(msg, "\".");
			dialogsOnScreen++;
			MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
			dialogsOnScreen--;
		}
		return true;
	} else {
		return SaveAs();
	}
}

bool SciTEBase::SaveAs(char *file) {
	if (file && *file) {
		SetFileName(file);
		Save();
		ReadProperties();
		SendEditor(SCI_COLOURISE, 0, -1);
		Redraw();
		SetWindowName();
		return true;
	} else {
		return SaveAsDialog();
	}
}

void SciTEBase::SaveToHTML(const char *saveName) {
	SendEditor(SCI_COLOURISE, 0, -1);
	//Colourise();   	// Ensure whole file styled
	int tabSize = props.GetInt("tabsize");
	if (tabSize == 0)
		tabSize = 4;
	FILE *fp = fopen(saveName, "wt");
	if (fp) {
		int styleCurrent = -1;
		fputs("<HTML>\n", fp);
		fputs("<HEAD>\n", fp);
		fputs("<STYLE>\n", fp);
		SString colour;
		for (int istyle = 0; istyle <= STYLE_DEFAULT; istyle++) {
			char key[200];
			sprintf(key, "style.*.%0d", istyle);
			char *valdef = StringDup(props.Get(key).c_str());
			sprintf(key, "style.%s.%0d", language.c_str(), istyle);
			char *val = StringDup(props.Get(key).c_str());
			SString family;
			SString fore;
			SString back;
			bool italics = false;
			bool bold = false;
			int size = 0;
			if ((valdef && *valdef) || (val && *val)) {
				if (istyle == STYLE_DEFAULT)
					fprintf(fp, "SPAN {\n");
				else
					fprintf(fp, ".S%0d {\n", istyle);
				if (valdef && *valdef) {
					char *opt = valdef;
					while (opt) {
						char *cpComma = strchr(opt, ',');
						if (cpComma)
							*cpComma = '\0';
						char *colon = strchr(opt, ':');
						if (colon)
							*colon++ = '\0';
						if (0 == strcmp(opt, "italics"))
							italics = true;
						if (0 == strcmp(opt, "notitalics"))
							italics = false;
						if (0 == strcmp(opt, "bold"))
							bold = true;
						if (0 == strcmp(opt, "notbold"))
							bold = false;
						if (0 == strcmp(opt, "font"))
							family = colon;
						if (0 == strcmp(opt, "fore"))
							fore = colon;
						if (0 == strcmp(opt, "back"))
							back = colon;
						if (0 == strcmp(opt, "size"))
							size = atoi(colon);
						if (cpComma)
							opt = cpComma + 1;
						else
							opt = 0;
					}
				}
				if (val && *val) {
					char *opt = val;
					while (opt) {
						char *cpComma = strchr(opt, ',');
						if (cpComma)
							*cpComma = '\0';
						char *colon = strchr(opt, ':');
						if (colon)
							*colon++ = '\0';
						if (0 == strcmp(opt, "italics"))
							italics = true;
						if (0 == strcmp(opt, "notitalics"))
							italics = false;
						if (0 == strcmp(opt, "bold"))
							bold = true;
						if (0 == strcmp(opt, "notbold"))
							bold = false;
						if (0 == strcmp(opt, "font"))
							family = colon;
						if (0 == strcmp(opt, "fore"))
							fore = colon;
						if (0 == strcmp(opt, "back"))
							back = colon;
						if (0 == strcmp(opt, "size"))
							size = atoi(colon);
						if (cpComma)
							opt = cpComma + 1;
						else
							opt = 0;
					}
				}
				if (italics)
					fprintf(fp, "\tfont-style: italic;\n");
				if (bold)
					fprintf(fp, "\tfont-weight: bold;\n");
				if (family.length())
					fprintf(fp, "\tfont-family: %s;\n", family.c_str());
				if (fore.length())
					fprintf(fp, "\tcolor: %s;\n", fore.c_str());
				if (back.length())
					fprintf(fp, "\tbackground: %s;\n", back.c_str());
				if (size)
					fprintf(fp, "\tfont-size: %0dpt;\n", size);
				fprintf(fp, "}\n");
			}
			if (val)
				delete []val;
			if (valdef)
				delete []valdef;
		}
		fputs("</STYLE>\n", fp);
		fputs("</HEAD>\n", fp);
		fputs("<BODY>\n", fp);
		fputs("<SPAN class=\'S0\'>", fp);
		int lengthDoc = LengthDocument();
		bool prevCR = false;
		WindowAccessor acc(wEditor.GetID(), props);
		for (int i = 0; i < lengthDoc; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);
			if (style != styleCurrent) {
				fputs("</SPAN>", fp);
				fprintf(fp, "<SPAN class=\'S%0d\'>", style);
				styleCurrent = style;
			}
			if (ch == ' ') {
				fputs("&nbsp;", fp);
			} else if (ch == '\t') {
				for (int itab = 0; itab < tabSize; itab++)
					fputs("&nbsp;", fp);
			} else if (ch == '\r') {
				fputs("<BR>\n", fp);
			} else if (ch == '\n') {
				if (!prevCR)
					fputs("<BR>\n", fp);
			} else if (ch == '<') {
				fputs("&lt;", fp);
			} else if (ch == '>') {
				fputs("&gt;", fp);
			} else if (ch == '&') {
				fputs("&amp;", fp);
			} else {
				fputc(ch, fp);
			}
			prevCR = ch == '\r';
		}
		fputs("</SPAN>", fp);
		fputs("</BODY>\n</HTML>\n", fp);
		fclose(fp);
	} else {
		char msg[200];
		strcpy(msg, "Could not save file \"");
		strcat(msg, fullPath);
		strcat(msg, "\".");
		dialogsOnScreen++;
		MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
		dialogsOnScreen--;
	}
}

void SciTEBase::OpenProperties(int propsFile) {
	if (SaveIfUnsure() != IDCANCEL) {
		char propfile[MAX_PATH + 20];
		if (propsFile == IDM_OPENLOCALPROPERTIES) {
			getcwd(propfile, sizeof(propfile));
			strcat(propfile, pathSepString);
			strcat(propfile, propFileName);
			Open(propfile);
		} else if (propsFile == IDM_OPENUSERPROPERTIES) {
			if (GetUserPropertiesFileName(propfile, sizeof(propfile))) {
				Open(propfile);
			}
		} else {	// IDM_OPENGLOBALPROPERTIES
			if (GetDefaultPropertiesFileName(propfile, sizeof(propfile))) {
				Open(propfile);
			}
		}
	}
}

void SciTEBase::SetSelection(int anchor, int currentPos) {
	CHARRANGE crange;
	crange.cpMin = anchor;
	crange.cpMax = currentPos;
	SendEditor(EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&crange));
}

static bool iswordcharforsel(char ch) {
	return isalnum(ch) || ch == '_';
}

void SciTEBase::SelectionIntoProperties() {
	int selStart = 0;
	int selEnd = 0;
	SendEditor(EM_GETSEL, reinterpret_cast<WPARAM>(&selStart),
	           reinterpret_cast<LPARAM>(&selEnd));
	char currentSelection[1000];
	if ((selStart < selEnd) && ((selEnd - selStart + 1) < static_cast<int>(sizeof(currentSelection)))) {
		GetRange(wEditor, selStart, selEnd, currentSelection);
		props.Set("CurrentSelection", currentSelection);
	}
}

void SciTEBase::SelectionIntoFind() {
	int selStart = 0;
	int selEnd = 0;
	int lengthDoc = LengthDocument();
	SendEditor(EM_GETSEL, reinterpret_cast<WPARAM>(&selStart),
	           reinterpret_cast<LPARAM>(&selEnd));
	if (selStart == selEnd) {
		WindowAccessor acc(wEditor.GetID(), props);
		// Try and find a word at the caret
		if (iswordcharforsel(acc[selStart])) {
			while ((selStart > 0) && (iswordcharforsel(acc[selStart - 1])))
				selStart--;
			while ((selEnd < lengthDoc - 1) && (iswordcharforsel(acc[selEnd + 1])))
				selEnd++;
			if (selStart < selEnd)
				selEnd++;   	// Because normal selections end one past
		}
	}
	if ((selStart < selEnd) && ((selEnd - selStart + 1) < static_cast<int>(sizeof(findWhat)))) {
		GetRange(wEditor, selStart, selEnd, findWhat);
	}
}

void SciTEBase::FindNext() {
	if (!findWhat[0]) {
		Find();
		return;
	}
	FINDTEXTEX ft = {{0,0},0,{0,0}};
	CHARRANGE crange;
	SendEditor(EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&crange));
	if (reverseFind) {
		ft.chrg.cpMin = crange.cpMin - 1;
		ft.chrg.cpMax = 0;
	} else {
		ft.chrg.cpMin = crange.cpMax + 1;
		ft.chrg.cpMax = LengthDocument();
	}
	ft.lpstrText = findWhat;
	ft.chrgText.cpMin = 0;
	ft.chrgText.cpMax = 0;
	int flags = (wholeWord ? FR_WHOLEWORD : 0) | (matchCase ? FR_MATCHCASE : 0);
	//DWORD dwStart = timeGetTime();
	int posFind = SendEditor(EM_FINDTEXTEX, flags, reinterpret_cast<LPARAM>(&ft));
	//DWORD dwEnd = timeGetTime();
	//Platform::DebugPrintf("<%s> found at %d took %d\n", findWhat, posFind, dwEnd - dwStart);
	if (posFind == -1) {
		// Failed to find in indicated direction so search on other side
		if (reverseFind) {
			ft.chrg.cpMin = LengthDocument();
			ft.chrg.cpMax = 0;
		} else {
			ft.chrg.cpMin = 0;
			ft.chrg.cpMax = LengthDocument();
		}
		posFind = SendEditor(EM_FINDTEXTEX, flags, reinterpret_cast<LPARAM>(&ft));
	}
	if (posFind == -1) {
		havefound = false;
		char msg[300];
		strcpy(msg, "Cannot find the string \"");
		strcat(msg, findWhat);
		strcat(msg, "\".");
		dialogsOnScreen++;
		if (wFindReplace.Created())
			MessageBox(wFindReplace.GetID(), msg, appName, MB_OK | MB_ICONWARNING);
		else
			MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
		dialogsOnScreen--;
	} else {
		havefound = true;
		EnsureRangeVisible(ft.chrgText.cpMin, ft.chrgText.cpMax);
		SetSelection(ft.chrgText.cpMin, ft.chrgText.cpMax);
		if (!replacing) {
			DestroyFindReplace();
		}
	}
}

void SciTEBase::ReplaceOnce() {
	if (havefound) {
		SendEditorString(EM_REPLACESEL, 0, replaceWhat);
		havefound = false;
		//Platform::DebugPrintf("Replace <%s> -> <%s>\n", findWhat, replaceWhat);
	}
	FindNext();
}

void SciTEBase::ReplaceAll() {
	FINDTEXTEX ft;
	ft.chrg.cpMin = 0;
	ft.chrg.cpMax = LengthDocument();
	ft.lpstrText = findWhat;
	ft.chrgText.cpMin = 0;
	ft.chrgText.cpMax = 0;
	int flags = (wholeWord ? FR_WHOLEWORD : 0) | (matchCase ? FR_MATCHCASE : 0);
	int posFind = SendEditor(EM_FINDTEXTEX, flags,
	                         reinterpret_cast<LPARAM>(&ft));
	if (posFind != -1) {
		SendEditor(SCI_BEGINUNDOACTION);
		while (posFind != -1) {
			SetSelection(ft.chrgText.cpMin, ft.chrgText.cpMax);
			SendEditorString(EM_REPLACESEL, 0, replaceWhat);
			ft.chrg.cpMin = posFind + strlen(replaceWhat) + 1;
			ft.chrg.cpMax = LengthDocument();
			posFind = SendEditor(EM_FINDTEXTEX, flags,
			                     reinterpret_cast<LPARAM>(&ft));
		}
		SendEditor(SCI_ENDUNDOACTION);
	} else {
		char msg[200];
		strcpy(msg, "No replacements because string \"");
		strcat(msg, findWhat);
		strcat(msg, "\" was not present.");
		dialogsOnScreen++;
#if PLAT_GTK
		MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
#else
		MessageBox(wFindReplace.GetID(), msg, appName, MB_OK);
#endif
		dialogsOnScreen--;
	}
	//Platform::DebugPrintf("ReplaceAll <%s> -> <%s>\n", findWhat, replaceWhat);
}

void SciTEBase::OutputAppendString(const char *s, int len) {
	if (len == -1)
		len = strlen(s);
	SendOutput(SCI_GOTOPOS, SendOutput(WM_GETTEXTLENGTH));
	SendOutput(SCI_ADDTEXT, len, reinterpret_cast<LPARAM>(s));
	SendOutput(SCI_GOTOPOS, SendOutput(WM_GETTEXTLENGTH));
}

void SciTEBase::Execute() {
	if (clearBeforeExecute) {
		SendOutput(SCI_CLEARALL);
	}

	SendOutput(SCI_MARKERDELETEALL, static_cast<WPARAM>(-1));
	SendEditor(SCI_MARKERDELETEALL, 0);
	// Ensure the output pane is visible
	if (heightOutput < 20) {
		if (splitVertical)
			heightOutput = NormaliseSplit(300);
		else
			heightOutput = NormaliseSplit(100);
		SizeSubWindows();
		Redraw();
	}

	cancelFlag = 0L;
	executing = true;
	CheckMenus();
	chdir(dirName);
	strcpy(dirNameAtExecute, dirName);
}

void SciTEBase::BookmarkToggle( int lineno ) {
	if (lineno==-1)
		lineno = GetCurrentLineNumber();
	int state = SendEditor(SCI_MARKERGET, lineno);
	if ( state & (1<<SciTE_MARKER_BOOKMARK))
		SendEditor(SCI_MARKERDELETE, lineno, SciTE_MARKER_BOOKMARK);
	else {
		// no need to do each time, but can't hurt either :-)
		SendEditor(SCI_MARKERDEFINE, SciTE_MARKER_BOOKMARK, SC_MARK_CIRCLE);
		SendEditor(SCI_MARKERSETFORE, SciTE_MARKER_BOOKMARK, Colour(0x7f, 0, 0).AsLong());
		SendEditor(SCI_MARKERSETBACK, SciTE_MARKER_BOOKMARK, Colour(0x80, 0xff, 0xff).AsLong());
		SendEditor(SCI_MARKERADD, lineno, SciTE_MARKER_BOOKMARK);
	}
		
}

void SciTEBase::BookmarkNext() {
	int lineno = GetCurrentLineNumber();
	int nextLine = SendEditor(SCI_MARKERNEXT, lineno+1, 1 << SciTE_MARKER_BOOKMARK);
	if (nextLine<0)
			nextLine = SendEditor(SCI_MARKERNEXT, 0, 1 << SciTE_MARKER_BOOKMARK);
	if (nextLine <0 || nextLine == lineno)
		; // how do I beep?
	else {
		SendEditor(SCI_ENSUREVISIBLE, nextLine);
		SendEditor(SCI_GOTOLINE, nextLine);
	}
}

void SciTEBase::CheckReload() {
	if (props.GetInt("load.on.activate")) {
		// Make a copy of fullPath as otherwise it gets aliased in Open
		char fullPathToCheck[MAX_PATH];
		strcpy(fullPathToCheck, fullPath);
		time_t newModTime = GetModTime(fullPathToCheck);
		//Platform::DebugPrintf("Times are %d %d\n", fileModTime, newModTime);
		if (newModTime > fileModTime) {
			if (isDirty) {
				static bool entered = false;   	// Stop reentrancy
				if (!entered && (0 == dialogsOnScreen)) {
					entered = true;
					char msg[MAX_PATH + 100];
					strcpy(msg, "The file \"");
					strcat(msg, fullPathToCheck);
					strcat(msg, "\" has been modified. Should it be reloaded?");
					dialogsOnScreen++;
					int decision = MessageBox(wSciTE.GetID(), msg, appName, MB_YESNO);
					dialogsOnScreen--;
					if (decision == IDYES) {
						Open(fullPathToCheck);
					}
					entered = false;
				}
			} else {
				Open(fullPathToCheck);
			}
		}
	}
}

void SciTEBase::Activate(bool activeApp) {
	if (activeApp) {
		CheckReload();
	} else {
		if (isDirty) {
			if (props.GetInt("save.on.deactivate") && fileName[0]) {
				// Trying to save at deactivation when no file name -> dialogs
				Save();
			}
		}
	}
}

PRectangle SciTEBase::GetClientRectangle() {
	return wContent.GetClientPosition();
}

void SciTEBase::Redraw() {
	wSciTE.InvalidateAll();
	wEditor.InvalidateAll();
	wOutput.InvalidateAll();
}

int DecodeMessage(char *cdoc, char *sourcePath, int format) {
	sourcePath[0] = '\0';
	if (format == 1) {
		// Python
		char *startPath = strchr(cdoc, '\"') + 1;
		char *endPath = strchr(startPath, '\"');
		strncpy(sourcePath, startPath, endPath - startPath);
		sourcePath[endPath - startPath] = 0;
		endPath++;
		while (*endPath && !isdigit(*endPath))
			endPath++;
		int sourceNumber = atoi(endPath) - 1;
		return sourceNumber;
	} else if (format == 2) {
		// GCC - look for number followed by colon to be line number
		// This will be preceded by file name
		for (int i = 0; cdoc[i]; i++) {
			if (isdigit(cdoc[i]) && cdoc[i + 1] == ':') {
				int j = i;
				while (j > 0 && isdigit(cdoc[j - 1]))
					j--;
				int sourceNumber = atoi(cdoc + j) - 1;
				strncpy(sourcePath, cdoc, j - 1);
				sourcePath[j - 1] = 0;
				return sourceNumber;
			}
		}
	} else if (format == 3) {
		// Visual *
		char *endPath = strchr(cdoc, '(');
		strncpy(sourcePath, cdoc, endPath - cdoc);
		sourcePath[endPath - cdoc] = 0;
		endPath++;
		return atoi(endPath) - 1;
	} else if (format == 5) {
		// Borland
		char *space = strchr(cdoc, ' ');
		if (space) {
			while (isspace(*space))
				space++;
			char *space2 = strchr(space, ' ');
			if (space2) {
				strncpy(sourcePath, space, space2-space);
				return atoi(space2) - 1;
			}
		}
	}
	return - 1;
}

void SciTEBase::GoMessage(int dir) {
	CHARRANGE crange;
	SendOutput(EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&crange));
	int selStart = crange.cpMin;
	int curLine = SendOutput(EM_LINEFROMCHAR, selStart);
	int maxLine = SendOutput(EM_GETLINECOUNT);
	int lookLine = curLine + dir;
	if (lookLine < 0)
		lookLine = maxLine - 1;
	else if (lookLine >= maxLine)
		lookLine = 0;
	WindowAccessor acc(wOutput.GetID(), props);
	while ((dir == 0) || (lookLine != curLine)) {
		int startPosLine = SendOutput(EM_LINEINDEX, lookLine, 0);
		int lineLength = SendOutput(SCI_LINELENGTH, lookLine, 0);
		//Platform::DebugPrintf("GOMessage %d %d %d of %d linestart = %d\n", selStart, curLine, lookLine, maxLine, startPosLine);
		char style = acc.StyleAt(startPosLine);
		if (style != 0 && style != 4) {
			//Platform::DebugPrintf("Marker to %d\n", lookLine);
			SendOutput(SCI_MARKERDELETEALL, static_cast<WPARAM>(-1));
			SendOutput(SCI_MARKERDEFINE, 0, SC_MARK_SMALLRECT);
			SendOutput(SCI_MARKERSETFORE, 0, Colour(0x7f, 0, 0).AsLong());
			SendOutput(SCI_MARKERSETBACK, 0, Colour(0xff, 0xff, 0).AsLong());
			SendOutput(SCI_MARKERADD, lookLine, 0);
			CHARRANGE crangeset = {startPosLine, startPosLine};
			SendOutput(EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&crangeset));
			char *cdoc = new char[lineLength + 1];
			if (!cdoc)
				return;
			GetRange(wOutput, startPosLine, startPosLine + lineLength, cdoc);
			char sourcePath[MAX_PATH];
			int sourceLine = DecodeMessage(cdoc, sourcePath, style);
			//Platform::DebugPrintf("<%s> %d %d\n",sourcePath, sourceLine, lookLine);
			if (sourceLine >= 0) {
				if (0 != strcmp(sourcePath, fileName)) {
					char messagePath[MAX_PATH];
					if (Exists(dirNameAtExecute, sourcePath, messagePath)) {
						if (SaveIfUnsure() == IDCANCEL) {
							delete []cdoc;
							return;
						} else {
							Open(messagePath);
						}
					} else if (Exists(dirName, sourcePath, messagePath)) {
						if (SaveIfUnsure() == IDCANCEL) {
							delete []cdoc;
							return;
						} else {
							Open(messagePath);
						}
					}
				}
				SendEditor(SCI_MARKERDELETEALL, 0);
				SendEditor(SCI_MARKERDEFINE, 0, SC_MARK_CIRCLE);
				SendEditor(SCI_MARKERSETFORE, 0, Colour(0x7f, 0, 0).AsLong());
				SendEditor(SCI_MARKERSETBACK, 0, Colour(0xff, 0xff, 0).AsLong());
				SendEditor(SCI_MARKERADD, sourceLine, 0);
				int startSourceLine = SendEditor(EM_LINEINDEX, sourceLine, 0);
				EnsureRangeVisible(startSourceLine, startSourceLine);
				SetSelection(startSourceLine, startSourceLine);
				SetFocus(wEditor.GetID());
			}
			delete []cdoc;
			return;
		}
		if (dir == 0)
			dir = 1;
		lookLine += dir;
		if (lookLine < 0)
			lookLine = maxLine - 1;
		else if (lookLine >= maxLine)
			lookLine = 0;
	}
}

inline bool nonFuncChar(char ch) {
	return isspace(ch) || ch=='*' || ch=='/' || ch=='%' || ch=='+' || ch=='-' || ch=='=' || ch=='(';
}

void SciTEBase::StartCallTip() {
	//Platform::DebugPrintf("StartCallTip\n");
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));
	int pos = SendEditor(SCI_GETCURRENTPOS);

	int startword = current - 1;
	while (startword > 0 && !nonFuncChar(linebuf[startword - 1]))
		startword--;
	linebuf[current] = '\0';
	int rootlen = current - startword;
	functionDefinition = "";
	//Platform::DebugPrintf("word  is [%s] %d %d %d\n", linebuf + startword, rootlen, pos, pos - rootlen);
	if (apis) {
		int i;
		for (i = 0; apis[i][0]; i++) {
			if (0 == strncmp(linebuf + startword, apis[i], rootlen)) {
				functionDefinition = apis[i];
				SendEditorString(SCI_CALLTIPSHOW, pos - rootlen, apis[i]);
				ContinueCallTip();
			}
		}
	}
}

void SciTEBase::ContinueCallTip() {
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));

	int commas = 0;
	for (int i = 0; i < current; i++) {
		if (linebuf[i] == ',')
			commas++;
	}

	int startHighlight = 0;
	while (functionDefinition[startHighlight] && functionDefinition[startHighlight] != '(')
		startHighlight++;
	if (functionDefinition[startHighlight] == '(')
		startHighlight++;
	while (functionDefinition[startHighlight] && commas > 0) {
		if (functionDefinition[startHighlight] == ',' || functionDefinition[startHighlight] == ')')
			commas--;
		startHighlight++;
	}
	if (functionDefinition[startHighlight] == ',' || functionDefinition[startHighlight] == ')')
		startHighlight++;
	int endHighlight = startHighlight;
	if (functionDefinition[endHighlight])
		endHighlight++;
	while (functionDefinition[endHighlight] && functionDefinition[endHighlight] != ',' && functionDefinition[endHighlight] != ')')
		endHighlight++;

	SendEditor(SCI_CALLTIPSETHLT, startHighlight, endHighlight);
}

void SciTEBase::StartAutoComplete() {
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));

	int startword = current;
	while (startword > 0 && !nonFuncChar(linebuf[startword - 1]))
		startword--;
	linebuf[current] = '\0';
	const char *root = linebuf + startword;
	int rootlen = current - startword;
	if (apis) {
		int lenchoices = 0;
		int i;
		for (i = 0; apis[i][0]; i++) {
			if (0 == strncmp(root, apis[i], rootlen)) {
				const char *brace = strchr(apis[i], '(');
				if (brace)
					lenchoices += brace - apis[i];
				else
					lenchoices += strlen(apis[i]);
				lenchoices++;
			}
		}
		if (lenchoices) {
			char *choices = new char[lenchoices + 1];
			if (choices) {
				char *endchoice = choices;
				for (i = 0; apis[i][0]; i++) {
					if (0 == strncmp(root, apis[i], rootlen)) {
						if (endchoice != choices)
							*endchoice++ = ' ';
						const char *brace = strchr(apis[i], '(');
						int len = strlen(apis[i]);
						if (brace)
							len = brace - apis[i];
						strncpy(endchoice, apis[i], len);
						endchoice += len;
					}
					*endchoice = '\0';
				}
				SendEditorString(SCI_AUTOCSHOW, rootlen, choices);
				delete []choices;
			}
		}
	}
}

int SciTEBase::GetCurrentLineNumber() {
	CHARRANGE crange;
	SendEditor(EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&crange));
	int selStart = crange.cpMin;
	return SendEditor(EM_LINEFROMCHAR, selStart);
}

int SciTEBase::GetCurrentScrollPosition() {
	int lineDisplayTop = SendEditor(EM_GETFIRSTVISIBLELINE);
	return SendEditor(SCI_DOCLINEFROMVISIBLE, lineDisplayTop);
}

void SciTEBase::UpdateStatusBar() {
	if (sbVisible) {
		SString msg;
		int caretPos = SendEditor(SCI_GETCURRENTPOS);
		int caretLine = SendEditor(EM_LINEFROMCHAR, caretPos);
		int caretLineStart = SendEditor(EM_LINEINDEX, caretLine);
		msg = "Column=";
		msg += SString(caretPos - caretLineStart + 1).c_str();
		msg += "    Line=";
		msg += SString(caretLine + 1).c_str();
		if (!(sbValue == msg)) {
			SetStatusBarText(msg.c_str());
			sbValue = msg;
		}
	} else {
		sbValue = "";
	}
}

int SciTEBase::SetLineIndentation(int line, int indent) {
	SendEditor(SCI_SETLINEINDENTATION, line, indent);
	int pos = GetLineIndentPosition(line);
	SetSelection(pos,pos);
	return pos;
}

int SciTEBase::GetLineIndentation(int line) {
	return SendEditor(SCI_GETLINEINDENTATION, line);
}

int SciTEBase::GetLineIndentPosition(int line) {
	return SendEditor(SCI_GETLINEINDENTPOSITION, line);
}

bool SciTEBase::RangeIsAllWhitespace(int start, int end) {
	WindowAccessor acc(wEditor.GetID(), props);
	for (int i=start;i<end;i++) {
		if ((acc[i] != ' ') && (acc[i] != '\t')) 
			return false;
	}
	return true;
}

void SciTEBase::GetLinePartsInStyle(int line, int style1, int style2, SString sv[], int len) {
	for (int i=0; i<len; i++)
		sv[i] = "";
	WindowAccessor acc(wEditor.GetID(), props);
	SString s;
	int part = 0;
	int thisLineStart = SendEditor(EM_LINEINDEX, line);
	int nextLineStart = SendEditor(EM_LINEINDEX, line+1);
	for (int pos=thisLineStart; pos < nextLineStart; pos++) {
		if ((acc.StyleAt(pos) == style1) || (acc.StyleAt(pos) == style2)) {
			char c[2];
			c[0] = acc[pos];
			c[1] = '\0';
			s += c;
		} else if (s.length() > 0) {
			if (part < len) {
				sv[part++] = s;
			}
			s = "";
		}
	}
	if ((s.length() > 0) && (part < len)) {
		sv[part++] = s;
	}
}

static bool includes(const StyleAndWords &symbols, const SString value) {
	if (symbols.words.length() == 0) {
		return false;
	} else if (strchr(symbols.words.c_str(), ' ')) {
		// Set of symbols separated by spaces
		int lenVal = value.length();
		const char *symbol = symbols.words.c_str();
		while (symbol) {
			const char *symbolEnd = strchr(symbol, ' ');
			int lenSymbol = strlen(symbol);
			if (symbolEnd)
				lenSymbol = symbolEnd - symbol;
			if (lenSymbol == lenVal) {
				if (strncmp(symbol, value.c_str(), lenSymbol) == 0) {
					return true;
				}
			}
			symbol = symbolEnd; 
			if (symbol) 
				symbol++;
		}
		return false;
	} else {
		// Set of individual characters. Only one character allowed for now
		char ch = symbols.words[0];
		return strchr(value.c_str(), ch) != 0;
	}
}

#define ELEMENTS(a)	(sizeof(a) / sizeof(a[0]))

int SciTEBase::GetIndentState(int line) {
	// C like language indentation defined by braces and keywords
	int indentState = 0;
	SString controlWords[10];
	GetLinePartsInStyle(line, SCE_C_WORD, -1, controlWords, ELEMENTS(controlWords));
	for (unsigned int i=0;i<ELEMENTS(controlWords);i++) {
		if (includes(statementIndent, controlWords[i]))
			indentState = 2;
	}
	// Braces override keywords
	SString controlStrings[10];
	GetLinePartsInStyle(line, SCE_C_OPERATOR, -1, controlStrings, ELEMENTS(controlStrings));
	for (unsigned int j=0;j<ELEMENTS(controlStrings);j++) {
		if (includes(blockEnd, controlStrings[j]))
			indentState = -1;
		if (includes(blockStart, controlStrings[j]))
			indentState = 1;
	}
	return indentState;
}

void SciTEBase::AutomaticIndentation(char ch) {
	CHARRANGE crange;
	SendEditor(EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&crange));
	int selStart = crange.cpMin;
	int curLine = GetCurrentLineNumber();
	int thisLineStart = SendEditor(EM_LINEINDEX, curLine);
	int indent = GetLineIndentation(curLine - 1);
	int indentBlock = indent;
	int backLine = curLine - 1;
	int indentState = 0;
	if (statementIndent.IsEmpty() && blockStart.IsEmpty() && blockEnd.IsEmpty())
		indentState = 1;	// Don't bother searching backwards
	while ((backLine >= 0) && (indentState == 0)) {
		indentState = GetIndentState(backLine);
		if (indentState != 0) {
			indentBlock = GetLineIndentation(backLine);
			if (indentState == 1) {
				if (!indentOpening)
					indentBlock += indentSize;
			}
			if (indentState == -1) {
				if (indentClosing)
					indentBlock -= indentSize;
				if (indentBlock < 0)
					indentBlock = 0;
			}
			if ((indentState == 2) && (backLine == (curLine - 1)))
				indentBlock += indentSize;
		}
		backLine--;
	}
	if (ch == blockEnd.words[0]) {	// Dedent maybe
		if (!indentClosing) {
			if (RangeIsAllWhitespace(thisLineStart, selStart-1)) {
				int pos = SetLineIndentation(curLine, indentBlock - indentSize);
				// Move caret after '}'
				SetSelection(pos+1, pos+1);
			}
		}
	} else if (ch == blockStart.words[0]) {	// Dedent maybe if first on line 
		if (!indentOpening) {
			if (RangeIsAllWhitespace(thisLineStart, selStart-1)) {
				int pos = SetLineIndentation(curLine, indentBlock - indentSize);
				// Move caret after '{'
				SetSelection(pos+1, pos+1);
			}
		}
	} else if ((ch == '\r' || ch == '\n') && (selStart == thisLineStart)) {
		SetLineIndentation(curLine, indentBlock);
	}
}

// Upon a character being added, SciTE may decide to perform some action
// such as displaying a completion list.
void SciTEBase::CharAdded(char ch) {
	CHARRANGE crange;
	SendEditor(EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&crange));
	int selStart = crange.cpMin;
	int selEnd = crange.cpMax;
	if ((selEnd == selStart) && (selStart > 0)) {
		int style = SendEditor(SCI_GETSTYLEAT, selStart - 1, 0);
		//Platform::DebugPrintf("Char added %d style = %d %d\n", ch, style, braceCount);
		if (style != 1) {
			if (SendEditor(SCI_CALLTIPACTIVE)) {
				if (ch == ')') {
					braceCount--;
					if (braceCount < 1)
						SendEditor(SCI_CALLTIPCANCEL);
				} else if (ch == '(') {
					braceCount++;
				} else {
					ContinueCallTip();
				}
			} else if (SendEditor(SCI_AUTOCACTIVE)) {
				if (ch == '(') {
					braceCount++;
					StartCallTip();
				} else if (ch == ')') {
					braceCount--;
				} else if (!isalpha(ch) && (ch != '_')) {
					SendEditor(SCI_AUTOCCANCEL);
				}
			} else {
				if (ch == '(') {
					braceCount = 1;
					StartCallTip();
				} else {
					if (props.GetInt("indent.automatic"))
						AutomaticIndentation(ch);
				}
			}
		}
	}
}

void SciTEBase::GoMatchingBrace() {
	int braceAtCaret = -1;
	int braceOpposite = -1;
	FindMatchingBracePosition(true, braceAtCaret, braceOpposite);
	if (braceOpposite >= 0) {
		//SendEditor(SCI_GETCHARAT, braceOpposite);
		braceOpposite++;
		EnsureRangeVisible(braceOpposite, braceOpposite);
		SetSelection(braceOpposite, braceOpposite);
	}
}

void SciTEBase::AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, bool) {
	if (cmd.length()) {
		jobQueue[commandCurrent].command = cmd;
		jobQueue[commandCurrent].directory = dir;
		jobQueue[commandCurrent].jobType = jobType;
		commandCurrent++;
	}
}

int ControlIDOfCommand(WPARAM wParam) {
	return wParam & 0xffff;
}

void SciTEBase::MenuCommand(int cmdID) {
	switch (cmdID) {
	case IDM_NEW:
		// For the New command, the are you sure question is always asked as this gives
		// an opportunity to abandon the edits made to a file when are.you.sure is turned off.
		if (SaveIfUnsure(true) != IDCANCEL) {
			New();
			ReadProperties();
		}
		break;
	case IDM_OPEN:
		if (SaveIfUnsure() != IDCANCEL) {
			Open();
			SetFocus(wEditor.GetID());
		}
		break;
	case IDM_CLOSE:
		if (SaveIfUnsure() != IDCANCEL) {
			DropFileStackTop();
			StackMenu(0);
			// If none left, New()
			SetFocus(wEditor.GetID());
		}
		break;
	case IDM_SAVE:
		Save();
		SetFocus(wEditor.GetID());
		break;
	case IDM_SAVEAS:
		SaveAs();
		SetFocus(wEditor.GetID());
		break;
	case IDM_SAVEASHTML:
		SaveAsHTML();
		SetFocus(wEditor.GetID());
		break;
	case IDM_REVERT:
		break;
	case IDM_PRINT:
		Print();
		break;
	case IDM_PRINTSETUP:
		PrintSetup();
		break;
	case IDM_ABOUT:
		AboutDialog();
		break;
	case IDM_QUIT:
		QuitProgram();
		break;
	case IDM_NEXTFILE:
		if (SaveIfUnsure() != IDCANCEL) {
			StackMenuNext();
		}
		break;
	case IDM_PREVFILE:
		if (SaveIfUnsure() != IDCANCEL) {
			StackMenuPrev();
		}
		break;

	case IDM_UNDO:
		SendFocused(WM_UNDO);
		break;
	case IDM_REDO:
		SendFocused(SCI_REDO);
		break;

	case IDM_CUT:
		SendFocused(WM_CUT);
		break;
	case IDM_COPY:
		SendFocused(WM_COPY);
		break;
	case IDM_PASTE:
		SendFocused(WM_PASTE);
		break;
	case IDM_CLEAR:
		SendFocused(WM_CLEAR);
		break;
	case IDM_SELECTALL:
		SendFocused(SCI_SELECTALL);
		break;

	case IDM_FIND:
		Find();
		break;

	case IDM_FINDNEXT:
		FindNext();
		break;

	case IDM_FINDINFILES:
		FindInFiles();
		break;

	case IDM_REPLACE:
		Replace();
		break;

	case IDM_GOTO:
		GoLineDialog();
		break;

	case IDM_MATCHBRACE:
		GoMatchingBrace();
		break;

	case IDM_COMPLETE:
		StartAutoComplete();
		break;

	case IDM_UPRCASE:
		SendFocused(SCI_UPPERCASE);
		break;

	case IDM_LWRCASE:
		SendFocused(SCI_LOWERCASE);
		break;

	case IDM_EXPAND:
		SendEditor(SCI_TOGGLEFOLD, GetCurrentLineNumber());
		break;
		
	case IDM_SPLITVERTICAL:
		splitVertical = !splitVertical;
		heightOutput = NormaliseSplit(heightOutput);
		SizeSubWindows();
		CheckMenus();
		Redraw();
		break;

	case IDM_LINENUMBERMARGIN:
		lineNumbers = !lineNumbers;
		SendEditor(SCI_SETMARGINWIDTHN, 0, lineNumbers ? lineNumbersWidth : 0);
		CheckMenus();
		break;

	case IDM_SELMARGIN:
		margin = !margin;
		SendEditor(SCI_SETMARGINWIDTHN, 1, margin ? marginWidth : 0);
		CheckMenus();
		break;
		
	case IDM_FOLDMARGIN:
		foldMargin = !foldMargin;
		SendEditor(SCI_SETMARGINWIDTHN, 2, foldMargin ? foldMarginWidth : 0);
		CheckMenus();
		break;
	
	case IDM_VIEWEOL:
		SendEditor(SCI_SETVIEWEOL, !SendEditor(SCI_GETVIEWEOL));
		CheckMenus();
		break;

	case IDM_VIEWTOOLBAR:
		tbVisible = !tbVisible;
		ShowToolBar();
		CheckMenus();
		break;

	case IDM_VIEWSTATUSBAR:
		sbVisible = !sbVisible;
		ShowStatusBar();
		UpdateStatusBar();
		CheckMenus();
		break;

	case IDM_EOL_CRLF:
		SendEditor(SCI_SETEOLMODE, SC_EOL_CRLF);
		CheckMenus();
		break;
		
	case IDM_EOL_CR:
		SendEditor(SCI_SETEOLMODE, SC_EOL_CR);
		CheckMenus();
		break;
	case IDM_EOL_LF:
		SendEditor(SCI_SETEOLMODE, SC_EOL_LF);
		CheckMenus();
		break;
	case IDM_EOL_CONVERT:
		SendEditor(SCI_CONVERTEOLS, SendEditor(SCI_GETEOLMODE));
		break;

	case IDM_VIEWSPACE: {
			int viewWS = SendEditor(SCI_GETVIEWWS, 0, 0);
			SendEditor(SCI_SETVIEWWS, !viewWS);
			CheckMenus();
			Redraw();
		}
		break;

	case IDM_COMPILE: {
			if (SaveIfUnsureForBuilt() != IDCANCEL) {
				SelectionIntoProperties();
				AddCommand(props.GetNewExpand("command.compile.", fileName), "",
				           SubsystemType("command.compile.subsystem."));
				if (commandCurrent > 0)
					Execute();
			}
		}
		break;

	case IDM_BUILD: {
			if (SaveIfUnsureForBuilt() != IDCANCEL) {
				SelectionIntoProperties();
				AddCommand(props.GetNewExpand("command.build.", fileName), "",
				           SubsystemType("command.build.subsystem."));
				if (commandCurrent > 0) {
					isBuilding = true;
					Execute();
				}
			}
		}
		break;

	case IDM_GO: {
			if (SaveIfUnsureForBuilt() != IDCANCEL) {
				SelectionIntoProperties();
				bool forceQueue = false;
				if (!isBuilt) {
					SString buildcmd = props.GetNewExpand("command.go.needs.", fileName);
					AddCommand(buildcmd, "",
					           SubsystemType("command.go.needs.subsystem."));
					if (buildcmd.length() > 0) {
						isBuilding = true;
						forceQueue = true;
					}
				}
				AddCommand(props.GetNewExpand("command.go.", fileName), "",
				           SubsystemType("command.go.subsystem."), forceQueue);
				if (commandCurrent > 0)
					Execute();
			}
		}
		break;

	case IDM_STOPEXECUTE:
		StopExecute();
		break;

	case IDM_NEXTMSG:
		GoMessage(1);
		break;

	case IDM_PREVMSG:
		GoMessage( -1);
		break;

	case IDM_OPENLOCALPROPERTIES:
		OpenProperties(IDM_OPENLOCALPROPERTIES);
		SetFocus(wEditor.GetID());
		break;

	case IDM_OPENUSERPROPERTIES:
		OpenProperties(IDM_OPENUSERPROPERTIES);
		SetFocus(wEditor.GetID());
		break;

	case IDM_OPENGLOBALPROPERTIES:
		OpenProperties(IDM_OPENGLOBALPROPERTIES);
		SetFocus(wEditor.GetID());
		break;

	case IDM_SRCWIN:
		break;

	case IDM_BOOKMARK_TOGGLE:
		BookmarkToggle();
		break;

	case IDM_BOOKMARK_NEXT:
		BookmarkNext();
		break;

	default:
		if ((cmdID >= fileStackCmdID) &&
		        (cmdID < fileStackCmdID + fileStackMax)) {
			if (SaveIfUnsure() != IDCANCEL) {
				StackMenu(cmdID - fileStackCmdID);
			}
		} else if (cmdID >= IDM_TOOLS && cmdID < IDM_TOOLS + 10) {
			ToolsMenu(cmdID - IDM_TOOLS);
		}
		break;
	}
}

void SciTEBase::FoldChanged(int line, int levelNow, int levelPrev) {
//Platform::DebugPrintf("Fold %d %x->%x\n", line, levelPrev, levelNow);
	if (levelNow & SC_FOLDLEVELHEADERFLAG) {
		SendEditor(SCI_SETFOLDEXPANDED, line, 1);
	} else if (levelPrev & SC_FOLDLEVELHEADERFLAG) {
		//Platform::DebugPrintf("Fold removed %d-%d\n", line, SendEditor(SCI_GETLASTCHILD, line));
		if (!SendEditor(SCI_GETFOLDEXPANDED, line)) {
			// Removing the fold from one that has been contracted so dhould expand 
			// otherwise lines are left invisibe with no war to make them visible
			Expand(line, true, false, 0, levelPrev);
		}
	}
}

void SciTEBase::Expand(int &line, bool doExpand, bool force, int visLevels, int level) {
	int lineMaxSubord = SendEditor(SCI_GETLASTCHILD, line, level);
	line++;
	while (line<=lineMaxSubord) {
		if (force) {
			if (visLevels > 0)
				SendEditor(SCI_SHOWLINES, line, line);
			else
				SendEditor(SCI_HIDELINES, line, line);
		} else {
			if (doExpand)
				SendEditor(SCI_SHOWLINES, line, line);
		}
		if (level == -1)
			level = SendEditor(SCI_GETFOLDLEVEL, line);
		if (level & SC_FOLDLEVELHEADERFLAG) {
			if (force) {
				if (visLevels > 1)
					SendEditor(SCI_SETFOLDEXPANDED, line, 1);
				else
					SendEditor(SCI_SETFOLDEXPANDED, line, 0);
				Expand(line, doExpand, force, visLevels-1);
			} else {
				if (doExpand && SendEditor(SCI_GETFOLDEXPANDED, line)) {
					Expand(line, true, force, visLevels-1);
				} else {
					Expand(line, false, force, visLevels-1);
				}
			}
		} else {
			line++;
		}
	}
}

void SciTEBase::FoldAll() {
	SendEditor(SCI_COLOURISE, 0, -1);
	//Colourise();
	int maxLine = SendEditor(EM_GETLINECOUNT);
	bool expanding= true;
	for (int lineSeek = 0; lineSeek < maxLine; lineSeek++) {
		if (SendEditor(SCI_GETFOLDLEVEL, lineSeek) & SC_FOLDLEVELHEADERFLAG) {
			expanding = !SendEditor(SCI_GETFOLDEXPANDED, lineSeek);
			break;
		}
	}
	for (int line = 0; line < maxLine; line++) {
		int level = SendEditor(SCI_GETFOLDLEVEL, line);
		if ((level & SC_FOLDLEVELHEADERFLAG) && 
			(SC_FOLDLEVELBASE == (level & SC_FOLDLEVELNUMBERMASK))) {
			if (expanding) {
				SendEditor(SCI_SETFOLDEXPANDED, line, 1);
				Expand(line, true);
				line--;
			} else {
				int lineMaxSubord = SendEditor(SCI_GETLASTCHILD, line, -1);
				SendEditor(SCI_SETFOLDEXPANDED, line, 0);
				if (lineMaxSubord > line)
					SendEditor(SCI_HIDELINES, line+1, lineMaxSubord);
			}
		}
	}
}

void SciTEBase::EnsureRangeVisible(int posStart, int posEnd) {
	int lineStart = SendEditor(EM_LINEFROMCHAR, Platform::Minimum(posStart, posEnd));
	int lineEnd = SendEditor(EM_LINEFROMCHAR, Platform::Maximum(posStart, posEnd));
	for (int line=lineStart; line<= lineEnd; line++) {
		SendEditor(SCI_ENSUREVISIBLE, line);
	}
}

bool SciTEBase::MarginClick(int position, int modifiers) {
	int lineClick = SendEditor(EM_LINEFROMCHAR, position);
	//Platform::DebugPrintf("Margin click %d %d %x\n", position, lineClick, 
	//	SendEditor(SCI_GETFOLDLEVEL, lineClick) & SC_FOLDLEVELHEADERFLAG);
	if ((modifiers & SHIFT_PRESSED) && (modifiers & LEFT_CTRL_PRESSED)) {
		FoldAll();
	} else if (SendEditor(SCI_GETFOLDLEVEL, lineClick) & SC_FOLDLEVELHEADERFLAG) {
		if (modifiers & SHIFT_PRESSED) {
			SendEditor(SCI_SETFOLDEXPANDED, lineClick, 1);
			Expand(lineClick, true, true, 1);
		} else if (modifiers & LEFT_CTRL_PRESSED) {
			if (SendEditor(SCI_GETFOLDEXPANDED, lineClick)) {
				SendEditor(SCI_SETFOLDEXPANDED, lineClick, 0);
				Expand(lineClick, false, true, 0);
			} else {
				SendEditor(SCI_SETFOLDEXPANDED, lineClick, 1);
				Expand(lineClick, true, true, 100);
			}
		} else {
			SendEditor(SCI_TOGGLEFOLD, lineClick);
		}
	}
	return true;
}

void SciTEBase::Notify(SCNotification *notification) {
//Platform::DebugPrintf("Notify %d\n", notification->nmhdr.code);
	switch (notification->nmhdr.code) {
	case EN_SETFOCUS:
		CheckMenus();
		break;
		
	case SCN_STYLENEEDED: {
// Colourisation is now performed by the SciLexer DLL
#ifdef OLD_CODE	
			if (notification->nmhdr.idFrom == IDM_SRCWIN) {
				int endStyled = SendEditor(SCI_GETENDSTYLED);
				int lineEndStyled = SendEditor(EM_LINEFROMCHAR, endStyled);
				endStyled = SendEditor(EM_LINEINDEX, lineEndStyled);
				Colourise(endStyled, notification->position);
			} else {
				int endStyled = SendOutput(SCI_GETENDSTYLED);
				int lineEndStyled = SendOutput(EM_LINEFROMCHAR, endStyled);
				endStyled = SendOutput(EM_LINEINDEX, lineEndStyled);
				Colourise(endStyled, notification->position, false);
			}
#endif
		}
		break;

	case SCN_CHARADDED:
		CharAdded(static_cast<char>(notification->ch));
		break;

	case SCN_SAVEPOINTREACHED:
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			isDirty = false;
			CheckMenus();
			SetWindowName();
		}
		break;

	case SCN_SAVEPOINTLEFT:
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			isDirty = true;
			isBuilt = false;
			CheckMenus();
			SetWindowName();
		}
		break;

	case SCN_DOUBLECLICK:
		if (notification->nmhdr.idFrom == IDM_RUNWIN) {
			//Platform::DebugPrintf("Double click 0\n");
			GoMessage(0);
		}
		break;

	case SCN_UPDATEUI:
		BraceMatch(notification->nmhdr.idFrom == IDM_SRCWIN);
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			UpdateStatusBar();
		}
		break;

	case SCN_MODIFIED:
		if (notification->modificationType == SC_MOD_CHANGEFOLD) {
			FoldChanged(notification->line, 
				notification->foldLevelNow, notification->foldLevelPrev);
		}
		break;
		
	case SCN_MARGINCLICK: {
			if (notification->margin == 2) {
				MarginClick(notification->position, notification->modifiers);
			}
		}
		break;
		
	case SCN_NEEDSHOWN: {
			EnsureRangeVisible(notification->position, notification->position + notification->length);
		}
		break;

	}
}

void SciTEBase::CheckMenus() {
	EnableAMenuItem(IDM_SAVE, isDirty);
	EnableAMenuItem(IDM_UNDO, SendFocused(EM_CANUNDO));
	EnableAMenuItem(IDM_REDO, SendFocused(SCI_CANREDO));
	EnableAMenuItem(IDM_PASTE, SendFocused(EM_CANPASTE));
	EnableAMenuItem(IDM_FINDINFILES, !executing);
	EnableAMenuItem(IDM_COMPLETE, apis != 0);
	CheckAMenuItem(IDM_SPLITVERTICAL, splitVertical);
	CheckAMenuItem(IDM_VIEWSPACE, SendEditor(SCI_GETVIEWWS));
	CheckAMenuItem(IDM_LINENUMBERMARGIN, lineNumbers);
	CheckAMenuItem(IDM_SELMARGIN, margin);
	CheckAMenuItem(IDM_FOLDMARGIN, foldMargin);
	CheckAMenuItem(IDM_VIEWEOL, SendEditor(SCI_GETVIEWEOL));
	CheckAMenuItem(IDM_VIEWTOOLBAR, tbVisible);
	CheckAMenuItem(IDM_VIEWSTATUSBAR, sbVisible);
	EnableAMenuItem(IDM_COMPILE, !executing);
	EnableAMenuItem(IDM_BUILD, !executing);
	EnableAMenuItem(IDM_GO, !executing);
	for (int toolItem = 0; toolItem < toolMax; toolItem++)
		EnableAMenuItem(IDM_TOOLS + toolItem, !executing);
	EnableAMenuItem(IDM_STOPEXECUTE, executing);
}


// Ensure that a splitter bar position is inside the main window
int SciTEBase::NormaliseSplit(int splitPos) {
	PRectangle rcClient = GetClientRectangle();
	int w = rcClient.Width();
	int h = rcClient.Height();
	if (splitPos < 20)
		splitPos = heightBar - heightBar;
	if (splitVertical) {
		if (splitPos > w - heightBar - 20)
			splitPos = w - heightBar;
	} else {
		if (splitPos > h - heightBar - 20)
			splitPos = h - heightBar;
	}
	return splitPos;
}

void SciTEBase::MoveSplit(Point ptNewDrag) {
	int newHeightOutput = heightOutputStartDrag + (ptStartDrag.y - ptNewDrag.y);
	if (splitVertical)
		newHeightOutput = heightOutputStartDrag + (ptStartDrag.x - ptNewDrag.x);
	newHeightOutput = NormaliseSplit(newHeightOutput);
	if (heightOutput != newHeightOutput) {
		heightOutput = newHeightOutput;
		SizeContentWindows();
		//Redraw();
	}
}
