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
#include "ScintillaWidget.h"
#include "Scintilla.h"
#include "SciLexer.h"
#include "Extender.h"
#include "SciTEBase.h"

#define SciTE_MARKER_BOOKMARK 1

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
#if PLAT_GTK
    "Icons Copyright(C) 1998 by Dean S. Jones",
    "    http://jfa.javalobby.org/projects/icons/",
#endif 
    "Ragnar Højland",
    "Christian Obrecht",
    "Andreas Neukoetter",
    "Adam Gates",
    "Steve Lhomme",
    "Ferdinand Prantl",
    "Jan Dries",
    "Markus Gritsch",
    "Tahir Karaca",
    "Ahmad Zawawi",
};

const char *extList[] = {
    "x", "x.cpp", "x.bas", "x.rc", "x.html", "x.xml", "x.js", "x.vbs",
    "x.properties", "x.bat", "x.mak", "x.err", "x.java", "x.lua", "x.py",
    "x.pl", "x.sql", "x.spec", "x.php3", "x.tex", "x.diff", "x.cs", "x.conf"
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
	                        reinterpret_cast<long>(static_cast < char * > (buf)));
}

void SetAboutStyle(WindowID wsci, int style, Colour fore) {
	Platform::SendScintilla(wsci, SCI_STYLESETFORE, style, fore.AsLong());
}

static void HackColour(int &n) {
	n += (rand() % 100) - 50;
	if (n > 0xE7)
		n = 0x60;
	if (n < 0)
		n = 0x80;
}

void SetAboutMessage(WindowID wsci, const char *appTitle) {
	if (wsci) {
		Platform::SendScintilla(wsci, SCI_SETSTYLEBITS, 7, 0);
		Platform::SendScintilla(wsci, SCI_STYLERESETDEFAULT, 0, 0);
		int fontSize = 15;
#if PLAT_GTK
		// On GTK+, new century schoolbook looks better in large sizes than default font
		Platform::SendScintilla(wsci, SCI_STYLESETFONT, STYLE_DEFAULT,
		                        reinterpret_cast<unsigned int>("new century schoolbook"));
		fontSize = 14;
#endif 
		Platform::SendScintilla(wsci, SCI_STYLESETSIZE, STYLE_DEFAULT, fontSize);
		Platform::SendScintilla(wsci, SCI_STYLESETBACK, STYLE_DEFAULT, Colour(0xff, 0xff, 0xff).AsLong());
		Platform::SendScintilla(wsci, SCI_STYLECLEARALL, 0, 0);

		SetAboutStyle(wsci, 0, Colour(0xff, 0xff, 0xff));
		Platform::SendScintilla(wsci, SCI_STYLESETSIZE, 0, fontSize);
		Platform::SendScintilla(wsci, SCI_STYLESETBACK, 0, Colour(0, 0, 0x80).AsLong());
		AddStyledText(wsci, appTitle, 0);
		AddStyledText(wsci, "\n", 0);
		SetAboutStyle(wsci, 1, Colour(0, 0, 0));
		AddStyledText(wsci, "Version 1.33\n", 1);
		SetAboutStyle(wsci, 2, Colour(0, 0, 0));
		Platform::SendScintilla(wsci, SCI_STYLESETITALIC, 2, 1);
		AddStyledText(wsci, "by Neil Hodgson.\n", 2);
		SetAboutStyle(wsci, 3, Colour(0, 0, 0));
		AddStyledText(wsci, "December 1998-November 2000.\n", 3);
		SetAboutStyle(wsci, 4, Colour(0, 0x7f, 0x7f));
		AddStyledText(wsci, "http://www.scintilla.org\n", 4);
		AddStyledText(wsci, "Contributors:\n", 1);
		srand(static_cast<unsigned>(time(0)));
		int r = rand() % 256;
		int g = rand() % 256;
		int b = rand() % 256;
		for (unsigned int co = 0;co < (sizeof(contributors) / sizeof(contributors[0]));co++) {
			HackColour(r);
			HackColour(g);
			HackColour(b);
			SetAboutStyle(wsci, 50 + co, Colour(r, g, b));
			AddStyledText(wsci, "    ", 50 + co);
			AddStyledText(wsci, contributors[co], 50 + co);
			AddStyledText(wsci, "\n", 50 + co);
		}
		Platform::SendScintilla(wsci, SCI_SETREADONLY, 1, 0);
	}
}

SciTEBase::SciTEBase(Extension *ext) : apis(true), extender(ext) {

	codePage = 0;
	characterSet = 0;
	language = "java";
	lexLanguage = SCLEX_CPP;
	functionDefinition = 0;
	indentSize = 8;
	indentOpening = true;
	indentClosing = true;
	statementLookback = 10;

	fnEditor = 0;
	ptrEditor = 0;
	fnOutput = 0;
	ptrOutput = 0;
	tbVisible = false;
	sbVisible = false;
	tabVisible = false;
	tabMultiLine = false;
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
	jobUsesOutputPane = false;
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

	indentationWSVisible = true;

	autoCompleteIgnoreCase = false;
	callTipIgnoreCase = false;

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

	if (extender)
		extender->Initialise(this);
}

SciTEBase::~SciTEBase() {
	if (extender)
		extender->Finalise();
}

long SciTEBase::SendEditor(unsigned int msg, unsigned long wParam, long lParam) {
	return fnEditor(ptrEditor, msg, wParam, lParam);
}

long SciTEBase::SendEditorString(unsigned int msg, unsigned long wParam, const char *s) {
	return SendEditor(msg, wParam, reinterpret_cast<long>(s));
}

long SciTEBase::SendOutput(unsigned int msg, unsigned long wParam, long lParam) {
	return fnOutput(ptrOutput, msg, wParam, lParam);
}

void SciTEBase::SendChildren(unsigned int msg, unsigned long wParam, long lParam) {
	SendEditor(msg, wParam, lParam);
	SendOutput(msg, wParam, lParam);
}

long SciTEBase::SendFocused(unsigned int msg, unsigned long wParam, long lParam) {
	if (wEditor.HasFocus())
		return SendEditor(msg, wParam, lParam);
	else
		return SendOutput(msg, wParam, lParam);
}

