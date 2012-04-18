// SciTE - Scintilla based Text Editor
/** @file SciTEWin.cxx
 ** Main code for the Windows version of the editor.
 **/
// Copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <time.h>

#include "SciTEWin.h"

#if defined(DTBG_CLIPRECT) && !defined(DISABLE_THEMES)
#define THEME_AVAILABLE
#endif

// Since Vsstyle.h and Vssym32.h are not available from all compilers just define the used symbols
#define CBS_NORMAL 1
#define CBS_HOT 2
#define CBS_PUSHED 3
#define WP_SMALLCLOSEBUTTON 19
#define TS_NORMAL 1
#define TS_HOT 2
#define TS_PRESSED 3
#define TS_CHECKED 5
#define TS_HOTCHECKED 6
#define TP_BUTTON 1

#ifndef WM_UPDATEUISTATE
#define WM_UPDATEUISTATE 0x0128
#endif

#ifndef UISF_HIDEACCEL
#define UISF_HIDEACCEL 2
#define UISF_HIDEFOCUS 1
#define UIS_CLEAR 2
#define UIS_SET 1
#endif

#ifndef NO_EXTENSIONS
#include "MultiplexExtension.h"

#ifndef NO_FILER
#include "DirectorExtension.h"
#endif

#ifndef NO_LUA
#include "LuaExtension.h"
#endif

#endif

#ifdef STATIC_BUILD
const GUI::gui_char appName[] = GUI_TEXT("Sc1");
#else
const GUI::gui_char appName[] = GUI_TEXT("SciTE");
#endif

static GUI::gui_string GetErrorMessage(DWORD nRet) {
	LPWSTR lpMsgBuf = NULL;
	::FormatMessage(
	    FORMAT_MESSAGE_ALLOCATE_BUFFER |
	    FORMAT_MESSAGE_FROM_SYSTEM |
	    FORMAT_MESSAGE_IGNORE_INSERTS,
	    NULL,
	    nRet,
	    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),   // Default language
	    reinterpret_cast<LPWSTR>(&lpMsgBuf),
	    0,
	    NULL
	);
	GUI::gui_string s= lpMsgBuf;
	::LocalFree(lpMsgBuf);
	return s;
}

long SciTEKeys::ParseKeyCode(const char *mnemonic) {
	int modsInKey = 0;
	int keyval = -1;

	if (mnemonic && *mnemonic) {
		SString sKey = mnemonic;

		if (sKey.contains("Ctrl+")) {
			modsInKey |= SCMOD_CTRL;
			sKey.remove("Ctrl+");
		}
		if (sKey.contains("Shift+")) {
			modsInKey |= SCMOD_SHIFT;
			sKey.remove("Shift+");
		}
		if (sKey.contains("Alt+")) {
			modsInKey |= SCMOD_ALT;
			sKey.remove("Alt+");
		}

		if (sKey.length() == 1) {
			keyval = VkKeyScan(sKey[0]) & 0xFF;
		} else if (sKey.length() > 1) {
			if ((sKey[0] == 'F') && (isdigit(sKey[1]))) {
				sKey.remove("F");
				int fkeyNum = sKey.value();
				if (fkeyNum >= 1 && fkeyNum <= 12)
					keyval = fkeyNum - 1 + VK_F1;
			} else if ((sKey[0] == 'V') && (isdigit(sKey[1]))) {
				sKey.remove("V");
				int vkey = sKey.value();
				if (vkey > 0 && vkey <= 0x7FFF)
					keyval = vkey;
			} else if (sKey.search("Keypad") == 0) {
				sKey.remove("Keypad");
				if (isdigit(sKey[0])) {
					int keyNum = sKey.value();
					if (keyNum >= 0 && keyNum <= 9)
						keyval = keyNum + VK_NUMPAD0;
				} else if (sKey == "Plus") {
					keyval = VK_ADD;
				} else if (sKey == "Minus") {
					keyval = VK_SUBTRACT;
				} else if (sKey == "Decimal") {
					keyval = VK_DECIMAL;
				} else if (sKey == "Divide") {
					keyval = VK_DIVIDE;
				} else if (sKey == "Multiply") {
					keyval = VK_MULTIPLY;
				}
			} else if (sKey == "Left") {
				keyval = VK_LEFT;
			} else if (sKey == "Right") {
				keyval = VK_RIGHT;
			} else if (sKey == "Up") {
				keyval = VK_UP;
			} else if (sKey == "Down") {
				keyval = VK_DOWN;
			} else if (sKey == "Insert") {
				keyval = VK_INSERT;
			} else if (sKey == "End") {
				keyval = VK_END;
			} else if (sKey == "Home") {
				keyval = VK_HOME;
			} else if (sKey == "Enter") {
				keyval = VK_RETURN;
			} else if (sKey == "Space") {
				keyval = VK_SPACE;
			} else if (sKey == "Tab") {
				keyval = VK_TAB;
			} else if (sKey == "Escape") {
				keyval = VK_ESCAPE;
			} else if (sKey == "Delete") {
				keyval = VK_DELETE;
			} else if (sKey == "PageUp") {
				keyval = VK_PRIOR;
			} else if (sKey == "PageDown") {
				keyval = VK_NEXT;
			} else if (sKey == "Win") {
				keyval = VK_LWIN;
			} else if (sKey == "Menu") {
				keyval = VK_APPS;
			}
		}
	}

	return (keyval > 0) ? (keyval | (modsInKey<<16)) : 0;
}

bool SciTEKeys::MatchKeyCode(long parsedKeyCode, int keyval, int modifiers) {
	// TODO: are the 0x11 and 0x10 special cases needed, or are they
	// just short-circuits?  If not needed, this test could removed.
	if (keyval == 0x11 || keyval == 0x10)
		return false;
	return parsedKeyCode && !(0xFFFF0000 & (keyval | modifiers)) && (parsedKeyCode == (keyval | (modifiers<<16)));
}

HINSTANCE SciTEWin::hInstance = 0;
const TCHAR *SciTEWin::className = NULL;
const TCHAR *SciTEWin::classNameInternal = NULL;
SciTEWin *SciTEWin::app = NULL;

SciTEWin::SciTEWin(Extension *ext) : SciTEBase(ext) {
	app = this;

	contents.SetSciTE(this);
	contents.SetLocalizer(&localiser);
	backgroundStrip.SetLocalizer(&localiser);
	searchStrip.SetLocalizer(&localiser);
	searchStrip.SetSearcher(this);
	findStrip.SetLocalizer(&localiser);
	findStrip.SetSearcher(this);
	replaceStrip.SetLocalizer(&localiser);
	replaceStrip.SetSearcher(this);

	cmdShow = 0;
	heightBar = 7;
	fontTabs = 0;
	wFocus = 0;

	winPlace.length = 0;

	openWhat[0] = '\0';
	modalParameters = false;
	filterDefault = 1;
	menuSource = 0;

	hWriteSubProcess = NULL;
	subProcessGroupId = 0;

	// Read properties resource into propsEmbed
	// The embedded properties file is to allow distributions to be sure
	// that they have a sensible default configuration even if the properties
	// files are missing. Also allows a one file distribution of Sc1.EXE.
	propsEmbed.Clear();
	// System type properties are also stored in the embedded properties.
	propsEmbed.Set("PLAT_WIN", "1");
	propsEmbed.Set("PLAT_WINNT", "1");

	HRSRC handProps = ::FindResource(hInstance, TEXT("Embedded"), TEXT("Properties"));
	if (handProps) {
		DWORD size = ::SizeofResource(hInstance, handProps);
		HGLOBAL hmem = ::LoadResource(hInstance, handProps);
		if (hmem) {
			const void *pv = ::LockResource(hmem);
			if (pv) {
				propsEmbed.ReadFromMemory(
				    reinterpret_cast<const char *>(pv), size, FilePath(), filter);
			}
		}
		::FreeResource(handProps);
	}

	pathAbbreviations = GetAbbrevPropertiesFileName();

	ReadGlobalPropFile();
	/// Need to copy properties to variables before setting up window
	SetPropertiesInitial();
	ReadAbbrevPropFile();

	hDevMode = 0;
	hDevNames = 0;
	::ZeroMemory(&pagesetupMargin, sizeof(pagesetupMargin));

	hHH = 0;
	hMM = 0;
	uniqueInstance.Init(this);

	hAccTable = ::LoadAccelerators(hInstance, TEXT("ACCELS")); // md

	cmdWorker.pSciTE = this;
}

SciTEWin::~SciTEWin() {
	if (hDevMode)
		::GlobalFree(hDevMode);
	if (hDevNames)
		::GlobalFree(hDevNames);
	if (hHH)
		::FreeLibrary(hHH);
	if (hMM)
		::FreeLibrary(hMM);
	if (fontTabs)
		::DeleteObject(fontTabs);
}

uptr_t SciTEWin::GetInstance() {
	return reinterpret_cast<uptr_t>(hInstance);
}

void SciTEWin::Register(HINSTANCE hInstance_) {
	const TCHAR resourceName[] = TEXT("SciTE");

	hInstance = hInstance_;

	WNDCLASS wndclass;

	// Register the frame window
	className = TEXT("SciTEWindow");
	wndclass.style = 0;
	wndclass.lpfnWndProc = SciTEWin::TWndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = sizeof(SciTEWin*);
	wndclass.hInstance = hInstance;
	wndclass.hIcon = ::LoadIcon(hInstance, resourceName);
	wndclass.hCursor = NULL;
	wndclass.hbrBackground = NULL;
	wndclass.lpszMenuName = resourceName;
	wndclass.lpszClassName = className;
	if (!::RegisterClass(&wndclass))
		exit(FALSE);

	// Register the window that holds the two Scintilla edit windows and the separator
	classNameInternal = TEXT("SciTEWindowContent");
	wndclass.lpfnWndProc = BaseWin::StWndProc;
	wndclass.lpszMenuName = 0;
	wndclass.lpszClassName = classNameInternal;
	if (!::RegisterClass(&wndclass))
		exit(FALSE);
}

static int CodePageFromName(const SString &encodingName) {
	struct Encoding {
		const char *name;
		int codePage;
	} knownEncodings[] = {
		{ "ascii", CP_UTF8 },
		{ "utf-8", CP_UTF8 },
		{ "latin1", 1252 },
		{ "latin2", 28592 },
		{ "big5", 950 },
		{ "gbk", 936 },
		{ "shift_jis", 932 },
		{ "euc-kr", 949 },
		{ "cyrillic", 1251 },
		{ "iso-8859-5", 28595 },
		{ "iso8859-11", 874 },
		{ "1250", 1250 },
		{ "windows-1251", 1251 },
		{ 0, 0 },
	};
	for (Encoding *enc=knownEncodings; enc->name; enc++) {
		if (encodingName == enc->name) {
			return enc->codePage;
		}
	}
	return CP_UTF8;
}

// Convert to UTF-8
static std::string ConvertEncoding(const char *original, int codePage) {
	if (codePage == CP_UTF8) {
		return original;
	} else {
		int cchWide = ::MultiByteToWideChar(codePage, 0, original, -1, NULL, 0);
		wchar_t *pszWide = new wchar_t[cchWide + 1];
		::MultiByteToWideChar(codePage, 0, original, -1, pszWide, cchWide + 1);
		GUI::gui_string sWide(pszWide);
		std::string ret = GUI::UTF8FromString(sWide);
		delete []pszWide;
		return ret;
	}
}

void SciTEWin::ReadLocalization() {
	SciTEBase::ReadLocalization();
	SString encoding = localiser.Get("translation.encoding");
	encoding.lowercase();
	if (encoding.length()) {
		int codePage = CodePageFromName(encoding);
		const char *key = NULL;
		const char *val = NULL;
		// Get encoding
		bool more = localiser.GetFirst(key, val);
		while (more) {
			std::string converted = ConvertEncoding(val, codePage);
			if (converted != "") {
				localiser.Set(key, converted.c_str());
			}
			more = localiser.GetNext(key, val);
		}
	}
}

void SciTEWin::GetWindowPosition(int *left, int *top, int *width, int *height, int *maximize) {
	winPlace.length = sizeof(winPlace);
	::GetWindowPlacement(MainHWND(), &winPlace);

	*left = winPlace.rcNormalPosition.left;
	*top = winPlace.rcNormalPosition.top;
	*width =  winPlace.rcNormalPosition.right - winPlace.rcNormalPosition.left;
	*height = winPlace.rcNormalPosition.bottom - winPlace.rcNormalPosition.top;
	*maximize = (winPlace.showCmd == SW_MAXIMIZE) ? 1 : 0;
}

// Allow UTF-8 file names and command lines to be used in calls to io.open and io.popen in Lua scripts.
// The scite_lua_win.h header redirects fopen and _popen to these functions.

extern "C" {

FILE *scite_lua_fopen(const char *filename, const char *mode) {
	GUI::gui_string sFilename = GUI::StringFromUTF8(filename);
	GUI::gui_string sMode = GUI::StringFromUTF8(mode);
	FILE *f = _wfopen(sFilename.c_str(), sMode.c_str());
	if (f == NULL)
		// Fallback on narrow string in case already in CP_ACP
		f = fopen(filename, mode);
	return f;
}

FILE *scite_lua_popen(const char *filename, const char *mode) {
	GUI::gui_string sFilename = GUI::StringFromUTF8(filename);
	GUI::gui_string sMode = GUI::StringFromUTF8(mode);
	FILE *f = _wpopen(sFilename.c_str(), sMode.c_str());
	if (f == NULL)
		// Fallback on narrow string in case already in CP_ACP
		f = _popen(filename, mode);
	return f;
}

}

void SciTEWin::ReadProperties() {
	SciTEBase::ReadProperties();
}

static FilePath GetSciTEPath(FilePath home) {
	if (home.IsSet()) {
		return FilePath(home);
	} else {
		GUI::gui_char path[MAX_PATH];
		::GetModuleFileNameW(0, path, ELEMENTS(path));
		// Remove the SciTE.exe
		GUI::gui_char *lastSlash = wcsrchr(path, pathSepChar);
		if (lastSlash)
			*lastSlash = '\0';
		return FilePath(path);
	}
}

FilePath SciTEWin::GetDefaultDirectory() {
	GUI::gui_char *home = _wgetenv(GUI_TEXT("SciTE_HOME"));
	return GetSciTEPath(home);
}

FilePath SciTEWin::GetSciteDefaultHome() {
	GUI::gui_char *home = _wgetenv(GUI_TEXT("SciTE_HOME"));
	return GetSciTEPath(home);
}

FilePath SciTEWin::GetSciteUserHome() {
	GUI::gui_char *home = _wgetenv(GUI_TEXT("SciTE_HOME"));
	if (!home)
		home = _wgetenv(GUI_TEXT("USERPROFILE"));
	return GetSciTEPath(home);
}

// Help command lines contain topic!path
void SciTEWin::ExecuteOtherHelp(const char *cmd) {
	GUI::gui_string s = GUI::StringFromUTF8(cmd);
	size_t pos = s.find_first_of('!');
	if (pos != GUI::gui_string::npos) {
		GUI::gui_string topic = s.substr(0, pos);
		GUI::gui_string path = s.substr(pos+1);
		::WinHelpW(MainHWND(),
			path.c_str(),
			HELP_KEY,
			reinterpret_cast<ULONG_PTR>(topic.c_str()));
 	}
}

// HH_AKLINK not in mingw headers
struct XHH_AKLINK {
	long cbStruct;
	BOOL fReserved;
	const wchar_t *pszKeywords;
	wchar_t *pszUrl;
	wchar_t *pszMsgText;
	wchar_t *pszMsgTitle;
	wchar_t *pszWindow;
	BOOL fIndexOnFail;
};

// Help command lines contain topic!path
void SciTEWin::ExecuteHelp(const char *cmd) {
	if (!hHH)
		hHH = ::LoadLibrary(TEXT("HHCTRL.OCX"));

	if (hHH) {
		GUI::gui_string s = GUI::StringFromUTF8(cmd);
		size_t pos = s.find_first_of('!');
		if (pos != GUI::gui_string::npos) {
			GUI::gui_string topic = s.substr(0, pos);
			GUI::gui_string path = s.substr(pos + 1);
			typedef HWND (WINAPI *HelpFn) (HWND, const wchar_t *, UINT, DWORD_PTR);
			HelpFn fnHHW = (HelpFn)::GetProcAddress(hHH, "HtmlHelpW");
			if (fnHHW) {
				XHH_AKLINK ak;
				ak.cbStruct = sizeof(ak);
				ak.fReserved = FALSE;
				ak.pszKeywords = topic.c_str();
				ak.pszUrl = NULL;
				ak.pszMsgText = NULL;
				ak.pszMsgTitle = NULL;
				ak.pszWindow = NULL;
				ak.fIndexOnFail = TRUE;
				fnHHW(NULL,
				      path.c_str(),
				      0x000d,          	// HH_KEYWORD_LOOKUP
				      reinterpret_cast<DWORD_PTR>(&ak)
				     );
			}
		}
	}
}

void SciTEWin::CopyAsRTF() {
	Sci_CharacterRange cr = GetSelection();
	char *fileNameTemp = _tempnam(NULL, "scite-tmp-");
	if (fileNameTemp) {
		SaveToRTF(GUI::StringFromUTF8(fileNameTemp), cr.cpMin, cr.cpMax);
		FILE *fp = fopen(fileNameTemp, "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			int len = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			HGLOBAL hand = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, len + 1);
			if (hand) {
				::OpenClipboard(MainHWND());
				::EmptyClipboard();
				char *ptr = static_cast<char *>(::GlobalLock(hand));
				fread(ptr, 1, len, fp);
				ptr[len] = '\0';
				::GlobalUnlock(hand);
				::SetClipboardData(::RegisterClipboardFormat(CF_RTF), hand);
				::CloseClipboard();
			}
			fclose(fp);
		}
		unlink(fileNameTemp);
		free(fileNameTemp);
	}
}

