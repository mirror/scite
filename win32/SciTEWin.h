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

#include <string>
#include <vector>
#include <deque>
#include <set>
#include <map>
#include <algorithm>
#include <sstream>
#include <iomanip>

#ifdef __MINGW_H
#define _WIN32_IE	0x0400
#endif

#undef _WIN32_WINNT
#undef WINVER
#ifdef WIN_TARGET
#define _WIN32_WINNT WIN_TARGET
#define WINVER WIN_TARGET
#else
#define _WIN32_WINNT  0x0501
#define WINVER 0x0501
#endif
#include <windows.h>
#include <windowsx.h>
#if defined(DISABLE_THEMES) || (defined(_MSC_VER) && (_MSC_VER <= 1200))
// Old compilers do not have Uxtheme.h
typedef void *HTHEME;
#else
#include <uxtheme.h>
#endif
#include <commctrl.h>
#include <richedit.h>
#include <shlwapi.h>

#include <io.h>
#include <process.h>
#include <mmsystem.h>
#include <commctrl.h>

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
#ifndef DFCS_HOT
#define DFCS_HOT 1000
#endif

#ifndef WM_UPDATEUISTATE
#define WM_UPDATEUISTATE 0x0128
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#include "Scintilla.h"
#include "ILexer.h"

#include "GUI.h"

#include "StringList.h"
#include "StringHelpers.h"
#include "FilePath.h"
#include "StyleDefinition.h"
#include "PropSetFile.h"
#include "StyleWriter.h"
#include "Extender.h"
#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"
#include "Cookie.h"
#include "Worker.h"
#include "FileWorker.h"
#include "MatchMarker.h"
#include "SciTEBase.h"
#include "SciTEKeys.h"
#include "UniqueInstance.h"
#include "StripDefinition.h"
#include "Strips.h"

const int SCITE_TRAY = WM_APP + 0;
const int SCITE_DROP = WM_APP + 1;
const int SCITE_WORKER = WM_APP + 2;
const int SCITE_SHOWOUTPUT = WM_APP + 3;

enum {
	WORK_EXECUTE = WORK_PLATFORM + 1
};

class SciTEWin;

class CommandWorker : public Worker {
public:
	SciTEWin *pSciTE;
	int icmd;
	int originalEnd;
	int exitStatus;
	GUI::ElapsedTime commandTime;
	std::string output;
	int flags;
	bool seenOutput;
	int outputScroll;

	CommandWorker();
	void Initialise(bool resetToStart);
	virtual void Execute();
};

class Dialog;

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
	friend class UserStrip;

