// SciTE - Scintilla based Text Editor
// SciTEWin.cxx - main code for the Windows version of the editor
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
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
#include "SciTEBase.h"

#ifdef STATIC_BUILD
const char appName[] = "Sc1";
#else
const char appName[] = "SciTE";
#endif

class SciTEWin : public SciTEBase {

protected:

	static HINSTANCE hInstance;
	static char *className;
	FINDREPLACE fr;
	char openWhat[200];
	int filterDefault;
	
	PRectangle  pagesetupMargin;
	HGLOBAL     hDevMode;
	HGLOBAL     hDevNames;

	virtual void SetMenuItem(int menuNumber, int position, int itemID, 
		const char *text, const char *mnemonic=0);
	virtual void DestroyMenuItem(int menuNumber, int itemID);
	virtual void CheckAMenuItem(int wIDCheckItem, bool val);
	virtual void EnableAMenuItem(int wIDCheckItem, bool val);
	virtual void CheckMenus();

	virtual void FixFilePath();
	virtual void AbsolutePath(char *fullPath, const char *basePath, int size);
	virtual bool OpenDialog();
	virtual bool SaveAsDialog();
	virtual void SaveAsHTML();

	virtual void Print();
	virtual void PrintSetup();

	BOOL HandleReplaceCommand(int cmd);
	
	virtual void AboutDialog();
	virtual void QuitProgram();

	virtual void Find();
	virtual void FindInFiles();
	virtual void Replace();
	virtual void FindReplace(bool replace);
	virtual void DestroyFindReplace();
	virtual void GoLineDialog();

	virtual PRectangle GetClientRectangle();
	virtual bool GetDefaultPropertiesFileName(char *pathDefaultProps, unsigned int lenPath);
	virtual bool GetUserPropertiesFileName(char *pathDefaultProps, unsigned int lenPath);

	virtual void Notify(SCNotification *notification);
	void Command(WPARAM wParam, LPARAM lParam);

public:

	SciTEWin();
	~SciTEWin();

	bool ModelessHandler(MSG *pmsg);
	
	void Run(const char *cmdLine);
	void ProcessExecute();
	virtual void Execute();
	virtual void StopExecute();
	virtual void AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, bool forceQueue = false);

	void Paint(Surface *surfaceWindow, PRectangle rcPaint);
	LRESULT WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK FindDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK ReplaceDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK GrepDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK GoLineDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static void Register(HINSTANCE hInstance_);
	static LRESULT PASCAL TWndProc(
		    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
	void ShellExec(const SString &cmd, const SString &dir);
};

char *SciTEWin::className = NULL;
HINSTANCE SciTEWin::hInstance = 0;

SciTEWin::SciTEWin() {
	memset(&fr, 0, sizeof(fr));
	strcpy(openWhat, "Custom Filter");
	openWhat[strlen(openWhat) + 1] = '\0';
	filterDefault = 1;

	ReadGlobalPropFile();

	// Read properties resource into propsEmbed
	// The embedded properties file is to allow distributions to be sure
	// that they have a sensible default configuration even if the properties
	// files are missing. Also allows a one file distribution of Sc1.EXE.
	HRSRC handProps = ::FindResource(hInstance, "Embedded", "Properties");
	if (handProps) {
		DWORD size = ::SizeofResource(hInstance, handProps);
		HGLOBAL hmem = ::LoadResource(hInstance, handProps);
		if (hmem) {
			const void *pv = ::LockResource(hmem);
			if (pv) {
				propsEmbed.ReadFromMemory(
				    reinterpret_cast<const char *>(pv), size);
				UnlockResource(hmem);
			}
		}
		::FreeResource(handProps);
	}

	// Pass 'this' pointer in lpParam of CreateWindow().
	int left = props.GetInt("position.left", CW_USEDEFAULT);
	int top = props.GetInt("position.top", CW_USEDEFAULT);
	int width = props.GetInt("position.width", CW_USEDEFAULT);
	int height = props.GetInt("position.height", CW_USEDEFAULT);
	wSciTE = ::CreateWindowEx(
	             WS_EX_CLIENTEDGE,
	             className,
	             windowName,
	             WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
	             WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
	             WS_MAXIMIZE | WS_CLIPCHILDREN,
	             left, top, width, height,
	             NULL,
	             NULL,
	             hInstance,
	             reinterpret_cast<LPSTR>(this));
	if (!wSciTE.Created())
		exit(FALSE);

	hDevMode  = 0;
	hDevNames = 0;
	ZeroMemory(&pagesetupMargin, sizeof(pagesetupMargin));
}

SciTEWin::~SciTEWin() {
	if (hDevMode)
		GlobalFree(hDevMode);
	if (hDevNames)
		GlobalFree(hDevNames);
}

bool SciTEWin::ModelessHandler(MSG *pmsg) {
	if (wFindReplace.GetID()) {
		if (::IsDialogMessage(wFindReplace.GetID(), pmsg))
			return true;
	}
	return false;
}

// DefaultDlg, DoDialog is a bit like something in PC Magazine May 28, 1991, page 357
// DefaultDlg is only used for about box
int PASCAL DefaultDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM) {
	switch (message) {

	case WM_INITDIALOG: {
			HWND wsci = GetDlgItem(hDlg, IDABOUTSCINTILLA);
#ifdef STATIC_BUILD
			SetAboutMessage(wsci, "Sc1  ");
#else
			SetAboutMessage(wsci, "SciTE");
#endif
		}
		return TRUE;

	case WM_CLOSE:
		SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDOK) {
			EndDialog(hDlg, IDOK);
			return TRUE;
		} else if (ControlIDOfCommand(wParam) == IDCANCEL) {
			EndDialog(hDlg, IDCANCEL);
			return FALSE;
		}
	}

	return FALSE;
}

int DoDialog(HINSTANCE hInst, const char *resName, HWND hWnd, DLGPROC lpProc,
             DWORD dwInitParam) {
	int result = -1;

	if (lpProc == NULL)
		lpProc = reinterpret_cast<DLGPROC>(DefaultDlg);

	if (!dwInitParam)
		result = ::DialogBox(hInst, resName, hWnd, lpProc);
	else
		result = ::DialogBoxParam(hInst, resName, hWnd, lpProc, dwInitParam);

	if (result == -1) {
		DWORD dwError = GetLastError();
		SString errormsg = "Failed to create Dialog box ";
		errormsg += SString(dwError).c_str();
		errormsg += ".";
		MessageBox(hWnd, errormsg.c_str(), appName, MB_OK);
	}

	return result;
}

void SciTEWin::Register(HINSTANCE hInstance_) {
	const char resourceName[] = "SciTE";

	WNDCLASS wndclass;      // Structure used to register Windows class.

	className = "SciTEWindow";
	hInstance = hInstance_;

	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = SciTEWin::TWndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = sizeof(SciTEWin*);
	wndclass.hInstance = hInstance;
	wndclass.hIcon = LoadIcon(hInstance, resourceName);
	wndclass.hCursor = NULL;
	wndclass.hbrBackground = NULL;
	wndclass.lpszMenuName = resourceName;
	wndclass.lpszClassName = className;

	if (!::RegisterClass(&wndclass))
		::exit(FALSE);
}

