// SciTE - Scintilla based Text Editor
/** @file SciTEWinBar.cxx
 ** Bar and menu code for the Windows version of the editor.
 **/
// Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include "SciTEWin.h"

/**
 * Update the status bar text.
 */
void SciTEWin::SetStatusBarText(const char *s) {
	::SendMessage(reinterpret_cast<HWND>(wStatusBar.GetID()), 
		SB_SETTEXT, 0, reinterpret_cast<LPARAM>(s));
}

#ifdef __MINGW_H	
// Mingw headers do not have NMMOUSE, TTN_GETDISPINFO or NMTTDISPINFO
struct NMMOUSE {
	NMHDR hdr;
	DWORD dwItemSpec;
	// Other fields
};
#define TTN_GETDISPINFO TTN_FIRST
struct NMTTDISPINFO {
	NMHDR hdr;
	LPSTR lpszText;
	// Other fields
};
#endif

/**
 * Manage Windows specific notifications.
 */
void SciTEWin::Notify(SCNotification *notification) {
	switch (notification->nmhdr.code) {
	case TCN_SELCHANGE:
		// Change of tab

		if (notification->nmhdr.idFrom == IDM_TABWIN) {
			int index = Platform::SendScintilla(wTabBar.GetID(), TCM_GETCURSEL, (WPARAM)0, (LPARAM)0);
			SetDocumentAt(index);
			CheckReload();
		}
		break;

	case NM_CLICK:
		// Click on a control
		if (notification->nmhdr.idFrom == IDM_STATUSWIN) {
			// Click on the status bar
			NMMOUSE *pNMMouse = (NMMOUSE *)notification;
			switch (pNMMouse->dwItemSpec) {
			case 0:		/* Display of status */
				sbNum++;
				if (sbNum > props.GetInt("statusbar.number")) {
					sbNum = 1;
				}
				UpdateStatusBar(true);
				break;
			default:
				break;
			}
		}
		break;

	case TTN_GETDISPINFO:
		// Ask for tooltip text
		{
			static char ttt[MAX_PATH];
			const char *ttext=0;
			NMTTDISPINFO *pDispInfo = (NMTTDISPINFO *)notification;
			// Toolbar tooltips
			switch (notification->nmhdr.idFrom) {
			case IDM_NEW:
				ttext = "New";
				break;
			case IDM_OPEN:
				ttext = "Open";
				break;
			case IDM_SAVE:
				ttext = "Save";
				break;
			case IDM_CLOSE:
				ttext = "Close";
				break;
			case IDM_PRINT:
				ttext = "Print";
				break;
			case IDM_CUT:
				ttext = "Cut";
				break;
			case IDM_COPY:
				ttext = "Copy";
				break;
			case IDM_PASTE:
				ttext = "Paste";
				break;
			case IDM_CLEAR:
				ttext = "Delete";
				break;
			case IDM_UNDO:
				ttext = "Undo";
				break;
			case IDM_REDO:
				ttext = "Redo";
				break;
			case IDM_FIND:
				ttext = "Find";
				break;
			case IDM_REPLACE:
				ttext = "Replace";
				break;
			case IDM_MACRORECORD:
				ttext = "Record Macro";
				break;
			case IDM_MACROSTOPRECORD:
				ttext = "Stop Recording";
				break;
			case IDM_MACROPLAY:
				ttext = "Run Macro";
				break;
			default:
				{
					// notification->nmhdr.idFrom appears to be the buffer number for tabbar tooltips
					Point ptCursor;
					::GetCursorPos(reinterpret_cast<POINT *>(&ptCursor));
					Point ptClient = ptCursor;
					::ScreenToClient(reinterpret_cast<HWND>(wTabBar.GetID()), 
						reinterpret_cast<POINT *>(&ptClient));
					TCHITTESTINFO info;
					info.pt.x = ptClient.x; info.pt.y = ptClient.y;
					int index = Platform::SendScintilla(wTabBar.GetID(), TCM_HITTEST, (WPARAM)0, (LPARAM) & info);
					pDispInfo->lpszText = const_cast<char *>(buffers.buffers[index].FullPath());
				}
				break;
			}
			if (ttext) {
				SString localised = LocaliseString(ttext);
				strcpy(ttt, localised.c_str());
				pDispInfo->lpszText = ttt;
			}
			break;
		}

	case SCN_CHARADDED:
		if ((notification->nmhdr.idFrom == IDM_RUNWIN) &&
			executing &&
			hWriteSubProcess) {
			char chToWrite = static_cast<char>(notification->ch);
			if (chToWrite != '\r') {
				DWORD bytesWrote = 0;
				::WriteFile(hWriteSubProcess, &chToWrite,
									   1, &bytesWrote, NULL);
			}
		} else {
			SciTEBase::Notify(notification);
		}
		break;
	
	default:    	// Scintilla notification, use default treatment
		SciTEBase::Notify(notification);
		break;
	}
}

