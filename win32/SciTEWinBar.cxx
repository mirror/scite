// SciTE - Scintilla based Text Editor
/** @file SciTEWinBar.cxx
 ** Bar and menu code for the Windows version of the editor.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include "SciTEWin.h"

/**
 * Update the status bar text.
 */
void SciTEWin::SetStatusBarText(const char *s) {
	::SendMessage(wStatusBar.GetID(), SB_SETTEXT, 1, reinterpret_cast<LPARAM>(s));
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
			case 1:		/* Display of status */
				sbNum++;
				if (sbNum > props.GetInt("statusbar.number")) {
					sbNum = 1;
				}
				UpdateStatusBar(true);
				break;
			case 2:		/* Refresh button */
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
			NMTTDISPINFO *pDispInfo = (NMTTDISPINFO *)notification;
			// Toolbar tooltips
			switch (notification->nmhdr.idFrom) {
			case IDM_NEW:
				pDispInfo->lpszText = "New";
				break;
			case IDM_OPEN:
				pDispInfo->lpszText = "Open";
				break;
			case IDM_SAVE:
				pDispInfo->lpszText = "Save";
				break;
			case IDM_PRINT:
				pDispInfo->lpszText = "Print";
				break;
			case IDM_CUT:
				pDispInfo->lpszText = "Cut";
				break;
			case IDM_COPY:
				pDispInfo->lpszText = "Copy";
				break;
			case IDM_PASTE:
				pDispInfo->lpszText = "Paste";
				break;
			case IDM_CLEAR:
				pDispInfo->lpszText = "Delete";
				break;
			case IDM_UNDO:
				pDispInfo->lpszText = "Undo";
				break;
			case IDM_REDO:
				pDispInfo->lpszText = "Redo";
				break;
			case IDM_FIND:
				pDispInfo->lpszText = "Find";
				break;
			case IDM_REPLACE:
				pDispInfo->lpszText = "Replace";
				break;
			case IDM_MACRORECORD:
				pDispInfo->lpszText = "Record Macro";
				break;
			case IDM_MACROSTOPRECORD:
				pDispInfo->lpszText = "Stop Recording";
				break;
			case IDM_MACROPLAY:
				pDispInfo->lpszText = "Run Macro";
				break;
			default:
				{
					// notification->nmhdr.idFrom appears to be the buffer number for tabbar tooltips
					Point ptCursor;
					::GetCursorPos(reinterpret_cast<POINT *>(&ptCursor));
					Point ptClient = ptCursor;
					::ScreenToClient(wTabBar.GetID(), reinterpret_cast<POINT *>(&ptClient));
					TCHITTESTINFO info;
					info.pt.x = ptClient.x; info.pt.y = ptClient.y;
					int index = Platform::SendScintilla(wTabBar.GetID(), TCM_HITTEST, (WPARAM)0, (LPARAM) & info);
					pDispInfo->lpszText = const_cast<char *>(buffers.buffers[index].FullPath());
				}
				break;
			}
			break;
		}

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

	//::SendMessage(wSciTE.GetID(), WM_SETREDRAW, false, 0); // suppress flashing
	visHeightTools = tbVisible ? heightTools : 0;

	if (tabVisible) {	// ? hide one tab only
		showTab = tabHideOne ?
		          ::SendMessage(wTabBar.GetID(), TCM_GETITEMCOUNT, 0, 0) > 1 :
		          true;
	}

	if (showTab) {
		int tabNb = ::SendMessage(wTabBar.GetID(), TCM_GETROWCOUNT, 0, 0);
		visHeightTab = tabNb * heightTab;
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
		                         rcClient.left, rcClient.top, rcClient.Width(), visHeightTools));
		wToolBar.Show(true);
	} else {
		wToolBar.Show(false);
		wToolBar.SetPosition(PRectangle(
		                         rcClient.left, rcClient.top - 2, rcClient.Width(), 1));
	}
	if (showTab) {
		wTabBar.SetPosition(PRectangle(
		                        rcClient.left, rcClient.top + visHeightTools,
		                        rcClient.Width(), visHeightTab + visHeightTools));
		wTabBar.Show(true);
	} else {
		wTabBar.Show(false);
		wTabBar.SetPosition(PRectangle(
		                        rcClient.left, rcClient.top - 2,
		                        rcClient.Width(), 1));
	}
	if (sbVisible) {
		int spw = props.GetInt("statusbar.size");
		if (spw <= 0) {
			spw = statusPosWidth;
		}
		int refreshWidth = 70;
		int startLineNum = rcClient.Width() - spw - refreshWidth;
		if (startLineNum < 0)
			startLineNum = 0;
		int widths[] = { startLineNum, rcClient.Width() - refreshWidth, rcClient.Width() };
		// Perhaps we can define a syntax to create more parts,
		// but it is probably an overkill for a marginal feature
		::SendMessage(wStatusBar.GetID(), SB_SETPARTS, 3,
		              reinterpret_cast<LPARAM>(widths));
		::SendMessage(wStatusBar.GetID(), SB_SETTEXT, 0 | SBT_NOBORDERS,
		              reinterpret_cast<LPARAM>(""));
		::SendMessage(wStatusBar.GetID(), SB_SETTEXT, 2 | SBT_POPOUT,
		              reinterpret_cast<LPARAM>("Refresh"));
		wStatusBar.SetPosition(PRectangle(rcClient.left,
		                                  rcClient.top + visHeightTools + visHeightTab + visHeightEditor,
		                                  rcClient.Width(), visHeightStatus));
		wStatusBar.Show(true);
	} else {
		wStatusBar.Show(false);
		wStatusBar.SetPosition(PRectangle(
		                           rcClient.left, rcClient.top - 2, rcClient.Width(), 1));
	}

	wContent.SetPosition(PRectangle(rcClient.left, visHeightTools + visHeightTab, rcClient.Width(),
	                                visHeightTools + visHeightTab + visHeightEditor));
	SizeContentWindows();
	//::SendMessage(wSciTE.GetID(), WM_SETREDRAW, true, 0);
	//::RedrawWindow(wSciTE.GetID(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void SciTEWin::SetMenuItem(int menuNumber, int position, int itemID,
                           const char *text, const char *mnemonic) {
	// On Windows the menu items are modified if they already exist or are created
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
	if (itemID) {
		HMENU hmenu = ::GetSubMenu(hmenuBar, menuNumber);
		::DeleteMenu(hmenu, itemID, MF_BYCOMMAND);
	} else {
		::DeleteMenu(hmenuBar, menuNumber, MF_BYPOSITION);
	}
}