bool SciTEWin::GetDefaultPropertiesFileName(char *pathDefaultProps, unsigned int lenPath) {
	GetModuleFileName(0, pathDefaultProps, lenPath);
	char *lastSlash = strrchr(pathDefaultProps, pathSepChar);
	if (lastSlash && ((lastSlash + 1 - pathDefaultProps + strlen(propGlobalFileName)) < lenPath)) {
		strcpy(lastSlash + 1, propGlobalFileName);
		return true;
	} else {
		return false;
	}
}

bool SciTEWin::GetUserPropertiesFileName(char *pathDefaultProps, unsigned int lenPath) {
	GetModuleFileName(0, pathDefaultProps, lenPath);
	char *lastSlash = strrchr(pathDefaultProps, pathSepChar);
	if (lastSlash && ((lastSlash + 1 - pathDefaultProps + strlen(propUserFileName)) < lenPath)) {
		strcpy(lastSlash + 1, propUserFileName);
		return true;
	} else {
		return false;
	}
}

void SciTEWin::Notify(SCNotification *notification) {
	SciTEBase::Notify(notification);
}

void SciTEWin::Command(WPARAM wParam, LPARAM lParam) {
	int cmdID = ControlIDOfCommand(wParam);
	switch (cmdID) {

	case IDM_ACTIVATE:
		Activate(lParam);
		break;

	case IDM_FINISHEDEXECUTE: {
			executing = false;
			CheckMenus();
			for (int icmd = 0; icmd < commandMax; icmd++) {
				jobQueue[icmd].Clear();
			}
			commandCurrent = 0;
			CheckReload();
		}
		break;

	default:
		SciTEBase::MenuCommand(cmdID);
	}
}

void SciTEWin::SetMenuItem(int menuNumber, int position, int itemID, 
	const char *text, const char *mnemonic) {
	// On Windows the menu items are modified if they already exist or created
	HMENU hmenuBar = ::GetMenu(wSciTE.GetID());
	HMENU hmenu = ::GetSubMenu(hmenuBar, menuNumber);
	SString sTextMnemonic = text;
	if (mnemonic) {
		sTextMnemonic += "\t";
		sTextMnemonic += mnemonic;
	}
	if (::GetMenuState(hmenu, itemID, MF_BYCOMMAND) == 0xffffffff) {
		if (text[0])
			::InsertMenu(hmenu, position, MF_BYPOSITION, itemID, sTextMnemonic.c_str());
		else
			::InsertMenu(hmenu, position, MF_BYPOSITION | MF_SEPARATOR, itemID, sTextMnemonic.c_str());
	} else {
		::ModifyMenu(hmenu, position, MF_BYCOMMAND, itemID, sTextMnemonic.c_str());
	}
}

void SciTEWin::DestroyMenuItem(int menuNumber, int itemID) {
	// On Windows menu items are destroyed as they can not be hidden and they can be recreated in any position
	HMENU hmenuBar = ::GetMenu(wSciTE.GetID());
	HMENU hmenu = ::GetSubMenu(hmenuBar, menuNumber);
	DeleteMenu(hmenu, itemID, MF_BYCOMMAND);
}

void SciTEWin::CheckAMenuItem(int wIDCheckItem, bool val) {
	if (val)
		CheckMenuItem(GetMenu(wSciTE.GetID()), wIDCheckItem, MF_CHECKED | MF_BYCOMMAND);
	else
		CheckMenuItem(GetMenu(wSciTE.GetID()), wIDCheckItem, MF_UNCHECKED | MF_BYCOMMAND);
}

void SciTEWin::EnableAMenuItem(int wIDCheckItem, bool val) {
	if (val)
		EnableMenuItem(GetMenu(wSciTE.GetID()), wIDCheckItem, MF_ENABLED | MF_BYCOMMAND);
	else
		EnableMenuItem(GetMenu(wSciTE.GetID()), wIDCheckItem, MF_DISABLED | MF_GRAYED | MF_BYCOMMAND);
}

void SciTEWin::CheckMenus() {
	SciTEBase::CheckMenus();
	CheckMenuRadioItem(GetMenu(wSciTE.GetID()), IDM_EOL_CRLF, IDM_EOL_LF,
	                   SendEditor(SCI_GETEOLMODE) - SC_EOL_CRLF + IDM_EOL_CRLF, 0);
}

void SciTEWin::FixFilePath() {
	// On windows file comparison is done case insensitively so the user can
	// enter scite.cxx and still open this file, SciTE.cxx. To ensure that the file
	// is saved with correct capitalisation FindFirstFile is used to find out the
	// real name of the file.
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(fullPath, &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE) {	// FindFirstFile found the file
		char *cpDirEnd = strrchr(fullPath, pathSepChar);
		if (cpDirEnd) {
			strcpy(fileName, FindFileData.cFileName);
			strcpy(dirName, fullPath);
			dirName[cpDirEnd - fullPath] = '\0';
			strcpy(fullPath, dirName);
			strcat(fullPath, pathSepString);
			strcat(fullPath, fileName);
		}
		FindClose(hFind);
	}
}

void SciTEWin::AbsolutePath(char *absPath, const char *relativePath, int size) {
	_fullpath(absPath, relativePath, size);
}

bool SciTEWin::OpenDialog() {
	char openName[MAX_PATH] = "\0";
	OPENFILENAME ofn = {sizeof(OPENFILENAME)};
	ofn.hwndOwner = wSciTE.GetID();
	ofn.hInstance = hInstance;
	ofn.lpstrFile = openName;
	ofn.nMaxFile = sizeof(openName);
	char *filter = 0;
	SString openFilter = props.Get("open.filter");
	if (openFilter.length()) {
		filter = StringDup(openFilter.c_str());
		for (int fc = 0; filter[fc]; fc++)
			if (filter[fc] == '|')
				filter[fc] = '\0';
	}
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = openWhat;
	ofn.nMaxCustFilter = sizeof(openWhat);
	ofn.nFilterIndex = filterDefault;
	ofn.lpstrTitle = "Open File";
	ofn.Flags = OFN_HIDEREADONLY;

	if (::GetOpenFileName(&ofn)) {
		filterDefault = ofn.nFilterIndex;
		delete []filter;
		//Platform::DebugPrintf("Open: <%s>\n", openName);
		Open(openName);
		return true;
	} else {
		delete []filter;
		return false;
	}
}

bool SciTEWin::SaveAsDialog() {
	bool choseOK = false;
	if (0 == dialogsOnScreen) {
		char openName[MAX_PATH] = "\0";
		strcpy(openName, fileName);
		OPENFILENAME ofn = {sizeof(ofn)};
		ofn.hwndOwner = wSciTE.GetID();
		ofn.hInstance = hInstance;
		ofn.lpstrFile = openName;
		ofn.nMaxFile = sizeof(openName);
		ofn.lpstrTitle = "Save File";
		ofn.Flags = OFN_HIDEREADONLY;

		dialogsOnScreen++;
		choseOK = ::GetSaveFileName(&ofn);
		if (choseOK) {
			Platform::DebugPrintf("Save: <%s>\n", openName);
			SetFileName(openName);
			Save();
			ReadProperties();
			Colourise();   	// In case extension was changed
			wEditor.InvalidateAll();
		}
		dialogsOnScreen--;
	}
	return choseOK;
}