void SciTEWin::ShowToolBar() {
	SizeSubWindows();
}

void SciTEWin::ShowTabBar() {
	SizeSubWindows();
}

void SciTEWin::ShowStatusBar() {
	SizeSubWindows();
}

/**
 * Resize the content windows, embedding the editor and output windows.
 */
void SciTEWin::SizeContentWindows() {
	PRectangle rcInternal = wContent.GetClientPosition();

	int w = rcInternal.Width();
	int h = rcInternal.Height();
	heightOutput = NormaliseSplit(heightOutput);

	if (splitVertical) {
		wEditor.SetPosition(PRectangle(0, 0, w - heightOutput - heightBar, h));
		wOutput.SetPosition(PRectangle(w - heightOutput, 0, w, h));
	} else {
		wEditor.SetPosition(PRectangle(0, 0, w, h - heightOutput - heightBar));
		wOutput.SetPosition(PRectangle(0, h - heightOutput, w, h));
	}
	wContent.InvalidateAll();
}

/**
 * Resize the sub-windows, ie. the toolbar, tab bar, status bar. And call @a SizeContentWindows.
 */
void SciTEWin::SizeSubWindows() {
	PRectangle rcClient = wSciTE.GetClientPosition();
	bool showTab = false;

	//::SendMessage(MainHWND(), WM_SETREDRAW, false, 0); // suppress flashing
	visHeightTools = tbVisible ? heightTools : 0;

	if (tabVisible) {	// ? hide one tab only
		showTab = tabHideOne ?
		          ::SendMessage(reinterpret_cast<HWND>(wTabBar.GetID()), TCM_GETITEMCOUNT, 0, 0) > 1 :
		          true;
	}

	if (showTab) {
		int tabNb = ::SendMessage(reinterpret_cast<HWND>(wTabBar.GetID()), TCM_GETROWCOUNT, 0, 0);
		visHeightTab = ((tabNb-1) * (heightTab-6)) + heightTab;
	} else {
		visHeightTab = 0;
	}
	visHeightStatus = sbVisible ? heightStatus : 0;
	visHeightEditor = rcClient.Height() - visHeightTools - visHeightStatus - visHeightTab;
	if (visHeightEditor < 1) {
		visHeightTools = 1;
		visHeightStatus = 1;
		visHeightTab = 1;
		visHeightEditor = rcClient.Height() - visHeightTools - visHeightStatus - visHeightTab;
	}
	if (tbVisible) {
		wToolBar.SetPosition(PRectangle(
			 rcClient.left, rcClient.top, rcClient.right, visHeightTools));
		wToolBar.Show(true);
	} else {
		wToolBar.Show(false);
		wToolBar.SetPosition(PRectangle(
			 rcClient.left, rcClient.top - 2, rcClient.Width(), 1));
	}
	if (showTab) {
		wTabBar.SetPosition(PRectangle(
			rcClient.left, rcClient.top + visHeightTools,
			rcClient.right, rcClient.top + visHeightTab + visHeightTools));
		wTabBar.Show(true);
	} else {
		wTabBar.Show(false);
		wTabBar.SetPosition(PRectangle(
		                        rcClient.left, rcClient.top - 2,
		                        rcClient.Width(), 1));
	}
	if (sbVisible) {
		wStatusBar.SetPosition(PRectangle(
			rcClient.left, rcClient.top + visHeightTools + visHeightTab + visHeightEditor,
			rcClient.right, 
			rcClient.top + visHeightTools + visHeightTab + visHeightEditor + visHeightStatus));
		wStatusBar.Show(true);
	} else {
		wStatusBar.Show(false);
		wStatusBar.SetPosition(PRectangle(
		                           rcClient.left, rcClient.top - 2, rcClient.Width(), 1));
	}

	wContent.SetPosition(PRectangle(
		rcClient.left, rcClient.top + visHeightTab + visHeightTools, 
		rcClient.right,
	        rcClient.top + visHeightTab + visHeightTools + visHeightEditor));
	SizeContentWindows();
	//::SendMessage(MainHWND(), WM_SETREDRAW, true, 0);
	//::RedrawWindow(MainHWND(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void SciTEWin::SetMenuItem(int menuNumber, int position, int itemID,
                           const char *text, const char *mnemonic) {
	// On Windows the menu items are modified if they already exist or are created
	HMENU hmenuBar = ::GetMenu(MainHWND());
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
	HMENU hmenuBar = ::GetMenu(MainHWND());
	if (itemID) {
		HMENU hmenu = ::GetSubMenu(hmenuBar, menuNumber);
		::DeleteMenu(hmenu, itemID, MF_BYCOMMAND);
	} else {
		::DeleteMenu(hmenuBar, menuNumber, MF_BYPOSITION);
	}
}

