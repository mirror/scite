// SciTE - Scintilla based Text Editor
/** @file SciTEWin.h
 ** Header of main code for the Windows version of the editor.
 **/
// Copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

#ifdef __MINGW_H
#define _WIN32_IE	0x0400
#endif

#undef _WIN32_WINNT
#define _WIN32_WINNT  0x0501
#undef WINVER
#define WINVER 0x0501
#ifdef _MSC_VER
// windows.h, et al, use a lot of nameless struct/unions - can't fix it, so allow it
#pragma warning(disable: 4201)
#endif
#include <windows.h>
#if defined(_MSC_VER) && (_MSC_VER <= 1200)
// Old compilers do not have Uxtheme.h
typedef HANDLE HTHEME;
#else
#include <uxtheme.h>
#endif
#ifdef _MSC_VER
// okay, that's done, don't allow it in our code
#pragma warning(default: 4201)
#endif
#include <commctrl.h>
#include <richedit.h>
#include <shlwapi.h>

#include <io.h>
#include <process.h>
#include <mmsystem.h>
#include <commctrl.h>
#ifdef _MSC_VER
#include <direct.h>
#endif
#ifdef __DMC__
#include <dir.h>
#endif

#include "Scintilla.h"

#include "GUI.h"

#include "SString.h"
#include "StringList.h"
#include "StringHelpers.h"
#include "FilePath.h"
#include "PropSetFile.h"
#include "StyleWriter.h"
#include "Extender.h"
#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"
#include "SciTEBase.h"
#include "SciTEKeys.h"
#include "UniqueInstance.h"

const int SCITE_TRAY = WM_APP + 0;
const int SCITE_DROP = WM_APP + 1;

class Dialog;

class SciTEWin;

inline HWND HwndOf(GUI::Window w) {
	return reinterpret_cast<HWND>(w.GetID());
}

class BaseWin : public GUI::Window {
protected:
	ILocalize *localiser;
public:
	BaseWin() : localiser(0) {
	}
	void SetLocalizer(ILocalize *localiser_) {
		localiser = localiser_;
	}
	HWND Hwnd() const {
		return reinterpret_cast<HWND>(GetID());
	}
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) = 0;
	static LRESULT PASCAL StWndProc(
	    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
};

class ContentWin : public BaseWin {
	SciTEWin *pSciTEWin;
	bool capturedMouse;
public:
	ContentWin() : pSciTEWin(0), capturedMouse(false) {
	}
	void SetSciTE(SciTEWin *pSciTEWin_) {
		pSciTEWin = pSciTEWin_;
	}
	void Paint(HDC hDC, GUI::Rectangle rcPaint);
	LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
};

class Strip : public BaseWin {
protected:
	HFONT fontText;
	HTHEME hTheme;
	bool capturedMouse;
	SIZE closeSize;
	enum stripCloseState { csNone, csOver, csClicked, csClickedOver } closeState;
	GUI::Window wToolTip;

	GUI::Window CreateText(const char *text);
	GUI::Window CreateButton(const char *text, int ident, bool check=false);
	void Tab(bool forwards);
	virtual void Creation();
	virtual void Destruction();
	virtual void Close();
	virtual bool KeyDown(WPARAM key);
	virtual bool Command(WPARAM wParam);
	virtual void Size();
	virtual void Paint(HDC hDC);
	GUI::Rectangle CloseArea();
	void InvalidateClose();
	bool MouseInClose(GUI::Point pt);
	void TrackMouse(GUI::Point pt);
	void SetTheme();
	virtual LRESULT CustomDraw(NMHDR *pnmh);
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
public:
	bool visible;
	Strip() : fontText(0), hTheme(0), capturedMouse(false), closeState(csNone), visible(false) {
		closeSize.cx = 11;
		closeSize.cy = 11;
	}
	virtual int Height() {
		return 25;
	}
};