void SciTEWin::SaveAsHTML() {
	if (0 == dialogsOnScreen) {
		char saveName[MAX_PATH] = "\0";
		strcpy(saveName, fileName);
		char *cpDot = strchr(saveName, '.');
		if (cpDot != NULL)
			strcpy(cpDot, ".html");
		else
			strcat(saveName, ".html");
		OPENFILENAME ofn = {sizeof(ofn)};
		ofn.hwndOwner = wSciTE.GetID();
		ofn.hInstance = hInstance;
		ofn.lpstrFile = saveName;
		ofn.nMaxFile = sizeof(saveName);
		ofn.lpstrTitle = "Save File As HTML";
		ofn.Flags = OFN_HIDEREADONLY;

		ofn.lpstrFilter = "Web (.html;.htm)\0*.html;*.htm\0";

		dialogsOnScreen++;
		if (::GetSaveFileName(&ofn)) {
			//Platform::DebugPrintf("Save As HTML: <%s>\n", saveName);
			SaveToHTML(saveName);
		}
		dialogsOnScreen--;
	}
}

void SciTEWin::Print() {

	PRINTDLG pdlg = {sizeof(PRINTDLG)};
	pdlg.hwndOwner = wSciTE.GetID();
	pdlg.hInstance = hInstance;
	pdlg.Flags = PD_USEDEVMODECOPIES | PD_ALLPAGES | PD_RETURNDC;
	pdlg.nFromPage = 1;
	pdlg.nToPage = 1;
	pdlg.nMinPage = 1;
	pdlg.nMaxPage = 0xffffU; // We do not know how many pages in the
		// document until the printer is selected and the paper size is known.
	pdlg.nCopies = 1;
	pdlg.hDC = 0;
	pdlg.hDevMode   = hDevMode;
	pdlg.hDevNames  = hDevNames;
	
	// See if a range has been selected
	int startPos = 0;
	int endPos = 0;
	
	if (SendEditor(EM_GETSEL,
		reinterpret_cast<WPARAM>(&startPos),
		reinterpret_cast<LPARAM>(&endPos)) == 0)
	pdlg.Flags |= PD_NOSELECTION;
	
	if (!::PrintDlg(&pdlg)) {
		return;
	}
	
	hDevMode   = pdlg.hDevMode;
	hDevNames  = pdlg.hDevNames;
    
	HDC hdc = pdlg.hDC;

	PRectangle rectMargins;
	Point      ptPage;
	
	// Start by getting the dimensions of the unprintable
	// part of the page (in device units).
	
	rectMargins.left = GetDeviceCaps(hdc, PHYSICALOFFSETX);
	rectMargins.top  = GetDeviceCaps(hdc, PHYSICALOFFSETY);

	// To get the right and lower unprintable area, we need to take
	// the entire width and height of the paper and
	// subtract everything else.
	
	// Get the physical page size (in device units).
	ptPage.x = GetDeviceCaps(hdc, PHYSICALWIDTH);   // device units
	ptPage.y = GetDeviceCaps(hdc, PHYSICALHEIGHT);  // device units
	
	rectMargins.right  = ptPage.x                       // total paper width
	                  - GetDeviceCaps(hdc, HORZRES) // printable width
	                  - rectMargins.left;           // left unprtable margin
	
	rectMargins.bottom = ptPage.y                       // total paper height
	                  - GetDeviceCaps(hdc, VERTRES) // printable ht
	                  - rectMargins.top;            // rt unprtable margin

	// At this point, rectMargins contains the widths of the
	// unprintable regions on all four sides of the page in device units.

	if (pagesetupMargin.left != 0 || pagesetupMargin.right != 0 ||
		pagesetupMargin.top  != 0 || pagesetupMargin.bottom != 0) {
		PRectangle rectSetup;
		Point      ptDpi;
		
		// Convert the HiMetric margin values from the Page Setup dialog
		// to device units and subtract the unprintable part we just
		// calculated. (2540 tenths of a mm in an inch)
		
		ptDpi.x = GetDeviceCaps(hdc, LOGPIXELSX);    // dpi in X direction
		ptDpi.y = GetDeviceCaps(hdc, LOGPIXELSY);    // dpi in Y direction

		rectSetup.left   = MulDiv (pagesetupMargin.left, ptDpi.x, 2540);
		rectSetup.top    = MulDiv (pagesetupMargin.top,  ptDpi.y, 2540);
		rectSetup.right  = ptPage.x - MulDiv (pagesetupMargin.right,ptDpi.x, 2540);
		rectSetup.bottom = ptPage.y - MulDiv (pagesetupMargin.bottom,ptDpi.y, 2540);

        	// Dont reduce margins below the minimum printable area
	        rectMargins.left   = Platform::Maximum(rectMargins.left, rectSetup.left);
	        rectMargins.top    = Platform::Maximum(rectMargins.top, rectSetup.top);
	        rectMargins.right  = Platform::Minimum(rectMargins.right, rectSetup.right);
	        rectMargins.bottom = Platform::Minimum(rectMargins.bottom, rectSetup.bottom);
	}
	
	// rectMargins now contains the values used to shrink the printable
	// area of the page.
	
	// Convert to logical units
	DPtoLP(hdc, (LPPOINT) &rectMargins, 2);
	
	// Convert page size to logical units and we're done!
	DPtoLP(hdc, (LPPOINT) &ptPage, 1);

	DOCINFO di = {sizeof(DOCINFO)};
	di.lpszDocName = windowName;
	di.lpszOutput = 0;
	di.lpszDatatype = 0;
	di.fwType = 0;
	if (::StartDoc(hdc, &di) < 0) {
		MessageBox(wSciTE.GetID(), "Can not start printer document.", 0, MB_OK);
		return;
	}

	LONG lengthDoc = SendEditor(SCI_GETLENGTH);
	LONG lengthDocMax  = lengthDoc;
	LONG lengthPrinted = 0;

	// Requested to print selection
	if (pdlg.Flags & PD_SELECTION) {
		if (startPos > endPos) {
			lengthPrinted = endPos;
			lengthDoc   = startPos;
		} else {
			lengthDoc   = startPos;
			lengthPrinted = endPos;
		}
		
		if (lengthPrinted < 0)
			lengthPrinted = 0;
		if (lengthDoc > lengthDocMax)
			lengthDoc = lengthDocMax;
	}

	int pageNum = 1;
	// Print each page
	while (lengthPrinted < lengthDoc) {
		bool printPage = (!(pdlg.Flags & PD_PAGENUMS) ||
			(pageNum >= pdlg.nFromPage) && (pageNum <= pdlg.nToPage));

		if (printPage)
			::StartPage(hdc);

		FORMATRANGE frPrint   = {0};
		frPrint.hdc           = hdc;
		frPrint.hdcTarget     = hdc;
		frPrint.rc.left       = rectMargins.left;
		frPrint.rc.top        = rectMargins.top;
		frPrint.rc.right      = ptPage.x - (rectMargins.right  + rectMargins.left);
		frPrint.rc.bottom     = ptPage.y - (rectMargins.bottom + rectMargins.top);
		frPrint.rcPage.left   = 0;
		frPrint.rcPage.top    = 0;
		frPrint.rcPage.right  = ptPage.x;
		frPrint.rcPage.bottom = ptPage.y;
		frPrint.chrg.cpMin    = lengthPrinted;
		frPrint.chrg.cpMax    = lengthDoc;
		
		lengthPrinted = SendEditor(EM_FORMATRANGE,
		                           printPage,
		                           reinterpret_cast<LPARAM>(&frPrint));
		if (printPage)
			::EndPage(hdc);

		if ((pdlg.Flags & PD_PAGENUMS) && (pageNum++ > pdlg.nToPage))
			 break;
    	};

	SendEditor(EM_FORMATRANGE, FALSE, 0);
	
	::EndDoc(hdc);
	::DeleteDC(hdc);
}