void SciTEWin::CopyPath() {
	if (filePath.IsUntitled())
		return;

	GUI::gui_string clipText(filePath.AsInternal());
	size_t blobSize = sizeof(GUI::gui_char)*(clipText.length()+1);
	HGLOBAL hand = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, blobSize);
	if (hand && ::OpenClipboard(MainHWND())) {
		::EmptyClipboard();
		GUI::gui_char *ptr = static_cast<GUI::gui_char*>(::GlobalLock(hand));
		memcpy(ptr, clipText.c_str(), blobSize);
		::GlobalUnlock(hand);
		::SetClipboardData(CF_UNICODETEXT, hand);
		::CloseClipboard();
	}
}

void SciTEWin::FullScreenToggle() {
	HWND wTaskBar = FindWindow(TEXT("Shell_TrayWnd"), TEXT(""));
	HWND wStartButton = FindWindow(TEXT("Button"), NULL);
	fullScreen = !fullScreen;
	if (fullScreen) {
		::SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
		::SystemParametersInfo(SPI_SETWORKAREA, 0, 0, SPIF_SENDCHANGE);
		if (wStartButton != NULL)
			::ShowWindow(wStartButton, SW_HIDE);
		::ShowWindow(wTaskBar, SW_HIDE);

		winPlace.length = sizeof(winPlace);
		::GetWindowPlacement(MainHWND(), &winPlace);
		int topStuff = ::GetSystemMetrics(SM_CYSIZEFRAME) + ::GetSystemMetrics(SM_CYCAPTION);
		if (props.GetInt("full.screen.hides.menu"))
			topStuff += ::GetSystemMetrics(SM_CYMENU);
		::SetWindowLongPtr(HwndOf(wContent),
			GWL_EXSTYLE, 0);
		::SetWindowPos(MainHWND(), HWND_TOP,
			-::GetSystemMetrics(SM_CXSIZEFRAME),
			-topStuff,
			::GetSystemMetrics(SM_CXSCREEN) + 2 * ::GetSystemMetrics(SM_CXSIZEFRAME),
			::GetSystemMetrics(SM_CYSCREEN) + topStuff + ::GetSystemMetrics(SM_CYSIZEFRAME),
			0);
	} else {
		::ShowWindow(wTaskBar, SW_SHOW);
		if (wStartButton != NULL)
			::ShowWindow(wStartButton, SW_SHOW);
		::SetWindowLongPtr(HwndOf(wContent),
			GWL_EXSTYLE, WS_EX_CLIENTEDGE);
		if (winPlace.length) {
			::SystemParametersInfo(SPI_SETWORKAREA, 0, &rcWorkArea, 0);
			if (winPlace.showCmd == SW_SHOWMAXIMIZED) {
				::ShowWindow(MainHWND(), SW_RESTORE);
				::ShowWindow(MainHWND(), SW_SHOWMAXIMIZED);
			} else {
				::SetWindowPlacement(MainHWND(), &winPlace);
			}
		}
	}
	::SetForegroundWindow(MainHWND());
	CheckMenus();
}

HWND SciTEWin::MainHWND() {
	return HwndOf(wSciTE);
}

void SciTEWin::Command(WPARAM wParam, LPARAM lParam) {
	int cmdID = ControlIDOfWParam(wParam);
	if (wParam & 0x10000) {
		// From accelerator -> goes to focused pane.
		menuSource = 0;
	}
	if (reinterpret_cast<HWND>(lParam) == wToolBar.GetID()) {
		// From toolbar -> goes to focused pane.
		menuSource = 0;
	}

	switch (cmdID) {

	case IDM_SRCWIN:
	case IDM_RUNWIN:
		if (HIWORD(wParam) == SCEN_SETFOCUS) {
			wFocus = reinterpret_cast<HWND>(lParam);
			CheckMenus();
		}
		if (HIWORD(wParam) == SCEN_KILLFOCUS) {
			CheckMenus();
		}
		break;

	case IDM_ACTIVATE:
		Activate(lParam);
		break;

	case IDM_FINISHEDEXECUTE: {
			jobQueue.SetExecuting(false);
			if (needReadProperties)
				ReadProperties();
			CheckMenus();
			jobQueue.ClearJobs();
			CheckReload();
		}
		break;

	case IDM_ONTOP:
		topMost = (topMost ? false : true);
		::SetWindowPos(MainHWND(), (topMost ? HWND_TOPMOST : HWND_NOTOPMOST ), 0, 0, 0, 0, SWP_NOMOVE + SWP_NOSIZE);
		CheckAMenuItem(IDM_ONTOP, topMost);
		break;

	case IDM_OPENFILESHERE:
		uniqueInstance.ToggleOpenFilesHere();
		break;

	case IDM_FULLSCREEN:
		FullScreenToggle();
		break;

	case IDC_TABCLOSE:
		CloseTab((int)lParam);
		break;

	case IDC_SHIFTTAB:
		ShiftTab(LOWORD(lParam), HIWORD(lParam));
		break;

	default:
		SciTEBase::MenuCommand(cmdID, menuSource);
	}
}

// from ScintillaWin.cxx
static UINT CodePageFromCharSet(DWORD characterSet, UINT documentCodePage) {
	CHARSETINFO ci = { 0, 0, { { 0, 0, 0, 0 }, { 0, 0 } } };
	BOOL bci = ::TranslateCharsetInfo((DWORD*)characterSet,
	                                  &ci, TCI_SRCCHARSET);

	UINT cp;
	if (bci)
		cp = ci.ciACP;
	else
		cp = documentCodePage;

	CPINFO cpi;
	if (!::IsValidCodePage(cp) && !::GetCPInfo(cp, &cpi))
		cp = CP_ACP;

	return cp;
}

void SciTEWin::OutputAppendEncodedStringSynchronised(GUI::gui_string s, int codePage) {
	int cchMulti = ::WideCharToMultiByte(codePage, 0, s.c_str(), static_cast<int>(s.size()), NULL, 0, NULL, NULL);
	char *pszMulti = new char[cchMulti + 1];
	::WideCharToMultiByte(codePage, 0, s.c_str(), static_cast<int>(s.size()), pszMulti, cchMulti + 1, NULL, NULL);
	pszMulti[cchMulti] = 0;
	OutputAppendStringSynchronised(pszMulti);
	delete []pszMulti;
}

CommandWorker::CommandWorker() : pSciTE(NULL) {
	Initialise(true);
}

void CommandWorker::Initialise(bool resetToStart) {
	if (resetToStart)
		icmd = 0;
    originalEnd = 0;
    exitStatus = 0;
    flags = 0;
	seenOutput = false;
	outputScroll = 1;
}

void CommandWorker::Execute() {
	pSciTE->ProcessExecute();
}

void SciTEWin::ResetExecution() {
	cmdWorker.Initialise(true);
	jobQueue.SetExecuting(false);
	if (needReadProperties)
		ReadProperties();
	CheckReload();
	CheckMenus();
	jobQueue.ClearJobs();
	::SendMessage(MainHWND(), WM_COMMAND, IDM_FINISHEDEXECUTE, 0);
}

void SciTEWin::ExecuteNext() {
	cmdWorker.icmd++;
	if (cmdWorker.icmd < jobQueue.commandCurrent && cmdWorker.icmd < jobQueue.commandMax && cmdWorker.exitStatus == 0) {
		Execute();
	} else {
		ResetExecution();
	}
}

/**
 * Run a command with redirected input and output streams
 * so the output can be put in a window.
 * It is based upon several usenet posts and a knowledge base article.
 * This is running in a separate thread to the user interface so should always
 * use ScintillaWindow::Send rather than a one of the direct function calls.
 */
DWORD SciTEWin::ExecuteOne(const Job &jobToRun) {
	DWORD exitcode = 0;

	if (jobToRun.jobType == jobShell) {
		ShellExec(jobToRun.command, jobToRun.directory.AsUTF8().c_str());
		return exitcode;
	}

	if (jobToRun.jobType == jobHelp) {
		ExecuteHelp(jobToRun.command.c_str());
		return exitcode;
	}

	if (jobToRun.jobType == jobOtherHelp) {
		ExecuteOtherHelp(jobToRun.command.c_str());
		return exitcode;
	}

	if (jobToRun.jobType == jobGrep) {
		// jobToRun.command is "(w|~)(c|~)(d|~)(b|~)\0files\0text"
		const char *grepCmd = jobToRun.command.c_str();
		if (*grepCmd) {
			GrepFlags gf = grepNone;
			if (*grepCmd == 'w')
				gf = static_cast<GrepFlags>(gf | grepWholeWord);
			grepCmd++;
			if (*grepCmd == 'c')
				gf = static_cast<GrepFlags>(gf | grepMatchCase);
			grepCmd++;
			if (*grepCmd == 'd')
				gf = static_cast<GrepFlags>(gf | grepDot);
			grepCmd++;
			if (*grepCmd == 'b')
				gf = static_cast<GrepFlags>(gf | grepBinary);
			const char *findFiles = grepCmd + 2;
			const char *findWhat = findFiles + strlen(findFiles) + 1;
			if (cmdWorker.outputScroll == 1)
				gf = static_cast<GrepFlags>(gf | grepScroll);
			sptr_t positionEnd = wOutput.Send(SCI_GETCURRENTPOS);
			InternalGrep(gf, jobToRun.directory.AsInternal(), GUI::StringFromUTF8(findFiles).c_str(), findWhat, positionEnd);
			if ((gf & grepScroll) && returnOutputToCommand)
				wOutput.Send(SCI_GOTOPOS, positionEnd, 0);
		}
		return exitcode;
	}

	UINT codePage = static_cast<UINT>(wOutput.Send(SCI_GETCODEPAGE));
	if (codePage != SC_CP_UTF8) {
		codePage = CodePageFromCharSet(characterSet, codePage);
	}

	SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), 0, 0};
	char buffer[16384];
	OutputAppendStringSynchronised(">");
	OutputAppendEncodedStringSynchronised(GUI::StringFromUTF8(jobToRun.command.c_str()), codePage);
	OutputAppendStringSynchronised("\n");

	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	SECURITY_DESCRIPTOR sd;
	// Make a real security thing to allow inheriting handles
	::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
	::SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = &sd;

	HANDLE hPipeWrite = NULL;
	HANDLE hPipeRead = NULL;
	// Create pipe for output redirection
	// read handle, write handle, security attributes,  number of bytes reserved for pipe - 0 default
	::CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0);

	// Create pipe for input redirection. In this code, you do not
	// redirect the output of the child process, but you need a handle
	// to set the hStdInput field in the STARTUP_INFO struct. For safety,
	// you should not set the handles to an invalid handle.

	hWriteSubProcess = NULL;
	subProcessGroupId = 0;
	HANDLE hRead2 = NULL;
	// read handle, write handle, security attributes,  number of bytes reserved for pipe - 0 default
	::CreatePipe(&hRead2, &hWriteSubProcess, &sa, 0);

	::SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, 0);
	::SetHandleInformation(hWriteSubProcess, HANDLE_FLAG_INHERIT, 0);

	// Make child process use hPipeWrite as standard out, and make
	// sure it does not show on screen.
	STARTUPINFOW si = {
			     sizeof(STARTUPINFO), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			 };
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	if (jobToRun.jobType == jobCLI)
		si.wShowWindow = SW_HIDE;
	else
		si.wShowWindow = SW_SHOW;
	si.hStdInput = hRead2;
	si.hStdOutput = hPipeWrite;
	si.hStdError = hPipeWrite;

	FilePath startDirectory = jobToRun.directory.AbsolutePath();

	PROCESS_INFORMATION pi = {0, 0, 0, 0};

	bool running = ::CreateProcessW(
			  NULL,
			  const_cast<wchar_t *>(GUI::StringFromUTF8(jobToRun.command.c_str()).c_str()),
			  NULL, NULL,
			  TRUE, CREATE_NEW_PROCESS_GROUP,
			  NULL,
			  startDirectory.IsSet() ?
			  startDirectory.AsInternal() : NULL,
			  &si, &pi);

	// if jobCLI "System cant find" - try calling with command processor
	if ((!running) && (jobToRun.jobType == jobCLI) && (::GetLastError() == ERROR_FILE_NOT_FOUND)) {

		SString runComLine = "cmd.exe /c ";
		runComLine = runComLine.append(jobToRun.command.c_str());

		running = ::CreateProcessW(
			  NULL,
			  const_cast<wchar_t*>(GUI::StringFromUTF8(runComLine.c_str()).c_str()),
			  NULL, NULL,
			  TRUE, CREATE_NEW_PROCESS_GROUP,
			  NULL,
			  startDirectory.IsSet() ?
			  startDirectory.AsInternal() : NULL,
			  &si, &pi);
	}

	if (running) {
		subProcessGroupId = pi.dwProcessId;

		bool cancelled = false;

		SString repSelBuf;

		size_t totalBytesToWrite = 0;
		if (jobToRun.flags & jobHasInput) {
			totalBytesToWrite = jobToRun.input.length();
		}

		if (totalBytesToWrite > 0 && !(jobToRun.flags & jobQuiet)) {
			SString input = jobToRun.input;
			input.substitute("\n", "\n>> ");

			OutputAppendStringSynchronised(">> ");
			OutputAppendStringSynchronised(input.c_str());
			OutputAppendStringSynchronised("\n");
		}

		unsigned writingPosition = 0;

		int countPeeks = 0;
		while (running) {
			if (writingPosition >= totalBytesToWrite) {
				if (countPeeks > 10)
					::Sleep(100L);
				else if (countPeeks > 2)
					::Sleep(10L);
				countPeeks++;
			}

			DWORD bytesRead = 0;
			DWORD bytesAvail = 0;

			if (!::PeekNamedPipe(hPipeRead, buffer,
					     sizeof(buffer), &bytesRead, &bytesAvail, NULL)) {
				bytesAvail = 0;
			}

			if ((bytesAvail < 1000) && (hWriteSubProcess != INVALID_HANDLE_VALUE) && (writingPosition < totalBytesToWrite)) {
				// There is input to transmit to the process.  Do it in small blocks, interleaved
				// with reads, so that our hRead buffer will not be overrun with results.

				size_t bytesToWrite;
				int eol_pos = jobToRun.input.search("\n", writingPosition);
				if (eol_pos == -1) {
					bytesToWrite = totalBytesToWrite - writingPosition;
				} else {
					bytesToWrite = eol_pos + 1 - writingPosition;
				}
				if (bytesToWrite > 250) {
					bytesToWrite = 250;
				}

				DWORD bytesWrote = 0;

				int bTest = ::WriteFile(hWriteSubProcess,
					    const_cast<char *>(jobToRun.input.c_str() + writingPosition),
					    static_cast<DWORD>(bytesToWrite), &bytesWrote, NULL);

				if (bTest) {
					if ((writingPosition + bytesToWrite) / 1024 > writingPosition / 1024) {
						// sleep occasionally, even when writing
						::Sleep(100L);
					}

					writingPosition += bytesWrote;

					if (writingPosition >= totalBytesToWrite) {
						::CloseHandle(hWriteSubProcess);
						hWriteSubProcess = INVALID_HANDLE_VALUE;
					}

				} else {
					// Is this the right thing to do when writing to the pipe fails?
					::CloseHandle(hWriteSubProcess);
					hWriteSubProcess = INVALID_HANDLE_VALUE;
					OutputAppendStringSynchronised("\n>Input pipe closed due to write failure.\n");
				}

			} else if (bytesAvail > 0) {
				int bTest = ::ReadFile(hPipeRead, buffer,
						       sizeof(buffer), &bytesRead, NULL);

				if (bTest && bytesRead) {

					if (jobToRun.flags & jobRepSelMask) {
						repSelBuf.append(buffer, bytesRead);
					}

					if (!(jobToRun.flags & jobQuiet)) {
						if (!cmdWorker.seenOutput) {
							MakeOutputVisible();
							cmdWorker.seenOutput = true;
						}
						// Display the data
						OutputAppendStringSynchronised(buffer, bytesRead);
					}

					::UpdateWindow(MainHWND());
				} else {
					running = false;
				}
			} else {
				if (::GetExitCodeProcess(pi.hProcess, &exitcode)) {
					if (STILL_ACTIVE != exitcode) {
						// Already dead
						running = false;
					}
				}
			}

			if (jobQueue.SetCancelFlag(0)) {
				if (WAIT_OBJECT_0 != ::WaitForSingleObject(pi.hProcess, 500)) {
					// We should use it only if the GUI process is stuck and
					// don't answer to a normal termination command.
					// This function is dangerous: dependant DLLs don't know the process
					// is terminated, and memory isn't released.
					OutputAppendStringSynchronised("\n>Process failed to respond; forcing abrupt termination...\n");
					::TerminateProcess(pi.hProcess, 1);
				}
				running = false;
				cancelled = true;
			}
		}

		if (WAIT_OBJECT_0 != ::WaitForSingleObject(pi.hProcess, 1000)) {
			OutputAppendStringSynchronised("\n>Process failed to respond; forcing abrupt termination...");
			::TerminateProcess(pi.hProcess, 2);
		}
		::GetExitCodeProcess(pi.hProcess, &exitcode);
		SString sExitMessage(static_cast<int>(exitcode));
		sExitMessage.insert(0, ">Exit code: ");
		if (jobQueue.TimeCommands()) {
			sExitMessage += "    Time: ";
			sExitMessage += SString(cmdWorker.commandTime.Duration(), 3);
		}
		sExitMessage += "\n";
		OutputAppendStringSynchronised(sExitMessage.c_str());

		::CloseHandle(pi.hProcess);
		::CloseHandle(pi.hThread);

		if (!cancelled) {
			bool doRepSel = false;
			if (jobToRun.flags & jobRepSelYes)
				doRepSel = true;
			else if (jobToRun.flags & jobRepSelAuto)
				doRepSel = (0 == exitcode);

			if (doRepSel) {
				int cpMin = static_cast<int>(wEditor.Send(SCI_GETSELECTIONSTART, 0, 0));
				wEditor.Send(SCI_REPLACESEL,0,reinterpret_cast<sptr_t>(repSelBuf.c_str()));
				wEditor.Send(SCI_SETSEL, cpMin, cpMin+repSelBuf.length());
			}
		}

		WarnUser(warnExecuteOK);

	} else {
		DWORD nRet = ::GetLastError();
		OutputAppendStringSynchronised(">");
		OutputAppendEncodedStringSynchronised(GetErrorMessage(nRet), codePage);
		WarnUser(warnExecuteKO);
	}
	::CloseHandle(hPipeRead);
	::CloseHandle(hPipeWrite);
	::CloseHandle(hRead2);
	::CloseHandle(hWriteSubProcess);
	hWriteSubProcess = NULL;
	subProcessGroupId = 0;
	return exitcode;
}