long SciTEBase::SendOutputEx(unsigned int msg, unsigned long wParam/*= 0*/, long lParam /*= 0*/, bool direct /*= true*/) {
	if (direct)
		return SendOutput(msg, wParam, lParam);
	return Platform::SendScintilla(wOutput.GetID(), msg, wParam, lParam);
}

void SciTEBase::ViewWhitespace(bool view) {
	if (view && indentationWSVisible)
		SendEditor(SCI_SETVIEWWS, SCWS_VISIBLEALWAYS);
	else if (view)
		SendEditor(SCI_SETVIEWWS, SCWS_VISIBLEAFTERINDENT);
	else
		SendEditor(SCI_SETVIEWWS, SCWS_INVISIBLE);
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

void SciTEBase::AssignKey(int key, int mods, int cmd) {
	SendEditor(SCI_ASSIGNCMDKEY,
	           Platform::LongFromTwoShorts(static_cast<short>(key),
	                                       static_cast<short>(mods)), cmd);
}

void SciTEBase::SetOverrideLanguage(int cmdID) {
	EnsureRangeVisible(0, SendEditor(SCI_GETLENGTH));
	// Zero all the style bytes
	SendEditor(SCI_CLEARDOCUMENTSTYLE);

	overrideExtension = extList[cmdID - LEXER_BASE];
	ReadProperties();
	SendEditor(SCI_COLOURISE, 0, -1);
	Redraw();
}

int SciTEBase::LengthDocument() {
	return SendEditor(SCI_GETLENGTH);
}

int SciTEBase::GetLine(char *text, int sizeText, int line) {
	if (line == -1) {
		return SendEditor(SCI_GETCURLINE, sizeText, reinterpret_cast<long>(text));
	} else {
		short buflen = static_cast<short>(sizeText);
		memcpy(text, &buflen, sizeof(buflen));
		return SendEditor(SCI_GETLINE, line, reinterpret_cast<long>(text));
	}
}

void SciTEBase::GetRange(Window &win, int start, int end, char *text) {
	TextRange tr;
	tr.chrg.cpMin = start;
	tr.chrg.cpMax = end;
	tr.lpstrText = text;
	Platform::SendScintilla(win.GetID(), SCI_GETTEXTRANGE, 0, reinterpret_cast<long>(&tr));
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

// Find if there is a brace next to the caret, checking before caret first, then
// after caret. If brace found also find its matching brace.
// Returns true if inside a bracket pair.
bool SciTEBase::FindMatchingBracePosition(bool editor, int &braceAtCaret, int &braceOpposite, bool sloppy) {
	bool isInside = false;
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
	if (charBefore && strchr("[](){}", charBefore) &&
	        ((styleBefore == bracesStyleCheck) || (!bracesStyle))) {
		braceAtCaret = caretPos - 1;
	}
	bool colonMode = false;
	if (lexLanguage == SCLEX_PYTHON && ':' == charBefore) {
		braceAtCaret = caretPos - 1;
		colonMode = true;
	}
	bool isAfter = true;
	if (sloppy && (braceAtCaret < 0)) {
		// No brace found so check other side
		char charAfter = acc[caretPos];
		char styleAfter = static_cast<char>(acc.StyleAt(caretPos) & 31);
		if (charAfter && strchr("[](){}", charAfter) && (styleAfter == bracesStyleCheck)) {
			braceAtCaret = caretPos;
			isAfter = false;
		}
		if (lexLanguage == SCLEX_PYTHON && ':' == charAfter) {
			braceAtCaret = caretPos;
			colonMode = true;
		}
	}
	if (braceAtCaret >= 0) {
		if (colonMode) {
			int lineStart = Platform::SendScintilla(win.GetID(), SCI_LINEFROMPOSITION, braceAtCaret);
			int lineMaxSubord = Platform::SendScintilla(win.GetID(), SCI_GETLASTCHILD, lineStart, -1);
			braceOpposite = Platform::SendScintilla(win.GetID(), SCI_GETLINEENDPOSITION, lineMaxSubord);
		} else {
			braceOpposite = Platform::SendScintilla(win.GetID(), SCI_BRACEMATCH, braceAtCaret, 0);
		}
		if (braceOpposite > braceAtCaret) {
			isInside = isAfter;
		} else {
			isInside = !isAfter;
		}
	}
	return isInside;
}

void SciTEBase::BraceMatch(bool editor) {
	if (!bracesCheck)
		return ;
	int braceAtCaret = -1;
	int braceOpposite = -1;
	FindMatchingBracePosition(editor, braceAtCaret, braceOpposite, bracesSloppy);
	Window &win = editor ? wEditor : wOutput;
	if ((braceAtCaret != -1) && (braceOpposite == -1)) {
		Platform::SendScintilla(win.GetID(), SCI_BRACEBADLIGHT, braceAtCaret, 0);
		SendEditor(SCI_SETHIGHLIGHTGUIDE, 0);
	} else {
		char chBrace = static_cast<char>(Platform::SendScintilla(
		                                         win.GetID(), SCI_GETCHARAT, braceAtCaret, 0));
		Platform::SendScintilla(win.GetID(), SCI_BRACEHIGHLIGHT, braceAtCaret, braceOpposite);
		int columnAtCaret = Platform::SendScintilla(win.GetID(), SCI_GETCOLUMN, braceAtCaret, 0);
		if (chBrace == ':') {
			int lineStart = Platform::SendScintilla(win.GetID(), SCI_LINEFROMPOSITION, braceAtCaret);
			int indentPos = Platform::SendScintilla(win.GetID(), SCI_GETLINEINDENTPOSITION, lineStart, 0);
			int indentPosNext = Platform::SendScintilla(win.GetID(), SCI_GETLINEINDENTPOSITION, lineStart + 1, 0);
			columnAtCaret = Platform::SendScintilla(win.GetID(), SCI_GETCOLUMN, indentPos, 0);
			int columnAtCaretNext = Platform::SendScintilla(win.GetID(), SCI_GETCOLUMN, indentPosNext, 0);
			indentSize = props.GetInt("indent.size");
			if (columnAtCaretNext - indentSize > 1)
				columnAtCaret = columnAtCaretNext - indentSize;
			//Platform::DebugPrintf(": %d %d %d\n", lineStart, indentPos, columnAtCaret);
		}


		int columnOpposite = Platform::SendScintilla(win.GetID(), SCI_GETCOLUMN, braceOpposite, 0);
		if (props.GetInt("highlight.indentation.guides"))
			Platform::SendScintilla(win.GetID(), SCI_SETHIGHLIGHTGUIDE, Platform::Minimum(columnAtCaret, columnOpposite), 0);
	}
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
	//Platform::DebugPrintf("SetWindowname %s\n", windowName);
}

CharacterRange SciTEBase::GetSelection() {
	CharacterRange crange;
	crange.cpMin = SendEditor(SCI_GETSELECTIONSTART);
	crange.cpMax = SendEditor(SCI_GETSELECTIONEND);
	return crange;
}

void SciTEBase::SetSelection(int anchor, int currentPos) {
	SendEditor(SCI_SETSEL, anchor, currentPos);
}

static bool iswordcharforsel(char ch) {
	return !strchr("\t\n\r !\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", ch);
}

void SciTEBase::SelectionWord(char *word, int len) {
	int lengthDoc = LengthDocument();
	CharacterRange cr = GetSelection();
	int selStart = cr.cpMin;
	int selEnd = cr.cpMax;
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
	word[0] = '\0';
	if ((selStart < selEnd) && ((selEnd - selStart + 1) < len)) {
		GetRange(wEditor, selStart, selEnd, word);
	}
}

void SciTEBase::SelectionIntoProperties() {
	CharacterRange cr = GetSelection();
	char currentSelection[1000];
	if ((cr.cpMin < cr.cpMax) && ((cr.cpMax - cr.cpMin + 1) < static_cast<int>(sizeof(currentSelection)))) {
		GetRange(wEditor, cr.cpMin, cr.cpMax, currentSelection);
		int len = strlen(currentSelection);
		if (len > 2 && iscntrl(currentSelection[len - 1]))
			currentSelection[len - 1] = '\0';
		if (len > 2 && iscntrl(currentSelection[len - 2]))
			currentSelection[len - 2] = '\0';
		props.Set("CurrentSelection", currentSelection);
	}
	char word[200];
	SelectionWord(word, sizeof(word));
	props.Set("CurrentWord", word);
}

void SciTEBase::SelectionIntoFind() {
	SelectionWord(findWhat, sizeof(findWhat));
}

void SciTEBase::FindMessageBox(const char *msg) {
	dialogsOnScreen++;
#if PLAT_GTK
	MessageBox(wSciTE.GetID(), msg, appName, MB_OK | MB_ICONWARNING);
#else
	MessageBox(wFindReplace.GetID(), msg, appName, MB_OK | MB_ICONWARNING);
#endif 
	dialogsOnScreen--;
}

void SciTEBase::FindNext(bool reverseDirection) {
	if (!findWhat[0]) {
		Find();
		return ;
	}
	TextToFind ft = {{0, 0}, 0, {0, 0}};
	CharacterRange crange = GetSelection();
	if (reverseDirection) {
		ft.chrg.cpMin = crange.cpMin - 1;
		ft.chrg.cpMax = 0;
	} else {
		ft.chrg.cpMin = crange.cpMax;
		ft.chrg.cpMax = LengthDocument();
	}
	ft.lpstrText = findWhat;
	ft.chrgText.cpMin = 0;
	ft.chrgText.cpMax = 0;
	int flags = (wholeWord ? SCFIND_WHOLEWORD : 0) | (matchCase ? SCFIND_MATCHCASE : 0);
	//DWORD dwStart = timeGetTime();
	int posFind = SendEditor(SCI_FINDTEXT, flags, reinterpret_cast<long>(&ft));
	//DWORD dwEnd = timeGetTime();
	//Platform::DebugPrintf("<%s> found at %d took %d\n", findWhat, posFind, dwEnd - dwStart);
	if (posFind == -1) {
		// Failed to find in indicated direction so search on other side
		if (reverseDirection) {
			ft.chrg.cpMin = LengthDocument();
			ft.chrg.cpMax = 0;
		} else {
			ft.chrg.cpMin = 0;
			ft.chrg.cpMax = LengthDocument();
		}
		posFind = SendEditor(SCI_FINDTEXT, flags, reinterpret_cast<long>(&ft));
	}
	if (posFind == -1) {
		havefound = false;
		char msg[300];
		strcpy(msg, "Cannot find the string \"");
		strcat(msg, findWhat);
		strcat(msg, "\".");
		if (wFindReplace.Created()) {
			FindMessageBox(msg);
		} else {
			dialogsOnScreen++;
			MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
			dialogsOnScreen--;
		}
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
		SendEditorString(SCI_REPLACESEL, 0, replaceWhat);
		havefound = false;
		//Platform::DebugPrintf("Replace <%s> -> <%s>\n", findWhat, replaceWhat);
	}



	FindNext(reverseFind);
}

void SciTEBase::ReplaceAll() {
	if (findWhat[0] == '\0') {
		FindMessageBox("Find string for Replace All must not be empty.");
		return ;
	}

	TextToFind ft;
	ft.chrg.cpMin = 0;
	ft.chrg.cpMax = LengthDocument();
	ft.lpstrText = findWhat;
	ft.chrgText.cpMin = 0;
	ft.chrgText.cpMax = 0;
	int flags = (wholeWord ? SCFIND_WHOLEWORD : 0) | (matchCase ? SCFIND_MATCHCASE : 0);
	int posFind = SendEditor(SCI_FINDTEXT, flags,
	                         reinterpret_cast<long>(&ft));
	if (posFind != -1) {
		SendEditor(SCI_BEGINUNDOACTION);
		while (posFind != -1) {
			SetSelection(ft.chrgText.cpMin, ft.chrgText.cpMax);
			SendEditorString(SCI_REPLACESEL, 0, replaceWhat);
			ft.chrg.cpMin = posFind + strlen(replaceWhat);
			ft.chrg.cpMax = LengthDocument();
			posFind = SendEditor(SCI_FINDTEXT, flags,
			                     reinterpret_cast<long>(&ft));
		}
		SendEditor(SCI_ENDUNDOACTION);
	} else {
		char msg[200];
		strcpy(msg, "No replacements because string \"");
		strcat(msg, findWhat);
		strcat(msg, "\" was not present.");
		FindMessageBox(msg);
	}
	//Platform::DebugPrintf("ReplaceAll <%s> -> <%s>\n", findWhat, replaceWhat);
}



void SciTEBase::OutputAppendString(const char *s, int len) {
	if (len == -1)
		len = strlen(s);
	SendOutput(SCI_GOTOPOS, SendOutput(SCI_GETTEXTLENGTH));
	SendOutput(SCI_ADDTEXT, len, reinterpret_cast<long>(s));
	SendOutput(SCI_GOTOPOS, SendOutput(SCI_GETTEXTLENGTH));
}

void SciTEBase::OutputAppendStringEx(const char *s, int len /*= -1*/, bool direct /*= true*/) {
	if (direct)
		OutputAppendString(s, len);
	else {
		if (len == -1)
			len = strlen(s);
		SendOutputEx(SCI_GOTOPOS, SendOutputEx(SCI_GETTEXTLENGTH, 0, 0, false));
		SendOutputEx(SCI_ADDTEXT, len, reinterpret_cast<long>(s), false);
		SendOutputEx(SCI_GOTOPOS, SendOutputEx(SCI_GETTEXTLENGTH, 0, 0, false), false);
	}
}

void SciTEBase::Execute() {
	if (clearBeforeExecute) {
		SendOutput(SCI_CLEARALL);
	}

	SendOutput(SCI_MARKERDELETEALL, static_cast<unsigned long>( -1));
	SendEditor(SCI_MARKERDELETEALL, 0);
	// Ensure the output pane is visible
	if (jobUsesOutputPane && heightOutput < 20) {
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
	if (lineno == -1)
		lineno = GetCurrentLineNumber();
	int state = SendEditor(SCI_MARKERGET, lineno);
	if ( state & (1 << SciTE_MARKER_BOOKMARK))
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
	int nextLine = SendEditor(SCI_MARKERNEXT, lineno + 1, 1 << SciTE_MARKER_BOOKMARK);
	if (nextLine < 0)
		nextLine = SendEditor(SCI_MARKERNEXT, 0, 1 << SciTE_MARKER_BOOKMARK);
	if (nextLine < 0 || nextLine == lineno)
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

bool SciTEBase::StartCallTip() {
	//Platform::DebugPrintf("StartCallTip\n");
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));
	int pos = SendEditor(SCI_GETCURRENTPOS);
	int braces;
	do {
		braces = 0;
		while (current > 0 && (braces || linebuf[current - 1] != '(')) {
			if (linebuf[current - 1] == '(')
				braces--;
			else if (linebuf[current - 1] == ')')
				braces++;
			current--;
			pos--;
		}
		if (current > 0) {
			current--;
			pos--;
		} else
			break;
		while (current > 0 && isspace(linebuf[current - 1])) {
			current--;
			pos--;
		}
	} while (current > 0 && nonFuncChar(linebuf[current - 1]));
	if (current <= 0)
		return true;

	int startword = current - 1;
	while (startword > 0 && !nonFuncChar(linebuf[startword - 1]))
		startword--;
	linebuf[current] = '\0';
	int rootlen = current - startword;
	functionDefinition = "";
	//Platform::DebugPrintf("word  is [%s] %d %d %d\n", linebuf + startword, rootlen, pos, pos - rootlen);
	if (apis) {
		const char *word = apis.GetNearestWord (linebuf + startword, rootlen, callTipIgnoreCase);
		if (word) {
			functionDefinition = word;
			SendEditorString(SCI_CALLTIPSHOW, pos - rootlen + 1, word);
			ContinueCallTip();
		}
	}
	return true;
}

static bool IsCallTipSeparator(char ch) {
	return (ch == ',') || (ch == ';');
}

void SciTEBase::ContinueCallTip() {
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));

	int commas = 0;
	for (int i = 0; i < current; i++) {
		if (IsCallTipSeparator(linebuf[i]))
			commas++;
	}

	int startHighlight = 0;
	while (functionDefinition[startHighlight] && functionDefinition[startHighlight] != '(')
		startHighlight++;
	if (functionDefinition[startHighlight] == '(')
		startHighlight++;
	while (functionDefinition[startHighlight] && commas > 0) {
		if (IsCallTipSeparator(functionDefinition[startHighlight]) || functionDefinition[startHighlight] == ')')
			commas--;
		startHighlight++;
	}
	if (IsCallTipSeparator(functionDefinition[startHighlight]) || functionDefinition[startHighlight] == ')')
		startHighlight++;
	int endHighlight = startHighlight;
	if (functionDefinition[endHighlight])
		endHighlight++;
	while (functionDefinition[endHighlight] && !IsCallTipSeparator(functionDefinition[endHighlight]) && functionDefinition[endHighlight] != ')')
		endHighlight++;

	SendEditor(SCI_CALLTIPSETHLT, startHighlight, endHighlight);
}