void SciTEWin::PrintSetup() {
	PAGESETUPDLG pdlg = {sizeof(PAGESETUPDLG)};
	
	pdlg.hwndOwner = wSciTE.GetID();
	pdlg.hInstance = hInstance;
	
	if (pagesetupMargin.left != 0 || pagesetupMargin.right != 0 ||
		pagesetupMargin.top  != 0 || pagesetupMargin.bottom != 0) {
		pdlg.Flags = PSD_MARGINS;
		
		pdlg.rtMargin.left   = pagesetupMargin.left;
		pdlg.rtMargin.top    = pagesetupMargin.top;
		pdlg.rtMargin.right  = pagesetupMargin.right;
		pdlg.rtMargin.bottom = pagesetupMargin.bottom;
	}
	
	pdlg.hDevMode  = hDevMode;
	pdlg.hDevNames = hDevNames;
	
	if (!PageSetupDlg(&pdlg))
		return;
	
	pagesetupMargin.left   = pdlg.rtMargin.left;
	pagesetupMargin.top    = pdlg.rtMargin.top;
	pagesetupMargin.right  = pdlg.rtMargin.right;
	pagesetupMargin.bottom = pdlg.rtMargin.bottom;
	
	hDevMode  = pdlg.hDevMode;
	hDevNames = pdlg.hDevNames;
}

static void FillComboFromMemory(HWND combo, const ComboMemory &mem) {
	for (int i = 0; i < mem.Length(); i++) {
		//Platform::DebugPrintf("Combo[%0d] = %s\n", i, mem.At(i).c_str());
		SendMessage(combo, CB_ADDSTRING, 0,
		            reinterpret_cast<LPARAM>(mem.At(i).c_str()));
	}
}

BOOL CALLBACK SciTEWin::FindDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static SciTEWin *sci;
	HWND wFindWhat = ::GetDlgItem(hDlg, IDFINDWHAT);
	HWND wWholeWord = ::GetDlgItem(hDlg, IDWHOLEWORD);
	HWND wMatchCase = ::GetDlgItem(hDlg, IDMATCHCASE);
	HWND wUp = ::GetDlgItem(hDlg, IDDIRECTIONUP);
	HWND wDown = ::GetDlgItem(hDlg, IDDIRECTIONDOWN);

	switch (message) {

	case WM_INITDIALOG:
		sci = reinterpret_cast<SciTEWin *>(lParam);
		::SetDlgItemText(hDlg, IDFINDWHAT, sci->findWhat);
		FillComboFromMemory(wFindWhat, sci->memFinds);
		if (sci->wholeWord)
			::SendMessage(wWholeWord, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->matchCase)
			::SendMessage(wMatchCase, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->reverseFind) {
			::SendMessage(wUp, BM_SETCHECK, BST_CHECKED, 0);
		} else {
			::SendMessage(wDown, BM_SETCHECK, BST_CHECKED, 0);
		}
		return TRUE;

	case WM_CLOSE:
		SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			sci->wFindReplace = 0;
			EndDialog(hDlg, IDCANCEL);
			return FALSE;
		} else if (ControlIDOfCommand(wParam) == IDOK) {
			//Platform::DebugPrintf("Finding\n");
			char s[200];
			GetDlgItemText(hDlg, IDFINDWHAT, s, sizeof(s));
			sci->props.Set("find.what", s);
			strcpy(sci->findWhat, s);
			sci->memFinds.Insert(s);
			sci->wholeWord = BST_CHECKED == 
				::SendMessage(wWholeWord, BM_GETCHECK, 0, 0);
			sci->matchCase = BST_CHECKED == 
				::SendMessage(wMatchCase, BM_GETCHECK, 0, 0);
			sci->reverseFind = BST_CHECKED == 
				::SendMessage(wUp, BM_GETCHECK, 0, 0);
			sci->wFindReplace = 0;
			EndDialog(hDlg, IDOK);
			sci->FindNext();
			return TRUE;
		}
	}

	return FALSE;
}

BOOL SciTEWin::HandleReplaceCommand(int cmd) {
	HWND wWholeWord = ::GetDlgItem(wFindReplace.GetID(), IDWHOLEWORD);
	HWND wMatchCase = ::GetDlgItem(wFindReplace.GetID(), IDMATCHCASE);
	if ((cmd == IDOK) || (cmd == IDREPLACE) || (cmd == IDREPLACEALL)) {
		::GetDlgItemText(wFindReplace.GetID(), IDFINDWHAT, findWhat, sizeof(findWhat));
		props.Set("find.what", findWhat);
		memFinds.Insert(findWhat);
		wholeWord = BST_CHECKED == 
			::SendMessage(wWholeWord, BM_GETCHECK, 0, 0);
		matchCase = BST_CHECKED == 
			::SendMessage(wMatchCase, BM_GETCHECK, 0, 0);
	}
	if ((cmd == IDREPLACE) || (cmd == IDREPLACEALL)) {
		::GetDlgItemText(wFindReplace.GetID(), IDREPLACEWITH, replaceWhat, sizeof(replaceWhat));
		memReplaces.Insert(replaceWhat);
	}
		
	if (cmd == IDOK) {
		FindNext();
	} else if (cmd == IDREPLACE) {
		if (havefound) {
			SendEditorString(EM_REPLACESEL, 0, replaceWhat);
			havefound = false;
		}
		FindNext();
	} else if (cmd == IDREPLACEALL) {
		ReplaceAll();
	}
	
	return TRUE;
}

BOOL CALLBACK SciTEWin::ReplaceDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static SciTEWin *sci;
	HWND wFindWhat = ::GetDlgItem(hDlg, IDFINDWHAT);
	HWND wReplaceWith = ::GetDlgItem(hDlg, IDREPLACEWITH);
	HWND wWholeWord = ::GetDlgItem(hDlg, IDWHOLEWORD);
	HWND wMatchCase = ::GetDlgItem(hDlg, IDMATCHCASE);

	switch (message) {

	case WM_INITDIALOG:
		sci = reinterpret_cast<SciTEWin *>(lParam);
		::SetDlgItemText(hDlg, IDFINDWHAT, sci->findWhat);
		FillComboFromMemory(wFindWhat, sci->memFinds);
		::SetDlgItemText(hDlg, IDREPLACEWITH, sci->replaceWhat);
		FillComboFromMemory(wReplaceWith, sci->memReplaces);
		if (sci->wholeWord)
			::SendMessage(wWholeWord, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->matchCase)
			::SendMessage(wMatchCase, BM_SETCHECK, BST_CHECKED, 0);
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			sci->wFindReplace = 0;
			::EndDialog(hDlg, IDCANCEL);
			return FALSE;
		} else {
			return sci->HandleReplaceCommand(ControlIDOfCommand(wParam));
		}
	}

	return FALSE;
}