/**
 * Run a command in the job queue, stopping if one fails.
 * This is running in a separate thread to the user interface so must be
 * careful when reading and writing shared state.
 */
void SciTEWin::ProcessExecute() {
	if (scrollOutput)
		wOutput.Send(SCI_GOTOPOS, wOutput.Send(SCI_GETTEXTLENGTH));

	cmdWorker.exitStatus = ExecuteOne(jobQueue.jobQueue[cmdWorker.icmd]);
	if (jobQueue.isBuilding) {
		// The build command is first command in a sequence so it is only built if
		// that command succeeds not if a second returns after document is modified.
		jobQueue.isBuilding = false;
		if (cmdWorker.exitStatus == 0)
			jobQueue.isBuilt = true;
	}

	// Move selection back to beginning of this run so that F4 will go
	// to first error of this run.
	// scroll and return only if output.scroll equals
	// one in the properties file
	if ((cmdWorker.outputScroll == 1) && returnOutputToCommand)
		wOutput.Send(SCI_GOTOPOS, cmdWorker.originalEnd, 0);
	returnOutputToCommand = true;
	PostOnMainThread(WORK_EXECUTE, &cmdWorker);
}

void SciTEWin::ShellExec(const SString &cmd, const char *dir) {
	char *mycmd;

	// guess if cmd is an executable, if this succeeds it can
	// contain spaces without enclosing it with "
	char *mycmdcopy = StringDup(cmd.c_str());
	strlwr(mycmdcopy);

	char *mycmd_end = NULL;
	char *myparams = NULL;

	char *s = strstr(mycmdcopy, ".exe");
	if (s == NULL)
		s = strstr(mycmdcopy, ".cmd");
	if (s == NULL)
		s = strstr(mycmdcopy, ".bat");
	if (s == NULL)
		s = strstr(mycmdcopy, ".com");
	if ((s != NULL) && ((*(s + 4) == '\0') || (*(s + 4) == ' '))) {
		ptrdiff_t len_mycmd = s - mycmdcopy + 4;
		delete []mycmdcopy;
		mycmdcopy = StringDup(cmd.c_str());
		mycmd = mycmdcopy;
		mycmd_end = mycmdcopy + len_mycmd;
	} else {
		delete []mycmdcopy;
		mycmdcopy = StringDup(cmd.c_str());
		if (*mycmdcopy != '"') {
			// get next space to separate cmd and parameters
			mycmd_end = strchr(mycmdcopy, ' ');
			mycmd = mycmdcopy;
		} else {
			// the cmd is surrounded by ", so it can contain spaces, but we must
			// strip the " for ShellExec
			mycmd = mycmdcopy + 1;
			char *s = strchr(mycmdcopy + 1, '"');
			if (s != NULL) {
				*s = '\0';
				mycmd_end = s + 1;
			}
		}
	}

	if ((mycmd_end != NULL) && (*mycmd_end != '\0')) {
		*mycmd_end = '\0';
		// test for remaining params after cmd, they may be surrounded by " but
		// we give them as-is to ShellExec
		++mycmd_end;
		while (*mycmd_end == ' ')
			++mycmd_end;

		if (*mycmd_end != '\0')
			myparams = mycmd_end;
	}

	GUI::gui_string sMycmd = GUI::StringFromUTF8(mycmd);
	GUI::gui_string sMyparams = GUI::StringFromUTF8(myparams);
	GUI::gui_string sDir = GUI::StringFromUTF8(dir);

	SHELLEXECUTEINFO exec= { sizeof (exec), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	exec.fMask= SEE_MASK_FLAG_NO_UI; // own msg box on return
	exec.hwnd= MainHWND();
	exec.lpVerb= L"open";  // better for executables to use "open" instead of NULL
	exec.lpFile= sMycmd.c_str();   // file to open
	exec.lpParameters= sMyparams.c_str(); // parameters
	exec.lpDirectory= sDir.c_str(); // launch directory
	exec.nShow= SW_SHOWNORMAL; //default show cmd

	if (::ShellExecuteEx(&exec)) {
		// it worked!
		delete []mycmdcopy;
		return;
	}
	DWORD rc = GetLastError();

	SString errormsg("Error while launching:\n\"");
	errormsg += mycmdcopy;
	if (myparams != NULL) {
		errormsg += "\" with Params:\n\"";
		errormsg += myparams;
	}
	errormsg += "\"\n";
	GUI::gui_string sErrorMsg = GUI::StringFromUTF8(errormsg.c_str()) + GetErrorMessage(rc);
	WindowMessageBox(wSciTE, sErrorMsg, MB_OK);

	delete []mycmdcopy;
}

void SciTEWin::Execute() {
	if (buffers.SavingInBackground())
		// May be saving file that should be used by command so wait until all saved
		return;

	SciTEBase::Execute();
	if (!jobQueue.HasCommandToRun())
		// No commands to execute - possibly cancelled in SciTEBase::Execute
		return;

	cmdWorker.Initialise(false);
	cmdWorker.outputScroll = props.GetInt("output.scroll", 1);
	cmdWorker.originalEnd = wOutput.Call(SCI_GETTEXTLENGTH);
	cmdWorker.commandTime.Duration(true);
	cmdWorker.flags = jobQueue.jobQueue[cmdWorker.icmd].flags;
	if (scrollOutput)
		wOutput.Send(SCI_GOTOPOS, wOutput.Send(SCI_GETTEXTLENGTH));

	if (jobQueue.jobQueue[cmdWorker.icmd].jobType == jobExtension) {
		// Execute extensions synchronously
		if (jobQueue.jobQueue[cmdWorker.icmd].flags & jobGroupUndo)
			wEditor.Send(SCI_BEGINUNDOACTION);

		if (extender)
			extender->OnExecute(jobQueue.jobQueue[cmdWorker.icmd].command.c_str());

		if (jobQueue.jobQueue[cmdWorker.icmd].flags & jobGroupUndo)
			wEditor.Send(SCI_ENDUNDOACTION);

		Redraw();
		// A Redraw "might" be needed, since Lua and Director
		// provide enough low-level capabilities to corrupt the
		// display.

		ExecuteNext();
	} else {
		// Execute other jobs asynchronously on a new thread
		PerformOnNewThread(&cmdWorker);
	}
}

void SciTEWin::StopExecute() {
	if (hWriteSubProcess && (hWriteSubProcess != INVALID_HANDLE_VALUE)) {
		char stop[] = "\032";
		DWORD bytesWrote = 0;
		::WriteFile(hWriteSubProcess, stop, static_cast<DWORD>(strlen(stop)), &bytesWrote, NULL);
		Sleep(500L);
	}

#ifdef USE_CONSOLE_EVENT
	if (subProcessGroupId) {
		// this also doesn't work
		OutputAppendStringSynchronised("\n>Attempting to cancel process...");

		if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, subProcessGroupId)) {
			LONG errCode = GetLastError();
			OutputAppendStringSynchronised("\n>BREAK Failed ");
			OutputAppendStringSynchronised(SString(errCode).c_str());
			OutputAppendStringSynchronised("\n");
		}
		Sleep(100L);
	}
#endif

	jobQueue.SetCancelFlag(1);
}

void SciTEWin::AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, const SString &input, int flags) {
	if (cmd.length()) {
		if ((jobType == jobShell) && ((flags & jobForceQueue) == 0)) {
			SString pCmd = cmd;
			parameterisedCommand = "";
			if (pCmd[0] == '*') {
				pCmd.remove(0, 1);
				parameterisedCommand = pCmd;
				if (!ParametersDialog(true)) {
					return;
				}
			} else {
				ParamGrab();
			}
			pCmd = props.Expand(pCmd.c_str());
			ShellExec(pCmd, dir.c_str());
		} else {
			SciTEBase::AddCommand(cmd, dir, jobType, input, flags);
		}
	}
}

static void WorkerThread(void *ptr) {
	Worker *pWorker = static_cast<Worker *>(ptr);
	pWorker->Execute();
}

bool SciTEWin::PerformOnNewThread(Worker *pWorker) {
	uintptr_t result = _beginthread(WorkerThread, 1024 * 1024, reinterpret_cast<void *>(pWorker));
	return result != static_cast<uintptr_t>(-1);
}

void SciTEWin::PostOnMainThread(int cmd, Worker *pWorker) {
	::PostMessage(reinterpret_cast<HWND>(wSciTE.GetID()), SCITE_WORKER, cmd, reinterpret_cast<LPARAM>(pWorker));
}

void SciTEWin::WorkerCommand(int cmd, Worker *pWorker) {
	if (cmd < WORK_PLATFORM) {
		SciTEBase::WorkerCommand(cmd, pWorker);
	} else {
		if (cmd == WORK_EXECUTE) {
			// Move to next command
			ExecuteNext();
		}
	}
}

void SciTEWin::QuitProgram() {
	quitting = false;
	if (SaveIfUnsureAll() != IDCANCEL) {
		if (fullScreen)	// Ensure tray visible on exit
			FullScreenToggle();
		quitting = true;
		// If ongoing saves, wait for them to complete.
		if (!buffers.SavingInBackground()) {
			::PostQuitMessage(0);
			wSciTE.Destroy();
		}
	}
}

void SciTEWin::RestorePosition() {
	int left = propsSession.GetInt("position.left", CW_USEDEFAULT);
	int top = propsSession.GetInt("position.top", CW_USEDEFAULT);
	int width = propsSession.GetInt("position.width", CW_USEDEFAULT);
	int height = propsSession.GetInt("position.height", CW_USEDEFAULT);
	cmdShow = propsSession.GetInt("position.maximize", 0) ? SW_MAXIMIZE : 0;

	if (left != static_cast<int>(CW_USEDEFAULT) &&
	    top != static_cast<int>(CW_USEDEFAULT) &&
	    width != static_cast<int>(CW_USEDEFAULT) &&
	    height != static_cast<int>(CW_USEDEFAULT)) {
		winPlace.length = sizeof(winPlace);
		winPlace.rcNormalPosition.left = left;
		winPlace.rcNormalPosition.right = left + width;
		winPlace.rcNormalPosition.top = top;
		winPlace.rcNormalPosition.bottom = top + height;
		::SetWindowPlacement(MainHWND(), &winPlace);
	}
}

void SciTEWin::CreateUI() {
	CreateBuffers();

	int left = props.GetInt("position.left", CW_USEDEFAULT);
	int top = props.GetInt("position.top", CW_USEDEFAULT);
	int width = props.GetInt("position.width", CW_USEDEFAULT);
	int height = props.GetInt("position.height", CW_USEDEFAULT);
	cmdShow = props.GetInt("position.maximize", 0) ? SW_MAXIMIZE : 0;
	if (width == -1 || height == -1) {
		cmdShow = SW_MAXIMIZE;
		width = CW_USEDEFAULT;
		height = CW_USEDEFAULT;
	}

	if (props.GetInt("position.tile") && ::FindWindow(TEXT("SciTEWindow"), NULL) &&
	        (left != static_cast<int>(CW_USEDEFAULT))) {
		left += width;
	}
	// Pass 'this' pointer in lpParam of CreateWindow().
	wSciTE = ::CreateWindowEx(
	             0,
	             className,
	             windowName.c_str(),
	             WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
	             WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
	             WS_CLIPCHILDREN,
	             left, top, width, height,
	             NULL,
	             NULL,
	             hInstance,
	             reinterpret_cast<LPSTR>(this));
	if (!wSciTE.Created())
		exit(FALSE);

	if (props.GetInt("save.position"))
		RestorePosition();

	LocaliseMenus();
	LocaliseAccelerators();
	SString pageSetup = props.Get("print.margins");
	char val[32];
	char *ps = StringDup(pageSetup.c_str());
	const char *next = GetNextPropItem(ps, val, 32);
	pagesetupMargin.left = atol(val);
	next = GetNextPropItem(next, val, 32);
	pagesetupMargin.right = atol(val);
	next = GetNextPropItem(next, val, 32);
	pagesetupMargin.top = atol(val);
	GetNextPropItem(next, val, 32);
	pagesetupMargin.bottom = atol(val);
	delete []ps;

	UIAvailable();
}

static bool IsASpace(int ch) {
	return (ch == ' ') || (ch == '\t');
}

/**
 * Break up the command line into individual arguments and strip double quotes
 * from each argument.
 * @return A string with each argument separated by '\n'.
 */
GUI::gui_string SciTEWin::ProcessArgs(const GUI::gui_char *cmdLine) {
	GUI::gui_string args;
	const GUI::gui_char *startArg = cmdLine;
	while (*startArg) {
		while (IsASpace(*startArg)) {
			startArg++;
		}
		const GUI::gui_char *endArg = startArg;
		if (*startArg == '"') {	// Opening double-quote
			startArg++;
			endArg = startArg;
			while (*endArg && *endArg != '\"') {
				endArg++;
			}
		} else {	// No double-quote, end of argument on first space
			while (*endArg && !IsASpace(*endArg)) {
				endArg++;
			}
		}
		GUI::gui_string arg(startArg, 0, endArg - startArg);
		if (args.size() > 0)
			args += GUI_TEXT("\n");
		args += arg;
		startArg = endArg;	// On a space or a double-quote, or on the end of the command line
		if (*startArg) {
			startArg++;
		}
	}

	return args;
}

/**
 * Process the command line, check for other instance wanting to open files,
 * create the SciTE window, perform batch processing (print) or transmit command line
 * to other instance and exit or just show the window and open files.
 */
void SciTEWin::Run(const GUI::gui_char *cmdLine) {
	// Load the default session file
	if (props.GetInt("save.session") || props.GetInt("save.position") || props.GetInt("save.recent")) {
		LoadSessionFile(GUI_TEXT(""));
	}

	// Break up the command line into individual arguments
	GUI::gui_string args = ProcessArgs(cmdLine);
	// Read the command line parameters:
	// In case the check.if.already.open property has been set or reset on the command line,
	// we still get a last chance to force checking or to open a separate instance;
	// Check if the user just want to print the file(s).
	// Don't process files yet.
	bool bBatchProcessing = ProcessCommandLine(args, 0);

	// No need to check for other instances when doing a batch job:
	// perform some tasks and exit immediately.
	if (!bBatchProcessing && props.GetInt("check.if.already.open") != 0) {
		uniqueInstance.CheckOtherInstance();
	}

	// We create the window, so it can be found by EnumWindows below,
	// and the Scintilla control is thus created, allowing to print the file(s).
	// We don't show it yet, so if it is destroyed (duplicate instance), it will
	// not flash on the taskbar or on the display.
	CreateUI();

	if (bBatchProcessing) {
		// Reprocess the command line and read the files
		ProcessCommandLine(args, 1);
		Print(false);	// Don't ask user for print parameters
		// Done, we exit the program
		::PostQuitMessage(0);
		wSciTE.Destroy();
		return;
	}

	if (props.GetInt("check.if.already.open") != 0 && uniqueInstance.FindOtherInstance()) {
		uniqueInstance.SendCommands(GUI::UTF8FromString(cmdLine).c_str());

		// Kill itself, leaving room to the previous instance
		::PostQuitMessage(0);
		wSciTE.Destroy();
		return;	// Don't do anything else
	}

	// OK, the instance will be displayed
	SizeSubWindows();
	wSciTE.Show();
	if (cmdShow) {	// assume SW_MAXIMIZE only
		::ShowWindow(MainHWND(), cmdShow);
	}

	// Open all files given on command line.
	// The filenames containing spaces must be enquoted.
	// In case of not using buffers they get closed immediately except
	// the last one, but they move to the MRU file list
	ProcessCommandLine(args, 1);
	Redraw();
}

/**
 * Draw the split bar.
 */