void SciTEWin::CheckAMenuItem(int wIDCheckItem, bool val) {
	if (val)
		CheckMenuItem(GetMenu(MainHWND()), wIDCheckItem, MF_CHECKED | MF_BYCOMMAND);
	else
		CheckMenuItem(GetMenu(MainHWND()), wIDCheckItem, MF_UNCHECKED | MF_BYCOMMAND);
}

void EnableButton(HWND wTools, int id, bool enable) {
	::SendMessage(wTools, TB_ENABLEBUTTON, id,
	              Platform::LongFromTwoShorts(static_cast<short>(enable ? TRUE : FALSE), 0));
}

void SciTEWin::EnableAMenuItem(int wIDCheckItem, bool val) {
	if (val)
		EnableMenuItem(::GetMenu(MainHWND()), wIDCheckItem, MF_ENABLED | MF_BYCOMMAND);
	else
		EnableMenuItem(::GetMenu(MainHWND()), wIDCheckItem, MF_DISABLED | MF_GRAYED | MF_BYCOMMAND);
	::EnableButton(reinterpret_cast<HWND>(wToolBar.GetID()), wIDCheckItem, val);
}

void SciTEWin::CheckMenus() {
	SciTEBase::CheckMenus();
	CheckMenuRadioItem(GetMenu(MainHWND()), IDM_EOL_CRLF, IDM_EOL_LF,
	                   SendEditor(SCI_GETEOLMODE) - SC_EOL_CRLF + IDM_EOL_CRLF, 0);
}

void SciTEWin::LocaliseMenu(HMENU hmenu) {
	for (int i=0; i<=::GetMenuItemCount(hmenu); i++) {
		char buff[200];
		MENUITEMINFO mii;
		memset(&mii, 0, sizeof(mii));
		mii.cbSize = sizeof(mii);
		mii.fMask = MIIM_CHECKMARKS | MIIM_DATA | MIIM_ID | 
			MIIM_STATE | MIIM_SUBMENU | MIIM_TYPE;
		mii.dwTypeData = buff;
		mii.cch = sizeof(buff)-1;
		if (::GetMenuItemInfo(hmenu, i, TRUE, &mii)) {
			if (mii.hSubMenu) {
				LocaliseMenu(mii.hSubMenu);
			}
			if (mii.fType == MFT_STRING) {
				if (mii.dwTypeData) {
					SString text(mii.dwTypeData);
					SString accel(mii.dwTypeData);
					int len = text.length();
					int tab = text.search("\t");
					if (tab != -1) {
						text.remove(tab, len - tab);
						accel.remove(0, tab + 1);
					} else {
						accel = "";
					}
					text = LocaliseString(text.c_str(), false);
					if (text.length()) {
						text += "\t";
						text += accel;
						mii.dwTypeData = const_cast<char *>(text.c_str());
						::SetMenuItemInfo(hmenu, i, TRUE, &mii);
					}
				}
			}
		}
	}
}