void SciTEWin::Find() {
	if (wFindReplace.Created())
		return;
	SelectionIntoFind();

	memset(&fr, 0, sizeof(fr));
	fr.lStructSize = sizeof(fr);
	fr.hwndOwner = wSciTE.GetID();
	fr.hInstance = hInstance;
	fr.Flags = 0;
	if (!reverseFind)
		fr.Flags |= FR_DOWN;
	fr.lpstrFindWhat = findWhat;
	fr.wFindWhatLen = sizeof(findWhat);
	
	wFindReplace = ::CreateDialogParam(hInstance,
		MAKEINTRESOURCE(IDD_FIND),
		wSciTE.GetID(),
		reinterpret_cast<DLGPROC>(FindDlg),
		reinterpret_cast<long>(this));
	wFindReplace.Show();
			
	//wFindReplace = ::FindText(&fr);
	replacing = false;
}

BOOL CALLBACK SciTEWin::GrepDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static SciTEWin *sci;
	HWND hFindWhat;
	HWND hFiles;

	switch (message) {

	case WM_INITDIALOG:
		sci = reinterpret_cast<SciTEWin *>(lParam);
		SetDlgItemText(hDlg, IDFINDWHAT, sci->props.Get("find.what").c_str());
		SetDlgItemText(hDlg, IDFILES, sci->props.Get("find.files").c_str());
		hFindWhat = GetDlgItem(hDlg, IDFINDWHAT);
		FillComboFromMemory(hFindWhat, sci->memFinds);
		hFiles = GetDlgItem(hDlg, IDFILES);
		FillComboFromMemory(hFiles, sci->memFiles);
		//SetDlgItemText(hDlg, IDDIRECTORY, props->Get("find.directory"));
		return TRUE;

	case WM_CLOSE:
		SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			EndDialog(hDlg, IDCANCEL);
			return FALSE;
		} else if (ControlIDOfCommand(wParam) == IDOK) {
			//Platform::DebugPrintf("Finding\n");
			char s[200];
			GetDlgItemText(hDlg, IDFINDWHAT, s, sizeof(s));
			sci->props.Set("find.what", s);
			strcpy(sci->findWhat, s);
			sci->memFinds.Insert(s);
			GetDlgItemText(hDlg, IDFILES, s, sizeof(s));
			sci->props.Set("find.files", s);
			sci->memFiles.Insert(s);
			//GetDlgItemText(hDlg, IDDIRECTORY, s, sizeof(s));
			//props->Set("find.directory", s);
			EndDialog(hDlg, IDOK);
			return TRUE;
		}
	}

	return FALSE;
}

void SciTEWin::FindInFiles() {
	SelectionIntoFind();
	props.Set("find.what", findWhat);
	props.Set("find.directory", ".");
	if (DoDialog(hInstance, "Grep", wSciTE.GetID(),
	             reinterpret_cast<DLGPROC>(GrepDlg),
	             reinterpret_cast<DWORD>(this)) == IDOK) {
		//Platform::DebugPrintf("asked to find %s %s %s\n", props.Get("find.what"), props.Get("find.files"), props.Get("find.directory"));
		SelectionIntoProperties();
		AddCommand(props.GetNewExpand("find.command", ""), "", jobCLI);
		if (commandCurrent > 0)
			Execute();
	}
}

void SciTEWin::Replace() {
	if (wFindReplace.Created())
		return;
	SelectionIntoFind();

	memset(&fr, 0, sizeof(fr));
	fr.lStructSize = sizeof(fr);
	fr.hwndOwner = wSciTE.GetID();
	fr.hInstance = hInstance;
	fr.Flags = FR_REPLACE;
	fr.lpstrFindWhat = findWhat;
	fr.lpstrReplaceWith = replaceWhat;
	fr.wFindWhatLen = sizeof(findWhat);
	fr.wReplaceWithLen = sizeof(replaceWhat);
	//wFindReplace = ReplaceText(&fr);
	
	wFindReplace = ::CreateDialogParam(hInstance,
		MAKEINTRESOURCE(IDD_REPLACE),
		wSciTE.GetID(),
		reinterpret_cast<DLGPROC>(ReplaceDlg),
		reinterpret_cast<long>(this));
	wFindReplace.Show();
	
	replacing = true;
	havefound = false;
}