void ContentWin::Paint(HDC hDC, GUI::Rectangle) {
	GUI::Rectangle rcInternal = GetClientPosition();

	int heightClient = rcInternal.Height();
	int widthClient = rcInternal.Width();

	int heightEditor = heightClient - pSciTEWin->heightOutput - pSciTEWin->heightBar;
	int yBorder = heightEditor;
	int xBorder = widthClient - pSciTEWin->heightOutput - pSciTEWin->heightBar;
	for (int i = 0; i < pSciTEWin->heightBar; i++) {
		HPEN pen;
		if (i == 1)
			pen = ::CreatePen(0,1,::GetSysColor(COLOR_3DHIGHLIGHT));
		else if (i == pSciTEWin->heightBar - 2)
			pen = ::CreatePen(0,1,::GetSysColor(COLOR_3DSHADOW));
		else if (i == pSciTEWin->heightBar - 1)
			pen = ::CreatePen(0,1,::GetSysColor(COLOR_3DDKSHADOW));
		else
			pen = ::CreatePen(0,1,::GetSysColor(COLOR_3DFACE));
		HPEN penOld = static_cast<HPEN>(::SelectObject(hDC, pen));
		if (pSciTEWin->splitVertical) {
			::MoveToEx(hDC, xBorder + i, 0, 0);
			::LineTo(hDC, xBorder + i, heightClient);
		} else {
			::MoveToEx(hDC, 0, yBorder + i, 0);
			::LineTo(hDC, widthClient, yBorder + i);
		}
		::SelectObject(hDC, penOld);
		::DeleteObject(pen);
	}
}

void SciTEWin::AboutDialog() {
#ifdef STATIC_BUILD
	AboutDialogWithBuild(1);
#else
	AboutDialogWithBuild(0);
#endif
}

/**
 * Open files dropped on the SciTE window.
 */
void SciTEWin::DropFiles(HDROP hdrop) {
	// If drag'n'drop inside the SciTE window but outside
	// Scintilla, hdrop is null, and an exception is generated!
	if (hdrop) {
		bool tempFilesSyncLoad = props.GetInt("temp.files.sync.load") != 0;
		GUI::gui_char tempDir[MAX_PATH];
		DWORD tempDirLen = ::GetTempPath(MAX_PATH, tempDir);
		bool isTempFile = false;
		int filesDropped = ::DragQueryFile(hdrop, 0xffffffff, NULL, 0);
		// Append paths to dropFilesQueue, to finish drag operation soon
		for (int i = 0; i < filesDropped; ++i) {
			GUI::gui_char pathDropped[MAX_PATH];
			::DragQueryFileW(hdrop, i, pathDropped, ELEMENTS(pathDropped));
			// Only do this for the first file in the drop op
			// as all are coming from the same drag location
			if (i == 0 && tempFilesSyncLoad) {
				// check if file's parent dir is temp
				if (::wcsncmp(tempDir, pathDropped, tempDirLen) == 0) {
					isTempFile = true;
				}
			}
			if (isTempFile) {
				if (!Open(pathDropped, ofSynchronous)) {
					break;
				}
			} else {
				dropFilesQueue.push_back(pathDropped);
			}
		}
		::DragFinish(hdrop);
		// Put SciTE to forefront
		// May not work for Win2k, but OK for lower versions
		// Note: how to drop a file to an iconic window?
		// Actually, it is the Send To command that generates a drop.
		if (::IsIconic(MainHWND())) {
			::ShowWindow(MainHWND(), SW_RESTORE);
		}
		::SetForegroundWindow(MainHWND());
		// Post message to ourself for opening the files so we can finish the drop message and
		// the drop source will respond when open operation takes long time (opening big files...)
		if (!dropFilesQueue.empty()) {
			::PostMessage(MainHWND(), SCITE_DROP, 0, 0);
		}
	}
}

/**
 * Handle simple wild-card file patterns and directory requests.
 */
