// SciTE - Scintilla based Text Editor
/** @file SciTEWin.cxx
 ** Main code for the Windows version of the editor.
 **/
// Copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <time.h>

#include "SciTEWin.h"
#include <Vsstyle.h>
#include <Vssym32.h>

#ifndef NO_EXTENSIONS
#include "MultiplexExtension.h"

#ifndef NO_FILER
#include "DirectorExtension.h"
#endif

#ifndef NO_LUA
#include "SingleThreadExtension.h"
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
	outputScroll = 1;

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
				    reinterpret_cast<const char *>(pv), size, FilePath());
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
	wndclass.lpfnWndProc = SciTEWin::IWndProc;
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

void SciTEWin::ReadProperties() {
	SciTEBase::ReadProperties();

	outputScroll = props.GetInt("output.scroll", 1);
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
	char *topic = StringDup(cmd);
	if (topic) {
		char *path = strchr(topic, '!');
		if (path) {
			*path = '\0';
			path++;	// After the !
			::WinHelpA(MainHWND(),
			          path,
			          HELP_KEY,
			          reinterpret_cast<ULONG_PTR>(topic));
		}
	}
	delete []topic;
}

// HH_AKLINK not in mingw headers
struct XHH_AKLINK {
	long cbStruct;
	BOOL fReserved;
	const char *pszKeywords;
	char *pszUrl;
	char *pszMsgText;
	char *pszMsgTitle;
	char *pszWindow;
	BOOL fIndexOnFail;
};