class SearchStrip : public Strip {
	int entered;
	int lineHeight;
	GUI::Window wStaticFind;
	GUI::Window wText;
	GUI::Window wButton;
	Searcher *pSearcher;
public:
	SearchStrip() : entered(0), lineHeight(20), pSearcher(0) {
	}
	virtual void Creation();
	virtual void Destruction();
	void SetSearcher(Searcher *pSearcher_);
	virtual void Close();
	void Focus();
	virtual bool KeyDown(WPARAM key);
	void Next(bool select);
	virtual bool Command(WPARAM wParam);
	virtual void Size();
	virtual void Paint(HDC hDC);
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
	virtual int Height() {
		return lineHeight + 1;
	}
};

class FindStrip : public Strip {
	int entered;
	int lineHeight;
	GUI::Window wStaticFind;
	GUI::Window wText;
	GUI::Window wButton;
	GUI::Window wButtonMarkAll;
	GUI::Window wCheckWord;
	GUI::Window wCheckCase;
	GUI::Window wCheckRE;
	GUI::Window wCheckBE;
	GUI::Window wCheckWrap;
	GUI::Window wCheckUp;
	Searcher *pSearcher;
public:
	FindStrip() : entered(0), lineHeight(20), pSearcher(0) {
	}
	virtual void Creation();
	virtual void Destruction();
	void SetSearcher(Searcher *pSearcher_);
	virtual void Close();
	void Focus();
	virtual bool KeyDown(WPARAM key);
	void Next(bool markAll);
	void AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked);
	void ShowPopup();
	virtual bool Command(WPARAM wParam);
	virtual void Size();
	virtual void Paint(HDC hDC);
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
	void CheckButtons();
	void Show();
	virtual int Height() {
		return lineHeight + 1;
	}
};

class ReplaceStrip : public Strip {
	int entered;
	int lineHeight;
	GUI::Window wStaticFind;
	GUI::Window wText;
	GUI::Window wCheckWord;
	GUI::Window wCheckCase;
	GUI::Window wButtonFind;
	GUI::Window wButtonReplaceAll;
	GUI::Window wCheckRE;
	GUI::Window wCheckWrap;
	GUI::Window wCheckBE;
	GUI::Window wStaticReplace;
	GUI::Window wReplace;
	GUI::Window wButtonReplace;
	GUI::Window wButtonReplaceInSelection;
	Searcher *pSearcher;
public:
	ReplaceStrip() : entered(0), lineHeight(20), pSearcher(0) {
	}
	virtual void Creation();
	virtual void Destruction();
	void SetSearcher(Searcher *pSearcher_);
	virtual void Close();
	void Focus();
	virtual bool KeyDown(WPARAM key);
	void AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked);
	void ShowPopup();
	void HandleReplaceCommand(int cmd);
	virtual bool Command(WPARAM wParam);
	virtual void Size();
	virtual void Paint(HDC hDC);
	virtual LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
	void CheckButtons();
	void Show();
	virtual int Height() {
		return lineHeight * 2 + 1;
	}
};

struct Band {
	bool visible;
	int height;
	bool expands;
	GUI::Window win;
	Band(bool visible_, int height_, bool expands_, GUI::Window win_) :
		visible(visible_),
		height(height_),
		expands(expands_),
		win(win_) {
	}
};

/** Windows specific stuff.
 **/
class SciTEWin : public SciTEBase {
	friend class ContentWin;
	friend class Strip;
	friend class SearchStrip;
	friend class FindStrip;
	friend class ReplaceStrip;

protected:

	int cmdShow;
	static HINSTANCE hInstance;
	static const TCHAR *className;
	static const TCHAR *classNameInternal;
	static SciTEWin *app;
	WINDOWPLACEMENT winPlace;
	RECT rcWorkArea;
	GUI::gui_char openWhat[200];
	bool modalParameters;
	int filterDefault;
	bool staticBuild;
	int menuSource;
	std::deque<GUI::gui_string> dropFilesQueue;

	// Fields also used in tool execution thread
	HANDLE hWriteSubProcess;
	DWORD subProcessGroupId;
	int outputScroll;