bool SciTEWin::PreOpenCheck(const GUI::gui_char *arg) {
	bool isHandled = false;
	HANDLE hFFile;
	WIN32_FIND_DATA ffile;
	DWORD fileattributes = ::GetFileAttributes(arg);
	GUI::gui_char filename[MAX_PATH];
	int nbuffers = props.GetInt("buffers");

	if (fileattributes != (DWORD) -1) {	// arg is an existing directory or filename
		// if the command line argument is a directory, use OpenDialog()
		if (fileattributes & FILE_ATTRIBUTE_DIRECTORY) {
			OpenDialog(FilePath(arg), GUI::StringFromUTF8(props.GetExpanded("open.filter").c_str()).c_str());
			isHandled = true;
		}
	} else if (nbuffers > 1 && (hFFile = ::FindFirstFile(arg, &ffile)) != INVALID_HANDLE_VALUE) {
		// If several buffers is accepted and the arg is a filename pattern matching at least an existing file
		isHandled = true;
		wcscpy(filename, arg);
		GUI::gui_char *lastslash;
		if (NULL == (lastslash = wcsrchr(filename, GUI_TEXT('\\'))))
			lastslash = filename;	// No path
		else
			lastslash++;
		// Open files matching the given pattern until no more files or all available buffers are exhausted
		do {
			if (!(ffile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {	// Skip directories
				wcscpy(lastslash, ffile.cFileName);
				Open(filename);
				--nbuffers;
			}
		} while (nbuffers > 0 && ::FindNextFile(hFFile, &ffile));
		::FindClose(hFFile);
	} else {
		const GUI::gui_char *lastslash = wcsrchr(arg, '\\');
		const GUI::gui_char *lastdot = wcsrchr(arg, '.');

		// if the filename is only an extension, open the dialog box with it as the extension filter
		if ((lastslash && lastdot && lastslash == lastdot - 1) || (!lastslash && lastdot == arg)) {
			isHandled = true;

			GUI::gui_char dir[MAX_PATH];
			if (lastslash) { // the arg contains a path, so copy that part to dirName
				wcsncpy(dir, arg, lastslash - arg + 1);
				dir[lastslash - arg + 1] = '\0';
			} else {
				wcscpy(dir, GUI_TEXT(".\\"));
			}

			wcscpy(filename, GUI_TEXT("*"));
			wcscat(filename, lastdot);
			wcscat(filename, GUI_TEXT("|"));
			wcscat(filename, GUI_TEXT("*"));
			wcscat(filename, lastdot);
			OpenDialog(FilePath(dir), filename);
		} else if (!lastdot || (lastslash && lastdot < lastslash)) {
			// if the filename has no extension, try to match a file with list of standard extensions
			SString extensions = props.GetExpanded("source.default.extensions");
			if (extensions.length()) {
				wcscpy(filename, arg);
				GUI::gui_char *endfilename = filename + wcslen(filename);
				extensions.substitute('|', '\0');
				size_t start = 0;
				while (start < extensions.length()) {
					GUI::gui_string filterName = GUI::StringFromUTF8(extensions.c_str() + start);
					wcscpy(endfilename, filterName.c_str());
					if (::GetFileAttributes(filename) != (DWORD) -1) {
						isHandled = true;
						Open(filename);
						break;	// Found!
					} else {
						// Next extension
						start += strlen(extensions.c_str() + start) + 1;
					}
				}
			}
		}
	}

	return isHandled;
}

/* return true if stdin is blocked:
	- stdin is the console (isatty() == 1)
	- a valid handle for stdin cannot be generated
	- the handle appears to be the console - this may be a duplicate of using isatty() == 1
	- the pipe cannot be peeked, which appears to be from command lines such as "scite <file.txt"
	otherwise it is unblocked
*/
bool SciTEWin::IsStdinBlocked() {
	DWORD unread_messages;
	INPUT_RECORD irec[1];
	char bytebuffer;
	HANDLE hStdIn = ::GetStdHandle(STD_INPUT_HANDLE);
	if (hStdIn == INVALID_HANDLE_VALUE) {
		/* an invalid handle, assume that stdin is blocked by falling to bottomn */;
	} else if (::PeekConsoleInput(hStdIn, irec, 1, &unread_messages) != 0) {
		/* it is the console, assume that stdin is blocked by falling to bottomn */;
	} else if (::GetLastError() == ERROR_INVALID_HANDLE) {
		for (int n = 0; n < 4; n++) {
			/*	if this fails, it is either
				- a busy pipe "scite \*.,cxx /s /b | s -@",
				- another type of pipe "scite - <file", or
				- a blocked pipe "findstrin nothing | scite -"
				in any case case, retry in a short bit
			*/
			if (::PeekNamedPipe(hStdIn, &bytebuffer, sizeof(bytebuffer), NULL,NULL, &unread_messages) != 0) {
				if (unread_messages != 0) {
					return false; /* is a pipe and it is not blocked */
				}
			}
			::Sleep(2500);
		}
	}
	return true;
}

void SciTEWin::MinimizeToTray() {
	TCHAR n[64] = TEXT("SciTE");
	NOTIFYICONDATA nid;
	memset(&nid, 0, sizeof(nid));
	nid.cbSize = sizeof(nid);
	nid.hWnd = MainHWND();
	nid.uID = 1;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = SCITE_TRAY;
	nid.hIcon = static_cast<HICON>(
	                ::LoadImage(hInstance, TEXT("SCITE"), IMAGE_ICON, 16, 16, LR_DEFAULTSIZE));
	lstrcpy(nid.szTip, n);
	::ShowWindow(MainHWND(), SW_MINIMIZE);
	if (::Shell_NotifyIcon(NIM_ADD, &nid)) {
		::ShowWindow(MainHWND(), SW_HIDE);
	}
}

void SciTEWin::RestoreFromTray() {
	NOTIFYICONDATA nid;
	memset(&nid, 0, sizeof(nid));
	nid.cbSize = sizeof(nid);
	nid.hWnd = MainHWND();
	nid.uID = 1;
	::ShowWindow(MainHWND(), SW_SHOW);
	::Sleep(100);
	::Shell_NotifyIcon(NIM_DELETE, &nid);
}

#ifndef VK_OEM_2
static const int VK_OEM_2=0xbf;
static const int VK_OEM_3=0xc0;
static const int VK_OEM_4=0xdb;
static const int VK_OEM_5=0xdc;
static const int VK_OEM_6=0xdd;
#endif
#ifndef VK_OEM_PLUS
static const int VK_OEM_PLUS=0xbb;
#endif

inline bool KeyMatch(const SString &sKey, int keyval, int modifiers) {
	return SciTEKeys::MatchKeyCode(
		SciTEKeys::ParseKeyCode(sKey.c_str()), keyval, modifiers);
}

LRESULT SciTEWin::KeyDown(WPARAM wParam) {
	// Look through lexer menu
	int modifiers =
	    (IsKeyDown(VK_SHIFT) ? SCMOD_SHIFT : 0) |
	    (IsKeyDown(VK_CONTROL) ? SCMOD_CTRL : 0) |
	    (IsKeyDown(VK_MENU) ? SCMOD_ALT : 0);

	if (extender && extender->OnKey(static_cast<int>(wParam), modifiers))
		return 1l;

	for (int j = 0; j < languageItems; j++) {
		if (KeyMatch(languageMenu[j].menuKey, static_cast<int>(wParam), modifiers)) {
			SciTEBase::MenuCommand(IDM_LANGUAGE + j);
			return 1l;
		}
	}

	// loop through the Tools menu's active commands.
	HMENU hMenu = ::GetMenu(MainHWND());
	HMENU hToolsMenu = ::GetSubMenu(hMenu, menuTools);
	for (int tool_i = 0; tool_i < toolMax; ++tool_i) {
		MENUITEMINFO mii;
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_DATA;
		if (::GetMenuItemInfo(hToolsMenu, IDM_TOOLS+tool_i, FALSE, &mii) && mii.dwItemData) {
			if (SciTEKeys::MatchKeyCode(reinterpret_cast<long&>(mii.dwItemData), static_cast<int>(wParam), modifiers)) {
				SciTEBase::MenuCommand(IDM_TOOLS+tool_i);
				return 1l;
			}
		}
	}

	// loop through the keyboard short cuts defined by user.. if found
	// exec it the command defined
	for (int cut_i = 0; cut_i < shortCutItems; cut_i++) {
		if (KeyMatch(shortCutItemList[cut_i].menuKey, static_cast<int>(wParam), modifiers)) {
			int commandNum = SciTEBase::GetMenuCommandAsInt(shortCutItemList[cut_i].menuCommand);
			if (commandNum != -1) {
				// its possible that the command is for scintilla directly
				// all scintilla commands are larger then 2000
				if (commandNum < 2000) {
					SciTEBase::MenuCommand(commandNum);
				} else {
					SciTEBase::CallFocused(commandNum);
				}
				return 1l;
			}
		}
	}

	return 0l;
}

LRESULT SciTEWin::KeyUp(WPARAM wParam) {
	if (wParam == VK_CONTROL) {
		EndStackedTabbing();
	}
	return 0l;
}

void SciTEWin::AddToPopUp(const char *label, int cmd, bool enabled) {
	GUI::gui_string localised = localiser.Text(label);
	HMENU menu = reinterpret_cast<HMENU>(popup.GetID());
	if (0 == localised.length())
		::AppendMenu(menu, MF_SEPARATOR, 0, TEXT(""));
	else if (enabled)
		::AppendMenu(menu, MF_STRING, cmd, localised.c_str());
	else
		::AppendMenu(menu, MF_STRING | MF_DISABLED | MF_GRAYED, cmd, localised.c_str());
}

LRESULT SciTEWin::ContextMenuMessage(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	GUI::ScintillaWindow *w = &wEditor;
	GUI::Point pt = PointFromLong(lParam);
	if ((pt.x == -1) && (pt.y == -1)) {
		// Caused by keyboard so display menu near caret
		if (wOutput.HasFocus())
			w = &wOutput;
		int position = w->Call(SCI_GETCURRENTPOS);
		pt.x = w->Call(SCI_POINTXFROMPOSITION, 0, position);
		pt.y = w->Call(SCI_POINTYFROMPOSITION, 0, position);
		POINT spt = {pt.x, pt.y};
		::ClientToScreen(static_cast<HWND>(w->GetID()), &spt);
		pt = GUI::Point(spt.x, spt.y);
	} else {
		GUI::Rectangle rcEditor = wEditor.GetPosition();
		if (!rcEditor.Contains(pt)) {
			GUI::Rectangle rcOutput = wOutput.GetPosition();
			if (rcOutput.Contains(pt)) {
				w = &wOutput;
			} else {	// In frame so use default.
				return ::DefWindowProc(MainHWND(), iMessage, wParam, lParam);
			}
		}
	}
	menuSource = ::GetDlgCtrlID(HwndOf(*w));
	ContextMenu(*w, pt, wSciTE);
	return 0;
}

LRESULT SciTEWin::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	int statusFailure = 0;
	static int boxesVisible = 0;
	try {
		LRESULT uim = uniqueInstance.CheckMessage(iMessage, wParam, lParam);
		if (uim != 0) {
			return uim;
		}

		switch (iMessage) {

		case WM_CREATE:
			Creation();
			break;

		case WM_COMMAND:
			Command(wParam, lParam);
			break;

		case WM_CONTEXTMENU:
			return ContextMenuMessage(iMessage, wParam, lParam);

		case WM_ENTERMENULOOP:
			if (!wParam)
				menuSource = 0;
			break;

		case WM_SYSCOMMAND:
			if ((wParam == SC_MINIMIZE) && props.GetInt("minimize.to.tray")) {
				MinimizeToTray();
				return 0;
			}
			return ::DefWindowProc(MainHWND(), iMessage, wParam, lParam);

		case SCITE_TRAY:
			if (lParam == WM_LBUTTONDOWN) {
				RestoreFromTray();
				::ShowWindow(MainHWND(), SW_RESTORE);
				::FlashWindow(MainHWND(), FALSE);
			}
			break;

		case SCITE_DROP:
			// Open the files
			while (!dropFilesQueue.empty()) {
				FilePath file(dropFilesQueue.front());
				dropFilesQueue.pop_front();
				if (file.Exists()) {
					Open(file.AsInternal());
				} else {
					GUI::gui_string msg = LocaliseMessage("Could not open file '^0'.", file.AsInternal());
					WindowMessageBox(wSciTE, msg, MB_OK | MB_ICONWARNING);
				}
			}
			break;

		case SCITE_WORKER:
			WorkerCommand(static_cast<int>(wParam), reinterpret_cast<Worker *>(lParam));
			break;

		case WM_NOTIFY:
			Notify(reinterpret_cast<SCNotification *>(lParam));
			break;

		case WM_KEYDOWN:
			return KeyDown(wParam);

		case WM_KEYUP:
			return KeyUp(wParam);

		case WM_SIZE:
			if (wParam != 1)
				SizeSubWindows();
			break;

		case WM_MOVE:
			wEditor.Call(SCI_CALLTIPCANCEL);
			break;

		case WM_GETMINMAXINFO: {
				MINMAXINFO *pmmi = reinterpret_cast<MINMAXINFO *>(lParam);
				if (fullScreen) {
					pmmi->ptMaxSize.x = ::GetSystemMetrics(SM_CXSCREEN) +
										2 * ::GetSystemMetrics(SM_CXSIZEFRAME);
					pmmi->ptMaxSize.y = ::GetSystemMetrics(SM_CYSCREEN) +
										::GetSystemMetrics(SM_CYCAPTION) +
										::GetSystemMetrics(SM_CYMENU) +
										2 * ::GetSystemMetrics(SM_CYSIZEFRAME);
					pmmi->ptMaxTrackSize.x = pmmi->ptMaxSize.x;
					pmmi->ptMaxTrackSize.y = pmmi->ptMaxSize.y;
					return 0;
				} else {
					return ::DefWindowProc(MainHWND(), iMessage, wParam, lParam);
				}
			}

		case WM_INITMENU:
			CheckMenus();
			break;

		case WM_CLOSE:
			QuitProgram();
			return 0;

		case WM_QUERYENDSESSION:
			QuitProgram();
			return 1;

		case WM_DESTROY:
			break;

		case WM_SETTINGCHANGE:
			wEditor.Call(WM_SETTINGCHANGE, wParam, lParam);
			wOutput.Call(WM_SETTINGCHANGE, wParam, lParam);
			break;

		case WM_SYSCOLORCHANGE:
			wEditor.Call(WM_SYSCOLORCHANGE, wParam, lParam);
			wOutput.Call(WM_SYSCOLORCHANGE, wParam, lParam);
			break;

		case WM_ACTIVATEAPP:
			wEditor.Call(SCI_HIDESELECTION, !wParam);
			// Do not want to display dialog yet as may be in middle of system mouse capture
			::PostMessage(MainHWND(), WM_COMMAND, IDM_ACTIVATE, wParam);
			break;

		case WM_ACTIVATE:
			if (wParam != WA_INACTIVE) {
				if (searchStrip.visible)
					searchStrip.Focus();
				else if (findStrip.visible)
					findStrip.Focus();
				else if (replaceStrip.visible)
					replaceStrip.Focus();
				else if (userStrip.visible)
					userStrip.Focus();
				else
					::SetFocus(wFocus);
			}
			break;

		case WM_TIMER:
			OnTimer();
			break;

		case WM_DROPFILES:
			DropFiles(reinterpret_cast<HDROP>(wParam));
			break;

		case WM_COPYDATA:
			return uniqueInstance.CopyData(reinterpret_cast<COPYDATASTRUCT *>(lParam));

		default:
			return ::DefWindowProcW(MainHWND(), iMessage, wParam, lParam);
		}
	} catch (GUI::ScintillaFailure &sf) {
		statusFailure = static_cast<int>(sf.status);
	}
	if ((statusFailure > 0) && (boxesVisible == 0)) {
		boxesVisible++;
		char buff[200];
		if (statusFailure == SC_STATUS_BADALLOC) {
			strcpy(buff, "Memory exhausted.");
		} else {
			sprintf(buff, "Scintilla failed with status %d.", statusFailure);
		}
		strcat(buff, " SciTE will now close.");
		GUI::gui_string sMessage = GUI::StringFromUTF8(buff);
		::MessageBox(MainHWND(), sMessage.c_str(), TEXT("Failure in Scintilla"), MB_OK | MB_ICONERROR | MB_APPLMODAL);
		exit(FALSE);
	}
	return 0l;
}

// Take care of 32/64 bit pointers
#ifdef GetWindowLongPtr
static void *PointerFromWindow(HWND hWnd) {
	return reinterpret_cast<void *>(::GetWindowLongPtr(hWnd, 0));
}
static void SetWindowPointer(HWND hWnd, void *ptr) {
	::SetWindowLongPtr(hWnd, 0, reinterpret_cast<LONG_PTR>(ptr));
}
#else
static void *PointerFromWindow(HWND hWnd) {
	return reinterpret_cast<void *>(::GetWindowLong(hWnd, 0));
}
static void SetWindowPointer(HWND hWnd, void *ptr) {
	::SetWindowLong(hWnd, 0, reinterpret_cast<LONG>(ptr));
}
#endif

LRESULT PASCAL SciTEWin::TWndProc(
    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {

	// Find C++ object associated with window.
	SciTEWin *scite = reinterpret_cast<SciTEWin *>(PointerFromWindow(hWnd));
	// scite will be zero if WM_CREATE not seen yet
	if (scite == 0) {
		if (iMessage == WM_CREATE) {
			LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			scite = reinterpret_cast<SciTEWin *>(cs->lpCreateParams);
			scite->wSciTE = hWnd;
			SetWindowPointer(hWnd, scite);
			return scite->WndProc(iMessage, wParam, lParam);
		} else
			return ::DefWindowProcW(hWnd, iMessage, wParam, lParam);
	} else
		return scite->WndProc(iMessage, wParam, lParam);
}

LRESULT ContentWin::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	try {
	switch (iMessage) {

	case WM_CREATE:
		pSciTEWin->wContent = GetID();
		return ::DefWindowProc(Hwnd(), iMessage, wParam, lParam);

	case WM_COMMAND:
	case WM_NOTIFY:
		return pSciTEWin->WndProc(iMessage, wParam, lParam);

	case WM_PAINT: {
			PAINTSTRUCT ps;
			::BeginPaint(Hwnd(), &ps);
			GUI::Rectangle rcPaint(ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
			Paint(ps.hdc, rcPaint);
			::EndPaint(Hwnd(), &ps);
			return 0;
		}

	case WM_ERASEBKGND: {
			RECT rc = {0, 0, 2000, 2000};
			HBRUSH hbrFace = CreateSolidBrush(::GetSysColor(COLOR_3DFACE));
			::FillRect(reinterpret_cast<HDC>(wParam), &rc, hbrFace);
			::DeleteObject(hbrFace);
			return 0;
		}

	case WM_LBUTTONDOWN:
		pSciTEWin->ptStartDrag = PointFromLong(lParam);
		capturedMouse = true;
		pSciTEWin->heightOutputStartDrag = pSciTEWin->heightOutput;
		::SetCapture(Hwnd());
		break;

	case WM_MOUSEMOVE:
		if (capturedMouse) {
			pSciTEWin->MoveSplit(PointFromLong(lParam));
		}
		break;

	case WM_LBUTTONUP:
		if (capturedMouse) {
			pSciTEWin->MoveSplit(PointFromLong(lParam));
			capturedMouse = false;
			::ReleaseCapture();
		}
		break;

	case WM_CAPTURECHANGED:
		capturedMouse = false;
		break;

	case WM_SETCURSOR:
		if (ControlIDOfCommand(static_cast<unsigned long>(lParam)) == HTCLIENT) {
			GUI::Point ptCursor;
			::GetCursorPos(reinterpret_cast<POINT *>(&ptCursor));
			GUI::Point ptClient = ptCursor;
			::ScreenToClient(pSciTEWin->MainHWND(), reinterpret_cast<POINT *>(&ptClient));
			GUI::Rectangle rcScintilla = pSciTEWin->wEditor.GetPosition();
			GUI::Rectangle rcOutput = pSciTEWin->wOutput.GetPosition();
			if (!rcScintilla.Contains(ptCursor) && !rcOutput.Contains(ptCursor)) {
				::SetCursor(::LoadCursor(NULL, pSciTEWin->splitVertical ? IDC_SIZEWE : IDC_SIZENS));
				return TRUE;
			}
		}
		return ::DefWindowProc(Hwnd(), iMessage, wParam, lParam);

	default:
		return ::DefWindowProc(Hwnd(), iMessage, wParam, lParam);

	}
	} catch (...) {
	}
	return 0l;
}

static void SetFontHandle(GUI::Window &w, HFONT hfont) {
	::SendMessage(HwndOf(w),
		WM_SETFONT, reinterpret_cast<WPARAM>(hfont),
		0);    // redraw option
}

static int WidthText(HFONT hfont, const GUI::gui_char *text) {
	HDC hdcMeasure = ::CreateCompatibleDC(NULL);
	HFONT hfontOriginal = static_cast<HFONT>(::SelectObject(hdcMeasure, hfont));
	RECT rcText = {0,0, 2000, 2000};
	::DrawText(hdcMeasure, const_cast<LPWSTR>(text), -1, &rcText, DT_CALCRECT);
	int width = rcText.right - rcText.left;
	::SelectObject(hdcMeasure, hfontOriginal);
	::DeleteDC(hdcMeasure);
	return width;
}

static int WidthControl(GUI::Window &w) {
	GUI::Rectangle rc = w.GetPosition();
	return rc.Width();
}

static GUI::gui_string ControlGText(GUI::Window w) {
	HWND wT = HwndOf(w);
	int len = ::GetWindowTextLengthW(wT) + 1;
	std::vector<GUI::gui_char> itemText(len);
	GUI::gui_string gsText;
	if (::GetWindowTextW(wT, &itemText[0], len)) {
		gsText = GUI::gui_string(&itemText[0]);
	}
	return gsText;
}

static SString ControlText(GUI::Window w) {
	HWND wT = HwndOf(w);
	int len = ::GetWindowTextLengthW(wT) + 1;
	std::vector<GUI::gui_char> itemText(len);
	GUI::gui_string gsFind;
	if (::GetWindowText(wT, &itemText[0], len)) {
		gsFind = GUI::gui_string(&itemText[0]);
	}
	return GUI::UTF8FromString(gsFind.c_str()).c_str();
}

static const char *textFindPrompt = "Fi&nd:";
static const char *textReplacePrompt = "Rep&lace:";
static const char *textFindNext = "&Find Next";
static const char *textMarkAll = "&Mark All";

static const char *textReplace = "&Replace";
static const char *textReplaceAll = "Replace &All";
static const char *textInSelection = "In &Selection";

static SearchOption toggles[] = {
	{"Match &whole word only", IDM_WHOLEWORD, IDWHOLEWORD},
	{"Case sensiti&ve", IDM_MATCHCASE, IDMATCHCASE},
	{"Regular &expression", IDM_REGEXP, IDREGEXP},
	{"Transform &backslash expressions", IDM_UNSLASH, IDUNSLASH},
	{"Wrap ar&ound", IDM_WRAPAROUND, IDWRAP},
	{"&Up", IDM_DIRECTIONUP, IDDIRECTIONUP},
	{0, 0, 0},
};

GUI::Window Strip::CreateText(const char *text) {
	GUI::gui_string localised = localiser->Text(text);
	int width = WidthText(fontText, localised.c_str()) + 4;
	GUI::Window w;
	w.SetID(::CreateWindowEx(0, TEXT("Static"), localised.c_str(),
		WS_CHILD | WS_CLIPSIBLINGS | SS_RIGHT,
		2, 2, width, 21,
		Hwnd(), reinterpret_cast<HMENU>(0), ::GetModuleHandle(NULL), 0));
	SetFontHandle(w, fontText);
	w.Show();
	return w;
}

#define PACKVERSION(major,minor) MAKELONG(minor,major)

static DWORD GetVersion(LPCTSTR lpszDllName) {
    DWORD dwVersion = 0;
    HINSTANCE hinstDll = ::LoadLibrary(lpszDllName);
    if (hinstDll) {
        DLLGETVERSIONPROC pDllGetVersion = (DLLGETVERSIONPROC)::GetProcAddress(hinstDll, "DllGetVersion");

        if (pDllGetVersion) {
            DLLVERSIONINFO dvi;
            ::ZeroMemory(&dvi, sizeof(dvi));
            dvi.cbSize = sizeof(dvi);

            HRESULT hr = (*pDllGetVersion)(&dvi);
            if (SUCCEEDED(hr)) {
               dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
            }
        }
        ::FreeLibrary(hinstDll);
    }
    return dwVersion;
}

GUI::Window Strip::CreateButton(const char *text, int ident, bool check) {
	GUI::gui_string localised = localiser->Text(text);
	int width = WidthText(fontText, localised.c_str());
	int height = 19 + 2 * ::GetSystemMetrics(SM_CYEDGE);
	if (check) {
		width += 6;
		int checkSize = ::GetSystemMetrics(SM_CXMENUCHECK);
		width += checkSize;
	} else {
		width += 2 * ::GetSystemMetrics(SM_CXEDGE);	// Allow for 3D borders
		width += 2 * WidthText(fontText, TEXT(" "));	// Allow a bit of space
	}
	if (check) {
		height = 16 + 3 * 2;
		width = 16 + 3 * 2;
	}
	GUI::Window w;
	w.SetID(::CreateWindowEx(0, TEXT("Button"), localised.c_str(),
		WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS |
		(check ? (BS_AUTOCHECKBOX | BS_PUSHLIKE | BS_BITMAP) : BS_PUSHBUTTON),
		2, 2, width, height,
		Hwnd(), reinterpret_cast<HMENU>(ident), ::GetModuleHandle(NULL), 0));
	if (check) {
		int resNum = 0;
		switch (ident) {
		case IDWHOLEWORD: resNum = IDBM_WORD; break;
		case IDMATCHCASE: resNum = IDBM_CASE; break;
		case IDREGEXP: resNum = IDBM_REGEX; break;
		case IDUNSLASH: resNum = IDBM_BACKSLASH; break;
		case IDWRAP: resNum = IDBM_AROUND; break;
		case IDDIRECTIONUP: resNum = IDBM_UP; break;
		}

		UINT flags = (GetVersion(TEXT("COMCTL32")) >= PACKVERSION(6,0)) ?
			(LR_DEFAULTSIZE) : (LR_DEFAULTSIZE|LR_LOADMAP3DCOLORS);
		HBITMAP bm = static_cast<HBITMAP>(::LoadImage(
			::GetModuleHandle(NULL), MAKEINTRESOURCE(resNum), IMAGE_BITMAP,
			16, 16, flags));

		::SendMessage(reinterpret_cast<HWND>(w.GetID()),
			BM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(bm));
	}
	SetFontHandle(w, fontText);
	w.Show();
#ifdef BCM_GETIDEALSIZE
	if (!check) {
		// Push buttons can be measured with BCM_GETIDEALSIZE
		SIZE sz = {0, 0};
		::SendMessage(reinterpret_cast<HWND>(w.GetID()),
			BCM_GETIDEALSIZE, 0, reinterpret_cast<LPARAM>(&sz));
		if (sz.cx > 0) {
			GUI::Rectangle rc(0,0, sz.cx + 2 * WidthText(fontText, TEXT(" ")), sz.cy);
			w.SetPosition(rc);
		}
	}
#endif
	TOOLINFO toolInfo;
	memset(&toolInfo, 0, sizeof(toolInfo));
	toolInfo.cbSize = sizeof(toolInfo);
	toolInfo.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
	toolInfo.hinst = ::GetModuleHandle(NULL);
	toolInfo.hwnd = Hwnd();
	toolInfo.uId = (UINT_PTR)w.GetID();
	toolInfo.lpszText = LPSTR_TEXTCALLBACK;
	::GetClientRect(Hwnd(), &toolInfo.rect);
	::SendMessageW(static_cast<HWND>(wToolTip.GetID()), TTM_ADDTOOLW,
		0, (LPARAM) &toolInfo);
	::SendMessage(static_cast<HWND>(wToolTip.GetID()), TTM_ACTIVATE, TRUE, 0);
	return w;
}

void Strip::Tab(bool forwards) {
	HWND wToFocus = ::GetNextDlgTabItem(Hwnd(), ::GetFocus(), !forwards);
	if (wToFocus)
		::SetFocus(wToFocus);
}

void Strip::Creation() {
	// Use the message box font for the strip's font
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(ncm);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, FALSE);
	fontText = ::CreateFontIndirect(&ncm.lfMessageFont);

	wToolTip = ::CreateWindowEx(0,
		TOOLTIPS_CLASSW, NULL,
		WS_POPUP | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		Hwnd(), NULL, ::GetModuleHandle(NULL), NULL);

	SetTheme();
}

void Strip::Destruction() {
	if (fontText)
		::DeleteObject(fontText);
	fontText = 0;
#ifdef THEME_AVAILABLE
	if (hTheme)
		::CloseThemeData(hTheme);
#endif
	hTheme = 0;
}

void Strip::Close() {
	visible = false;
}

bool Strip::KeyDown(WPARAM key) {
	if (!visible)
		return false;
	switch (key) {
	case VK_MENU:
		::SendMessage(Hwnd(), WM_UPDATEUISTATE, (UISF_HIDEACCEL|UISF_HIDEFOCUS) << 16 | UIS_CLEAR, 0);
		return false;
	case VK_TAB:
		if (IsChild(Hwnd(), ::GetFocus())) {
			::SendMessage(Hwnd(), WM_UPDATEUISTATE, (UISF_HIDEACCEL|UISF_HIDEFOCUS) << 16 | UIS_CLEAR, 0);
			Tab((::GetKeyState(VK_SHIFT) & 0x80000000) == 0);
			return true;
		} else {
			return false;
		}
	case VK_ESCAPE:
		Close();
		return true;
	default:
		if ((::GetKeyState(VK_MENU) & 0x80000000) != 0) {
			HWND wChild = ::GetWindow(Hwnd(), GW_CHILD);
			while (wChild) {
				enum { capSize = 2000 };
				GUI::gui_char className[capSize];
				::GetClassName(wChild, className, capSize);
				if ((wcscmp(className, TEXT("Button")) == 0) ||
					(wcscmp(className, TEXT("Static")) == 0)) {
					GUI::gui_char caption[capSize];
					::GetWindowText(wChild, caption, capSize);
					for (int i=0; caption[i]; i++) {
						if ((caption[i] == L'&') && (toupper(caption[i+1]) == static_cast<int>(key))) {
							if (wcscmp(className, TEXT("Button")) == 0) {
								::SendMessage(wChild, BM_CLICK, 0, 0);
							} else {	// Static caption
								wChild = ::GetWindow(wChild, GW_HWNDNEXT);
								::SetFocus(wChild);
							}
							return true;
						}
					}
				}
				wChild = ::GetWindow(wChild, GW_HWNDNEXT);
			};
		}
	}
	return false;
}

bool Strip::Command(WPARAM) {
	return false;
}

void Strip::Size() {
}

void Strip::Paint(HDC hDC) {
	GUI::Rectangle rcStrip = GetClientPosition();
	RECT rc = {rcStrip.left, rcStrip.top, rcStrip.right, rcStrip.bottom};
	HBRUSH hbrFace = CreateSolidBrush(::GetSysColor(COLOR_3DFACE));
	::FillRect(hDC, &rc, hbrFace);
	::DeleteObject(hbrFace);

	if (HasClose()){
		// Draw close box
		GUI::Rectangle rcClose = CloseArea();
		if (hTheme) {
#ifdef THEME_AVAILABLE
			int closeAppearence = CBS_NORMAL;
			if (closeState == csOver) {
				closeAppearence = CBS_HOT;
			} else if (closeState == csClickedOver) {
				closeAppearence = CBS_PUSHED;
			}
			//DrawThemeBackground(htheme, hDC, WP_CLOSEBUTTON, closeAppearence,
			//	reinterpret_cast<RECT *>(&rcClose), reinterpret_cast<RECT *>(&rcClose));
			::DrawThemeBackground(hTheme, hDC, WP_SMALLCLOSEBUTTON, closeAppearence,
				reinterpret_cast<RECT *>(&rcClose), NULL);
			//::DrawThemeBackground(hTheme, hDC, WP_MDICLOSEBUTTON, closeAppearence,
			//	reinterpret_cast<RECT *>(&rcClose), NULL);
#endif
		} else {
			int closeAppearence = 0;
			if (closeState == csOver) {
				closeAppearence = DFCS_HOT;
			} else if (closeState == csClickedOver) {
				closeAppearence = DFCS_PUSHED;
			}

			DrawFrameControl(hDC, reinterpret_cast<RECT *>(&rcClose), DFC_CAPTION,
				DFCS_CAPTIONCLOSE | closeAppearence);
		}
	}
}

bool Strip::HasClose() const {
	return true;
}

GUI::Rectangle Strip::CloseArea() {
	if (HasClose()) {
		GUI::Rectangle rcClose = GetClientPosition();
		rcClose.right -= 2;
		rcClose.left = rcClose.right - closeSize.cx;
		rcClose.top += 2;
		rcClose.bottom = rcClose.top + closeSize.cy;
		return rcClose;
	} else {
		return GUI::Rectangle(-1,-1,-1,-1);
	}
}

void Strip::InvalidateClose() {
	GUI::Rectangle rcClose = CloseArea();
	RECT rc = {
		rcClose.left,
		rcClose.top,
		rcClose.right,
		rcClose.bottom};
	::InvalidateRect(Hwnd(), &rc, TRUE);
}

bool Strip::MouseInClose(GUI::Point pt) {
	GUI::Rectangle rcClose = CloseArea();
	return rcClose.Contains(pt);
}

void Strip::TrackMouse(GUI::Point pt) {
	stripCloseState closeStateStart = closeState;
	if (MouseInClose(pt)) {
		if (closeState == csNone)
			closeState = csOver;
		if (closeState == csClicked)
			closeState = csClickedOver;
	} else {
		if (closeState == csOver)
			closeState = csNone;
		if (closeState == csClickedOver)
			closeState = csClicked;
	}
	if ((closeState != csNone) && !capturedMouse) {
		TRACKMOUSEEVENT tme;
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = Hwnd();
		TrackMouseEvent(&tme);
	}
	if (closeStateStart != closeState)
		InvalidateClose();
}

void Strip::SetTheme() {
#ifdef THEME_AVAILABLE
	if (hTheme)
		::CloseThemeData(hTheme);
	hTheme = ::OpenThemeData(Hwnd(), TEXT("Window"));
	if (hTheme) {
		HRESULT hr = ::GetThemePartSize(hTheme, NULL, WP_SMALLCLOSEBUTTON, CBS_NORMAL,
			NULL, TS_TRUE, &closeSize);
		//HRESULT hr = ::GetThemePartSize(hTheme, NULL, WP_MDICLOSEBUTTON, CBS_NORMAL,
		//	NULL, TS_TRUE, &closeSize);
		if (!SUCCEEDED(hr)) {
			closeSize.cx = 11;
			closeSize.cy = 11;
		}
	}
#endif
}

static bool HideKeyboardCues() {
	BOOL b;
	::SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &b, 0);
	return !b;
}