// ProcessExecute runs a command with redirected input and output streams
// so the output can be put in a window.
// It is based upon several usenet posts and a knowledge base article.
void SciTEWin::ProcessExecute() {

	DWORD exitcode = 0;

	SendOutput(SCI_GOTOPOS, SendOutput(WM_GETTEXTLENGTH));
	int originalEnd = SendOutput(SCI_GETCURRENTPOS);

	for (int icmd = 0; icmd < commandCurrent && icmd < commandMax && exitcode == 0; icmd++) {

		if (jobQueue[icmd].jobType == jobShell) {
			ShellExec(jobQueue[icmd].command, jobQueue[icmd].directory);
			continue;
		}

		OSVERSIONINFO osv = {sizeof(OSVERSIONINFO)};
		GetVersionEx(&osv);
		bool windows95 = osv.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;

		SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES)};
		PROCESS_INFORMATION pi = {0};
		HANDLE hPipeWrite = NULL;
		HANDLE hPipeRead = NULL;
		HANDLE hWrite2 = NULL;
		HANDLE hRead2 = NULL;
		char buffer[256];
		//Platform::DebugPrintf("Execute <%s>\n", command);
		OutputAppendString(">");
		OutputAppendString(jobQueue[icmd].command.c_str());
		OutputAppendString("\n");

		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = NULL;

		SECURITY_DESCRIPTOR sd;
		// If NT make a real security thing to allow inheriting handles
		if (!windows95) {
			InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
			SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = &sd;
		}

		// Create pipe for output redirection
		CreatePipe(&hPipeRead,     // read handle
		           &hPipeWrite,    // write handle
		           &sa,         // security attributes
		           0      // number of bytes reserved for pipe - 0 default
		          );

		//Platform::DebugPrintf("2Execute <%s>\n");
		// Create pipe for input redirection. In this code, you do not
		// redirect the output of the child process, but you need a handle
		// to set the hStdInput field in the STARTUP_INFO struct. For safety,
		// you should not set the handles to an invalid handle.

		CreatePipe(&hRead2,      // read handle
		           &hWrite2,       // write handle
		           &sa,         // security attributes
		           0      // number of bytes reserved for pipe - 0 default
		          );

		//Platform::DebugPrintf("3Execute <%s>\n");
		// Make child process use hPipeWrite as standard out, and make
		// sure it does not show on screen.
		STARTUPINFO si = {sizeof(STARTUPINFO)};
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		if (jobQueue[icmd].jobType == jobCLI)
			si.wShowWindow = SW_HIDE;
		else
			si.wShowWindow = SW_SHOW;
		si.hStdInput = hWrite2;
		si.hStdOutput = hPipeWrite;
		si.hStdError = hPipeWrite;

		bool worked = CreateProcess(
		                  NULL,
		                  const_cast<char *>(jobQueue[icmd].command.c_str()),
		                  NULL, NULL,
		                  TRUE, 0,
		                  NULL, NULL,
		                  &si, &pi);

		if (!worked) {
			OutputAppendString(">Failed to CreateProcess\n");
		}

		/* now that this has been inherited, close it to be safe.
		You don't want to write to it accidentally */
		CloseHandle(hPipeWrite);

		bool completed = !worked;
		DWORD timeDetectedDeath = 0;
		while (!completed) {

			Sleep(100L);

			if (cancelFlag) {
				TerminateProcess(pi.hProcess, 1);
				break;
			}

			bool NTOrData = true;

			DWORD bytesRead = 0;

			if (windows95) {
				DWORD bytesAvail = 0;
				if (PeekNamedPipe(hPipeRead, buffer, sizeof(buffer), &bytesRead, &bytesAvail, NULL)) {
					if (0 == bytesAvail) {
						NTOrData = false;
						DWORD dwExitCode = STILL_ACTIVE;
						if (GetExitCodeProcess(pi.hProcess, &dwExitCode)) {
							if (STILL_ACTIVE != dwExitCode) {
								// Process is dead, but wait a second in case there is some output in transit
								if (timeDetectedDeath == 0) {
									timeDetectedDeath = timeGetTime();
								} else {
									if ((timeGetTime() - timeDetectedDeath) > 
										static_cast<unsigned int>(props.GetInt("win95.death.delay", 500))) {
										completed = true;    // It's a dead process
									}
								}
							}
						}
					}
				}
			}

			if (!completed && NTOrData) {
				int bTest = ReadFile(
				                hPipeRead,
				                buffer,
				                sizeof(buffer),            // number of bytes to read
				                &bytesRead,
				                NULL         // non-overlapped.
				            );

				if (bTest && bytesRead) {
					// Display the data
					OutputAppendString(buffer, bytesRead);
					UpdateWindow(wSciTE.GetID());
				} else {
					completed = true;
				}
			}
		}

		if (worked) {
			WaitForSingleObject(pi.hProcess, INFINITE);
			GetExitCodeProcess(pi.hProcess, &exitcode);
			char exitmessage[80];
			if (isBuilding) {
				// The build command is first command in a sequence so it is only built if
				// that command succeeds not if a second returns after document is modified.
				isBuilding = false;
				if (exitcode == 0)
					isBuilt = true;
			}
			sprintf(exitmessage, ">Exit code: %ld\n", exitcode);
			OutputAppendString(exitmessage);
			CloseHandle(pi.hProcess);
		}
		CloseHandle(hPipeRead);
	}

	// Move selection back to beginning of this run so that F4 will go
	// to first error of this run.
	SendOutput(SCI_GOTOPOS, originalEnd);
	SendMessage(wSciTE.GetID(), WM_COMMAND, IDM_FINISHEDEXECUTE, 0);
}

void ExecThread(void *ptw) {
	SciTEWin *tw = reinterpret_cast<SciTEWin *>(ptw);
	tw->ProcessExecute();
}

struct ShellErr {
	DWORD code;
	const char *descr;
};

void SciTEWin::ShellExec(const SString &cmd, const SString &dir) {

	char *mycmd;

	// guess if cmd is an executable, if this succeeds it can
	// contain spaces without enclosing it with "
	char *mycmdcopy = strdup(cmd.c_str());
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
	if ((s != NULL) && ((*(s+4) == '\0') || (*(s+4) == ' '))) {
		int len_mycmd = s - mycmdcopy  + 4;
		free(mycmdcopy);
		mycmdcopy = strdup(cmd.c_str());
		mycmd = mycmdcopy;
		mycmd_end = mycmdcopy + len_mycmd;
	} else {
		free(mycmdcopy);
		mycmdcopy = strdup(cmd.c_str());
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

	DWORD rc = reinterpret_cast<DWORD>(
					ShellExecute(
						wSciTE.GetID(), // parent wnd for msgboxes during app start
						NULL,  // cmd is open
						mycmd, // file to open
						myparams, // parameters
						dir.c_str(), // launch directory
						SW_SHOWNORMAL)); //default show cmd

	if (rc > 32) {
		// it worked!
		free(mycmdcopy);
		return;
	}

	const int numErrcodes = 15;
	static const ShellErr field[numErrcodes] = {
		{ 0, "The operating system is out of memory or resources." },
		{ ERROR_FILE_NOT_FOUND, "The specified file was not found." },
		{ ERROR_PATH_NOT_FOUND, "The specified path was not found." },
		{ ERROR_BAD_FORMAT, "The .exe file is invalid (non-Win32\256 .exe or error in .exe image)." },
		{ SE_ERR_ACCESSDENIED, "The operating system denied access to the specified file." },
		{ SE_ERR_ASSOCINCOMPLETE, "The file name association is incomplete or invalid." },
		{ SE_ERR_DDEBUSY, "The DDE transaction could not be completed because other DDE transactions were being processed." },
		{ SE_ERR_DDEFAIL, "The DDE transaction failed." },
		{ SE_ERR_DDETIMEOUT, "The DDE transaction could not be completed because the request timed out." },
		{ SE_ERR_DLLNOTFOUND, "The specified dynamic-link library was not found." },
		{ SE_ERR_FNF, "The specified file was not found." },
		{ SE_ERR_NOASSOC, "There is no application associated with the given file name extension." },
		{ SE_ERR_OOM, "There was not enough memory to complete the operation." },
		{ SE_ERR_PNF, "The specified path was not found." },
		{ SE_ERR_SHARE, "A sharing violation occurred." },
	};

	int i;
	for (i = 0; i < numErrcodes; ++i) {
		if (field[i].code == rc)
			break;
	}

	SString errormsg("Error while launching:\n\"");
	errormsg += mycmdcopy;
	if (myparams != NULL) {
		errormsg += "\" with Params:\n\"";
		errormsg += myparams;
	}
	errormsg += "\"\n";
	if (i < numErrcodes) {
		errormsg += field[i].descr;
	} else {
		errormsg += "Unknown error code:";
		errormsg += SString(rc).c_str();
	}
	MessageBox(wSciTE.GetID(), errormsg.c_str(), appName, MB_OK);

	free(mycmdcopy);
}

void SciTEWin::Execute() {
	SciTEBase::Execute();

	_beginthread(ExecThread, 1024 * 1024, reinterpret_cast<void *>(this));
}

void SciTEWin::StopExecute() {
	InterlockedExchange(&cancelFlag, 1L);
}

void SciTEWin::AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, bool forceQueue) {
	if (cmd.length()) {
		if ((jobType == jobShell) && !forceQueue) {
			ShellExec(cmd, dir);
		} else {
			SciTEBase::AddCommand(cmd, dir, jobType, forceQueue);
		}
	}
}