	HACCEL hAccTable;

	GUI::Rectangle pagesetupMargin;
	HGLOBAL hDevMode;
	HGLOBAL hDevNames;

	UniqueInstance uniqueInstance;

	/// HTMLHelp module
	HMODULE hHH;
	/// Multimedia (sound) module
	HMODULE hMM;

	// Tab Bar
	TCITEM tie;
	HFONT fontTabs;

	/// Preserve focus during deactivation
	HWND wFocus;

	GUI::Window wFindInFiles;
	GUI::Window wFindReplace;
	GUI::Window wParameters;

	ContentWin contents;
	SearchStrip searchStrip;
	FindStrip findStrip;
	ReplaceStrip replaceStrip;

	enum { bandTool, bandTab, bandContents, bandSearch, bandFind, bandReplace, bandStatus };
	std::vector<Band> bands;

	virtual void ReadLocalization();
	virtual void GetWindowPosition(int *left, int *top, int *width, int *height, int *maximize);

	virtual void ReadProperties();

	virtual void SizeContentWindows();
	virtual void SizeSubWindows();

	virtual void SetMenuItem(int menuNumber, int position, int itemID,
	                         const GUI::gui_char *text, const GUI::gui_char *mnemonic = 0);
	virtual void RedrawMenu();
	virtual void DestroyMenuItem(int menuNumber, int itemID);
	virtual void CheckAMenuItem(int wIDCheckItem, bool val);
	virtual void EnableAMenuItem(int wIDCheckItem, bool val);
	virtual void CheckMenus();

	void LocaliseAccelerators();
	GUI::gui_string LocaliseAccelerator(const GUI::gui_char *Accelerator, int cmd);
	void LocaliseMenu(HMENU hmenu);
	void LocaliseMenus();
	void LocaliseControl(HWND w);
	void LocaliseDialog(HWND wDialog);

	int DoDialog(HINSTANCE hInst, const TCHAR *resName, HWND hWnd, DLGPROC lpProc);
	GUI::gui_string DialogFilterFromProperty(const GUI::gui_char *filterProperty);
	virtual bool OpenDialog(FilePath directory, const GUI::gui_char *filter);
	FilePath ChooseSaveName(FilePath directory, const char *title, const GUI::gui_char *filter=0, const char *ext=0);
	virtual bool SaveAsDialog();
	virtual void SaveACopy();
	virtual void SaveAsHTML();
	virtual void SaveAsRTF();
	virtual void SaveAsPDF();
	virtual void SaveAsTEX();
	virtual void SaveAsXML();
	virtual void LoadSessionDialog();
	virtual void SaveSessionDialog();
	virtual bool PreOpenCheck(const GUI::gui_char *file);
	virtual bool IsStdinBlocked();

	/// Print the current buffer.
	virtual void Print(bool showDialog);
	/// Handle default print setup values and ask the user its preferences.
	virtual void PrintSetup();

	BOOL HandleReplaceCommand(int cmd);

	virtual int WindowMessageBox(GUI::Window &w, const GUI::gui_string &msg, int style);
	virtual void FindMessageBox(const SString &msg, const SString *findItem=0);
	virtual void AboutDialog();
	void DropFiles(HDROP hdrop);
	void MinimizeToTray();
	void RestoreFromTray();
	GUI::gui_string ProcessArgs(const GUI::gui_char *cmdLine);
	virtual void QuitProgram();

	virtual FilePath GetDefaultDirectory();
	virtual FilePath GetSciteDefaultHome();
	virtual FilePath GetSciteUserHome();

	virtual void SetFileProperties(PropSetFile &ps);
	virtual void SetStatusBarText(const char *s);

	virtual void TabInsert(int index, const GUI::gui_char *title);
	virtual void TabSelect(int index);
	virtual void RemoveAllTabs();

	/// Warn the user, by means defined in its properties.
	virtual void WarnUser(int warnID);