void SciTEWin::LocaliseMenus() {
	LocaliseMenu(::GetMenu(MainHWND()));
	::DrawMenuBar(MainHWND());
}

void SciTEWin::LocaliseControl(HWND w) {
	char wtext[200];
	if (::GetWindowText(w, wtext, sizeof(wtext))) {
		SString text = LocaliseString(wtext, false);
		if (text.length())
			::SetWindowText(w, text.c_str());
	}
}

void SciTEWin::LocaliseDialog(HWND wDialog) {
	LocaliseControl(wDialog);
	HWND wChild = ::GetWindow(wDialog, GW_CHILD);
	while (wChild) {
		LocaliseControl(wChild);
		wChild = GetWindow(wChild, GW_HWNDNEXT);
	}
}

// Mingw headers do not have this:
#ifndef TBSTYLE_FLAT
#define TBSTYLE_FLAT 0x0800
#endif
#ifndef TB_LOADIMAGES
#define TB_LOADIMAGES (WM_USER + 50)
#endif

#define ELEMENTS(a)	(sizeof(a) / sizeof(a[0]))

struct BarButton {
	int id;
	int cmd;
};

static BarButton bbs[] = {
    { -1, 0 },
    { STD_FILENEW, IDM_NEW },
    { STD_FILEOPEN, IDM_OPEN },
    { STD_FILESAVE, IDM_SAVE },
    { 0, IDM_CLOSE },
    { -1, 0 },
    { STD_PRINT, IDM_PRINT },
    { -1, 0 },
    { STD_CUT, IDM_CUT },
    { STD_COPY, IDM_COPY },
    { STD_PASTE, IDM_PASTE },
    { STD_DELETE, IDM_CLEAR },
    { -1, 0 },
    { STD_UNDO, IDM_UNDO },
    { STD_REDOW, IDM_REDO },
    { -1, 0 },
    { STD_FIND, IDM_FIND },
    { STD_REPLACE, IDM_REPLACE },
};

/**
 * Create all the needed windows.
 */
