// SciTE - Scintilla based Text Editor
/** @file SciTEWinBar.cxx
 ** Bar and menu code for the Windows version of the editor.
 **/
// Copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include "SciTEWin.h"

/**
 * Set up properties for FileTime, FileDate, CurrentTime, CurrentDate and FileAttr.
 */
void SciTEWin::SetFileProperties(
    PropSetFile &ps) {			///< Property set to update.

	const int TEMP_LEN = 100;
	char temp[TEMP_LEN];
	HANDLE hf = ::CreateFileW(filePath.AsInternal(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hf != INVALID_HANDLE_VALUE) {
		FILETIME ft = {};
		::GetFileTime(hf, NULL, NULL, &ft);
		::CloseHandle(hf);
		FILETIME lft = {};
		::FileTimeToLocalFileTime(&ft, &lft);
		SYSTEMTIME st = {};
		if (::FileTimeToSystemTime(&lft, &st) == 0)
			st = SYSTEMTIME();
		::GetTimeFormatA(LOCALE_USER_DEFAULT,
		                0, &st,
		                NULL, temp, TEMP_LEN);
		ps.Set("FileTime", temp);

		::GetDateFormatA(LOCALE_USER_DEFAULT,
		                DATE_SHORTDATE, &st,
		                NULL, temp, TEMP_LEN);
		ps.Set("FileDate", temp);

		DWORD attr = ::GetFileAttributesW(filePath.AsInternal());
		std::string fa;
		if (attr & FILE_ATTRIBUTE_READONLY) {
			fa += "R";
		}
		if (attr & FILE_ATTRIBUTE_HIDDEN) {
			fa += "H";
		}
		if (attr & FILE_ATTRIBUTE_SYSTEM) {
			fa += "S";
		}
		ps.Set("FileAttr", fa.c_str());
	} else {
		/* Reset values for new buffers with no file */
		ps.Set("FileTime", "");
		ps.Set("FileDate", "");
		ps.Set("FileAttr", "");
	}

	::GetDateFormatA(LOCALE_USER_DEFAULT,
	                DATE_SHORTDATE, NULL,     	// Current date
	                NULL, temp, TEMP_LEN);
	ps.Set("CurrentDate", temp);

	::GetTimeFormatA(LOCALE_USER_DEFAULT,
	                0, NULL,     	// Current time
	                NULL, temp, TEMP_LEN);
	ps.Set("CurrentTime", temp);
}

/**
 * Update the status bar text.
 */
void SciTEWin::SetStatusBarText(const char *s) {
	GUI::gui_string barText = GUI::StringFromUTF8(s);
	::SendMessage(HwndOf(wStatusBar),
	              SB_SETTEXT, 0, reinterpret_cast<LPARAM>(barText.c_str()));
}

void SciTEWin::TabInsert(int index, const GUI::gui_char *title) {
	TCITEMW tie;
	tie.mask = TCIF_TEXT | TCIF_IMAGE;
	tie.iImage = -1;
	GUI::gui_string titleCopy(title, title + wcslen(title) + 1);
	tie.pszText = &titleCopy[0];
	::SendMessage(HwndOf(wTabBar), TCM_INSERTITEMW, index, reinterpret_cast<LPARAM>(&tie));
}

void SciTEWin::TabSelect(int index) {
	::SendMessage(HwndOf(wTabBar), TCM_SETCURSEL, index, 0);
}

void SciTEWin::RemoveAllTabs() {
	::SendMessage(HwndOf(wTabBar), TCM_DELETEALLITEMS, 0, 0);
}

/**
 * Manage Windows specific notifications.
 */
void SciTEWin::Notify(const SCNotification *notification) {
	switch (notification->nmhdr.code) {
	case TCN_SELCHANGE:
		// Change of tab
		if (notification->nmhdr.idFrom == IDM_TABWIN) {
			int index = static_cast<int>(
				::SendMessage(HwndOf(wTabBar), TCM_GETCURSEL, 0, 0));
			SetDocumentAt(index);
			CheckReload();
		}
		break;

	case NM_RCLICK:
		// Right click on a control
		if (notification->nmhdr.idFrom == IDM_TABWIN) {

			GUI::Point ptCursor;
			::GetCursorPos(PointPointer(&ptCursor));
			GUI::Point ptClient = ptCursor;
			::ScreenToClient(HwndOf(wTabBar), PointPointer(&ptClient));
			TCHITTESTINFO info;
			info.pt.x = ptClient.x;
			info.pt.y = ptClient.y;

			int tabbarHitLast = TabCtrl_HitTest(HwndOf(wTabBar), &info);

			if (buffers.Current() != tabbarHitLast) {
				SetDocumentAt(tabbarHitLast);
				CheckReload();
			}

			// Pop up menu here:
			popup.CreatePopUp();
			AddToPopUp("Close", IDM_CLOSE, true);
			AddToPopUp("");
			AddToPopUp("Save", IDM_SAVE, true);
			AddToPopUp("Save As", IDM_SAVEAS, true);
			AddToPopUp("");

			bool bAddSeparator = false;
			for (int item = 0; item < toolMax; item++) {
				int itemID = IDM_TOOLS + item;
				std::string prefix = "command.name.";
				prefix += StdStringFromInteger(item);
				prefix += ".";
				std::string commandName = props.GetNewExpandString(prefix.c_str(), filePath.AsUTF8().c_str());
				if (commandName.length()) {
					AddToPopUp(commandName.c_str(), itemID, true);
					bAddSeparator = true;
				}
			}

			if (bAddSeparator)
				AddToPopUp("");

			AddToPopUp("Print", IDM_PRINT, true);
			popup.Show(ptCursor, wSciTE);
		}
		break;

	case NM_CLICK:
		// Click on a control
		if (notification->nmhdr.idFrom == IDM_STATUSWIN) {
			// Click on the status bar
			NMMOUSE *pNMMouse = (NMMOUSE *)notification;
			switch (pNMMouse->dwItemSpec) {
			case 0: 		/* Display of status */
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
			const GUI::gui_char *ttext = 0;
			NMTTDISPINFOW *pDispInfo = (NMTTDISPINFOW *)notification;
			// Toolbar tooltips
			switch (notification->nmhdr.idFrom) {
			case IDM_NEW:
				ttext = GUI_TEXT("New");
				break;
			case IDM_OPEN:
				ttext = GUI_TEXT("Open");
				break;
			case IDM_SAVE:
				ttext = GUI_TEXT("Save");
				break;
			case IDM_CLOSE:
				ttext = GUI_TEXT("Close");
				break;
			case IDM_PRINT:
				ttext = GUI_TEXT("Print");
				break;
			case IDM_CUT:
				ttext = GUI_TEXT("Cut");
				break;
			case IDM_COPY:
				ttext = GUI_TEXT("Copy");
				break;
			case IDM_PASTE:
				ttext = GUI_TEXT("Paste");
				break;
			case IDM_CLEAR:
				ttext = GUI_TEXT("Delete");
				break;
			case IDM_UNDO:
				ttext = GUI_TEXT("Undo");
				break;
			case IDM_REDO:
				ttext = GUI_TEXT("Redo");
				break;
			case IDM_FIND:
				ttext = GUI_TEXT("Find");
				break;
			case IDM_REPLACE:
				ttext = GUI_TEXT("Replace");
				break;
			case IDM_MACRORECORD:
				ttext = GUI_TEXT("Record Macro");
				break;
			case IDM_MACROSTOPRECORD:
				ttext = GUI_TEXT("Stop Recording");
				break;
			case IDM_MACROPLAY:
				ttext = GUI_TEXT("Run Macro");
				break;
			default: {
					// notification->nmhdr.idFrom appears to be the buffer number for tabbar tooltips
					GUI::Point ptCursor;
					::GetCursorPos(PointPointer(&ptCursor));
					GUI::Point ptClient = ptCursor;
					::ScreenToClient(HwndOf(wTabBar), PointPointer(&ptClient));
					TCHITTESTINFO info;
					info.pt.x = ptClient.x;
					info.pt.y = ptClient.y;
					int index = static_cast<int>(
						::SendMessage(HwndOf(wTabBar), TCM_HITTEST, (WPARAM)0, (LPARAM) & info));
					if (index >= 0) {
						GUI::gui_string path = buffers.buffers[index].AsInternal();
						// Handle '&' characters in path, since they are interpreted in
						// tooltips.
						size_t amp = 0;
						while ((amp = path.find(GUI_TEXT("&"), amp)) != GUI::gui_string::npos) {
							path.insert(amp, GUI_TEXT("&"));
							amp += 2;
						}
						StringCopy(tooltipText, path.c_str());
						pDispInfo->lpszText = tooltipText;
					}
				}
				break;
			}
			if (ttext) {
				GUI::gui_string localised = localiser.Text(GUI::UTF8FromString(ttext).c_str());
				StringCopy(tooltipText, localised.c_str());
				pDispInfo->lpszText = tooltipText;
			}
			break;
		}

	case SCN_CHARADDED:
		if ((notification->nmhdr.idFrom == IDM_RUNWIN) &&
		        jobQueue.IsExecuting() &&
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

	case SCN_FOCUSIN:
		if ((notification->nmhdr.idFrom == IDM_SRCWIN) ||
			(notification->nmhdr.idFrom == IDM_RUNWIN))
			wFocus = static_cast<HWND>(notification->nmhdr.hwndFrom);
		SciTEBase::Notify(notification);
		break;

	default:     	// Scintilla notification, use default treatment
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

void SciTEWin::ActivateWindow(const char *) {
	// This does nothing as, on Windows, you can no longer activate yourself
}

enum { tickerID = 100 };

void SciTEWin::TimerStart(int mask) {
	int maskNew = timerMask | mask;
	if (timerMask != maskNew) {
		if (timerMask == 0) {
			// Create a 1 second ticker
			::SetTimer(HwndOf(wSciTE), tickerID, 1000, NULL);
		}
		timerMask = maskNew;
	}
}

void SciTEWin::TimerEnd(int mask) {
	int maskNew = timerMask & ~mask;
	if (timerMask != maskNew) {
		if (maskNew == 0) {
			::KillTimer(HwndOf(wSciTE), tickerID);
		}
		timerMask = maskNew;
	}
}

void SciTEWin::ShowOutputOnMainThread() {
	::PostMessage(MainHWND(), SCITE_SHOWOUTPUT, 0, 0);
}

/**
 * Resize the content windows, embedding the editor and output windows.
 */
void SciTEWin::SizeContentWindows() {
	GUI::Rectangle rcInternal = wContent.GetClientPosition();

	int w = rcInternal.Width();
	int h = rcInternal.Height();
	heightOutput = NormaliseSplit(heightOutput);

	if (splitVertical) {
		wEditor.SetPosition(GUI::Rectangle(0, 0, w - heightOutput - heightBar, h));
		wOutput.SetPosition(GUI::Rectangle(w - heightOutput, 0, w, h));
	} else {
		wEditor.SetPosition(GUI::Rectangle(0, 0, w, h - heightOutput - heightBar));
		wOutput.SetPosition(GUI::Rectangle(0, h - heightOutput, w, h));
	}
	wContent.InvalidateAll();
}

/**
 * Resize the sub-windows, ie. the toolbar, tab bar, status bar. And call @a SizeContentWindows.
 */
void SciTEWin::SizeSubWindows() {
	GUI::Rectangle rcClient = wSciTE.GetClientPosition();
	bool showTab = false;

	//::SendMessage(MainHWND(), WM_SETREDRAW, false, 0); // suppress flashing
	visHeightTools = tbVisible ? (tbLarge ? heightToolsBig : heightTools) : 0;
	bands[bandTool].visible = tbVisible;

	if (tabVisible) {	// ? hide one tab only
		showTab = tabHideOne ?
		          ::SendMessage(HwndOf(wTabBar), TCM_GETITEMCOUNT, 0, 0) > 1 :
		          true;
	}

	bands[bandTab].visible = showTab;
	if (showTab && tabMultiLine) {
		wTabBar.SetPosition(GUI::Rectangle(
			rcClient.left, rcClient.top + visHeightTools,
			rcClient.right, rcClient.top + heightTab + visHeightTools));
	}

	RECT r = { rcClient.left, 0, rcClient.right, 0 };
	::SendMessage(HwndOf(wTabBar),
		TCM_ADJUSTRECT, TRUE, LPARAM(&r));
	bands[bandTab].height = r.bottom - r.top - 4;

	bands[bandBackground].visible = backgroundStrip.visible;
	bands[bandUser].height = userStrip.Height();
	bands[bandUser].visible = userStrip.visible;
	bands[bandSearch].visible = searchStrip.visible;
	bands[bandFind].visible = findStrip.visible;
	bands[bandReplace].visible = replaceStrip.visible;

	GUI::Rectangle rcSB = wStatusBar.GetPosition();
	bands[bandStatus].height = rcSB.Height() - 2;	// -2 hides a top border
	bands[bandStatus].visible = sbVisible;

	int heightContent = rcClient.Height();
	if (heightContent <= 0)
		heightContent = 1;

	for (size_t i=0;i<bands.size();i++) {
		if (bands[i].visible && !bands[i].expands)
			heightContent -= bands[i].height;
	}
	if (heightContent <= 0) {
		heightContent = rcClient.Height();
		for (size_t i=0;i<bands.size();i++) {
			if (i != bandContents)
				bands[i].visible = false;
		}
	}
	bands[bandContents].height = heightContent;

	// May need to copy some values out to other variables

	HDWP hdwp = BeginDeferWindowPos(10);

	int yPos = rcClient.top;
	for (size_t b=0; b<bands.size(); b++) {
		if (bands[b].visible) {
			GUI::Rectangle rcToSet(rcClient.left, yPos, rcClient.right, yPos + bands[b].height);
			if (hdwp)
				hdwp = ::DeferWindowPos(hdwp, HwndOf(bands[b].win),
				0, rcToSet.left, rcToSet.top, rcToSet.Width(), rcToSet.Height(),
				SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
			//bands[b].win.Show(true);
			yPos += bands[b].height;
		} else {
			GUI::Rectangle rcToSet(rcClient.left, rcClient.top - 41, rcClient.Width(), rcClient.top - 40);
			if (hdwp)
				hdwp = ::DeferWindowPos(hdwp, HwndOf(bands[b].win),
				0, rcToSet.left, rcToSet.top, rcToSet.Width(), rcToSet.Height(),
				SWP_NOZORDER|SWP_NOACTIVATE|SWP_HIDEWINDOW);
			//bands[b].win.Show(false);
			//bands[b].win.SetPosition();
		}
	}
	if (hdwp)
		::EndDeferWindowPos(hdwp);

	visHeightTools = bands[bandTool].height;
	visHeightTab = bands[bandTab].height;
	visHeightEditor = bands[bandContents].height;
	visHeightStatus = bands[bandStatus].height;

	SizeContentWindows();
	//::SendMessage(MainHWND(), WM_SETREDRAW, true, 0);
	//::RedrawWindow(MainHWND(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}



// Keymod param is interpreted using the same notation (and much the same
// code) as KeyMatch uses in SciTEWin.cxx.



void SciTEWin::SetMenuItem(int menuNumber, int position, int itemID,
                           const GUI::gui_char *text, const GUI::gui_char *mnemonic) {
	// On Windows the menu items are modified if they already exist or are created
	HMENU hmenu = ::GetSubMenu(::GetMenu(MainHWND()), menuNumber);
	GUI::gui_string sTextMnemonic = text;
	long keycode = 0;
	if (mnemonic && *mnemonic) {
		keycode = SciTEKeys::ParseKeyCode(GUI::UTF8FromString(mnemonic).c_str());
		if (keycode) {
			sTextMnemonic += GUI_TEXT("\t");
			sTextMnemonic += mnemonic;
		}
		// the keycode could be used to make a custom accelerator table
		// but for now, the menu's item data is used instead for command
		// tools, and for other menu entries it is just discarded.
	}

	UINT typeFlags = (text[0]) ? MF_STRING : MF_SEPARATOR;
	if (::GetMenuState(hmenu, itemID, MF_BYCOMMAND) == (UINT)(-1)) {
		// Not present so insert
		::InsertMenuW(hmenu, position, MF_BYPOSITION | typeFlags, itemID, sTextMnemonic.c_str());
	} else {
		::ModifyMenuW(hmenu, itemID, MF_BYCOMMAND | typeFlags, itemID, sTextMnemonic.c_str());
	}

	if (itemID >= IDM_TOOLS && itemID < IDM_TOOLS + toolMax) {
		// Stow the keycode for later retrieval.
		// Do this even if 0, in case the menu already existed (e.g. ModifyMenu)
		MENUITEMINFO mii;
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_DATA;
		mii.dwItemData = keycode;
		::SetMenuItemInfo(hmenu, itemID, FALSE, &mii);
	}
}

void SciTEWin::RedrawMenu() {
	// Make previous change visible.
	::DrawMenuBar(HwndOf(wSciTE));
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
		CheckMenuItem(::GetMenu(MainHWND()), wIDCheckItem, MF_CHECKED | MF_BYCOMMAND);
	else
		CheckMenuItem(::GetMenu(MainHWND()), wIDCheckItem, MF_UNCHECKED | MF_BYCOMMAND);
}

void EnableButton(HWND wTools, int id, bool enable) {
	if (wTools) {
		::SendMessage(wTools, TB_ENABLEBUTTON, id,
	              LongFromTwoShorts(static_cast<short>(enable ? TRUE : FALSE), 0));
	}
}

void SciTEWin::EnableAMenuItem(int wIDCheckItem, bool val) {
	if (val)
		::EnableMenuItem(::GetMenu(MainHWND()), wIDCheckItem, MF_ENABLED | MF_BYCOMMAND);
	else
		::EnableMenuItem(::GetMenu(MainHWND()), wIDCheckItem, MF_DISABLED | MF_GRAYED | MF_BYCOMMAND);
	::EnableButton(HwndOf(wToolBar), wIDCheckItem, val);
}

void SciTEWin::CheckMenus() {
	SciTEBase::CheckMenus();
	::CheckMenuRadioItem(::GetMenu(MainHWND()), IDM_EOL_CRLF, IDM_EOL_LF,
	                   wEditor.Call(SCI_GETEOLMODE) - SC_EOL_CRLF + IDM_EOL_CRLF, 0);
	::CheckMenuRadioItem(::GetMenu(MainHWND()), IDM_ENCODING_DEFAULT, IDM_ENCODING_UCOOKIE,
	                   CurrentBuffer()->unicodeMode + IDM_ENCODING_DEFAULT, 0);
}

void SciTEWin::LocaliseMenu(HMENU hmenu) {
	for (int i = 0; i <= ::GetMenuItemCount(hmenu); i++) {
		GUI::gui_char buff[200];
		buff[0] = '\0';
		MENUITEMINFOW mii = {};
		mii.cbSize = sizeof(mii);
		mii.fMask = MIIM_CHECKMARKS | MIIM_DATA | MIIM_ID |
		            MIIM_STATE | MIIM_SUBMENU | MIIM_TYPE;
		mii.dwTypeData = buff;
		mii.cch = sizeof(buff) - 1;
		if (::GetMenuItemInfoW(hmenu, i, TRUE, &mii)) {
			if (mii.hSubMenu) {
				LocaliseMenu(mii.hSubMenu);
			}
			if (mii.fType == MFT_STRING || mii.fType == MFT_RADIOCHECK) {
				if (mii.dwTypeData) {
					GUI::gui_string text(mii.dwTypeData);
					GUI::gui_string accel(mii.dwTypeData);
					size_t len = text.length();
					size_t tab = text.find(GUI_TEXT("\t"));
					if (tab != GUI::gui_string::npos) {
						text.erase(tab, len - tab);
						accel.erase(0, tab + 1);
					} else {
						accel = GUI_TEXT("");
					}
					text = localiser.Text(GUI::UTF8FromString(text.c_str()).c_str(), true);
					if (text.length()) {
						if (accel != GUI_TEXT("")) {
							text += GUI_TEXT("\t");
							text += accel;
						}
						text.append(1, 0);
						mii.dwTypeData = &text[0];
						::SetMenuItemInfoW(hmenu, i, TRUE, &mii);
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
	if (::GetWindowTextA(w, wtext, sizeof(wtext))) {
		GUI::gui_string text = localiser.Text(wtext, false);
		if (text.length())
			::SetWindowTextW(w, text.c_str());
	}
}

void SciTEWin::LocaliseDialog(HWND wDialog) {
	LocaliseControl(wDialog);
	HWND wChild = ::GetWindow(wDialog, GW_CHILD);
	while (wChild) {
		LocaliseControl(wChild);
		wChild = ::GetWindow(wChild, GW_HWNDNEXT);
	}
}

// Mingw headers do not have this:
#ifndef TBSTYLE_FLAT
#define TBSTYLE_FLAT 0x0800
#endif
#ifndef TB_LOADIMAGES
#define TB_LOADIMAGES (WM_USER + 50)
#endif

struct BarButton {
	int id;
	int cmd;
};

static BarButton bbs[] = {
    { -1,           0 },
    { STD_FILENEW,  IDM_NEW },
    { STD_FILEOPEN, IDM_OPEN },
    { STD_FILESAVE, IDM_SAVE },
    { 0,            IDM_CLOSE },
    { -1,           0 },
    { STD_PRINT,    IDM_PRINT },
    { -1,           0 },
    { STD_CUT,      IDM_CUT },
    { STD_COPY,     IDM_COPY },
    { STD_PASTE,    IDM_PASTE },
    { STD_DELETE,   IDM_CLEAR },
    { -1,           0 },
    { STD_UNDO,     IDM_UNDO },
    { STD_REDOW,    IDM_REDO },
    { -1,           0 },
    { STD_FIND,     IDM_FIND },
    { STD_REPLACE,  IDM_REPLACE },
};

static WNDPROC stDefaultTabProc = NULL;
static LRESULT PASCAL TabWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {

	static BOOL st_bDragBegin = FALSE;
	static int st_iDraggingTab = -1;
	static int st_iLastClickTab = -1;
	static HWND st_hwndLastFocus = NULL;

	switch (iMessage) {

	case WM_LBUTTONDOWN: {
			GUI::Point pt = PointFromLong(lParam);
			TCHITTESTINFO thti;
			thti.pt.x = pt.x;
			thti.pt.y = pt.y;
			thti.flags = 0;
			st_iLastClickTab = static_cast<int>(::SendMessage(hWnd, TCM_HITTEST, (WPARAM)0, (LPARAM) & thti));
		}
		break;
	}

	LRESULT retResult;
	if (stDefaultTabProc != NULL) {
		retResult = CallWindowProc(stDefaultTabProc, hWnd, iMessage, wParam, lParam);
	} else {
		retResult = ::DefWindowProc(hWnd, iMessage, wParam, lParam);
	}

	switch (iMessage) {

	case WM_MBUTTONDOWN: {
			// Check if on tab bar
			GUI::Point pt = PointFromLong(lParam);
			TCHITTESTINFO thti;
			thti.pt.x = pt.x;
			thti.pt.y = pt.y;
			thti.flags = 0;
			int tab = static_cast<int>(::SendMessage(hWnd, TCM_HITTEST, (WPARAM)0, (LPARAM) & thti));
			if (tab >= 0) {
				::SendMessage(::GetParent(hWnd), WM_COMMAND, IDC_TABCLOSE, (LPARAM)tab);
			}
		}
		break;

	case WM_LBUTTONUP: {
			st_iLastClickTab = -1;
			if (st_bDragBegin == TRUE) {
				if (st_hwndLastFocus != NULL) ::SetFocus(st_hwndLastFocus);
				::ReleaseCapture();
				::SetCursor(::LoadCursor(NULL, IDC_ARROW));
				st_bDragBegin = FALSE;
				GUI::Point pt = PointFromLong(lParam);
				TCHITTESTINFO thti;
				thti.pt.x = pt.x;
				thti.pt.y = pt.y;
				thti.flags = 0;
				int tab = static_cast<int>(::SendMessage(hWnd, TCM_HITTEST, (WPARAM)0, (LPARAM) & thti));
				if (tab > -1 && st_iDraggingTab > -1 && st_iDraggingTab != tab) {
					::SendMessage(::GetParent(hWnd),
					        WM_COMMAND,
					        IDC_SHIFTTAB,
					        MAKELPARAM(st_iDraggingTab, tab));
				}
				st_iDraggingTab = -1;
			}
		}
		break;

	case WM_KEYDOWN: {
			if (wParam == VK_ESCAPE) {
				if (st_bDragBegin == TRUE) {
					if (st_hwndLastFocus != NULL) ::SetFocus(st_hwndLastFocus);
					::ReleaseCapture();
					::SetCursor(::LoadCursor(NULL, IDC_ARROW));
					st_bDragBegin = FALSE;
					st_iDraggingTab = -1;
					st_iLastClickTab = -1;
					::InvalidateRect(hWnd, NULL, FALSE);
				}
			}
		}
		break;

	case WM_MOUSEMOVE: {

			GUI::Point pt = PointFromLong(lParam);
			TCHITTESTINFO thti;
			thti.pt.x = pt.x;
			thti.pt.y = pt.y;
			thti.flags = 0;
			int tab = static_cast<int>(::SendMessage(hWnd, TCM_HITTEST, (WPARAM)0, (LPARAM) & thti));
			int tabcount = static_cast<int>(::SendMessage(hWnd, TCM_GETITEMCOUNT, (WPARAM)0, (LPARAM)0));

			if (wParam == MK_LBUTTON &&
			        tabcount > 1 &&
			        tab > -1 &&
			        st_iLastClickTab == tab &&
			        st_bDragBegin == FALSE) {
				st_iDraggingTab = tab;
				::SetCapture(hWnd);
				st_hwndLastFocus = ::SetFocus(hWnd);
				st_bDragBegin = TRUE;
				HCURSOR hcursor = ::LoadCursor(::GetModuleHandle(NULL),
				        MAKEINTRESOURCE(IDC_DRAGDROP));
				if (hcursor) ::SetCursor(hcursor);
			} else {
				if (st_bDragBegin == TRUE) {
					if (tab > -1 && st_iDraggingTab > -1 /*&& st_iDraggingTab != tab*/) {
						HCURSOR hcursor = ::LoadCursor(::GetModuleHandle(NULL),
						        MAKEINTRESOURCE(IDC_DRAGDROP));
						if (hcursor) ::SetCursor(hcursor);
					} else {
						::SetCursor(::LoadCursor(NULL, IDC_NO));
					}
				}
			}
		}
		break;

	case WM_PAINT: {
			if (st_bDragBegin == TRUE && st_iDraggingTab != -1) {

				GUI::Point ptCursor;
				::GetCursorPos(PointPointer(&ptCursor));
				GUI::Point ptClient = ptCursor;
				::ScreenToClient(hWnd, PointPointer(&ptClient));
				TCHITTESTINFO thti;
				thti.pt.x = ptClient.x;
				thti.pt.y = ptClient.y;
				thti.flags = 0;
				int tab = static_cast<int>(::SendMessage(hWnd, TCM_HITTEST, (WPARAM)0, (LPARAM) & thti));

				RECT tabrc;
				if (tab != -1 &&
				        tab != st_iDraggingTab &&
				        TabCtrl_GetItemRect(hWnd, tab, &tabrc)) {

					HDC hDC = ::GetDC(hWnd);
					if (hDC) {

						int xLeft = tabrc.left + 8;
						int yLeft = tabrc.top + (tabrc.bottom - tabrc.top) / 2;
						POINT ptsLeftArrow[] = {
							{xLeft, yLeft - 2},
							{xLeft - 2, yLeft - 2},
							{xLeft - 2, yLeft - 5},
							{xLeft - 7, yLeft},
							{xLeft - 2, yLeft + 5},
							{xLeft - 2, yLeft + 2},
							{xLeft, yLeft + 2}
						};

						int xRight = tabrc.right - 10;
						int yRight = tabrc.top + (tabrc.bottom - tabrc.top) / 2;
						POINT ptsRightArrow[] = {
							{xRight, yRight - 2},
							{xRight + 2, yRight - 2},
							{xRight + 2, yRight - 5},
							{xRight + 7, yRight},
							{xRight + 2, yRight + 5},
							{xRight + 2, yRight + 2},
							{xRight, yRight + 2}
						};

						HPEN pen = ::CreatePen(0,1,RGB(255, 0, 0));
						HPEN penOld = static_cast<HPEN>(::SelectObject(hDC, pen));
						COLORREF colourNearest = ::GetNearestColor(hDC, RGB(255, 0, 0));
						HBRUSH brush = ::CreateSolidBrush(colourNearest);
						HBRUSH brushOld = static_cast<HBRUSH>(::SelectObject(hDC, brush));
						::Polygon(hDC, tab < st_iDraggingTab ? ptsLeftArrow : ptsRightArrow, 7);
						::SelectObject(hDC, brushOld);
						::DeleteObject(brush);
						::SelectObject(hDC, penOld);
						::DeleteObject(pen);
						::ReleaseDC(hWnd, hDC);
					}
				}
			}
		}
		break;
	}

	return retResult;
}

/**
 * Create all the needed windows.
 */
void SciTEWin::Creation() {

	wContent = ::CreateWindowEx(
	               WS_EX_CLIENTEDGE,
	               classNameInternal,
	               TEXT("Source"),
	               WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	               0, 0,
	               100, 100,
	               MainHWND(),
	               HmenuID(2000),
	               hInstance,
	               &contents);
	wContent.Show();

	wEditor.SetID(::CreateWindowEx(
	              0,
	              TEXT("Scintilla"),
	              TEXT("Source"),
	              WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	              0, 0,
	              100, 100,
	              HwndOf(wContent),
	              HmenuID(IDM_SRCWIN),
	              hInstance,
	              0));
	if (!wEditor.CanCall())
		exit(FALSE);
	wEditor.Show();
	wEditor.Call(SCI_USEPOPUP, 0);
	WindowSetFocus(wEditor);

	wOutput.SetID(::CreateWindowEx(
	              0,
	              TEXT("Scintilla"),
	              TEXT("Run"),
	              WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	              0, 0,
	              100, 100,
	              HwndOf(wContent),
	              HmenuID(IDM_RUNWIN),
	              hInstance,
	              0));
	if (!wOutput.CanCall())
		exit(FALSE);
	wOutput.Show();
	// No selection margin on output window
	wOutput.Call(SCI_SETMARGINWIDTHN, 1, 0);
	//wOutput.Call(SCI_SETCARETPERIOD, 0);
	wOutput.Call(SCI_USEPOPUP, 0);
	::DragAcceptFiles(MainHWND(), true);

	HWND hwndToolBar = ::CreateWindowEx(
	               0,
	               TOOLBARCLASSNAME,
	               TEXT(""),
	               WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
	               TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_NORESIZE,
	               0, 0,
	               100, tbLarge ? heightToolsBig : heightTools,
	               MainHWND(),
	               HmenuID(IDM_TOOLWIN),
	               hInstance,
	               0);
	wToolBar = hwndToolBar;

	::SendMessage(hwndToolBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	::SendMessage(hwndToolBar, TB_SETBITMAPSIZE, 0, tbLarge ? MAKELPARAM(24, 24) : MAKELPARAM(16, 16));
	::SendMessage(hwndToolBar, TB_LOADIMAGES, 
	              tbLarge ? IDB_STD_LARGE_COLOR : IDB_STD_SMALL_COLOR,
	              reinterpret_cast<LPARAM>(HINST_COMMCTRL));

	TBADDBITMAP addbmp = { hInstance, IDR_CLOSEFILE };

	if (tbLarge) {
		addbmp.nID = IDR_CLOSEFILE24;
	}
	
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

	WNDCLASS wndClass = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	if (::GetClassInfo(NULL, WC_TABCONTROL, &wndClass) == 0)
		exit(FALSE);
	stDefaultTabProc = wndClass.lpfnWndProc;
	wndClass.lpfnWndProc = TabWndProc;
	wndClass.style = wndClass.style | CS_DBLCLKS;
	wndClass.lpszClassName = TEXT("SciTeTabCtrl");
	wndClass.hInstance = hInstance;
	if (RegisterClass(&wndClass) == 0)
		exit(FALSE);

	wTabBar = ::CreateWindowEx(
	              0,
	              TEXT("SciTeTabCtrl"),
	              TEXT("Tab"),
	              WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
	              TCS_FOCUSNEVER | TCS_TOOLTIPS,
	              0, 0,
	              100, heightTab,
	              MainHWND(),
	              HmenuID(IDM_TABWIN),
	              hInstance,
	              0);

	if (!wTabBar.Created())
		exit(FALSE);

	LOGFONT lfIconTitle;
	ZeroMemory(&lfIconTitle, sizeof(lfIconTitle));
	if (::SystemParametersInfo(SPI_GETICONTITLELOGFONT,sizeof(lfIconTitle),&lfIconTitle,FALSE) == 0)
		exit(FALSE);
	fontTabs = ::CreateFontIndirect(&lfIconTitle);
	SetWindowFont(HwndOf(wTabBar), fontTabs, 0);

	wTabBar.Show();

	::CreateWindowEx(
	               0,
	               classNameInternal,
	               TEXT("userStrip"),
	               WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	               0, 0,
	               100, 100,
	               MainHWND(),
	               HmenuID(2001),
	               hInstance,
	               &userStrip);

	::CreateWindowEx(
	               0,
	               classNameInternal,
	               TEXT("backgroundStrip"),
	               WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	               0, 0,
	               100, 100,
	               MainHWND(),
	               HmenuID(2001),
	               hInstance,
	               &backgroundStrip);

	::CreateWindowEx(
	               0,
	               classNameInternal,
	               TEXT("searchStrip"),
	               WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	               0, 0,
	               100, 100,
	               MainHWND(),
	               HmenuID(2001),
	               hInstance,
	               &searchStrip);

	::CreateWindowEx(
	               0,
	               classNameInternal,
	               TEXT("FindStrip"),
	               WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	               0, 0,
	               100, 100,
	               MainHWND(),
	               HmenuID(2002),
	               hInstance,
	               &findStrip);

	::CreateWindowEx(
	               0,
	               classNameInternal,
	               TEXT("ReplaceStrip"),
	               WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	               0, 0,
	               100, 100,
	               MainHWND(),
	               HmenuID(2003),
	               hInstance,
	               &replaceStrip);

	wStatusBar = ::CreateWindowEx(
	                 0,
	                 STATUSCLASSNAME,
	                 TEXT(""),
	                 WS_CHILD | WS_CLIPSIBLINGS,
	                 0, 0,
	                 100, heightStatus,
	                 MainHWND(),
	                 HmenuID(IDM_STATUSWIN),
	                 hInstance,
	                 0);
	wStatusBar.Show();
	int widths[] = { 4000 };
	// Perhaps we can define a syntax to create more parts,
	// but it is probably an overkill for a marginal feature
	::SendMessage(HwndOf(wStatusBar),
	              SB_SETPARTS, 1,
	              reinterpret_cast<LPARAM>(widths));

	bands.push_back(Band(true, tbLarge ? heightToolsBig : heightTools, false, wToolBar));
	bands.push_back(Band(true, heightTab, false, wTabBar));
	bands.push_back(Band(true, 100, true, wContent));
	bands.push_back(Band(true, userStrip.Height(), false, userStrip));
	bands.push_back(Band(true, backgroundStrip.Height(), false, backgroundStrip));
	bands.push_back(Band(true, searchStrip.Height(), false, searchStrip));
	bands.push_back(Band(true, findStrip.Height(), false, findStrip));
	bands.push_back(Band(true, replaceStrip.Height(), false, replaceStrip));
	bands.push_back(Band(true, heightStatus, false, wStatusBar));

#ifndef NO_LUA
		if (props.GetExpandedString("ext.lua.startup.script").length() == 0)
			DestroyMenuItem(menuOptions,IDM_OPENLUAEXTERNALFILE);
#else
		DestroyMenuItem(menuOptions,IDM_OPENLUAEXTERNALFILE);
#endif
}