bool SciTEBase::StartAutoComplete() {
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));

	int startword = current;
	while (startword > 0 && !nonFuncChar(linebuf[startword - 1]))
		startword--;
	if (startword == current)
		return true;
	linebuf[current] = '\0';
	const char *root = linebuf + startword;
	int rootlen = current - startword;
	if (apis) {
		char *words = apis.GetNearestWords(root, rootlen, autoCompleteIgnoreCase);
		if (words) {
			SendEditorString(SCI_AUTOCSHOW, rootlen, words);
			free(words);
		}
	}
	return true;
}

const char *strcasestr(const char *str, const char *pattn) {
	int i;
	int pattn0 = tolower (pattn[0]);

	for (; *str; str++) {
		if (tolower (*str) == pattn0) {
			for (i = 1; tolower (str[i]) == tolower (pattn[i]); i++)
				if (pattn[i] == '\0')
					return str;
			if (pattn[i] == '\0')
				return str;
		}
	}
	return NULL;
}

bool SciTEBase::StartAutoCompleteWord() {
	char linebuf[1000];
	int current = GetLine(linebuf, sizeof(linebuf));

	int startword = current;
	while (startword > 0 && !nonFuncChar(linebuf[startword - 1]))
		startword--;
	if (startword == current)
		return true;
	linebuf[current] = '\0';
	const char *root = linebuf + startword;
	int rootlen = current - startword;
	int doclen = LengthDocument();
	TextToFind ft = {{0, 0}, 0, {0, 0}};
	ft.lpstrText = const_cast<char*>(root);
	ft.chrg.cpMin = 0;
	ft.chrgText.cpMin = 0;
	ft.chrgText.cpMax = 0;
	int flags = SCFIND_WORDSTART | (autoCompleteIgnoreCase ? 0 : SCFIND_MATCHCASE);
	//int flags = (autoCompleteIgnoreCase ? 0 : SCFIND_MATCHCASE);
	int posCurrentWord = SendEditor (SCI_GETCURRENTPOS) - rootlen;
	//DWORD dwStart = timeGetTime();
	int length = 0;	// variables for reallocatable array creation
	int newlength;
#undef WORDCHUNK
#define WORDCHUNK 100
	int size = WORDCHUNK;
	char *words = (char*) malloc(size);
	*words = '\0';
	for (;;) {	// search all the document
		ft.chrg.cpMax = doclen;
		int posFind = SendEditor(SCI_FINDTEXT, flags, reinterpret_cast<long>(&ft));
		if (posFind == -1 || posFind >= doclen)
			break;
		if (posFind == posCurrentWord) {
			ft.chrg.cpMin = posFind + rootlen;
			continue;
		}
		char wordstart[WORDCHUNK];
		GetRange(wEditor, posFind, Platform::Minimum(posFind + WORDCHUNK - 1, doclen), wordstart);
		char *wordend = wordstart + rootlen;
		while (iswordcharforsel(*wordend))
			wordend++;
		*wordend = '\0';
		int wordlen = wordend - wordstart;
		const char *wordbreak = words;
		const char *wordpos;
		for (;;) {	// searches the found word in the storage
			wordpos = strstr (wordbreak, wordstart);
			if (!wordpos)
				break;
			if (wordpos > words && wordpos[ -1] != ' ' ||
			        wordpos[wordlen] && wordpos[wordlen] != ' ')
				wordbreak = wordpos + wordlen;
			else
				break;
		}
		if (!wordpos) {	// add a new entry
			newlength = length + wordlen;
			if (length)
				newlength++;
			if (newlength >= size) {
				do
					size += WORDCHUNK;
				while (size <= newlength);
				words = (char*) realloc (words, size);
			}
			if (length)
				words[length++] = ' ';
			memcpy (words + length, wordstart, wordlen);
			length = newlength;
			words[length] = '\0';
		}
		ft.chrg.cpMin = posFind + wordlen;
	}
	//DWORD dwEnd = timeGetTime();
	//Platform::DebugPrintf("<%s> found %d characters took %d\n", root, length, dwEnd - dwStart);
	if (length) {
		SendEditorString(SCI_AUTOCSHOW, rootlen, words);
	}
	free(words);
	return true;
}