void SciTEWin::CheckAMenuItem(int wIDCheckItem, bool val) {
	if (val)
		CheckMenuItem(GetMenu(wSciTE.GetID()), wIDCheckItem, MF_CHECKED | MF_BYCOMMAND);
	else
		CheckMenuItem(GetMenu(wSciTE.GetID()), wIDCheckItem, MF_UNCHECKED | MF_BYCOMMAND);
}

void EnableButton(HWND wTools, int id, bool enable) {
	::SendMessage(wTools, TB_ENABLEBUTTON, id,
	              Platform::LongFromTwoShorts(static_cast<short>(enable ? TRUE : FALSE), 0));
}

void SciTEWin::EnableAMenuItem(int wIDCheckItem, bool val) {
	if (val)
		EnableMenuItem(GetMenu(wSciTE.GetID()), wIDCheckItem, MF_ENABLED | MF_BYCOMMAND);
	else
		EnableMenuItem(GetMenu(wSciTE.GetID()), wIDCheckItem, MF_DISABLED | MF_GRAYED | MF_BYCOMMAND);
	::EnableButton(wToolBar.GetID(), wIDCheckItem, val);
}

void SciTEWin::CheckMenus() {
	SciTEBase::CheckMenus();
	CheckMenuRadioItem(GetMenu(wSciTE.GetID()), IDM_EOL_CRLF, IDM_EOL_LF,
	                   SendEditor(SCI_GETEOLMODE) - SC_EOL_CRLF + IDM_EOL_CRLF, 0);
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
	               wSciTE.GetID(),
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
	              wContent.GetID(),
	              reinterpret_cast<HMENU>(IDM_SRCWIN),
	              hInstance,
	              0);
	if (!wEditor.Created())
		exit(FALSE);
	fnEditor = reinterpret_cast<SciFnDirect>(::SendMessage(
	               wEditor.GetID(), SCI_GETDIRECTFUNCTION, 0, 0));
	ptrEditor = ::SendMessage(wEditor.GetID(), SCI_GETDIRECTPOINTER, 0, 0);
	wEditor.Show();
	//	SendEditor(SCI_ASSIGNCMDKEY, VK_RETURN, SCI_NEWLINE);
	//	SendEditor(SCI_ASSIGNCMDKEY, VK_TAB, SCI_TAB);
	//	SendEditor(SCI_ASSIGNCMDKEY, VK_TAB | (SCMOD_SHIFT << 16), SCI_BACKTAB);
	SetFocus(wEditor.GetID());

	wOutput = ::CreateWindowEx(
	              0,
	              "Scintilla",
	              "Run",
	              WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	              0, 0,
	              100, 100,
	              wContent.GetID(),
	              reinterpret_cast<HMENU>(IDM_RUNWIN),
	              hInstance,
	              0);
	if (!wOutput.Created())
		exit(FALSE);
	fnOutput = reinterpret_cast<SciFnDirect>(::SendMessage(
	               wOutput.GetID(), SCI_GETDIRECTFUNCTION, 0, 0));
	ptrOutput = ::SendMessage(wOutput.GetID(), SCI_GETDIRECTPOINTER, 0, 0);
	wOutput.Show();
	// No selection margin on output window
	SendOutput(SCI_SETMARGINWIDTHN, 1, 0);
	//SendOutput(SCI_SETCARETPERIOD, 0);
	//	SendOutput(SCI_ASSIGNCMDKEY, VK_RETURN, SCI_NEWLINE);
	//	SendOutput(SCI_ASSIGNCMDKEY, VK_TAB, SCI_TAB);
	//	SendOutput(SCI_ASSIGNCMDKEY, VK_TAB | (SCMOD_SHIFT << 16), SCI_BACKTAB);
	::DragAcceptFiles(wSciTE.GetID(), true);

	wToolBar = ::CreateWindowEx(
	               0,
	               TOOLBARCLASSNAME,
	               "",
	               WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
	               CCS_ADJUSTABLE |
	               TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
	               0, 0,
	               100, heightTools,
	               wSciTE.GetID(),
	               reinterpret_cast<HMENU>(IDM_TOOLWIN),
	               hInstance,
	               0);
	::SendMessage(wToolBar.GetID(), TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	::SendMessage(wToolBar.GetID(), TB_LOADIMAGES, IDB_STD_SMALL_COLOR,
	              reinterpret_cast<LPARAM>(HINST_COMMCTRL));

	TBBUTTON tbb[ELEMENTS(bbs)];
	for (unsigned int i = 0;i < ELEMENTS(bbs);i++) {
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

	::SendMessage(wToolBar.GetID(), TB_ADDBUTTONS, ELEMENTS(bbs), reinterpret_cast<LPARAM>(tbb));
	::SendMessage(wToolBar.GetID(), TB_AUTOSIZE, 0, 0);

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
	              wSciTE.GetID(),
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
	::SendMessage(wTabBar.GetID(),
	              WM_SETFONT,
	              (WPARAM) fontTabs,     // handle to font
	              (LPARAM) 0);    // redraw option

	wTabBar.Show();

	wStatusBar = ::CreateWindowEx(
	                 0,
	                 STATUSCLASSNAME,
	                 "",
	                 WS_CHILD | WS_CLIPSIBLINGS,
	                 0, 0,
	                 100, heightStatus,
	                 wSciTE.GetID(),
	                 reinterpret_cast<HMENU>(IDM_STATUSWIN),
	                 hInstance,
	                 0);
	wStatusBar.Show();
}