protected:

	int cmdShow;
	static HINSTANCE hInstance;
	static const TCHAR *className;
	static const TCHAR *classNameInternal;
	static SciTEWin *app;
	WINDOWPLACEMENT winPlace;
	RECT rcWorkArea;
	GUI::gui_char openWhat[200];
	GUI::gui_char tooltipText[MAX_PATH*2 + 1];
	bool tbLarge;
	bool modalParameters;
	int filterDefault;
	bool staticBuild;
	int menuSource;
	std::deque<GUI::gui_string> dropFilesQueue;

	// Fields also used in tool execution thread
	CommandWorker cmdWorker;
	HANDLE hWriteSubProcess;
	DWORD subProcessGroupId;

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
	HFONT fontTabs;

	/// Preserve focus during deactivation
	HWND wFocus;

	GUI::Window wFindInFiles;
	GUI::Window wFindReplace;
	GUI::Window wParameters;

	ContentWin contents;
	BackgroundStrip backgroundStrip;
	UserStrip userStrip;
	SearchStrip searchStrip;
	FindStrip findStrip;
	ReplaceStrip replaceStrip;

	enum { bandTool, bandTab, bandContents, bandUser, bandBackground, bandSearch, bandFind, bandReplace, bandStatus };
	std::vector<Band> bands;

	virtual void ReadLocalization();
	virtual void GetWindowPosition(int *left, int *top, int *width, int *height, int *maximize);
	void SetScaleFactor(int scale);

	virtual void ReadEmbeddedProperties();
	virtual void ReadPropertiesInitial();
	virtual void ReadProperties();

	virtual void TimerStart(int mask);
	virtual void TimerEnd(int mask);

	virtual void ShowOutputOnMainThread();
	virtual void SizeContentWindows();
	virtual void SizeSubWindows();

	virtual void SetMenuItem(int menuNumber, int position, int itemID,
	                         const GUI::gui_char *text, const GUI::gui_char *mnemonic = 0);
	virtual void RedrawMenu();
	virtual void DestroyMenuItem(int menuNumber, int itemID);
	virtual void CheckAMenuItem(int wIDCheckItem, bool val);
	virtual void EnableAMenuItem(int wIDCheckItem, bool val);
	virtual void CheckMenus();

	void LocaliseMenu(HMENU hmenu);
	void LocaliseMenus();
	void LocaliseControl(HWND w);
	void LocaliseDialog(HWND wDialog);

	int DoDialog(const TCHAR *resName, DLGPROC lpProc);
	HWND CreateParameterisedDialog(LPCWSTR lpTemplateName, DLGPROC lpProc);
	GUI::gui_string DialogFilterFromProperty(const GUI::gui_char *filterProperty);
	void CheckCommonDialogError();
	virtual bool OpenDialog(const FilePath &directory, const GUI::gui_char *filesFilter);
	FilePath ChooseSaveName(const FilePath &directory, const char *title, const GUI::gui_char *filesFilter = 0, const char *ext = 0);
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

	BOOL HandleReplaceCommand(int cmd, bool reverseDirection = false);

	virtual MessageBoxChoice WindowMessageBox(GUI::Window &w, const GUI::gui_string &msg, MessageBoxStyle style = mbsIconWarning);
	virtual void FindMessageBox(const std::string &msg, const std::string *findItem = 0);
	virtual void AboutDialog();
	void DropFiles(HDROP hdrop);
	void MinimizeToTray();
	void RestoreFromTray();
	static GUI::gui_string ProcessArgs(const GUI::gui_char *cmdLine);
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

	virtual void Notify(SCNotification *notification) override;
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

	virtual void UserStripShow(const char *description);
	virtual void UserStripSet(int control, const char *value);
	virtual void UserStripSetList(int control, const char *value);
	virtual const char *UserStripValue(int control);
	void UserStripClosed();
	virtual void ShowBackgroundProgress(const GUI::gui_string &explanation, size_t size, size_t progress);
	BOOL FindMessage(HWND hDlg, UINT message, WPARAM wParam);
	static INT_PTR CALLBACK FindDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	BOOL ReplaceMessage(HWND hDlg, UINT message, WPARAM wParam);
	static INT_PTR CALLBACK ReplaceDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	virtual void UIClosed();
	void PerformGrep();
	void FillCombos(Dialog &dlg);
	BOOL GrepMessage(HWND hDlg, UINT message, WPARAM wParam);
	static INT_PTR CALLBACK GrepDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	virtual void FindIncrement();
	virtual void Find();
	virtual void FindInFiles();
	virtual void Replace();
	virtual void FindReplace(bool replace);
	virtual void DestroyFindReplace();

	BOOL GoLineMessage(HWND hDlg, UINT message, WPARAM wParam);
	static INT_PTR CALLBACK GoLineDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	virtual void GoLineDialog();

	BOOL AbbrevMessage(HWND hDlg, UINT message, WPARAM wParam);
	static INT_PTR CALLBACK AbbrevDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	virtual bool AbbrevDialog();

	BOOL TabSizeMessage(HWND hDlg, UINT message, WPARAM wParam);
	static INT_PTR CALLBACK TabSizeDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	virtual void TabSizeDialog();

	virtual bool ParametersOpen();
	void ParamGrab();
	virtual bool ParametersDialog(bool modal);
	BOOL ParametersMessage(HWND hDlg, UINT message, WPARAM wParam);
	static INT_PTR CALLBACK ParametersDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

	BOOL AboutMessage(HWND hDlg, UINT message, WPARAM wParam);
	static INT_PTR CALLBACK AboutDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	void AboutDialogWithBuild(int staticBuild_);

	void RestorePosition();

public:

	explicit SciTEWin(Extension *ext = 0);
	~SciTEWin();

	static bool DialogHandled(GUI::WindowID id, MSG *pmsg);
	bool ModelessHandler(MSG *pmsg);

	void CreateUI();
	/// Management of the command line parameters.
	void Run(const GUI::gui_char *cmdLine);
	uptr_t EventLoop();
	void OutputAppendEncodedStringSynchronised(const GUI::gui_string &s, int codePageDocument);
	void ResetExecution();
	void ExecuteNext();
	DWORD ExecuteOne(const Job &jobToRun);
	void ProcessExecute();
	void ShellExec(const std::string &cmd, const char *dir);
	virtual void Execute();
	virtual void StopExecute();
	virtual void AddCommand(const std::string &cmd, const std::string &dir, JobSubsystem jobType, const std::string &input = "", int flags = 0);

	virtual bool PerformOnNewThread(Worker *pWorker);
	virtual void PostOnMainThread(int cmd, Worker *pWorker);
	virtual void WorkerCommand(int cmd, Worker *pWorker);

	void Creation();
	LRESULT KeyDown(WPARAM wParam);
	LRESULT KeyUp(WPARAM wParam);
	virtual void AddToPopUp(const char *label, int cmd=0, bool enabled=true);
	LRESULT ContextMenuMessage(UINT iMessage, WPARAM wParam, LPARAM lParam);
	void CheckForScintillaFailure(int statusFailure);
	LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);

	virtual std::string EncodeString(const std::string &s);
	virtual std::string GetRangeInUIEncoding(GUI::ScintillaWindow &wCurrent, int selStart, int selEnd);

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

GUI::Point PointOfCursor();
GUI::Point ClientFromScreen(HWND hWnd, GUI::Point ptScreen);

// Common minor conversions

inline GUI::Point PointFromLong(LPARAM lPoint) {
	return GUI::Point(static_cast<short>(LOWORD(lPoint)), static_cast<short>(HIWORD(lPoint)));
}

inline int ControlIDOfWParam(WPARAM wParam) {
	return wParam & 0xffff;
}

inline HWND HwndOf(GUI::Window w) {
	return static_cast<HWND>(w.GetID());
}

inline HMENU HmenuID(size_t id) {
	return reinterpret_cast<HMENU>(id);
}

inline POINT *PointPointer(GUI::Point *pt) {
	return reinterpret_cast<POINT *>(pt);
}