int SciTEBase::GetCurrentLineNumber() {
	CharacterRange crange = GetSelection();
	int selStart = crange.cpMin;
	return SendEditor(SCI_LINEFROMPOSITION, selStart);
}

int SciTEBase::GetCurrentScrollPosition() {
	int lineDisplayTop = SendEditor(SCI_GETFIRSTVISIBLELINE);
	return SendEditor(SCI_DOCLINEFROMVISIBLE, lineDisplayTop);
}

void SciTEBase::UpdateStatusBar() {
	if (sbVisible) {
		SString msg;
		int caretPos = SendEditor(SCI_GETCURRENTPOS);
		int caretLine = SendEditor(SCI_LINEFROMPOSITION, caretPos);
		int caretColumn = SendEditor(SCI_GETCOLUMN, caretPos);
		msg = "Column=";
		msg += SString(caretColumn + 1).c_str();
		msg += "    Line=";
		msg += SString(caretLine + 1).c_str();
		msg += SendEditor(SCI_GETOVERTYPE) ? "    OVR" : "    INS";
		if (sbValue != msg) {
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
	SetSelection(pos, pos);
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
	for (int i = start;i < end;i++) {
		if ((acc[i] != ' ') && (acc[i] != '\t'))
			return false;
	}
	return true;
}

void SciTEBase::GetLinePartsInStyle(int line, int style1, int style2, SString sv[], int len) {
	for (int i = 0; i < len; i++)
		sv[i] = "";
	WindowAccessor acc(wEditor.GetID(), props);
	SString s;
	int part = 0;
	int thisLineStart = SendEditor(SCI_POSITIONFROMLINE, line);
	int nextLineStart = SendEditor(SCI_POSITIONFROMLINE, line + 1);
	for (int pos = thisLineStart; pos < nextLineStart; pos++) {
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
		sv[part] = s;
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
	for (unsigned int i = 0;i < ELEMENTS(controlWords);i++) {
		if (includes(statementIndent, controlWords[i]))
			indentState = 2;
	}
	// Braces override keywords
	SString controlStrings[10];
	GetLinePartsInStyle(line, SCE_C_OPERATOR, -1, controlStrings, ELEMENTS(controlStrings));
	for (unsigned int j = 0;j < ELEMENTS(controlStrings);j++) {
		if (includes(blockEnd, controlStrings[j]))
			indentState = -1;
		if (includes(blockStart, controlStrings[j]))
			indentState = 1;
	}
	return indentState;
}

void SciTEBase::AutomaticIndentation(char ch) {
	CharacterRange crange = GetSelection();
	int selStart = crange.cpMin;
	int curLine = GetCurrentLineNumber();
	int thisLineStart = SendEditor(SCI_POSITIONFROMLINE, curLine);
	int indent = GetLineIndentation(curLine - 1);
	int indentBlock = indent;
	int backLine = curLine - 1;
	int indentState = 0;
	if (statementIndent.IsEmpty() && blockStart.IsEmpty() && blockEnd.IsEmpty())
		indentState = 1;	// Don't bother searching backwards

	int lineLimit = curLine - statementLookback;
	if (lineLimit < 0)
		lineLimit = 0;
	while ((backLine >= lineLimit) && (indentState == 0)) {
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
			if (RangeIsAllWhitespace(thisLineStart, selStart - 1)) {
				int pos = SetLineIndentation(curLine, indentBlock - indentSize);
				// Move caret after '}'
				SetSelection(pos + 1, pos + 1);
			}
		}
	} else if (ch == blockStart.words[0]) {	// Dedent maybe if first on line
		if (!indentOpening) {
			if (RangeIsAllWhitespace(thisLineStart, selStart - 1)) {
				int pos = SetLineIndentation(curLine, indentBlock - indentSize);
				// Move caret after '{'
				SetSelection(pos + 1, pos + 1);
			}
		}
	} else if ((ch == '\r' || ch == '\n') && (selStart == thisLineStart)) {
		SetLineIndentation(curLine, indentBlock);
	}
}

// Upon a character being added, SciTE may decide to perform some action
// such as displaying a completion list.
void SciTEBase::CharAdded(char ch) {
	CharacterRange crange = GetSelection();
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

void SciTEBase::GoMatchingBrace(bool select) {
	int braceAtCaret = -1;
	int braceOpposite = -1;
	bool isInside = FindMatchingBracePosition(true, braceAtCaret, braceOpposite, true);
	// Convert the chracter positions into caret positions based on whether
	// the caret position was inside or outside the braces.
	if (isInside) {
		if (braceOpposite > braceAtCaret) {
			braceAtCaret++;
		} else {
			braceOpposite++;
		}
	} else {    // Outside
		if (braceOpposite > braceAtCaret) {
			braceOpposite++;
		} else {
			braceAtCaret++;
		}
	}
	if (braceOpposite >= 0) {
		EnsureRangeVisible(braceOpposite, braceOpposite);
		if (select) {
			SetSelection(braceAtCaret, braceOpposite);
		} else {
			SetSelection(braceOpposite, braceOpposite);
		}
	}
}

void SciTEBase::AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, bool) {
	if (commandCurrent == 0)
		jobUsesOutputPane = false;
	if (cmd.length()) {
		jobQueue[commandCurrent].command = cmd;
		jobQueue[commandCurrent].directory = dir;
		jobQueue[commandCurrent].jobType = jobType;
		commandCurrent++;
		if ((jobType == jobCLI) || (jobType == jobExtension))
			jobUsesOutputPane = true;
	}
}

int ControlIDOfCommand(unsigned long wParam) {
	return wParam & 0xffff;
}

void SciTEBase::MenuCommand(int cmdID) {
	switch (cmdID) {
	case IDM_NEW:
		// For the New command, the are you sure question is always asked as this gives
		// an opportunity to abandon the edits made to a file when are.you.sure is turned off.


		if (IsBufferAvailable() || (SaveIfUnsure(true) != IDCANCEL)) {
			New();
			ReadProperties();
		}
		break;
	case IDM_OPEN:
		if (IsBufferAvailable() || (SaveIfUnsure() != IDCANCEL)) {
			OpenDialog();
			SetFocus(wEditor.GetID());
		}
		break;
	case IDM_CLOSE:
		if (SaveIfUnsure() != IDCANCEL) {
			Close();
			SetFocus(wEditor.GetID());
		}
		break;
	case IDM_CLOSEALL:
		CloseAllBuffers();
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
	case IDM_SAVEASRTF:
		SaveAsRTF();
		SetFocus(wEditor.GetID());
		break;
	case IDM_REVERT:
		if (SaveIfUnsure() != IDCANCEL) {
			Revert();
			SetFocus(wEditor.GetID());
		}
		break;
	case IDM_PRINT:
		Print(true);
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
		if (IsBufferAvailable()) {
			Prev(); // Use Prev to tabs move left-to-right
			SetFocus(wEditor.GetID());
		} else {
			// Not using buffers - switch to next file on MRU
			if (SaveIfUnsure() != IDCANCEL)
			StackMenuNext();
		}
		break;
	case IDM_PREVFILE:
		if (IsBufferAvailable()) {
			Next(); // Use Next to tabs move right-to-left
			SetFocus(wEditor.GetID());
		} else {
			// Not using buffers - switch to previous file on MRU
			if (SaveIfUnsure() != IDCANCEL)
			StackMenuPrev();
		}
		break;

	case IDM_UNDO:
		SendFocused(SCI_UNDO);
		break;
	case IDM_REDO:
		SendFocused(SCI_REDO);
		break;

	case IDM_CUT:
		SendFocused(SCI_CUT);
		break;
	case IDM_COPY:
		SendFocused(SCI_COPY);
		break;
	case IDM_PASTE:
		SendFocused(SCI_PASTE);
		break;
	case IDM_CLEAR:
		SendFocused(SCI_CLEAR);
		break;
	case IDM_SELECTALL:
		SendFocused(SCI_SELECTALL);
		break;

	case IDM_FIND:
		Find();
		break;

	case IDM_FINDNEXT:
		FindNext(reverseFind);
		break;

	case IDM_FINDNEXTBACK:
		FindNext(!reverseFind);
		break;

	case IDM_FINDNEXTSEL:
		SelectionIntoFind();
		FindNext(reverseFind);
		break;

	case IDM_FINDNEXTBACKSEL:
		SelectionIntoFind();
		FindNext(!reverseFind);
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
		GoMatchingBrace(false);
		break;

	case IDM_SELECTTOBRACE:
		GoMatchingBrace(true);
		break;

	case IDM_SHOWCALLTIP:
		StartCallTip();
		break;

	case IDM_COMPLETE:
		StartAutoComplete();
		break;

	case IDM_COMPLETEWORD:
		StartAutoCompleteWord();
		break;

	case IDM_TOGGLE_FOLDALL:
		FoldAll();
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

	case IDM_VIEWTABBAR:
		tabVisible = !tabVisible;
		ShowTabBar();
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

	case IDM_WORDPARTLEFT:
		SendEditor(SCI_WORDPARTLEFT);
		break;
	case IDM_WORDPARTLEFTEXTEND:
		SendEditor(SCI_WORDPARTLEFTEXTEND);
		break;
	case IDM_WORDPARTRIGHT:
		SendEditor(SCI_WORDPARTRIGHT);
		break;
	case IDM_WORDPARTRIGHTEXTEND:
		SendEditor(SCI_WORDPARTRIGHTEXTEND);
		break;
	
	case IDM_VIEWSPACE:
		ViewWhitespace(!SendEditor(SCI_GETVIEWWS));
		CheckMenus();
		Redraw();
		break;

	case IDM_VIEWGUIDES: {
			int viewIG = SendEditor(SCI_GETINDENTATIONGUIDES, 0, 0);
			SendEditor(SCI_SETINDENTATIONGUIDES, !viewIG);
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

	case IDM_TABSIZE:
		TabSizeDialog();
		break;

	case IDM_LEXER_NONE:
	case IDM_LEXER_CPP:
	case IDM_LEXER_VB:
	case IDM_LEXER_RC:
	case IDM_LEXER_HTML:
	case IDM_LEXER_XML:
	case IDM_LEXER_JS:
	case IDM_LEXER_WSCRIPT:
	case IDM_LEXER_PROPS:
	case IDM_LEXER_BATCH:
	case IDM_LEXER_MAKE:
	case IDM_LEXER_ERRORL:
	case IDM_LEXER_JAVA:
	case IDM_LEXER_LUA:
	case IDM_LEXER_PYTHON:
	case IDM_LEXER_PERL:
	case IDM_LEXER_SQL:
	case IDM_LEXER_PLSQL:
	case IDM_LEXER_PHP:
	case IDM_LEXER_LATEX:
	case IDM_LEXER_DIFF:
	case IDM_LEXER_CS:
	case IDM_LEXER_CONF:
		SetOverrideLanguage(cmdID);
		break;

	case IDM_HELP: {
			SelectionIntoProperties();
			AddCommand(props.GetNewExpand("command.help.", fileName), "",
			           SubsystemType("command.help.subsystem."));
			if (commandCurrent > 0) {
				isBuilding = true;
				Execute();
			}
		}
		break;

	default:
		if ((cmdID >= bufferCmdID) &&
		        (cmdID < bufferCmdID + buffers.size)) {
			SetDocumentAt(cmdID - bufferCmdID);
		} else if ((cmdID >= fileStackCmdID) &&
		           (cmdID < fileStackCmdID + fileStackMax)) {
			if (IsBufferAvailable() || (SaveIfUnsure() != IDCANCEL)) {
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
	while (line <= lineMaxSubord) {
		if (force) {
			if (visLevels > 0)
				SendEditor(SCI_SHOWLINES, line, line);
			else
				SendEditor(SCI_HIDELINES, line, line);
		} else {
			if (doExpand)
				SendEditor(SCI_SHOWLINES, line, line);
		}
		int levelLine = level;
		if (levelLine == -1)
			levelLine = SendEditor(SCI_GETFOLDLEVEL, line);
		if (levelLine & SC_FOLDLEVELHEADERFLAG) {
			if (force) {
				if (visLevels > 1)
					SendEditor(SCI_SETFOLDEXPANDED, line, 1);
				else
					SendEditor(SCI_SETFOLDEXPANDED, line, 0);
				Expand(line, doExpand, force, visLevels - 1);
			} else {
				if (doExpand && SendEditor(SCI_GETFOLDEXPANDED, line)) {
					Expand(line, true, force, visLevels - 1);
				} else {
					Expand(line, false, force, visLevels - 1);
				}
			}
		} else {
			line++;
		}
	}
}

void SciTEBase::FoldAll() {
	SendEditor(SCI_COLOURISE, 0, -1);
	int maxLine = SendEditor(SCI_GETLINECOUNT);
	bool expanding = true;
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
					SendEditor(SCI_HIDELINES, line + 1, lineMaxSubord);
			}
		}
	}
}

void SciTEBase::EnsureRangeVisible(int posStart, int posEnd) {
	int lineStart = SendEditor(SCI_LINEFROMPOSITION, Platform::Minimum(posStart, posEnd));
	int lineEnd = SendEditor(SCI_LINEFROMPOSITION, Platform::Maximum(posStart, posEnd));
	for (int line = lineStart; line <= lineEnd; line++) {
		SendEditor(SCI_ENSUREVISIBLE, line);
	}
}

bool SciTEBase::MarginClick(int position, int modifiers) {
	int lineClick = SendEditor(SCI_LINEFROMPOSITION, position);
	//Platform::DebugPrintf("Margin click %d %d %x\n", position, lineClick,
	//	SendEditor(SCI_GETFOLDLEVEL, lineClick) & SC_FOLDLEVELHEADERFLAG);
	if ((modifiers & SCMOD_SHIFT) && (modifiers & SCMOD_CTRL)) {
		FoldAll();
	} else if (SendEditor(SCI_GETFOLDLEVEL, lineClick) & SC_FOLDLEVELHEADERFLAG) {
		if (modifiers & SCMOD_SHIFT) {
			// Ensure all children visible
			SendEditor(SCI_SETFOLDEXPANDED, lineClick, 1);
			Expand(lineClick, true, true, 100);
		} else if (modifiers & SCMOD_CTRL) {
			if (SendEditor(SCI_GETFOLDEXPANDED, lineClick)) {
				// Contract this line and all children
				SendEditor(SCI_SETFOLDEXPANDED, lineClick, 0);
				Expand(lineClick, false, true, 0);
			} else {
				// Expand this line and all children
				SendEditor(SCI_SETFOLDEXPANDED, lineClick, 1);
				Expand(lineClick, true, true, 100);
			}
		} else {
			// Toggle this line
			SendEditor(SCI_TOGGLEFOLD, lineClick);
		}
	}
	return true;
}

void SciTEBase::Notify(SCNotification *notification) {
	bool handled = false;
	//Platform::DebugPrintf("Notify %d\n", notification->nmhdr.code);
	switch (notification->nmhdr.code) {
	case SCEN_SETFOCUS:
	case SCEN_KILLFOCUS:
		CheckMenus();
		break;

	case SCN_STYLENEEDED: {
			if (extender) {
				// Colourisation may be performed by script
				if ((notification->nmhdr.idFrom == IDM_SRCWIN) && (lexLanguage == SCLEX_CONTAINER)) {
					int endStyled = SendEditor(SCI_GETENDSTYLED);
					int lineEndStyled = SendEditor(SCI_LINEFROMPOSITION, endStyled);
					endStyled = SendEditor(SCI_POSITIONFROMLINE, lineEndStyled);
					WindowAccessor styler(wEditor.GetID(), props);
					int styleStart = 0;
					if (endStyled > 0)
						styleStart = styler.StyleAt(endStyled - 1);
					styler.SetCodePage(codePage);
					extender->OnStyle(endStyled, notification->position - endStyled,
					                  styleStart, &styler);
					styler.Flush();
				}
			}
			// Colourisation is now normally performed by the SciLexer DLL
#ifdef OLD_CODE
			if (notification->nmhdr.idFrom == IDM_SRCWIN) {
				int endStyled = SendEditor(SCI_GETENDSTYLED);
				int lineEndStyled = SendEditor(SCI_LINEFROMPOSITION, endStyled);
				endStyled = SendEditor(SCI_POSITIONFROMLINE, lineEndStyled);
				Colourise(endStyled, notification->position);
			} else {
				int endStyled = SendOutput(SCI_GETENDSTYLED);
				int lineEndStyled = SendOutput(SCI_LINEFROMPOSITION, endStyled);
				endStyled = SendOutput(SCI_POSITIONFROMLINE, lineEndStyled);
				Colourise(endStyled, notification->position, false);
			}
#endif 
		}
		break;

	case SCN_CHARADDED:
		if (extender)
			handled = extender->OnChar(static_cast<char>(notification->ch));
		if (!handled)
			CharAdded(static_cast<char>(notification->ch));
		break;

	case SCN_SAVEPOINTREACHED:
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			if (extender)
				handled = extender->OnSavePointReached();
			if (!handled) {
				isDirty = false;
				CheckMenus();
				SetWindowName();
				BuffersMenu();
			}
		}
		break;

	case SCN_SAVEPOINTLEFT:
		if (notification->nmhdr.idFrom == IDM_SRCWIN) {
			if (extender)
				handled = extender->OnSavePointLeft();
			if (!handled) {
				isDirty = true;
				isBuilt = false;
				CheckMenus();
				SetWindowName();
				BuffersMenu();
			}
		}
		break;

	case SCN_DOUBLECLICK:
		if (extender)
			handled = extender->OnDoubleClick();
		if (!handled && notification->nmhdr.idFrom == IDM_RUNWIN) {
			//Platform::DebugPrintf("Double click 0\n");
			GoMessage(0);
		}
		break;

	case SCN_UPDATEUI:
		if (extender)
			handled = extender->OnUpdateUI();
		if (!handled) {
			BraceMatch(notification->nmhdr.idFrom == IDM_SRCWIN);
			if (notification->nmhdr.idFrom == IDM_SRCWIN) {
				UpdateStatusBar();
			}
		}
		break;

	case SCN_MODIFIED:
		if (notification->modificationType == SC_MOD_CHANGEFOLD) {
			FoldChanged(notification->line,
			            notification->foldLevelNow, notification->foldLevelPrev);
		}
		break;

	case SCN_MARGINCLICK: {
			if (extender)
				handled = extender->OnMarginClick();
			if (!handled) {
				if (notification->margin == 2) {
					MarginClick(notification->position, notification->modifiers);
				}
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
	EnableAMenuItem(IDM_UNDO, SendFocused(SCI_CANUNDO));
	EnableAMenuItem(IDM_REDO, SendFocused(SCI_CANREDO));
	EnableAMenuItem(IDM_PASTE, SendFocused(SCI_CANPASTE));
	EnableAMenuItem(IDM_FINDINFILES, !executing);
	EnableAMenuItem(IDM_SHOWCALLTIP, apis != 0);
	EnableAMenuItem(IDM_COMPLETE, apis != 0);
	CheckAMenuItem(IDM_SPLITVERTICAL, splitVertical);
	CheckAMenuItem(IDM_VIEWSPACE, SendEditor(SCI_GETVIEWWS));
	CheckAMenuItem(IDM_VIEWGUIDES, SendEditor(SCI_GETINDENTATIONGUIDES));
	CheckAMenuItem(IDM_LINENUMBERMARGIN, lineNumbers);
	CheckAMenuItem(IDM_SELMARGIN, margin);
	CheckAMenuItem(IDM_FOLDMARGIN, foldMargin);
	CheckAMenuItem(IDM_VIEWEOL, SendEditor(SCI_GETVIEWEOL));
	CheckAMenuItem(IDM_VIEWTOOLBAR, tbVisible);
	CheckAMenuItem(IDM_VIEWTABBAR, tabVisible);
	CheckAMenuItem(IDM_VIEWSTATUSBAR, sbVisible);
	EnableAMenuItem(IDM_COMPILE, !executing);
	EnableAMenuItem(IDM_BUILD, !executing);
	EnableAMenuItem(IDM_GO, !executing);
	for (int toolItem = 0; toolItem < toolMax; toolItem++)
		EnableAMenuItem(IDM_TOOLS + toolItem, !executing);
	EnableAMenuItem(IDM_STOPEXECUTE, executing);
	if (buffers.size > 0) {
#if PLAT_WIN
		// Tab Bar
#ifndef TCM_DESELECTALL
#define TCM_DESELECTALL TCM_FIRST+50
#endif
		::SendMessage(wTabBar.GetID(), TCM_DESELECTALL, (WPARAM)0, (LPARAM)0);
		::SendMessage(wTabBar.GetID(), TCM_SETCURSEL, (WPARAM)buffers.current, (LPARAM)0);
#endif
		for (int bufferItem = 0; bufferItem < buffers.length; bufferItem++) {
			CheckAMenuItem(IDM_BUFFER + bufferItem, bufferItem == buffers.current);
		}
	}
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

// Implement ExtensionAPI methods
int SciTEBase::Send(Pane p, unsigned int msg, unsigned long wParam, long lParam) {
	if (p == paneEditor)
		return SendEditor(msg, wParam, lParam);
	else
		return SendOutput(msg, wParam, lParam);
}

char *SciTEBase::Range(Pane p, int start, int end) {
	int len = end - start;
	char *s = new char[len + 1];
	if (s) {
		if (p == paneEditor)
			GetRange(wEditor, start, end, s);
		else
			GetRange(wOutput, start, end, s);
	}
	return s;
}

void SciTEBase::Remove(Pane p, int start, int end) {
	// Should have a scintilla call for this
	if (p == paneEditor) {
		SendEditor(SCI_SETSEL, start, end);
		SendEditor(SCI_CLEAR);
	} else {
		SendOutput(SCI_SETSEL, start, end);
		SendOutput(SCI_CLEAR);
	}
}

void SciTEBase::Insert(Pane p, int pos, const char *s) {
	if (p == paneEditor)
		SendEditorString(SCI_INSERTTEXT, pos, s);
	else
		SendOutput(SCI_INSERTTEXT, pos, reinterpret_cast<long>(s));
}

void SciTEBase::Trace(const char *s) {
	OutputAppendString(s);
}

char *SciTEBase::Property(const char *key) {
	SString value = props.GetExpanded(key);
	char *retval = new char[value.length() + 1];
	if (retval)
		strcpy(retval, value.c_str());
	return retval;
}