LRESULT Strip::CustomDraw(NMHDR *pnmh) {
	int btnStyle = ::GetWindowLong(pnmh->hwndFrom, GWL_STYLE);
	if ((btnStyle & BS_AUTOCHECKBOX) != BS_AUTOCHECKBOX) {
		return CDRF_DODEFAULT;
	}
#ifdef THEME_AVAILABLE
	LPNMCUSTOMDRAW pcd = reinterpret_cast<LPNMCUSTOMDRAW>(pnmh);
	if (pcd->dwDrawStage == CDDS_PREERASE) {
		::DrawThemeParentBackground(pnmh->hwndFrom, pcd->hdc, &pcd->rc);
	}

	if ((pcd->dwDrawStage == CDDS_PREERASE) || (pcd->dwDrawStage == CDDS_PREPAINT)) {
		HTHEME hThemeButton = ::OpenThemeData(pnmh->hwndFrom, TEXT("Toolbar"));
		if (!hThemeButton) {
			return CDRF_DODEFAULT;
		}
		bool checked = ::SendMessage(pnmh->hwndFrom, BM_GETCHECK, 0, 0) == BST_CHECKED;

		int buttonAppearence = checked ? TS_CHECKED : TS_NORMAL;
		if (pcd->uItemState & CDIS_SELECTED)
			buttonAppearence = TS_PRESSED;
		else if (pcd->uItemState & CDIS_HOT)
			buttonAppearence = checked ? TS_HOTCHECKED : TS_HOT;
		HBRUSH hbrFace = CreateSolidBrush(::GetSysColor(COLOR_3DFACE));
		::FillRect(pcd->hdc, &pcd->rc, hbrFace);
		::DeleteObject(hbrFace);
		::DrawThemeBackground(hThemeButton, pcd->hdc, TP_BUTTON, buttonAppearence,
			&pcd->rc, NULL);

		RECT rcButton = pcd->rc;
		rcButton.bottom--;
		::GetThemeBackgroundContentRect(hThemeButton, pcd->hdc, TP_BUTTON,
			buttonAppearence, &pcd->rc, &rcButton);

		HBITMAP hBitmap = reinterpret_cast<HBITMAP>(::SendMessage(
			pnmh->hwndFrom, BM_GETIMAGE, IMAGE_BITMAP, 0));

		// Retrieve the bitmap dimensions
		BITMAPINFO rbmi;
		memset(&rbmi, 0, sizeof (BITMAPINFO));
		rbmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
		::GetDIBits(pcd->hdc, hBitmap, 0, 0, NULL, &rbmi, DIB_RGB_COLORS);

		DWORD colourTransparent = RGB(0xC0,0xC0,0xC0);

		// Offset from button edge to contents.
		int xOffset = ((rcButton.right - rcButton.left) - rbmi.bmiHeader.biWidth) / 2 + 1;
		int yOffset = ((rcButton.bottom - rcButton.top) - rbmi.bmiHeader.biHeight) / 2;

		HDC hdcBM = ::CreateCompatibleDC(NULL);
		HBITMAP hbmOriginal = static_cast<HBITMAP>(::SelectObject(hdcBM, hBitmap));
		::TransparentBlt(reinterpret_cast<HDC>(pcd->hdc), xOffset, yOffset,
			rbmi.bmiHeader.biWidth, rbmi.bmiHeader.biHeight,
			hdcBM, 0, 0, rbmi.bmiHeader.biWidth, rbmi.bmiHeader.biHeight, colourTransparent);
		::SelectObject(hdcBM, hbmOriginal);
		::DeleteDC(hdcBM);

		if (pcd->uItemState & CDIS_FOCUS) {
			// Draw focus rectangle
			rcButton.left += 2;
			rcButton.top += 3;
			rcButton.right -= 2;
			rcButton.bottom -= 3;
			::DrawFocusRect(pcd->hdc, &rcButton);
		}
		::CloseThemeData(hThemeButton);
		return CDRF_SKIPDEFAULT;
	}
#endif
	return CDRF_DODEFAULT;
}

LRESULT Strip::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	switch (iMessage) {
	case WM_CREATE:
		Creation();
		return ::DefWindowProc(Hwnd(), iMessage, wParam, lParam);

	case WM_DESTROY:
		Destruction();
		return ::DefWindowProc(Hwnd(), iMessage, wParam, lParam);

	case WM_COMMAND:
		if (Command(wParam))
			return 0;
		else
			return ::DefWindowProc(Hwnd(), iMessage, wParam, lParam);

	case WM_SETCURSOR:
		::SetCursor(::LoadCursor(NULL, IDC_ARROW));
		break;

	case WM_SIZE:
		Size();
		break;

	case WM_WINDOWPOSCHANGED:
		if ((reinterpret_cast<WINDOWPOS *>(lParam)->flags & SWP_SHOWWINDOW) && HideKeyboardCues())
			::SendMessage(Hwnd(), WM_UPDATEUISTATE, (UISF_HIDEACCEL|UISF_HIDEFOCUS) << 16 | UIS_SET, 0);
		return ::DefWindowProc(Hwnd(), iMessage, wParam, lParam);

	case WM_PAINT: {
			PAINTSTRUCT ps;
			::BeginPaint(Hwnd(), &ps);
			Paint(ps.hdc);
			::EndPaint(Hwnd(), &ps);
			return 0;
		}

#ifdef THEME_AVAILABLE
	case WM_THEMECHANGED:
		SetTheme();
		break;
#endif

	case WM_LBUTTONDOWN:
		if (MouseInClose(PointFromLong(lParam))) {
			closeState = csClickedOver;
			InvalidateClose();
			capturedMouse = true;
			::SetCapture(Hwnd());
		}
		break;

	case WM_MOUSEMOVE:
		TrackMouse(PointFromLong(lParam));
		break;

	case WM_LBUTTONUP:
		if (capturedMouse) {
			if (MouseInClose(PointFromLong(lParam))) {
				Close();
			}
			capturedMouse = false;
			closeState = csNone;
			InvalidateClose();
			::ReleaseCapture();
		}
		break;

	case WM_MOUSELEAVE:
		if (!capturedMouse) {
			closeState = csNone;
			InvalidateClose();
		}
		break;

	case WM_NOTIFY: {
			NMHDR *pnmh = reinterpret_cast<LPNMHDR>(lParam);
			if (pnmh->code == static_cast<unsigned int>(NM_CUSTOMDRAW)) {
				return CustomDraw(pnmh);
			} else if (pnmh->code == static_cast<unsigned int>(TTN_GETDISPINFO)) {
				NMTTDISPINFOW *pnmtdi = (LPNMTTDISPINFO) lParam;
				int idButton = static_cast<int>(
					(pnmtdi->uFlags & TTF_IDISHWND) ?
					::GetDlgCtrlID(reinterpret_cast<HWND>(pnmtdi->hdr.idFrom)) : pnmtdi->hdr.idFrom);
				for (size_t i=0; toggles[i].label; i++) {
					if (toggles[i].id == idButton) {
						GUI::gui_string localised = localiser->Text(toggles[i].label);
						wcscpy(pnmtdi->szText, localised.c_str());
					}
				}
				return 0;
			} else {
				return ::DefWindowProc(Hwnd(), iMessage, wParam, lParam);
			}
		}

	default:
		return ::DefWindowProc(Hwnd(), iMessage, wParam, lParam);
	}

	return 0l;
}

void BackgroundStrip::Creation() {
	Strip::Creation();

	wExplanation = ::CreateWindowEx(0, TEXT("Static"), TEXT(""),
		WS_CHILD | WS_CLIPSIBLINGS,
		2, 2, 100, 21,
		Hwnd(), reinterpret_cast<HMENU>(0), ::GetModuleHandle(NULL), 0);
	wExplanation.Show();
	SetFontHandle(wExplanation, fontText);

	wProgress = ::CreateWindowEx(0, PROGRESS_CLASS, TEXT(""),
		WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
		2, 2, 100, 21,
		Hwnd(), reinterpret_cast<HMENU>(0), ::GetModuleHandle(NULL), 0);
}

void BackgroundStrip::Destruction() {
	Strip::Destruction();
}

void BackgroundStrip::Close() {
	entered++;
	::SetWindowText(HwndOf(wExplanation), TEXT(""));
	entered--;
	Strip::Close();
}

void BackgroundStrip::Size() {
	if (!visible)
		return;
	Strip::Size();
	GUI::Rectangle rcArea = GetPosition();

	rcArea.bottom -= rcArea.top;
	rcArea.right -= rcArea.left;

	rcArea.left = 2;
	rcArea.top = 2;
	rcArea.right -= 2;
	rcArea.bottom -= 2;

	rcArea.right -= closeSize.cx + 2;	// Allow for close box and gap

	const int progWidth = 200;

	GUI::Rectangle rcProgress = rcArea;
	rcProgress.right = rcProgress.left + progWidth;
	wProgress.SetPosition(rcProgress);

	GUI::Rectangle rcExplanation = rcArea;
	rcExplanation.left += progWidth + 8;
	rcExplanation.top -= 1;
	rcExplanation.bottom += 1;
	wExplanation.SetPosition(rcExplanation);

	::InvalidateRect(Hwnd(), NULL, TRUE);
}

bool BackgroundStrip::HasClose() const {
	return false;
}

void BackgroundStrip::Focus() {
	::SetFocus(HwndOf(wExplanation));
}

bool BackgroundStrip::KeyDown(WPARAM key) {
	if (!visible)
		return false;
	if (Strip::KeyDown(key))
		return true;

	return false;
}

bool BackgroundStrip::Command(WPARAM /* wParam */) {
	return false;
}

LRESULT BackgroundStrip::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	try {

	return Strip::WndProc(iMessage, wParam, lParam);

	} catch (...) {
	}
	return 0l;
}

void BackgroundStrip::SetProgress(const GUI::gui_string &explanation, int size, int progress) {
	if (explanation != ControlGText(wExplanation)) {
		::SetWindowTextW(HwndOf(wExplanation), explanation.c_str());
	}
	::SendMessage(HwndOf(wProgress), PBM_SETRANGE32, 0, size);
	::SendMessage(HwndOf(wProgress), PBM_SETPOS, progress, 0);
}

void SearchStrip::Creation() {
	Strip::Creation();

	wStaticFind = CreateText(textFindPrompt);

	wText = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"), TEXT(""),
		WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | ES_AUTOHSCROLL,
		50, 2, 300, 21,
		Hwnd(), reinterpret_cast<HMENU>(IDC_INCFINDTEXT), ::GetModuleHandle(NULL), 0);
	wText.Show();

	SetFontHandle(wText, fontText);

	wButton = CreateButton(textFindNext, IDC_INCFINDBTNOK);

	GUI::Rectangle rcButton = wButton.GetPosition();
	lineHeight = rcButton.Height() + 3;
}

void SearchStrip::Destruction() {
	Strip::Destruction();
}

void SearchStrip::SetSearcher(Searcher *pSearcher_) {
	pSearcher = pSearcher_;
}

void SearchStrip::Close() {
	entered++;
	::SetWindowText(HwndOf(wText), TEXT(""));
	entered--;
	Strip::Close();
	pSearcher->UIClosed();
}

void SearchStrip::Size() {
	if (!visible)
		return;
	Strip::Size();
	GUI::Rectangle rcArea = GetPosition();

	rcArea.bottom -= rcArea.top;
	rcArea.right -= rcArea.left;

	rcArea.left = 2;
	rcArea.top = 2;
	rcArea.right -= 2;
	rcArea.bottom -= 2;

	rcArea.right -= closeSize.cx + 2;	// Allow for close box and gap

	GUI::Rectangle rcButton = rcArea;
	rcButton.top -= 1;
	rcButton.bottom += 1;
	rcButton.left = rcButton.right - WidthControl(wButton);
	wButton.SetPosition(rcButton);

	GUI::Rectangle rcText = rcArea;
	rcText.left = WidthControl(wStaticFind) + 8;
	rcText.right = rcButton.left - 4;
	wText.SetPosition(rcText);

	rcText.right = rcText.left - 4;
	rcText.left = 4;
	rcText.top = rcArea.top + 3;

	wStaticFind.SetPosition(rcText);

	::InvalidateRect(Hwnd(), NULL, TRUE);
}

void SearchStrip::Paint(HDC hDC) {
	Strip::Paint(hDC);
}

void SearchStrip::Focus() {
	::SetFocus(HwndOf(wText));
}

bool SearchStrip::KeyDown(WPARAM key) {
	if (!visible)
		return false;
	if (Strip::KeyDown(key))
		return true;

	if (key == VK_RETURN) {
		if (IsChild(Hwnd(), ::GetFocus())) {
			Next(false);
			return true;
		}
	}

	return false;
}

void SearchStrip::Next(bool select) {
	SString ffLastWhat = pSearcher->findWhat;
	pSearcher->findWhat = ControlText(wText);

	if (select) {
		if (ffLastWhat.length()) {
			pSearcher->MoveBack(static_cast<int>(ffLastWhat.length()));
		}
	}
	pSearcher->wholeWord = false;
	if (pSearcher->FindHasText())
		pSearcher->FindNext(false, false);
 	if ((!pSearcher->havefound) &&
		strncmp(pSearcher->findWhat.c_str(), ffLastWhat.c_str(), ffLastWhat.length()) == 0) {
		// Could not find string with added character so revert to previous value.
		pSearcher->findWhat = ffLastWhat;
		entered++;
		GUI::gui_string gsPrevious = GUI::StringFromUTF8(ffLastWhat.c_str());
		::SetWindowText(HwndOf(wText), gsPrevious.c_str());
		::SendMessage(HwndOf(wText), EM_SETSEL, gsPrevious.length(), gsPrevious.length());
		entered--;
	}
}