// Help command lines contain topic!path
void SciTEWin::ExecuteHelp(const char *cmd) {
	if (!hHH)
		hHH = ::LoadLibrary(TEXT("HHCTRL.OCX"));

	if (hHH) {
		char *topic = StringDup(cmd);
		char *path = strchr(topic, '!');
		if (topic && path) {
			*path = '\0';
			path++;	// After the !
			typedef HWND (WINAPI *HelpFn) (HWND, const char *, UINT, DWORD_PTR);
			HelpFn fnHHA = (HelpFn)::GetProcAddress(hHH, "HtmlHelpA");
			if (fnHHA) {
				XHH_AKLINK ak;
				ak.cbStruct = sizeof(ak);
				ak.fReserved = FALSE;
				ak.pszKeywords = topic;
				ak.pszUrl = NULL;
				ak.pszMsgText = NULL;
				ak.pszMsgTitle = NULL;
				ak.pszWindow = NULL;
				ak.fIndexOnFail = TRUE;
				fnHHA(NULL,
				      path,
				      0x000d,          	// HH_KEYWORD_LOOKUP
				      reinterpret_cast<DWORD_PTR>(&ak)
				     );
			}
		}
		delete []topic;
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
		::SetWindowLongPtr(reinterpret_cast<HWND>(wContent.GetID()),
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
		::SetWindowLongPtr(reinterpret_cast<HWND>(wContent.GetID()),
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
	return reinterpret_cast<HWND>(wSciTE.GetID());
}

void SciTEWin::Command(WPARAM wParam, LPARAM lParam) {
	int cmdID = ControlIDOfCommand(wParam);
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
			for (int icmd = 0; icmd < jobQueue.commandMax; icmd++) {
				jobQueue.jobQueue[icmd].Clear();
			}
			jobQueue.commandCurrent = 0;
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
	int cchMulti = ::WideCharToMultiByte(codePage, 0, s.c_str(), s.size(), NULL, 0, NULL, NULL);
	char *pszMulti = new char[cchMulti + 1];
	::WideCharToMultiByte(codePage, 0, s.c_str(), s.size(), pszMulti, cchMulti + 1, NULL, NULL);
	pszMulti[cchMulti] = 0;
	OutputAppendStringSynchronised(pszMulti);
	delete []pszMulti;
}

/**
 * Run a command with redirected input and output streams
 * so the output can be put in a window.
 * It is based upon several usenet posts and a knowledge base article.
 * This is running in a separate thread to the user interface so should always
 * use ScintillaWindow::Send rather than a one of the direct function calls.
 */
DWORD SciTEWin::ExecuteOne(const Job &jobToRun, bool &seenOutput) {
	DWORD exitcode = 0;
	GUI::ElapsedTime commandTime;

	if (jobToRun.jobType == jobShell) {
		ShellExec(jobToRun.command, jobToRun.directory.AsUTF8().c_str());
		return exitcode;
	}

	if (jobToRun.jobType == jobExtension) {
		if (extender) {
			// Problem: we are in the wrong thread!  That is the cause of the cursed PC.
			// It could also lead to other problems.

			if (jobToRun.flags & jobGroupUndo)
				wEditor.Send(SCI_BEGINUNDOACTION);

			extender->OnExecute(jobToRun.command.c_str());

			if (jobToRun.flags & jobGroupUndo)
				wEditor.Send(SCI_ENDUNDOACTION);

			Redraw();
			// A Redraw "might" be needed, since Lua and Director
			// provide enough low-level capabilities to corrupt the
			// display.
			// (That might have been due to a race condition, and might now be
			// corrected by SingleThreadExtension.  Should check it some time.)
		}
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
		InternalGrep(gf, jobToRun.directory.AsInternal(), GUI::StringFromUTF8(findFiles).c_str(), findWhat);
		return exitcode;
	}

	UINT codePage = wOutput.Send(SCI_GETCODEPAGE);
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

		while (running) {
			if (writingPosition >= totalBytesToWrite) {
				::Sleep(100L);
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
					    bytesToWrite, &bytesWrote, NULL);

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
						if (!seenOutput) {
							MakeOutputVisible();
							seenOutput = true;
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
		SString sExitMessage(exitcode);
		sExitMessage.insert(0, ">Exit code: ");
		if (jobQueue.TimeCommands()) {
			sExitMessage += "    Time: ";
			sExitMessage += SString(commandTime.Duration(), 3);
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
				int cpMin = wEditor.Send(SCI_GETSELECTIONSTART, 0, 0);
				wEditor.Send(SCI_REPLACESEL,0,(sptr_t)(repSelBuf.c_str()));
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
 * Run the commands in the job queue, stopping if one fails.
 * This is running in a separate thread to the user interface so must be
 * careful when reading and writing shared state.
 */
void SciTEWin::ProcessExecute() {
	DWORD exitcode = 0;
	if (scrollOutput)
		wOutput.Send(SCI_GOTOPOS, wOutput.Send(SCI_GETTEXTLENGTH));
	int originalEnd = wOutput.Send(SCI_GETCURRENTPOS);
	bool seenOutput = false;

	for (int icmd = 0; icmd < jobQueue.commandCurrent && icmd < jobQueue.commandMax && exitcode == 0; icmd++) {
		exitcode = ExecuteOne(jobQueue.jobQueue[icmd], seenOutput);
		if (jobQueue.isBuilding) {
			// The build command is first command in a sequence so it is only built if
			// that command succeeds not if a second returns after document is modified.
			jobQueue.isBuilding = false;
			if (exitcode == 0)
				jobQueue.isBuilt = true;
		}
	}

	// Move selection back to beginning of this run so that F4 will go
	// to first error of this run.
	// scroll and return only if output.scroll equals
	// one in the properties file
	if ((outputScroll == 1) && returnOutputToCommand)
		wOutput.Send(SCI_GOTOPOS, originalEnd, 0);
	returnOutputToCommand = true;
	::SendMessage(MainHWND(), WM_COMMAND, IDM_FINISHEDEXECUTE, 0);
}

void ExecThread(void *ptw) {
	SciTEWin *tw = reinterpret_cast<SciTEWin *>(ptw);
	tw->ProcessExecute();
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
		int len_mycmd = s - mycmdcopy + 4;
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
	SciTEBase::Execute();

	_beginthread(ExecThread, 1024 * 1024, reinterpret_cast<void *>(this));
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

void SciTEWin::QuitProgram() {
	if (SaveIfUnsureAll() != IDCANCEL) {
		if (fullScreen)	// Ensure tray visible on exit
			FullScreenToggle();
		::PostQuitMessage(0);
		wSciTE.Destroy();
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
void SciTEWin::Paint(HDC hDC, GUI::Rectangle) {
	GUI::Rectangle rcInternal = GetClientRectangle();

	int heightClient = rcInternal.Height();
	int widthClient = rcInternal.Width();

	int heightEditor = heightClient - heightOutput - heightBar;
	int yBorder = heightEditor;
	int xBorder = widthClient - heightOutput - heightBar;
	for (int i = 0; i < heightBar; i++) {
		HPEN pen;
		if (i == 1)
			pen = ::CreatePen(0,1,::GetSysColor(COLOR_3DHIGHLIGHT));
		else if (i == heightBar - 2)
			pen = ::CreatePen(0,1,::GetSysColor(COLOR_3DSHADOW));
		else if (i == heightBar - 1)
			pen = ::CreatePen(0,1,::GetSysColor(COLOR_3DDKSHADOW));
		else
			pen = ::CreatePen(0,1,::GetSysColor(COLOR_3DFACE));
		HPEN penOld = static_cast<HPEN>(::SelectObject(hDC, pen));
		if (splitVertical) {
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
		int filesDropped = ::DragQueryFile(hdrop, 0xffffffff, NULL, 0);
		// Append paths to dropFilesQueue, to finish drag operation soon
		for (int i = 0; i < filesDropped; ++i) {
			GUI::gui_char pathDropped[MAX_PATH];
			::DragQueryFileW(hdrop, i, pathDropped, ELEMENTS(pathDropped));
			dropFilesQueue.push_back(pathDropped);
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
		if (filesDropped > 0) {
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

	if (extender && extender->OnKey(wParam, modifiers))
		return 1l;

	for (int j = 0; j < languageItems; j++) {
		if (KeyMatch(languageMenu[j].menuKey, wParam, modifiers)) {
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
			if (SciTEKeys::MatchKeyCode(reinterpret_cast<long&>(mii.dwItemData), wParam, modifiers)) {
				SciTEBase::MenuCommand(IDM_TOOLS+tool_i);
				return 1l;
			}
		}
	}

	// loop through the keyboard short cuts defined by user.. if found
	// exec it the command defined
	for (int cut_i = 0; cut_i < shortCutItems; cut_i++) {
		if (KeyMatch(shortCutItemList[cut_i].menuKey, wParam, modifiers)) {
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
	menuSource = ::GetDlgCtrlID(reinterpret_cast<HWND>(w->GetID()));
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

		case WM_PALETTECHANGED:
			if (wParam != reinterpret_cast<WPARAM>(MainHWND())) {
				wEditor.Call(WM_PALETTECHANGED, wParam, lParam);
				//wOutput.Call(WM_PALETTECHANGED, wParam, lParam);
			}

			break;

		case WM_QUERYNEWPALETTE:
			wEditor.Call(WM_QUERYNEWPALETTE, wParam, lParam);
			//wOutput.Call(WM_QUERYNEWPALETTE, wParam, lParam);
			return TRUE;

		case WM_ACTIVATEAPP:
			wEditor.Call(SCI_HIDESELECTION, !wParam);
			// Do not want to display dialog yet as may be in middle of system mouse capture
			::PostMessage(MainHWND(), WM_COMMAND, IDM_ACTIVATE, wParam);
			break;

		case WM_ACTIVATE:
			if (wParam != WA_INACTIVE) {
				::SetFocus(wFocus);
			}
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
		statusFailure = sf.status;
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

LRESULT SciTEWin::WndProcI(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	try {
	switch (iMessage) {

	case WM_COMMAND:
		Command(wParam, lParam);
		break;

	case WM_NOTIFY:
		Notify(reinterpret_cast<SCNotification *>(lParam));
		break;

	case WM_PAINT: {
			PAINTSTRUCT ps;
			::BeginPaint(reinterpret_cast<HWND>(wContent.GetID()), &ps);
			GUI::Rectangle rcPaint(ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
			Paint(ps.hdc, rcPaint);
			::EndPaint(reinterpret_cast<HWND>(wContent.GetID()), &ps);
			return 0;
		}

	case WM_LBUTTONDOWN:
		ptStartDrag = PointFromLong(lParam);
		capturedMouse = true;
		heightOutputStartDrag = heightOutput;
		::SetCapture(reinterpret_cast<HWND>(wContent.GetID()));
		break;

	case WM_MOUSEMOVE:
		if (capturedMouse) {
			MoveSplit(PointFromLong(lParam));
		}
		break;

	case WM_LBUTTONUP:
		if (capturedMouse) {
			MoveSplit(PointFromLong(lParam));
			capturedMouse = false;
			::ReleaseCapture();
		}
		break;

	case WM_CAPTURECHANGED:
		capturedMouse = false;
		break;

	case WM_SETCURSOR:
		if (ControlIDOfCommand(lParam) == HTCLIENT) {
			GUI::Point ptCursor;
			::GetCursorPos(reinterpret_cast<POINT *>(&ptCursor));
			GUI::Point ptClient = ptCursor;
			::ScreenToClient(MainHWND(), reinterpret_cast<POINT *>(&ptClient));
			if ((ptClient.y > (visHeightTools + visHeightTab)) && (ptClient.y < visHeightTools + visHeightTab + visHeightEditor)) {
				GUI::Rectangle rcScintilla = wEditor.GetPosition();
				GUI::Rectangle rcOutput = wOutput.GetPosition();
				if (!rcScintilla.Contains(ptCursor) && !rcOutput.Contains(ptCursor)) {
					::SetCursor(::LoadCursor(NULL, splitVertical ? IDC_SIZEWE : IDC_SIZENS));
					return TRUE;
				}
			}
		}
		return ::DefWindowProc(MainHWND(), iMessage, wParam, lParam);

	default:
		return ::DefWindowProc(reinterpret_cast<HWND>(wContent.GetID()),
		                       iMessage, wParam, lParam);
	}
	} catch (...) {
	}
	return 0l;
}

LRESULT PASCAL SciTEWin::IWndProc(
    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	// Find C++ object associated with window.
	SciTEWin *scite = reinterpret_cast<SciTEWin *>(::PointerFromWindow(hWnd));
	// scite will be zero if WM_CREATE not seen yet
	if (scite == 0) {
		if (iMessage == WM_CREATE) {
			LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			scite = reinterpret_cast<SciTEWin *>(cs->lpCreateParams);
			scite->wContent = hWnd;
			SetWindowPointer(hWnd, scite);
			return scite->WndProcI(iMessage, wParam, lParam);
		} else
			return ::DefWindowProc(hWnd, iMessage, wParam, lParam);
	} else
		return scite->WndProcI(iMessage, wParam, lParam);
}

// Convert String from UTF-8 to doc encoding
SString SciTEWin::EncodeString(const SString &s) {
	//::MessageBox(GetFocus(),SString(s).c_str(),"EncodeString:in",0);

	UINT codePage = wEditor.Call(SCI_GETCODEPAGE);

	if (codePage != SC_CP_UTF8) {
		codePage = CodePageFromCharSet(characterSet, codePage);

		int cchWide = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), s.length(), NULL, 0);
		wchar_t *pszWide = new wchar_t[cchWide + 1];
		::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), s.length(), pszWide, cchWide + 1);

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

		int cchWide = ::MultiByteToWideChar(codePage, 0, s.c_str(), s.length(), NULL, 0);
		wchar_t *pszWide = new wchar_t[cchWide + 1];
		::MultiByteToWideChar(codePage, 0, s.c_str(), s.length(), pszWide, cchWide + 1);

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

int SciTEWin::EventLoop() {
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

#ifdef NO_EXTENSIONS
	Extension *extender = 0;
#else
	MultiplexExtension multiExtender;
	Extension *extender = &multiExtender;

#ifndef NO_LUA
	SingleThreadExtension luaAdapter(LuaExtension::Instance());
	multiExtender.RegisterExtension(luaAdapter);
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

	int result;
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

	return result;
}