BOOL CALLBACK SciTEWin::GoLineDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static int *pLineNo;

	switch (message) {

	case WM_INITDIALOG:
		pLineNo = reinterpret_cast<int *>(lParam);
		SendDlgItemMessage(hDlg, IDGOLINE, EM_LIMITTEXT, 10, 1);
		SetDlgItemInt(hDlg, IDCURRLINE, pLineNo[0], FALSE);
		SetDlgItemInt(hDlg, IDLASTLINE, pLineNo[1], FALSE);
		return TRUE;

	case WM_CLOSE:
		SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			EndDialog(hDlg, IDCANCEL);
			return FALSE;
		} else if (ControlIDOfCommand(wParam) == IDOK) {
			BOOL bOK;
			pLineNo[0] = static_cast<int>(GetDlgItemInt(hDlg, IDGOLINE, &bOK, FALSE));
			if (!bOK)
				pLineNo[0] = -1;
			//pLineNo[1] = (SendDlgItemMessage(hDlg, IDEXTEND, BM_GETCHECK, 0, 0L) ==
			//	BST_CHECKED ? TRUE : FALSE);
			EndDialog(hDlg, IDOK);
			return TRUE;
		}
	}

	return FALSE;
}

void SciTEWin::GoLineDialog() {
	int lineNo[2] = { 0, 0 };
	lineNo[0] = SendEditor(EM_LINEFROMCHAR, static_cast<WPARAM>(-1), 0L) + 1;
	lineNo[1] = SendEditor(EM_GETLINECOUNT, 0, 0L);
	if (DoDialog(hInstance, "GoLine", wSciTE.GetID(),
	             reinterpret_cast<DLGPROC>(GoLineDlg),
	             reinterpret_cast<DWORD>(lineNo)) == IDOK) {
		//Platform::DebugPrintf("asked to go to %d\n", lineNo);
		if (lineNo[0] != -1) {
			//if (lineNo[1] == TRUE) {
			//	int selStart, selStop;
			//	selStart = SendEditor(SCI_GETANCHOR, 0, 0L);
			//	selStop = SendEditor(EM_LINEINDEX, lineNo[0] - 1, 0L);
			//	SendEditor(EM_SETSEL, selStart, selStop);
			//} else {
			SendEditor(SCI_GOTOLINE, lineNo[0]-1);
			//}
		}
	}
	SetFocus(wEditor.GetID());
}

void SciTEWin::FindReplace(bool replace) {
	replacing = replace;
}

void SciTEWin::DestroyFindReplace() {
	if (wFindReplace.Created()) {
		::EndDialog(wFindReplace.GetID(), IDCANCEL);
		wFindReplace = 0;
	}
}

void SciTEWin::AboutDialog() {
	DoDialog(hInstance, "About", wSciTE.GetID(), NULL, 0);
	::SetFocus(wEditor.GetID());
}

void SciTEWin::QuitProgram() {
	if (SaveIfUnsure() != IDCANCEL) {
		::PostQuitMessage(0);
	}
}

PRectangle SciTEWin::GetClientRectangle() {
	return wSciTE.GetClientPosition();
}

void SciTEWin::Run(const char *cmdLine) {
	Open(cmdLine, true);
	wSciTE.Show();
}

void SciTEWin::Paint(Surface *surfaceWindow, PRectangle) {
	PRectangle rcClient = GetClientRectangle();
	int heightClient = rcClient.bottom - rcClient.top;
	int widthClient = rcClient.right - rcClient.left;

	surfaceWindow->FillRectangle(rcClient, GetSysColor(COLOR_3DFACE));

	if (splitVertical) {
		surfaceWindow->PenColour(GetSysColor(COLOR_3DHILIGHT));
		surfaceWindow->MoveTo(widthClient - (heightOutput + heightBar - 1), 0);
		surfaceWindow->LineTo(widthClient - (heightOutput + heightBar - 1), heightClient);

		surfaceWindow->PenColour(GetSysColor(COLOR_3DSHADOW));
		surfaceWindow->MoveTo(widthClient - (heightOutput + 2), 0);
		surfaceWindow->LineTo(widthClient - (heightOutput + 2), heightClient);

		surfaceWindow->PenColour(GetSysColor(COLOR_3DDKSHADOW));
		surfaceWindow->MoveTo(widthClient - (heightOutput + 1), 0);
		surfaceWindow->LineTo(widthClient - (heightOutput + 1), heightClient);
	} else {
		surfaceWindow->PenColour(GetSysColor(COLOR_3DHILIGHT));
		surfaceWindow->MoveTo(0, heightClient - (heightOutput + heightBar - 1));
		surfaceWindow->LineTo(widthClient, heightClient - (heightOutput + heightBar - 1));

		surfaceWindow->PenColour(GetSysColor(COLOR_3DSHADOW));
		surfaceWindow->MoveTo(0, heightClient - (heightOutput + 2));
		surfaceWindow->LineTo(widthClient, heightClient - (heightOutput + 2));

		surfaceWindow->PenColour(GetSysColor(COLOR_3DDKSHADOW));
		surfaceWindow->MoveTo(0, heightClient - (heightOutput + 1));
		surfaceWindow->LineTo(widthClient, heightClient - (heightOutput + 1));
	}
}