bool SearchStrip::Command(WPARAM wParam) {
	if (entered)
		return false;
	int control = ControlIDOfWParam(wParam);
	int subCommand = static_cast<int>(wParam >> 16);
	if (((control == IDC_INCFINDTEXT) && (subCommand == EN_CHANGE)) ||
		(control == IDC_INCFINDBTNOK)) {
		Next(control != IDC_INCFINDBTNOK);
		return true;
	}
	return false;
}

LRESULT SearchStrip::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	try {

	return Strip::WndProc(iMessage, wParam, lParam);

	/*
	switch (iMessage) {

	default:

	}
	*/
	} catch (...) {
	}
	return 0l;
}

void FindStrip::Creation() {
	Strip::Creation();

	wStaticFind = CreateText(textFindPrompt);

	wText = CreateWindowEx(0, TEXT("ComboBox"), TEXT(""),
		WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | CBS_DROPDOWN | CBS_AUTOHSCROLL,
		50, 2, 300, 80,
		Hwnd(), reinterpret_cast<HMENU>(IDFINDWHAT), ::GetModuleHandle(NULL), 0);
	SetFontHandle(wText, fontText);
	wText.Show();

	GUI::Rectangle rcCombo = wText.GetPosition();
	lineHeight = rcCombo.Height() + 3;

	wButton = CreateButton(textFindNext, IDOK);
	wButtonMarkAll = CreateButton(textMarkAll, IDMARKALL);

	wCheckWord = CreateButton(toggles[SearchOption::tWord].label, toggles[SearchOption::tWord].id, true);
	wCheckCase = CreateButton(toggles[SearchOption::tCase].label, toggles[SearchOption::tCase].id, true);
	wCheckRE = CreateButton(toggles[SearchOption::tRegExp].label, toggles[SearchOption::tRegExp].id, true);
	wCheckBE = CreateButton(toggles[SearchOption::tBackslash].label, toggles[SearchOption::tBackslash].id, true);
	wCheckWrap = CreateButton(toggles[SearchOption::tWrap].label, toggles[SearchOption::tWrap].id, true);
	wCheckUp = CreateButton(toggles[SearchOption::tUp].label, toggles[SearchOption::tUp].id, true);
}

void FindStrip::Destruction() {
	Strip::Destruction();
}

void FindStrip::SetSearcher(Searcher *pSearcher_) {
	pSearcher = pSearcher_;
}

void FindStrip::Size() {
	if (!visible)
		return;
	Strip::Size();
	GUI::Rectangle rcArea = GetPosition();

	rcArea.bottom -= rcArea.top;
	rcArea.right -= rcArea.left;

	rcArea.left = 2;
	rcArea.top = 2;
	rcArea.right -= 2;
	rcArea.bottom -= 2;

	rcArea.right -= closeSize.cx + 2;	// Allow for close box and gap

	GUI::Rectangle rcButton = rcArea;
	rcButton.top -= 1;
	rcButton.bottom += 1;

	int checkWidth = rcButton.Height() - 2;	// Using height to make square
	rcButton.left = rcButton.right - checkWidth;
	wCheckUp.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - checkWidth;
	wCheckWrap.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - checkWidth;
	wCheckBE.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - checkWidth;
	wCheckRE.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - checkWidth;
	wCheckCase.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - checkWidth;
	wCheckWord.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - WidthControl(wButtonMarkAll);
	wButtonMarkAll.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - WidthControl(wButton);
	wButton.SetPosition(rcButton);

	GUI::Rectangle rcText = rcArea;
	rcText.bottom += 60;
	rcText.left = WidthControl(wStaticFind) + 8;
	rcText.right = rcButton.left - 4;
	wText.SetPosition(rcText);
	wText.Show();

	rcText.right = rcText.left - 4;
	rcText.left = 4;
	rcText.top = rcArea.top + 3;
	wStaticFind.SetPosition(rcText);

	::InvalidateRect(Hwnd(), NULL, TRUE);
}

void FindStrip::Paint(HDC hDC) {
	Strip::Paint(hDC);
}

void FindStrip::Focus() {
	::SetFocus(HwndOf(wText));
}

void FindStrip::Close() {
	Strip::Close();
	pSearcher->UIClosed();
}

bool FindStrip::KeyDown(WPARAM key) {
	if (!visible)
		return false;
	if (Strip::KeyDown(key))
		return true;
	switch (key) {
	case VK_RETURN:
		if (IsChild(Hwnd(), ::GetFocus())) {
			Next(false, IsKeyDown(VK_SHIFT));
			return true;
		}
	}
	return false;
}

void FindStrip::Next(bool markAll, bool invertDirection) {
	pSearcher->SetFind(ControlText(wText).c_str());
	if (markAll){
		pSearcher->MarkAll();
	}
	pSearcher->FindNext(pSearcher->reverseFind ^ invertDirection);
	if (pSearcher->closeFind) {
		visible = false;
		pSearcher->UIClosed();
	}
}

void FindStrip::AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked) {
	GUI::gui_string localised = localiser->Text(label);
	HMENU menu = reinterpret_cast<HMENU>(popup.GetID());
	if (0 == localised.length())
		::AppendMenu(menu, MF_SEPARATOR, 0, TEXT(""));
	else
		::AppendMenu(menu, MF_STRING | (checked ? MF_CHECKED : 0), cmd, localised.c_str());
}

void FindStrip::ShowPopup() {
	GUI::Menu popup;
	popup.CreatePopUp();
	for (int i=SearchOption::tWord; i<=SearchOption::tUp; i++) {
		AddToPopUp(popup, toggles[i].label, toggles[i].cmd, pSearcher->FlagFromCmd(toggles[i].cmd));
	}
	GUI::Rectangle rcButton = wButton.GetPosition();
	GUI::Point pt(rcButton.left, rcButton.bottom);
	popup.Show(pt, *this);
}

bool FindStrip::Command(WPARAM wParam) {
	if (entered)
		return false;
	int control = ControlIDOfWParam(wParam);
	if ((control == IDOK) || (control == IDMARKALL)) {
		Next(control == IDMARKALL, false);
		return true;
	} else {
		pSearcher->FlagFromCmd(control) = !pSearcher->FlagFromCmd(control);
	}
	return false;
}

LRESULT FindStrip::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	try {
	switch (iMessage) {

	case WM_CONTEXTMENU:
		ShowPopup();
		return 0;

	default:
		return Strip::WndProc(iMessage, wParam, lParam);

	}
	} catch (...) {
	}
	return 0l;
}

