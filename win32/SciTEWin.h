// SciTE - Scintilla based Text Editor
/** @file SciTEWin.h
 ** Header of main code for the Windows version of the editor.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "Platform.h"

#include <io.h>
#include <process.h>
#include <mmsystem.h>
#include <commctrl.h>
#ifdef _MSC_VER
#include <direct.h>
#endif
#ifdef __BORLANDC__
#include <dir.h>
#endif

#include "SciTE.h"
#include "PropSet.h"
#include "Accessor.h"
#include "KeyWords.h"
#include "Scintilla.h"
#include "Extender.h"
#include "SciTEBase.h"
#ifdef LUA_SCRIPTING
#include "LuaExtension.h"
#endif

/// Structure passed back and forth with the Indentation Setting dialog
typedef struct {
    unsigned tabSize;
    unsigned indentSize;
    bool useTabs;
} IndentationSettings;

/** Windows specific stuff.
 **/
class SciTEWin : public SciTEBase {

protected:

	int cmdShow;
	static HINSTANCE hInstance;
	static char *className;
	static char *classNameInternal;
	FINDREPLACE fr;
	char openWhat[200];
	int filterDefault;

	PRectangle pagesetupMargin;
	HGLOBAL hDevMode;
	HGLOBAL hDevNames;

	/// HTMLHelp module
	HMODULE hHH;
	/// Multimedia (sound) module
	HMODULE hMM;

	// Tab Bar
	TCITEM tie;
	HFONT fontTabs;

	virtual void SizeContentWindows();
	virtual void SizeSubWindows();

	virtual void SetMenuItem(int menuNumber, int position, int itemID,
	                         const char *text, const char *mnemonic = 0);
	virtual void DestroyMenuItem(int menuNumber, int itemID);
	virtual void CheckAMenuItem(int wIDCheckItem, bool val);
	virtual void EnableAMenuItem(int wIDCheckItem, bool val);
	virtual void CheckMenus();

	virtual void FixFilePath();
	virtual void AbsolutePath(char *fullPath, const char *basePath, int size);
	virtual bool OpenDialog();
	virtual bool SaveAsDialog();
	virtual void SaveAsHTML();
	virtual void SaveAsRTF();
	virtual void SaveAsPDF();

	/// Print the current buffer.
	virtual void Print(bool showDialog);
	/// Handle default print setup values and ask the user its preferences.
	virtual void PrintSetup();

	BOOL HandleReplaceCommand(int cmd);

	virtual void AboutDialog();
	void AboutDialogWithBuild(int staticBuild);
	virtual void QuitProgram();

	virtual void Find();
	virtual void FindInFiles();
	virtual void Replace();
	virtual void FindReplace(bool replace);
	virtual void DestroyFindReplace();
	virtual void GoLineDialog();
	virtual void TabSizeDialog();

	virtual void GetDefaultDirectory(char *directory, size_t size);
	virtual bool GetSciteDefaultHome(char *path, unsigned int lenPath);
	virtual bool GetSciteUserHome(char *path, unsigned int lenPath);
	virtual bool GetDefaultPropertiesFileName(char *pathDefaultProps,
	        char *pathDefaultDir, unsigned int lenPath);
	virtual bool GetUserPropertiesFileName(char *pathUserProps,
	                                       char *pathUserDir, unsigned int lenPath);

	virtual void SetStatusBarText(const char *s);

	/// Warn the user, by means defined in its properties.
	virtual void WarnUser(int warnID);

	virtual void Notify(SCNotification *notification);
	virtual void ShowToolBar();
	virtual void ShowTabBar();
	virtual void ShowStatusBar();
	void ExecuteHelp(const char *cmd);
	void ExecuteOtherHelp(const char *cmd);
	void CopyAsRTF();
	void Command(WPARAM wParam, LPARAM lParam);

public:

	SciTEWin(Extension *ext = 0);
	~SciTEWin();

	bool ModelessHandler(MSG *pmsg);

	void CreateUI();
	/// Management of the command line parameters.
	void Run(const char *cmdLine);
	void ProcessExecute();
	virtual void Execute();
	virtual void StopExecute();
	virtual void AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, bool forceQueue = false);

	void Paint(Surface *surfaceWindow, PRectangle rcPaint);
	void Creation();
	LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
	LRESULT WndProcI(UINT iMessage, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK FindDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK ReplaceDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK GrepDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK GoLineDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK TabSizeDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	uptr_t GetInstance();
	static void Register(HINSTANCE hInstance_);
	static LRESULT PASCAL TWndProc(
	    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
	static LRESULT PASCAL IWndProc(
	    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
	void ShellExec(const SString &cmd, const SString &dir);
};