void SciTEWin::Creation() {

	wContent = ::CreateWindowEx(
	               WS_EX_CLIENTEDGE,
	               classNameInternal,
	               "Source",
	               WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	               0, 0,
	               100, 100,
	               MainHWND(),
	               reinterpret_cast<HMENU>(2000),
	               hInstance,
	               reinterpret_cast<LPSTR>(this));
	wContent.Show();

	wEditor = ::CreateWindowEx(
	              0,
	              "Scintilla",
	              "Source",
	              WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	              0, 0,
	              100, 100,
	              reinterpret_cast<HWND>(wContent.GetID()),
	              reinterpret_cast<HMENU>(IDM_SRCWIN),
	              hInstance,
	              0);
	if (!wEditor.Created())
		exit(FALSE);
	fnEditor = reinterpret_cast<SciFnDirect>(::SendMessage(
	               reinterpret_cast<HWND>(wEditor.GetID()), SCI_GETDIRECTFUNCTION, 0, 0));
	ptrEditor = ::SendMessage(reinterpret_cast<HWND>(wEditor.GetID()), 
		SCI_GETDIRECTPOINTER, 0, 0);
	wEditor.Show();
	SendEditor(SCI_USEPOPUP, 0);
	WindowSetFocus(wEditor);

	wOutput = ::CreateWindowEx(
	              0,
	              "Scintilla",
	              "Run",
	              WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	              0, 0,
	              100, 100,
	              reinterpret_cast<HWND>(wContent.GetID()),
	              reinterpret_cast<HMENU>(IDM_RUNWIN),
	              hInstance,
	              0);
	if (!wOutput.Created())
		exit(FALSE);
	fnOutput = reinterpret_cast<SciFnDirect>(::SendMessage(
	               reinterpret_cast<HWND>(wOutput.GetID()), SCI_GETDIRECTFUNCTION, 0, 0));
	ptrOutput = ::SendMessage(reinterpret_cast<HWND>(wOutput.GetID()), 
		SCI_GETDIRECTPOINTER, 0, 0);
	wOutput.Show();
	// No selection margin on output window
	SendOutput(SCI_SETMARGINWIDTHN, 1, 0);
	//SendOutput(SCI_SETCARETPERIOD, 0);
	SendOutput(SCI_USEPOPUP, 0);
	::DragAcceptFiles(MainHWND(), true);

	HWND hwndToolBar = ::CreateWindowEx(
	               0,
	               TOOLBARCLASSNAME,
	               "",
	               WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
	               TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
	               0, 0,
	               100, heightTools,
	               MainHWND(),
	               reinterpret_cast<HMENU>(IDM_TOOLWIN),
	               hInstance,
	               0);
	wToolBar = hwndToolBar;
	
	::SendMessage(hwndToolBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	::SendMessage(hwndToolBar, TB_LOADIMAGES, IDB_STD_SMALL_COLOR,
	              reinterpret_cast<LPARAM>(HINST_COMMCTRL));

	TBADDBITMAP addbmp= { hInstance, IDR_CLOSEFILE };
	::SendMessage(hwndToolBar, TB_ADDBITMAP, 1, (LPARAM)&addbmp);

	TBBUTTON tbb[ELEMENTS(bbs)];
	for (unsigned int i = 0;i < ELEMENTS(bbs);i++) {
		if (bbs[i].cmd == IDM_CLOSE)
			tbb[i].iBitmap = STD_PRINT + 1;
		else
			tbb[i].iBitmap = bbs[i].id;
		tbb[i].idCommand = bbs[i].cmd;
		tbb[i].fsState = TBSTATE_ENABLED;
		if ( -1 == bbs[i].id)
			tbb[i].fsStyle = TBSTYLE_SEP;
		else
			tbb[i].fsStyle = TBSTYLE_BUTTON;
		tbb[i].dwData = 0;
		tbb[i].iString = 0;
	}

	::SendMessage(hwndToolBar, TB_ADDBUTTONS, ELEMENTS(bbs), reinterpret_cast<LPARAM>(tbb));

	wToolBar.Show();

	INITCOMMONCONTROLSEX icce;
	icce.dwSize = sizeof(icce);
	icce.dwICC = ICC_TAB_CLASSES;
	InitCommonControlsEx(&icce);
	wTabBar = ::CreateWindowEx(
	              0,
	              WC_TABCONTROL,
	              "Tab",
	              WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
	              TCS_FOCUSNEVER | TCS_TOOLTIPS,
	              0, 0,
	              100, heightTab,
	              MainHWND(),
	              reinterpret_cast<HMENU>(IDM_TABWIN),
	              hInstance,
	              0 );

	if (!wTabBar.Created())
		exit(FALSE);

	fontTabs = ::CreateFont( 8, 0, 0, 0,
	                         FW_NORMAL,
	                         0, 0, 0, 0,
	                         0, 0, 0, 0,
	                         "Ms Sans Serif");
	::SendMessage(reinterpret_cast<HWND>(wTabBar.GetID()),
	              WM_SETFONT,
	              reinterpret_cast<WPARAM>(fontTabs),     // handle to font
	              0);    // redraw option

	wTabBar.Show();

	wStatusBar = ::CreateWindowEx(
	                 0,
	                 STATUSCLASSNAME,
	                 "",
	                 WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPSIBLINGS,
	                 0, 0,
	                 100, heightStatus,
	                 MainHWND(),
	                 reinterpret_cast<HMENU>(IDM_STATUSWIN),
	                 hInstance,
	                 0);
	wStatusBar.Show();
	int widths[] = { 4000 };
	// Perhaps we can define a syntax to create more parts,
	// but it is probably an overkill for a marginal feature
	::SendMessage(reinterpret_cast<HWND>(wStatusBar.GetID()), 
		      SB_SETPARTS, 1,
		      reinterpret_cast<LPARAM>(widths));
}