void FindStrip::CheckButtons() {
	entered++;
	::SendMessage(reinterpret_cast<HWND>(wCheckWord.GetID()),
		BM_SETCHECK, pSearcher->wholeWord ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(reinterpret_cast<HWND>(wCheckCase.GetID()),
		BM_SETCHECK, pSearcher->matchCase ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(reinterpret_cast<HWND>(wCheckRE.GetID()),
		BM_SETCHECK, pSearcher->regExp ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(reinterpret_cast<HWND>(wCheckWrap.GetID()),
		BM_SETCHECK, pSearcher->wrapFind ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(reinterpret_cast<HWND>(wCheckBE.GetID()),
		BM_SETCHECK, pSearcher->unSlash ? BST_CHECKED : BST_UNCHECKED, 0);
	entered--;
}

void FindStrip::Show() {
	Focus();
	HWND combo = GetDlgItem(Hwnd(), IDFINDWHAT);
	::SendMessage(combo, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < pSearcher->memFinds.Length(); i++) {
		GUI::gui_string gs = GUI::StringFromUTF8(pSearcher->memFinds.At(i).c_str());
		::SendMessageW(combo, CB_ADDSTRING, 0,
					reinterpret_cast<LPARAM>(gs.c_str()));
	}
	//::SendMessage(combo, CB_SETCURSEL, 0, 0);
	::SetWindowText(HwndOf(wText), GUI::StringFromUTF8(pSearcher->findWhat.c_str()).c_str());
	::SendMessage(HwndOf(wText), CB_SETEDITSEL, 0, MAKELPARAM(0, -1));
	CheckButtons();
	pSearcher->ScrollEditorIfNeeded();
}

void ReplaceStrip::Creation() {
	Strip::Creation();

	lineHeight = 23;

	wStaticFind = CreateText(textFindPrompt);

	wText = CreateWindowEx(0, TEXT("ComboBox"), TEXT(""),
		WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | CBS_DROPDOWN | CBS_AUTOHSCROLL,
		50, 2, 300, 80,
		Hwnd(), reinterpret_cast<HMENU>(IDFINDWHAT), ::GetModuleHandle(NULL), 0);
	SetFontHandle(wText, fontText);
	wText.Show();

	GUI::Rectangle rcCombo = wText.GetPosition();
	lineHeight = rcCombo.Height() + 3;

	wStaticReplace = CreateText(textReplacePrompt);

	wReplace = CreateWindowEx(0, TEXT("ComboBox"), TEXT(""),
		WS_CHILD | WS_TABSTOP | CBS_DROPDOWN | CBS_AUTOHSCROLL,
		50, 2, 300, 80,
		Hwnd(), reinterpret_cast<HMENU>(IDREPLACEWITH), ::GetModuleHandle(NULL), 0);
	SetFontHandle(wReplace, fontText);
	wReplace.Show();

	wButtonFind = CreateButton(textFindNext, IDOK);
	wButtonReplace = CreateButton(textReplace, IDREPLACE);

	wButtonReplaceAll = CreateButton(textReplaceAll, IDREPLACEALL);
	wButtonReplaceInSelection = CreateButton(textInSelection, IDREPLACEINSEL);

	wCheckWord = CreateButton(toggles[SearchOption::tWord].label, toggles[SearchOption::tWord].id, true);
	wCheckRE = CreateButton(toggles[SearchOption::tRegExp].label, toggles[SearchOption::tRegExp].id, true);

	wCheckCase = CreateButton(toggles[SearchOption::tCase].label, toggles[SearchOption::tCase].id, true);
	wCheckBE = CreateButton(toggles[SearchOption::tBackslash].label, toggles[SearchOption::tBackslash].id, true);

	wCheckWrap = CreateButton(toggles[SearchOption::tWrap].label, toggles[SearchOption::tWrap].id, true);
}

void ReplaceStrip::Destruction() {
	Strip::Destruction();
}

void ReplaceStrip::SetSearcher(Searcher *pSearcher_) {
	pSearcher = pSearcher_;
}

void ReplaceStrip::Size() {
	if (!visible)
		return;
	Strip::Size();
	GUI::Rectangle rcArea = GetPosition();

	int widthCaption = Maximum(WidthControl(wStaticFind), WidthControl(wStaticReplace));

	rcArea.bottom -= rcArea.top;
	rcArea.right -= rcArea.left;

	rcArea.left = 2;
	rcArea.top = 2;
	rcArea.right -= 2;
	rcArea.bottom -= 4;

	rcArea.right -= closeSize.cx + 2;	// Allow for close box and gap

	GUI::Rectangle rcLine = rcArea;
	rcLine.bottom = rcLine.top + lineHeight - 2;

	int widthButtons = Maximum(WidthControl(wButtonFind), WidthControl(wButtonReplace));
	int widthLastButtons = Maximum(WidthControl(wButtonReplaceAll), WidthControl(wButtonReplaceInSelection));

	GUI::Rectangle rcButton = rcLine;
	rcButton.top -= 1;

	int checkWidth = rcButton.Height() - 2;	// Using height to make square

	// Allow empty slot to match wrap button on next line
	rcButton.right = rcButton.right - (checkWidth + 4);

	rcButton.left = rcButton.right - checkWidth;
	wCheckCase.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - checkWidth;
	wCheckWord.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - widthLastButtons;
	wButtonReplaceAll.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - widthButtons;
	wButtonFind.SetPosition(rcButton);

	GUI::Rectangle rcText = rcLine;
	rcText.bottom += 60;
	rcText.left = widthCaption + 8;
	rcText.right = rcButton.left - 4;
	wText.SetPosition(rcText);

	GUI::Rectangle rcStatic = rcLine;
	rcStatic.right = rcText.left - 4;
	rcStatic.left = 4;
	rcStatic.top = rcLine.top + 3;
	wStaticFind.SetPosition(rcStatic);

	rcLine.top = rcLine.top + lineHeight;
	rcLine.bottom = rcLine.bottom + lineHeight;

	rcButton = rcLine;
	rcButton.top -= 1;

	rcButton.left = rcButton.right - checkWidth;
	wCheckWrap.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - checkWidth;
	wCheckBE.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - checkWidth;
	wCheckRE.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - widthLastButtons;
	wButtonReplaceInSelection.SetPosition(rcButton);

	rcButton.right = rcButton.left - 4;
	rcButton.left = rcButton.right - widthButtons;
	wButtonReplace.SetPosition(rcButton);

	GUI::Rectangle rcReplace = rcLine;
	rcReplace.bottom += 60;
	rcReplace.left = rcText.left;
	rcReplace.right = rcText.right;
	wReplace.SetPosition(rcReplace);

	rcStatic = rcLine;
	rcStatic.right = rcReplace.left - 4;
	rcStatic.left = 4;
	rcStatic.top = rcLine.top + 3;
	wStaticReplace.SetPosition(rcStatic);

	::InvalidateRect(Hwnd(), NULL, TRUE);
}

void ReplaceStrip::Paint(HDC hDC) {
	Strip::Paint(hDC);
}

void ReplaceStrip::Focus() {
	::SetFocus(HwndOf(wText));
}

void ReplaceStrip::Close() {
	Strip::Close();
	pSearcher->UIClosed();
}

static bool IsSameOrChild(GUI::Window &wParent, HWND wChild) {
	HWND hwnd = reinterpret_cast<HWND>(wParent.GetID());
	return (wChild == hwnd) || IsChild(hwnd, wChild);
}

bool ReplaceStrip::KeyDown(WPARAM key) {
	if (!visible)
		return false;
	if (Strip::KeyDown(key))
		return true;
	switch (key) {
	case VK_RETURN:
		if (IsChild(Hwnd(), ::GetFocus())) {
			if (IsSameOrChild(wButtonFind, ::GetFocus()))
				HandleReplaceCommand(IDOK, IsKeyDown(VK_SHIFT));
			else if (IsSameOrChild(wReplace, ::GetFocus()))
				HandleReplaceCommand(IDOK, IsKeyDown(VK_SHIFT));
			else if (IsSameOrChild(wButtonReplace, ::GetFocus()))
				HandleReplaceCommand(IDREPLACE);
			else if (IsSameOrChild(wButtonReplaceAll, ::GetFocus()))
				HandleReplaceCommand(IDREPLACEALL);
			else if (IsSameOrChild(wButtonReplaceInSelection, ::GetFocus()))
				HandleReplaceCommand(IDREPLACEINSEL);
			else
				HandleReplaceCommand(IDOK, IsKeyDown(VK_SHIFT));
			return true;
		}
	}
	return false;
}

void ReplaceStrip::AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked) {
	GUI::gui_string localised = localiser->Text(label);
	HMENU menu = reinterpret_cast<HMENU>(popup.GetID());
	if (0 == localised.length())
		::AppendMenu(menu, MF_SEPARATOR, 0, TEXT(""));
	else
		::AppendMenu(menu, MF_STRING | (checked ? MF_CHECKED : 0), cmd, localised.c_str());
}

void ReplaceStrip::ShowPopup() {
	GUI::Menu popup;
	popup.CreatePopUp();
	for (int i=SearchOption::tWord; i<=SearchOption::tWrap; i++) {
		AddToPopUp(popup, toggles[i].label, toggles[i].cmd, pSearcher->FlagFromCmd(toggles[i].cmd));
	}
	GUI::Rectangle rcButton = wCheckWord.GetPosition();
	GUI::Point pt(rcButton.left, rcButton.bottom);
	popup.Show(pt, *this);
}

void ReplaceStrip::HandleReplaceCommand(int cmd, bool reverseFind) {
	pSearcher->SetFind(ControlText(wText).c_str());
	if (cmd != IDOK) {
		pSearcher->SetReplace(ControlText(wReplace).c_str());
	}
	//int replacements = 0;
	if (cmd == IDOK) {
		if (pSearcher->FindHasText()) {
			pSearcher->FindNext(reverseFind);
		}
	} else if (cmd == IDREPLACE) {
		pSearcher->ReplaceOnce();
	} else if ((cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL)) {
		//~ replacements = pSciTEWin->ReplaceAll(cmd == IDREPLACEINSEL);
		pSearcher->ReplaceAll(cmd == IDREPLACEINSEL);
	}
	//GUI::gui_string replDone = GUI::StringFromInteger(replacements);
	//dlg.SetItemText(IDREPLDONE, replDone.c_str());
}

bool ReplaceStrip::Command(WPARAM wParam) {
	if (entered)
		return false;
	int control = ControlIDOfWParam(wParam);
	switch (control) {

	case IDFINDWHAT:
		switch (HIWORD(wParam)) {
		case CBN_SETFOCUS:
		case CBN_KILLFOCUS:
		case CBN_SELENDCANCEL:
			return false;
		default:
			return false;
		}

	case IDOK:
	case IDREPLACEALL:
	case IDREPLACE:
	case IDREPLACEINSEL:
		HandleReplaceCommand(control);
		return true;

	default:
		pSearcher->FlagFromCmd(control) = !pSearcher->FlagFromCmd(control);
		break;
	}
	CheckButtons();
	return false;
}

LRESULT ReplaceStrip::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	try {
	switch (iMessage) {

	case WM_CONTEXTMENU:
		ShowPopup();
		return 0;

	default:
		return Strip::WndProc(iMessage, wParam, lParam);

	}
	} catch (...) {
	}
	return 0l;
}

void ReplaceStrip::CheckButtons() {
	entered++;
	::SendMessage(reinterpret_cast<HWND>(wCheckWord.GetID()),
		BM_SETCHECK, pSearcher->wholeWord ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(reinterpret_cast<HWND>(wCheckCase.GetID()),
		BM_SETCHECK, pSearcher->matchCase ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(reinterpret_cast<HWND>(wCheckRE.GetID()),
		BM_SETCHECK, pSearcher->regExp ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(reinterpret_cast<HWND>(wCheckWrap.GetID()),
		BM_SETCHECK, pSearcher->wrapFind ? BST_CHECKED : BST_UNCHECKED, 0);
	::SendMessage(reinterpret_cast<HWND>(wCheckBE.GetID()),
		BM_SETCHECK, pSearcher->unSlash ? BST_CHECKED : BST_UNCHECKED, 0);
	entered--;
}

void ReplaceStrip::Show() {
	Focus();
	HWND combo = GetDlgItem(Hwnd(), IDFINDWHAT);
	::SendMessage(combo, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < pSearcher->memFinds.Length(); i++) {
		GUI::gui_string gs = GUI::StringFromUTF8(pSearcher->memFinds.At(i).c_str());
		::SendMessageW(combo, CB_ADDSTRING, 0,
					reinterpret_cast<LPARAM>(gs.c_str()));
	}
	//::SendMessage(combo, CB_SETCURSEL, 0, 0);
	::SetWindowText(HwndOf(wText), GUI::StringFromUTF8(pSearcher->findWhat.c_str()).c_str());
	::SendMessage(HwndOf(wText), CB_SETEDITSEL, 0, MAKELPARAM(0, -1));

	HWND comboReplace = GetDlgItem(Hwnd(), IDREPLACEWITH);
	::SendMessage(comboReplace, CB_RESETCONTENT, 0, 0);
	for (int j = 0; j < pSearcher->memReplaces.Length(); j++) {
		GUI::gui_string gs = GUI::StringFromUTF8(pSearcher->memReplaces.At(j).c_str());
		::SendMessageW(comboReplace, CB_ADDSTRING, 0,
					reinterpret_cast<LPARAM>(gs.c_str()));
	}

	CheckButtons();

	if (pSearcher->FindHasText() != 0 && pSearcher->focusOnReplace) {
		::SetFocus(comboReplace);
	}

	pSearcher->ScrollEditorIfNeeded();
}

void UserStrip::Creation() {
	Strip::Creation();
}

void UserStrip::Destruction() {
	delete psd;
	psd = NULL;
	Strip::Destruction();
}

void UserStrip::Close() {
	Strip::Close();
	if (pSciTEWin)
		pSciTEWin->UserStripClosed();
}

void UserStrip::Size() {
	if (!visible)
		return;
	Strip::Size();
	GUI::Rectangle rcArea = GetPosition();

	rcArea.bottom -= rcArea.top;
	rcArea.right -= rcArea.left;

	rcArea.left = 2;
	rcArea.top = 2;
	rcArea.right -= 2;
	rcArea.bottom -= 2;

	if (HasClose())
		rcArea.right -= closeSize.cx + 2;	// Allow for close box and gap

#ifdef BCM_GETIDEALSIZE
	for (size_t line=0; line<psd->controls.size(); line++) {
		std::vector<UserControl> &uc = psd->controls[line];
		// Push buttons can be measured with BCM_GETIDEALSIZE
		for (std::vector<UserControl>::iterator ctl=uc.begin(); ctl != uc.end(); ++ctl) {
			if (ctl->controlType == UserControl::ucButton) {
				SIZE sz = {0, 0};
				::SendMessage(reinterpret_cast<HWND>(ctl->w.GetID()),
					BCM_GETIDEALSIZE, 0, reinterpret_cast<LPARAM>(&sz));
				if (sz.cx > 0) {
					ctl->widthDesired = sz.cx + 2 * WidthText(fontText, TEXT(" "));
				}
			}
		}
	}
#endif

	psd->CalculateColumnWidths(rcArea.Width());

	for (unsigned int line=0; line<psd->controls.size(); line++) {
		int top = rcArea.top + line * lineHeight;
		int left = rcArea.left;
		size_t column = 0;
		std::vector<UserControl> &uc = psd->controls[line];
		for (std::vector<UserControl>::iterator ctl=uc.begin(); ctl != uc.end(); ++ctl) {
			ctl->widthAllocated = psd->widths[column].widthAllocated;

			GUI::Rectangle rcSize = ctl->w.GetClientPosition();
			int topWithFix = top;
			if (ctl->controlType == UserControl::ucButton)
				topWithFix--;
			if (ctl->controlType == UserControl::ucStatic)
				topWithFix += 3;
			if (ctl->controlType == UserControl::ucEdit)
				rcSize.bottom = rcSize.top + 23;
			if (ctl->controlType == UserControl::ucCombo)
				rcSize.bottom = rcSize.top + 180;
			GUI::Rectangle rcControl(left, topWithFix, left + ctl->widthAllocated, topWithFix + rcSize.Height());
			ctl->w.SetPosition(rcControl);
			left += ctl->widthAllocated + 4;

			column++;
		}
	}

	::InvalidateRect(Hwnd(), NULL, TRUE);
}

bool UserStrip::HasClose() const {
	return psd && psd->hasClose;
}

void UserStrip::Focus() {
	for (std::vector<std::vector<UserControl> >::iterator line=psd->controls.begin(); line != psd->controls.end(); ++line) {
		for (std::vector<UserControl>::iterator ctl=line->begin(); ctl != line->end(); ++ctl) {
			if (ctl->controlType != UserControl::ucStatic) {
				::SetFocus(HwndOf(ctl->w));
				return;
			}
		}
	}
}

bool UserStrip::KeyDown(WPARAM key) {
	if (!visible)
		return false;
	if (Strip::KeyDown(key))
		return true;
	if (key == VK_RETURN) {
		if (IsChild(Hwnd(), ::GetFocus())) {
			// Treat Enter as pressing the first default button
			for (std::vector<std::vector<UserControl> >::iterator line=psd->controls.begin(); line != psd->controls.end(); ++line) {
				for (std::vector<UserControl>::iterator ctl=line->begin(); ctl != line->end(); ++ctl) {
					if (ctl->controlType == UserControl::ucDefaultButton) {
						extender->OnUserStrip(ctl->item, scClicked);
						return true;
					}
				}
			}
		}
	}

	return false;
}

static StripCommand NotificationToStripCommand(int notification) {
	switch (notification) {
		case BN_CLICKED:
			return scClicked;
		case EN_CHANGE:
		case CBN_EDITCHANGE:
			return scChange;
		case EN_UPDATE:
			return scUnknown;
		case EN_SETFOCUS:
			return scFocusIn;
		case EN_KILLFOCUS:
			return scFocusOut;
		default: 
			return scUnknown;
	}
}

bool UserStrip::Command(WPARAM wParam) {
	if (entered)
		return false;
	int control = ControlIDOfWParam(wParam);
	int notification = HIWORD(wParam);
	if (extender) {
		StripCommand sc = NotificationToStripCommand(notification);
		if (sc != scUnknown)
			return extender->OnUserStrip(control, sc);
	}
	return false;
}

LRESULT UserStrip::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	try {

	return Strip::WndProc(iMessage, wParam, lParam);

	} catch (...) {
	}
	return 0l;
}

int UserStrip::Lines() {
	return psd ? static_cast<int>(psd->controls.size()) : 1;
}

void UserStrip::SetDescription(const char *description) {
	entered++;
	GUI::gui_string sDescription = GUI::StringFromUTF8(description);
	bool resetting = psd != 0;
	if (psd) {
		for (std::vector<std::vector<UserControl> >::iterator line=psd->controls.begin(); line != psd->controls.end(); ++line) {
			for (std::vector<UserControl>::iterator ctl=line->begin(); ctl != line->end(); ++ctl) {
				ctl->w.Destroy();
			}
		}
	}
	delete psd;
	psd = new StripDefinition(sDescription);
	int controlID=0;
	for (unsigned int line=0; line<psd->controls.size(); line++) {
		std::vector<UserControl> &uc = psd->controls[line];
		for (unsigned int control=0; control<uc.size(); control++) {
			UserControl *puc = &(uc[control]);
			switch (puc->controlType) {
			case UserControl::ucEdit:
				puc->widthDesired = 100;
				puc->fixedWidth = false;
				puc->w = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"), puc->text.c_str(),
					WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | ES_AUTOHSCROLL,
					60 * control, line * lineHeight + 2, puc->widthDesired, 27,
					Hwnd(), reinterpret_cast<HMENU>(controlID), ::GetModuleHandle(NULL), 0);
				break;

			case UserControl::ucCombo:
				puc->widthDesired = 100;
				puc->fixedWidth = false;
				puc->w = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("ComboBox"), puc->text.c_str(),
					WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | CBS_DROPDOWN | CBS_AUTOHSCROLL,
					60 * control, line * lineHeight + 2, puc->widthDesired, 180,
					Hwnd(), reinterpret_cast<HMENU>(controlID), ::GetModuleHandle(NULL), 0);
				break;

			case UserControl::ucButton:
			case UserControl::ucDefaultButton:
				puc->widthDesired = WidthText(fontText, puc->text.c_str()) + 
					2 * ::GetSystemMetrics(SM_CXEDGE) +
					2 * WidthText(fontText, TEXT(" "));
				puc->w = ::CreateWindowEx(0, TEXT("Button"), puc->text.c_str(),
					WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | 
					((puc->controlType == UserControl::ucDefaultButton) ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
					60 * control, line * lineHeight + 2, puc->widthDesired, 25,
					Hwnd(), reinterpret_cast<HMENU>(controlID), ::GetModuleHandle(NULL), 0);
				break;

			default:
				puc->widthDesired = WidthText(fontText, puc->text.c_str());
				puc->w = ::CreateWindowEx(0, TEXT("Static"), puc->text.c_str(),
					WS_CHILD | WS_CLIPSIBLINGS | ES_RIGHT,
					60 * control, line * lineHeight + 2, puc->widthDesired, 21,
					Hwnd(), reinterpret_cast<HMENU>(controlID), ::GetModuleHandle(NULL), 0);
				break;
			}
			puc->w.Show();
			SetFontHandle(puc->w, fontText);
			controlID++;
		}
	}
	if (resetting)
		Size();
	entered--;
	Focus();
}

void UserStrip::SetExtender(Extension *extender_) {
	extender = extender_;
}

void UserStrip::SetSciTE(SciTEWin *pSciTEWin_) {
	pSciTEWin = pSciTEWin_;
}

UserControl *UserStrip::FindControl(int control) {
	return psd->FindControl(control);
}

void UserStrip::Set(int control, const char *value) {
	UserControl *ctl = FindControl(control);
	if (ctl) {
		GUI::gui_string sValue = GUI::StringFromUTF8(value);
		::SetWindowTextW(HwndOf(ctl->w), sValue.c_str());
	}
}

void UserStrip::SetList(int control, const char *value) {
	UserControl *ctl = FindControl(control);
	if (ctl) {
		if (ctl->controlType == UserControl::ucCombo) {
			GUI::gui_string sValue = GUI::StringFromUTF8(value);
			std::vector<GUI::gui_string> listValues = ListFromString(sValue);
			HWND combo = HwndOf(ctl->w);
			::SendMessage(combo, CB_RESETCONTENT, 0, 0);
			for (std::vector<GUI::gui_string>::iterator i = listValues.begin(); i != listValues.end(); ++i) {
				::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(i->c_str()));
			}
		}
	}
}

std::string UserStrip::GetValue(int control) {
	UserControl *ctl = FindControl(control);
	if (ctl) {
		return ControlText(ctl->w).c_str();
	}
	return "";
}

LRESULT PASCAL BaseWin::StWndProc(
    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	// Find C++ object associated with window.
	BaseWin *base = reinterpret_cast<BaseWin *>(::PointerFromWindow(hWnd));
	// scite will be zero if WM_CREATE not seen yet
	if (base == 0) {
		if (iMessage == WM_CREATE) {
			LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			base = reinterpret_cast<BaseWin *>(cs->lpCreateParams);
			SetWindowPointer(hWnd, base);
			base->SetID(hWnd);
			return base->WndProc(iMessage, wParam, lParam);
		} else
			return ::DefWindowProc(hWnd, iMessage, wParam, lParam);
	} else
		return base->WndProc(iMessage, wParam, lParam);
}

// Convert String from UTF-8 to doc encoding
SString SciTEWin::EncodeString(const SString &s) {
	//::MessageBox(GetFocus(),SString(s).c_str(),"EncodeString:in",0);

	UINT codePage = wEditor.Call(SCI_GETCODEPAGE);

	if (codePage != SC_CP_UTF8) {
		codePage = CodePageFromCharSet(characterSet, codePage);

		int cchWide = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.length()), NULL, 0);
		wchar_t *pszWide = new wchar_t[cchWide + 1];
		::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.length()), pszWide, cchWide + 1);

		int cchMulti = ::WideCharToMultiByte(codePage, 0, pszWide, cchWide, NULL, 0, NULL, NULL);
		char *pszMulti = new char[cchMulti + 1];
		::WideCharToMultiByte(codePage, 0, pszWide, cchWide, pszMulti, cchMulti + 1, NULL, NULL);
		pszMulti[cchMulti] = 0;

		SString result(pszMulti);

		delete []pszWide;
		delete []pszMulti;

		//::MessageBox(GetFocus(),result.c_str(),"EncodeString:out",0);
		return result;
	}
	return SciTEBase::EncodeString(s);
}

// Convert String from doc encoding to UTF-8
SString SciTEWin::GetRangeInUIEncoding(GUI::ScintillaWindow &win, int selStart, int selEnd) {
	SString s = SciTEBase::GetRangeInUIEncoding(win, selStart, selEnd);

	UINT codePage = wEditor.Call(SCI_GETCODEPAGE);

	if (codePage != SC_CP_UTF8) {
		codePage = CodePageFromCharSet(characterSet, codePage);

		int cchWide = ::MultiByteToWideChar(codePage, 0, s.c_str(), static_cast<int>(s.length()), NULL, 0);
		wchar_t *pszWide = new wchar_t[cchWide + 1];
		::MultiByteToWideChar(codePage, 0, s.c_str(), static_cast<int>(s.length()), pszWide, cchWide + 1);

		int cchMulti = ::WideCharToMultiByte(CP_UTF8, 0, pszWide, cchWide, NULL, 0, NULL, NULL);
		char *pszMulti = new char[cchMulti + 1];
		::WideCharToMultiByte(CP_UTF8, 0, pszWide, cchWide, pszMulti, cchMulti + 1, NULL, NULL);
		pszMulti[cchMulti] = 0;

		SString result(pszMulti);

		delete []pszWide;
		delete []pszMulti;

		return result;
	}
	return s;
}

uptr_t SciTEWin::EventLoop() {
	MSG msg;
	msg.wParam = 0;
	bool going = true;
	while (going) {
		going = ::GetMessageW(&msg, NULL, 0, 0);
		if (going) {
			if (!ModelessHandler(&msg)) {
				if (!GetID() ||
					::TranslateAccelerator(reinterpret_cast<HWND>(GetID()), GetAcceleratorTable(), &msg) == 0) {
					::TranslateMessage(&msg);
					::DispatchMessageW(&msg);
				}
			}
		}
	}
	return msg.wParam;
}

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

	typedef BOOL (WINAPI *SetDllDirectorySig)(LPCTSTR lpPathName);
	SetDllDirectorySig SetDllDirectoryFn = (SetDllDirectorySig)::GetProcAddress(
		::GetModuleHandle(TEXT("kernel32.dll")), "SetDllDirectoryW");
	if (SetDllDirectoryFn) {
		// For security, remove current directory from the DLL search path
		SetDllDirectoryFn(TEXT(""));
	}

#ifdef NO_EXTENSIONS
	Extension *extender = 0;
#else
	MultiplexExtension multiExtender;
	Extension *extender = &multiExtender;

#ifndef NO_LUA
	multiExtender.RegisterExtension(LuaExtension::Instance());
#endif

#ifndef NO_FILER
	multiExtender.RegisterExtension(DirectorExtension::Instance());
#endif
#endif

	SciTEWin::Register(hInstance);
#ifdef STATIC_BUILD

	Scintilla_LinkLexers();
	Scintilla_RegisterClasses(hInstance);
#else

	HMODULE hmod = ::LoadLibrary(TEXT("SciLexer.DLL"));
	if (hmod == NULL)
		::MessageBox(NULL, TEXT("The Scintilla DLL could not be loaded.  SciTE will now close"),
			TEXT("Error loading Scintilla"), MB_OK | MB_ICONERROR);
#endif

	uptr_t result;
	{
		SciTEWin MainWind(extender);
		LPTSTR lptszCmdLine = GetCommandLine();
		if (*lptszCmdLine == '\"') {
			lptszCmdLine++;
			while (*lptszCmdLine && (*lptszCmdLine != '\"'))
				lptszCmdLine++;
			if (*lptszCmdLine == '\"')
				lptszCmdLine++;
		} else {
			while (*lptszCmdLine && (*lptszCmdLine != ' '))
				lptszCmdLine++;
		}
		while (*lptszCmdLine == ' ')
			lptszCmdLine++;
		MainWind.Run(lptszCmdLine);
		result = MainWind.EventLoop();
	}

#ifdef STATIC_BUILD
	Scintilla_ReleaseResources();
#else

	::FreeLibrary(hmod);
#endif

	return static_cast<int>(result);
}