LRESULT SciTEWin::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	//Platform::DebugPrintf("start wnd proc %x %x\n",iMessage, wSciTE.GetID());
	switch (iMessage) {

	case WM_CREATE:
		wEditor = ::CreateWindow(
		              "Scintilla",
		              "Source",
		              WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN,
		              0, 0,
		              100, 100,
		              wSciTE.GetID(),
		              reinterpret_cast<HMENU>(IDM_SRCWIN),
		              hInstance,
		              0);
		if (!wEditor.Created())
			exit(FALSE);
		wEditor.Show();
		SendEditor(SCI_ASSIGNCMDKEY, VK_RETURN, SCI_NEWLINE);
		SendEditor(SCI_ASSIGNCMDKEY, VK_TAB, SCI_TAB);
		SendEditor(SCI_ASSIGNCMDKEY, VK_TAB | (SHIFT_PRESSED << 16), SCI_BACKTAB);
		SetFocus(wEditor.GetID());

		wOutput = ::CreateWindow(
		              "Scintilla",
		              "Run",
		              WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN,
		              0, 0,
		              100, 100,
		              wSciTE.GetID(),
		              reinterpret_cast<HMENU>(IDM_RUNWIN),
		              hInstance,
		              0);
		if (!wOutput.Created())
			exit(FALSE);
		wOutput.Show();
		// No selection margin on output window
		SendOutput(SCI_SETMARGINWIDTHN, 1, 0);
		//SendOutput(SCI_SETCARETPERIOD, 0);
		SendOutput(SCI_ASSIGNCMDKEY, VK_RETURN, SCI_NEWLINE);
		SendOutput(SCI_ASSIGNCMDKEY, VK_TAB, SCI_TAB);
		SendOutput(SCI_ASSIGNCMDKEY, VK_TAB | (SHIFT_PRESSED << 16), SCI_BACKTAB);
		::DragAcceptFiles(wSciTE.GetID(), true);
		break;

	case WM_PAINT: {
			PAINTSTRUCT ps;
			::BeginPaint(wSciTE.GetID(), &ps);
			Surface surfaceWindow;
			surfaceWindow.Init(ps.hdc);
			PRectangle rcPaint(ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
			Paint(&surfaceWindow, rcPaint);
			surfaceWindow.Release();
			::EndPaint(wSciTE.GetID(), &ps);
			return 0;
		}

	case WM_COMMAND:
		Command(wParam, lParam);
		break;

	case WM_NOTIFY:
		Notify(reinterpret_cast<SCNotification *>(lParam));
		break;

	case WM_KEYDOWN:
		//Platform::DebugPrintf("keydown %d %x %x\n",iMessage, wParam, lParam);
		break;

	case WM_KEYUP:
		//Platform::DebugPrintf("keyup %d %x %x\n",iMessage, wParam, lParam);
		break;

	case WM_SIZE:
		//Platform::DebugPrintf("size %d %x %x\n",iMessage, wParam, lParam);
		if (wParam != 1)
			SizeSubWindows();
		break;

	case WM_GETMINMAXINFO:
		return ::DefWindowProc(wSciTE.GetID(), iMessage, wParam, lParam);

	case WM_LBUTTONDOWN:
		ptStartDrag = Point::FromLong(lParam);
		capturedMouse = true;
		heightOutputStartDrag = heightOutput;
		::SetCapture(wSciTE.GetID());
		//Platform::DebugPrintf("Click %x %x\n", wParam, lParam);
		break;

	case WM_MOUSEMOVE:
		if (capturedMouse) {
			MoveSplit(Point::FromLong(lParam));
		}
		break;

	case WM_LBUTTONUP:
		if (capturedMouse) {
			MoveSplit(Point::FromLong(lParam));
			capturedMouse = false;
			::ReleaseCapture();
		}
		break;

	case WM_SETCURSOR:
		if (ControlIDOfCommand(lParam) == HTCLIENT) {
			Point ptCursor;
			GetCursorPos(reinterpret_cast<POINT *>(&ptCursor));
			PRectangle rcScintilla = wEditor.GetPosition();
			PRectangle rcOutput = wOutput.GetPosition();
			if (!rcScintilla.Contains(ptCursor) && !rcOutput.Contains(ptCursor)) {
				wSciTE.SetCursor(splitVertical ? Window::cursorHoriz : Window::cursorVert);
				return TRUE;
			}
		}
		return ::DefWindowProc(wSciTE.GetID(), iMessage, wParam, lParam);

	case WM_INITMENU:
		CheckMenus();
		break;

	case WM_CLOSE:
		if (SaveIfUnsure() != IDCANCEL) {
			::PostQuitMessage(0);
		}
		return 0;
		
	case WM_DESTROY:
		// Unhook before destroying wEditor and thus its document.
		wEditor.Destroy();
		wOutput.Destroy();
		break;

	case WM_SETTINGCHANGE:
		//Platform::DebugPrintf("** Setting Changed\n");
		SendEditor(WM_SETTINGCHANGE, wParam, lParam);
		SendOutput(WM_SETTINGCHANGE, wParam, lParam);
		break;

	case WM_SYSCOLORCHANGE:
		//Platform::DebugPrintf("** Color Changed\n");
		SendEditor(WM_SYSCOLORCHANGE, wParam, lParam);
		SendOutput(WM_SYSCOLORCHANGE, wParam, lParam);
		break;

	case WM_PALETTECHANGED:
		//Platform::DebugPrintf("** Palette Changed\n");
		if (wParam != reinterpret_cast<WPARAM>(wSciTE.GetID())) {
			SendEditor(WM_PALETTECHANGED, wParam, lParam);
			//SendOutput(WM_PALETTECHANGED, wParam, lParam);
		}
		break;

	case WM_QUERYNEWPALETTE:
		//Platform::DebugPrintf("** Query palette\n");
		SendEditor(WM_QUERYNEWPALETTE, wParam, lParam);
		//SendOutput(WM_QUERYNEWPALETTE, wParam, lParam);
		return TRUE;

	case WM_ACTIVATEAPP:
		SendEditor(EM_HIDESELECTION, !wParam);
		// Do not want to display dialog yet as may be in middle of system mouse capture
		::PostMessage(wSciTE.GetID(), WM_COMMAND, IDM_ACTIVATE, wParam);
		break;

	case WM_ACTIVATE:
		SetFocus(wEditor.GetID());
		break;

	case WM_DROPFILES:
		if (SaveIfUnsure() != IDCANCEL) {
			char pathDropped[MAX_PATH];
			HDROP hdrop = reinterpret_cast<HDROP>(wParam);
			int filesDropped = DragQueryFile(hdrop, 0xffffffff, pathDropped, sizeof(pathDropped));
			if (filesDropped == 1) {
				DragQueryFile(hdrop, 0, pathDropped, sizeof(pathDropped));
				Open(pathDropped);
			}
			DragFinish(hdrop);
		}
		break;

	default:
		//Platform::DebugPrintf("default wnd proc %x %d %d\n",iMessage, wParam, lParam);
		return ::DefWindowProc(wSciTE.GetID(), iMessage, wParam, lParam);
	}
	//Platform::DebugPrintf("end wnd proc\n");
	return 0l;
}

LRESULT PASCAL SciTEWin::TWndProc(
    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	//Platform::DebugPrintf("W:%x M:%d WP:%x L:%x\n", hWnd, iMessage, wParam, lParam);

	// Find C++ object associated with window.
	SciTEWin *scite = reinterpret_cast<SciTEWin *>(GetWindowLong(hWnd, 0));
	// scite will be zero if WM_CREATE not seen yet
	if (scite == 0) {
		if (iMessage == WM_CREATE) {
			LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			scite = reinterpret_cast<SciTEWin *>(cs->lpCreateParams);
			scite->wSciTE = hWnd;
			SetWindowLong(hWnd, 0, reinterpret_cast<LONG>(scite));
			return scite->WndProc(iMessage, wParam, lParam);
		} else
			return DefWindowProc(hWnd, iMessage, wParam, lParam);
	} else
		return scite->WndProc(iMessage, wParam, lParam);
}

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpszCmdLine, int) {

	//Platform::DebugPrintf("Command line is \n%s\n<<", lpszCmdLine);

	HACCEL hAccTable = LoadAccelerators(hInstance, "ACCELS");

	SciTEWin::Register(hInstance);
#ifdef STATIC_BUILD
	Scintilla_RegisterClasses(hInstance);
#else
	HMODULE hmod = ::LoadLibrary("SciLexer.DLL");
	if (hmod==NULL)
		MessageBox(NULL, "The Scintilla DLL could not be loaded.  SciTE will now close", "Error loading Scintilla", MB_OK | MB_ICONERROR);
#endif

	MSG msg;
	msg.wParam = 0;
	{
		SciTEWin MainWind;
		MainWind.Run(lpszCmdLine);
		bool going = true;
		while (going) {
			going = ::GetMessage(&msg, NULL, 0, 0);
			if (going) {
				if (!MainWind.ModelessHandler(&msg)) {
					if (::TranslateAccelerator(MainWind.GetID(), hAccTable, &msg) == 0) {
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
				}
			}
		}
	}

#ifndef STATIC_BUILD
	::FreeLibrary(hmod);
#endif

	return msg.wParam;
}