	virtual void Notify(SCNotification *notification);
	virtual void ShowToolBar();
	virtual void ShowTabBar();
	virtual void ShowStatusBar();
	virtual void ActivateWindow(const char *timestamp);
	void ExecuteHelp(const char *cmd);
	void ExecuteOtherHelp(const char *cmd);
	void CopyAsRTF();
	void CopyPath();
	void FullScreenToggle();
	void Command(WPARAM wParam, LPARAM lParam);
	HWND MainHWND();

	BOOL FindMessage(HWND hDlg, UINT message, WPARAM wParam);
	static BOOL CALLBACK FindDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	BOOL ReplaceMessage(HWND hDlg, UINT message, WPARAM wParam);
	static BOOL CALLBACK ReplaceDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	virtual void UIClosed();
	void PerformGrep();
	void FillCombos(Dialog &dlg);
	BOOL GrepMessage(HWND hDlg, UINT message, WPARAM wParam);
	static BOOL CALLBACK GrepDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	virtual void FindIncrement();
	bool FindReplaceAdvanced();
	virtual void Find();
	virtual void FindInFiles();
	virtual void Replace();
	virtual void FindReplace(bool replace);
	virtual void DestroyFindReplace();

	BOOL GoLineMessage(HWND hDlg, UINT message, WPARAM wParam);
	static BOOL CALLBACK GoLineDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	virtual void GoLineDialog();

	BOOL AbbrevMessage(HWND hDlg, UINT message, WPARAM wParam);
	static BOOL CALLBACK AbbrevDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	virtual bool AbbrevDialog();

	BOOL TabSizeMessage(HWND hDlg, UINT message, WPARAM wParam);
	static BOOL CALLBACK TabSizeDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	virtual void TabSizeDialog();

	virtual bool ParametersOpen();
	void ParamGrab();
	virtual bool ParametersDialog(bool modal);
	BOOL ParametersMessage(HWND hDlg, UINT message, WPARAM wParam);
	static BOOL CALLBACK ParametersDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

	BOOL AboutMessage(HWND hDlg, UINT message, WPARAM wParam);
	static BOOL CALLBACK AboutDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	void AboutDialogWithBuild(int staticBuild);

	void MakeAccelerator(SString sKey, ACCEL &Accel);

	void RestorePosition();

public:

	SciTEWin(Extension *ext = 0);
	~SciTEWin();

	bool DialogHandled(GUI::WindowID id, MSG *pmsg);
	bool ModelessHandler(MSG *pmsg);

	void CreateUI();
	/// Management of the command line parameters.
	void Run(const GUI::gui_char *cmdLine);
    int EventLoop();
	void OutputAppendEncodedStringSynchronised(GUI::gui_string s, int codePage);
	DWORD ExecuteOne(const Job &jobToRun, bool &seenOutput);
	void ProcessExecute();
	void ShellExec(const SString &cmd, const char *dir);
	virtual void Execute();
	virtual void StopExecute();
	virtual void AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, const SString &input = "", int flags=0);

	void Creation();
	LRESULT KeyDown(WPARAM wParam);
	LRESULT KeyUp(WPARAM wParam);
	virtual void AddToPopUp(const char *label, int cmd=0, bool enabled=true);
	LRESULT ContextMenuMessage(UINT iMessage, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);

	virtual SString EncodeString(const SString &s);
	virtual SString GetRangeInUIEncoding(GUI::ScintillaWindow &wCurrent, int selStart, int selEnd);

	HACCEL GetAcceleratorTable() {
		return hAccTable;
	}

	uptr_t GetInstance();
	static void Register(HINSTANCE hInstance_);
	static LRESULT PASCAL TWndProc(
	    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

	friend class UniqueInstance;
};

inline bool IsKeyDown(int key) {
	return (::GetKeyState(key) & 0x80000000) != 0;
}


inline GUI::Point PointFromLong(long lPoint) {
	return GUI::Point(static_cast<short>(LOWORD(lPoint)), static_cast<short>(HIWORD(lPoint)));
}
