// SciTE - Scintilla based Text Editor
/** @file Strips.cxx
 ** Implementation of UI strips.
 **/
// Copyright 2013 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include "SciTEWin.h"

void *PointerFromWindow(HWND hWnd) {
	return reinterpret_cast<void *>(::GetWindowLongPtr(hWnd, 0));
}

void SetWindowPointer(HWND hWnd, void *ptr) {
	::SetWindowLongPtr(hWnd, 0, reinterpret_cast<LONG_PTR>(ptr));
}

GUI::gui_string TextOfWindow(HWND hWnd) {
	const int len = ::GetWindowTextLengthW(hWnd);
	std::vector<GUI::gui_char> itemText(len+1);
	GUI::gui_string gsText;
	if (::GetWindowTextW(hWnd, &itemText[0], len+1)) {
		gsText = GUI::gui_string(&itemText[0], len);
	}
	return gsText;
}

GUI::gui_string ClassNameOfWindow(HWND hWnd) {
	// In the documentation of WNDCLASS:
	// "The maximum length for lpszClassName is 256."
	const size_t maxClassNameLength = 256+1;	// +1 for NUL
	GUI::gui_char className[maxClassNameLength];
	if (::GetClassNameW(hWnd, className, maxClassNameLength))
		return GUI::gui_string(className);
	else
		return GUI::gui_string();
}

static void SetFontHandle(GUI::Window &w, HFONT hfont) {
	SetWindowFont(HwndOf(w), hfont, 0);
}

static void CheckButton(GUI::Window &wButton, bool checked) {
	::SendMessage(HwndOf(wButton),
		BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

static int WidthText(HFONT hfont, const GUI::gui_char *text) {
	HDC hdcMeasure = ::CreateCompatibleDC(NULL);
	HFONT hfontOriginal = static_cast<HFONT>(::SelectObject(hdcMeasure, hfont));
	RECT rcText = {0,0, 2000, 2000};
	::DrawText(hdcMeasure, text, -1, &rcText, DT_CALCRECT);
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
	return TextOfWindow(HwndOf(w));
}

static std::string ControlText(GUI::Window w) {
	const GUI::gui_string gsText = ControlGText(w);
	return GUI::UTF8FromString(gsText);
}

static std::string ComboSelectionText(GUI::Window w) {
	GUI::gui_string gsText;
	HWND wT = HwndOf(w);
	const LRESULT selection = ::SendMessageW(wT, CB_GETCURSEL, 0, 0);
	if (selection != CB_ERR) {
		const LRESULT len = ::SendMessageW(wT, CB_GETLBTEXTLEN, selection, 0) + 1;
		std::vector<GUI::gui_char> itemText(len);
		const LRESULT lenActual = ComboBox_GetLBText(wT, selection, &itemText[0]);
		if (lenActual != CB_ERR) {
			gsText = GUI::gui_string(&itemText[0], lenActual);
		}
	}
	return GUI::UTF8FromString(gsText.c_str());
}

static void SetComboFromMemory(GUI::Window w, const ComboMemory &mem) {
	HWND combo = HwndOf(w);
	::SendMessage(combo, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < mem.Length(); i++) {
		GUI::gui_string gs = GUI::StringFromUTF8(mem.At(i));
		ComboBox_AddString(HwndOf(w), gs.c_str());
	}
}

LRESULT PASCAL BaseWin::StWndProc(
    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	// Find C++ object associated with window.
	BaseWin *base = static_cast<BaseWin *>(::PointerFromWindow(hWnd));
	// scite will be zero if WM_CREATE not seen yet
	if (base == 0) {
		if (iMessage == WM_CREATE) {
			LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			base = static_cast<BaseWin *>(cs->lpCreateParams);
			SetWindowPointer(hWnd, base);
			base->SetID(hWnd);
			return base->WndProc(iMessage, wParam, lParam);
		} else
			return ::DefWindowProc(hWnd, iMessage, wParam, lParam);
	} else
		return base->WndProc(iMessage, wParam, lParam);
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
		Hwnd(), HmenuID(0), ::GetModuleHandle(NULL), 0));
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

GUI::Window Strip::CreateButton(const char *text, size_t ident, bool check) {
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

	int bmpDimension = 16;
	int resDifference = 0;
	if (scale >= 192) {
		bmpDimension = 32;
		resDifference = 300;
	} else if (scale >= 144) {
		bmpDimension = 24;
		resDifference = 200;
	} else if (scale >= 120) {
		bmpDimension = 20;
		resDifference = 100;
	}

	if (check) {
		height = bmpDimension + 3 * 2;
		width = bmpDimension + 3 * 2;
	}
	GUI::Window w;
	w.SetID(::CreateWindowEx(0, TEXT("Button"), localised.c_str(),
		WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS |
		(check ? (BS_AUTOCHECKBOX | BS_PUSHLIKE | BS_BITMAP) : BS_PUSHBUTTON),
		2, 2, width, height,
		Hwnd(), HmenuID(ident), ::GetModuleHandle(NULL), 0));
	if (check) {
		int resNum = IDBM_WORD;
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
			::GetModuleHandle(NULL), MAKEINTRESOURCE(resNum + resDifference), IMAGE_BITMAP,
			bmpDimension, bmpDimension, flags));

		::SendMessage(HwndOf(w),
			BM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(bm));
	}
	SetFontHandle(w, fontText);
	w.Show();
#ifdef BCM_GETIDEALSIZE
	if (!check) {
		// Push buttons can be measured with BCM_GETIDEALSIZE
		SIZE sz = {0, 0};
		::SendMessage(HwndOf(w),
			BCM_GETIDEALSIZE, 0, reinterpret_cast<LPARAM>(&sz));
		if (sz.cx > 0) {
			GUI::Rectangle rc(0,0, sz.cx + 2 * WidthText(fontText, TEXT(" ")), sz.cy);
			w.SetPosition(rc);
		}
	}
#endif
	TOOLINFO toolInfo = TOOLINFO();
	toolInfo.cbSize = sizeof(toolInfo);
	toolInfo.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
	toolInfo.hinst = ::GetModuleHandle(NULL);
	toolInfo.hwnd = Hwnd();
	toolInfo.uId = (UINT_PTR)w.GetID();
	toolInfo.lpszText = LPSTR_TEXTCALLBACK;
	::GetClientRect(Hwnd(), &toolInfo.rect);
	::SendMessageW(HwndOf(wToolTip), TTM_ADDTOOLW,
		0, (LPARAM) &toolInfo);
	::SendMessage(HwndOf(wToolTip), TTM_ACTIVATE, TRUE, 0);
	return w;
}

void Strip::Tab(bool forwards) {
	HWND wToFocus = ::GetNextDlgTabItem(Hwnd(), ::GetFocus(), !forwards);
	if (wToFocus)
		::SetFocus(wToFocus);
}

void Strip::Creation() {
	// Use the message box font for the strip's font
	NONCLIENTMETRICS ncm = NONCLIENTMETRICS();
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
				const GUI::gui_string className = ClassNameOfWindow(wChild);
				if ((className == TEXT("Button")) || (className == TEXT("Static"))) {
					GUI::gui_string caption = TextOfWindow(wChild);
					for (int i=0; caption[i]; i++) {
						if ((caption[i] == L'&') && (toupper(caption[i+1]) == static_cast<int>(key))) {
							if (className == TEXT("Button")) {
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
		LPRECT prcClose = reinterpret_cast<RECT *>(&rcClose);
		if (hTheme) {
#ifdef THEME_AVAILABLE
			int closeAppearence = CBS_NORMAL;
			if (closeState == csOver) {
				closeAppearence = CBS_HOT;
			} else if (closeState == csClickedOver) {
				closeAppearence = CBS_PUSHED;
			}
			::DrawThemeBackground(hTheme, hDC, WP_SMALLCLOSEBUTTON, closeAppearence,
				prcClose, NULL);
#endif
		} else {
			int closeAppearence = 0;
			if (closeState == csOver) {
				closeAppearence = DFCS_HOT;
			} else if (closeState == csClickedOver) {
				closeAppearence = DFCS_PUSHED;
			}

			DrawFrameControl(hDC, prcClose, DFC_CAPTION,
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
		rcClose.right -= space;
		rcClose.left = rcClose.right - closeSize.cx;
		rcClose.top += space;
		rcClose.bottom = rcClose.top + closeSize.cy;
		return rcClose;
	} else {
		return GUI::Rectangle(-1,-1,-1,-1);
	}
}

GUI::Rectangle Strip::LineArea(int line) {
	GUI::Rectangle rcLine = GetPosition();

	rcLine.bottom -= rcLine.top;
	rcLine.right -= rcLine.left;

	rcLine.left = space;
	rcLine.top = space + line * lineHeight;
	rcLine.right -= space;
	rcLine.bottom = rcLine.top + lineHeight - space;

	if (HasClose())
		rcLine.right -= closeSize.cx + space;	// Allow for close box and gap

	return rcLine;
}

int Strip::Lines() const {
	return 1;
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
	scale = 96;
	hTheme = ::OpenThemeData(Hwnd(), TEXT("Window"));
	if (hTheme) {
		HDC hdc = ::GetDC(Hwnd());
		scale = ::GetDeviceCaps(hdc, LOGPIXELSX);
		::ReleaseDC(Hwnd(), hdc);
		space = scale * 2 / 96;
		HRESULT hr = ::GetThemePartSize(hTheme, NULL, WP_SMALLCLOSEBUTTON, CBS_NORMAL,
			NULL, TS_TRUE, &closeSize);
		//HRESULT hr = ::GetThemePartSize(hTheme, NULL, WP_MDICLOSEBUTTON, CBS_NORMAL,
		//	NULL, TS_TRUE, &closeSize);
		if (!SUCCEEDED(hr)) {
			closeSize.cx = 11;
			closeSize.cy = 11;
		}
		closeSize.cx = closeSize.cx * scale / 96;
		closeSize.cy = closeSize.cy * scale / 96;
	}
#endif
}

static bool HideKeyboardCues() {
	BOOL b=FALSE;
	if (::SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &b, 0) == 0)
		return FALSE;
	return !b;
}

LRESULT Strip::EditColour(HWND hwnd, HDC hdc) {
	return ::DefWindowProc(Hwnd(), WM_CTLCOLOREDIT, WPARAM(hdc), LPARAM(hwnd));
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
		HRESULT hr = ::GetThemeBackgroundContentRect(hThemeButton, pcd->hdc, TP_BUTTON,
			buttonAppearence, &pcd->rc, &rcButton);
		if (!SUCCEEDED(hr)) {
			return CDRF_DODEFAULT;
		}

		HBITMAP hBitmap = reinterpret_cast<HBITMAP>(::SendMessage(
			pnmh->hwndFrom, BM_GETIMAGE, IMAGE_BITMAP, 0));

		// Retrieve the bitmap dimensions
		BITMAPINFO rbmi = BITMAPINFO();
		rbmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
		::GetDIBits(pcd->hdc, hBitmap, 0, 0, NULL, &rbmi, DIB_RGB_COLORS);

		DWORD colourTransparent = RGB(0xC0,0xC0,0xC0);

		// Offset from button edge to contents.
		int xOffset = ((rcButton.right - rcButton.left) - rbmi.bmiHeader.biWidth) / 2 + 1;
		int yOffset = ((rcButton.bottom - rcButton.top) - rbmi.bmiHeader.biHeight) / 2;

		HDC hdcBM = ::CreateCompatibleDC(NULL);
		HBITMAP hbmOriginal = static_cast<HBITMAP>(::SelectObject(hdcBM, hBitmap));
		::TransparentBlt(pcd->hdc, xOffset, yOffset,
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

	case WM_CONTEXTMENU:
		ShowPopup();
		return 0;

	case WM_CTLCOLOREDIT:
		return EditColour(reinterpret_cast<HWND>(lParam), reinterpret_cast<HDC>(wParam));

	case WM_NOTIFY: {
			NMHDR *pnmh = reinterpret_cast<LPNMHDR>(lParam);
			if (pnmh->code == static_cast<unsigned int>(NM_CUSTOMDRAW)) {
				return CustomDraw(pnmh);
			} else if (pnmh->code == static_cast<unsigned int>(TTN_GETDISPINFO)) {
				NMTTDISPINFOW *pnmtdi = reinterpret_cast<LPNMTTDISPINFO>(lParam);
				int idButton = static_cast<int>(
					(pnmtdi->uFlags & TTF_IDISHWND) ?
					::GetDlgCtrlID(reinterpret_cast<HWND>(pnmtdi->hdr.idFrom)) : pnmtdi->hdr.idFrom);
				for (size_t i=0; toggles[i].label; i++) {
					if (toggles[i].id == idButton) {
						GUI::gui_string localised = localiser->Text(toggles[i].label);
						StringCopy(pnmtdi->szText, localised.c_str());
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

void Strip::ShowPopup() {
}

void BackgroundStrip::Creation() {
	Strip::Creation();

	wExplanation = ::CreateWindowEx(0, TEXT("Static"), TEXT(""),
		WS_CHILD | WS_CLIPSIBLINGS,
		2, 2, 100, 21,
		Hwnd(), HmenuID(0), ::GetModuleHandle(NULL), 0);
	wExplanation.Show();
	SetFontHandle(wExplanation, fontText);

	wProgress = ::CreateWindowEx(0, PROGRESS_CLASS, TEXT(""),
		WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
		2, 2, 100, 21,
		Hwnd(), HmenuID(0), ::GetModuleHandle(NULL), 0);
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
	GUI::Rectangle rcArea = LineArea(0);

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

void BackgroundStrip::SetProgress(const GUI::gui_string &explanation, size_t size, size_t progress) {
	if (explanation != ControlGText(wExplanation)) {
		::SetWindowTextW(HwndOf(wExplanation), explanation.c_str());
	}
	// Scale values by 1000 as PBM_SETRANGE32 limited to 32-bit
	const int scaleProgress = 1000;
	::SendMessage(HwndOf(wProgress), PBM_SETRANGE32, 0, size/scaleProgress);
	::SendMessage(HwndOf(wProgress), PBM_SETPOS, progress/scaleProgress, 0);
}

static const COLORREF colourNoMatch = RGB(0xff,0x66,0x66);

void SearchStripBase::Creation() {
	Strip::Creation();

	hbrNoMatch = CreateSolidBrush(colourNoMatch);
}

void SearchStripBase::Destruction() {
	::DeleteObject(hbrNoMatch);
	hbrNoMatch = 0;
	Strip::Destruction();
}

LRESULT SearchStripBase::NoMatchColour(HDC hdc) {
	SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
	SetBkColor(hdc, colourNoMatch);
	return reinterpret_cast<LRESULT>(hbrNoMatch);
}

void SearchStrip::Creation() {
	SearchStripBase::Creation();

	wStaticFind = CreateText(textFindPrompt);

	wText = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"), TEXT(""),
		WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | ES_AUTOHSCROLL,
		50, 2, 300, 21,
		Hwnd(), HmenuID(IDC_INCFINDTEXT), ::GetModuleHandle(NULL), 0);
	wText.Show();

	SetFontHandle(wText, fontText);

	wButton = CreateButton(textFindNext, IDC_INCFINDBTNOK);

	GUI::Rectangle rcButton = wButton.GetPosition();
	lineHeight = rcButton.Height() + space + 1;
}

void SearchStrip::Destruction() {
	SearchStripBase::Destruction();
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
	GUI::Rectangle rcArea = LineArea(0);

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
	if (select) {
		pSearcher->MoveBack();
	}
	pSearcher->SetFindText(ControlText(wText).c_str());
	pSearcher->wholeWord = false;
	if (pSearcher->FindHasText()) {
		pSearcher->FindNext(false, false);
		if (!select) {
			pSearcher->SetCaretAsStart();
		}
	}
	wText.InvalidateAll();
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

LRESULT SearchStrip::EditColour(HWND hwnd, HDC hdc) {
	if (GetDlgItem(static_cast<HWND>(GetID()),IDC_INCFINDTEXT) == hwnd) {
		if (pSearcher->FindHasText() && pSearcher->failedfind) {
			return NoMatchColour(hdc);
		}
	}
	return Strip::EditColour(hwnd, hdc);
}

LRESULT SearchStrip::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	try {
		return Strip::WndProc(iMessage, wParam, lParam);
	} catch (...) {
	}
	return 0l;
}

LRESULT FindReplaceStrip::EditColour(HWND hwnd, HDC hdc) {
	if (GetDlgItem(static_cast<HWND>(GetID()),IDFINDWHAT) == ::GetParent(hwnd)) {
		if (pSearcher->FindHasText() && 
			(incrementalBehaviour != simple) &&
			pSearcher->failedfind) {
			return NoMatchColour(hdc);
		}
	}
	return Strip::EditColour(hwnd, hdc);
}

void FindReplaceStrip::NextIncremental(ChangingSource source) {
	if (incrementalBehaviour == simple)
		return;
	if (pSearcher->findWhat.length()) {
		pSearcher->MoveBack();
	}

	if (source == changingEdit) {
		pSearcher->SetFindText(ControlText(wText).c_str());
	} else {
		pSearcher->SetFindText(ComboSelectionText(wText).c_str());
	}

	if (pSearcher->FindHasText()) {
		pSearcher->FindNext(pSearcher->reverseFind, false, true);
		pSearcher->SetCaretAsStart();
	}
	MarkIncremental();	// Mark all secondary hits
	wText.InvalidateAll();
}

void FindReplaceStrip::SetIncrementalBehaviour(int behaviour) {
	incrementalBehaviour = static_cast<FindReplaceStrip::IncrementalBehaviour>(behaviour);
}

void FindReplaceStrip::MarkIncremental() {
	if (incrementalBehaviour == showAllMatches) {
		pSearcher->MarkAll(Searcher::markIncremental);
	}
}

void FindReplaceStrip::Close() {
	if (pSearcher->havefound) {
		pSearcher->InsertFindInMemory();
	}
	Strip::Close();
	pSearcher->UIClosed();
}

void FindStrip::Creation() {
	SearchStripBase::Creation();

	wStaticFind = CreateText(textFindPrompt);

	wText = CreateWindowEx(0, TEXT("ComboBox"), TEXT(""),
		WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | CBS_DROPDOWN | CBS_AUTOHSCROLL,
		50, 2, 300, 80,
		Hwnd(), HmenuID(IDFINDWHAT), ::GetModuleHandle(NULL), 0);
	SetFontHandle(wText, fontText);
	wText.Show();

	GUI::Rectangle rcCombo = wText.GetPosition();
	lineHeight = rcCombo.Height() + space + 1;

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
	SearchStripBase::Destruction();
}

void FindStrip::Size() {
	if (!visible)
		return;
	Strip::Size();
	GUI::Rectangle rcArea = LineArea(0);

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
	rcText.bottom = rcArea.bottom;
	wStaticFind.SetPosition(rcText);

	::InvalidateRect(Hwnd(), NULL, TRUE);
}

void FindStrip::Paint(HDC hDC) {
	Strip::Paint(hDC);
}

void FindStrip::Focus() {
	::SetFocus(HwndOf(wText));
}

bool FindStrip::KeyDown(WPARAM key) {
	if (!visible)
		return false;
	if (Strip::KeyDown(key))
		return true;
	switch (key) {
	case VK_RETURN:
		if (IsChild(Hwnd(), ::GetFocus())) {
			if (incrementalBehaviour == simple) {
				Next(false, IsKeyDown(VK_SHIFT));
			} else {
				if (pSearcher->closeFind != Searcher::CloseFind::closePrevent) {
					Close();
				}
			}
			return true;
		}
	}
	return false;
}

void FindStrip::Next(bool markAll, bool invertDirection) {
	pSearcher->SetFind(ControlText(wText).c_str());
	if (markAll){
		pSearcher->MarkAll(Searcher::markWithBookMarks);
	}
	const bool found = pSearcher->FindNext(pSearcher->reverseFind ^ invertDirection) >= 0;
	if (pSearcher->ShouldClose(found)) {
		Close();
	}
}

void FindStrip::AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked) {
	GUI::gui_string localised = localiser->Text(label);
	HMENU menu = static_cast<HMENU>(popup.GetID());
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
	int subCommand = static_cast<int>(wParam >> 16);
	if (control == IDOK) {
		if (incrementalBehaviour == simple) {
			Next(false, false);
		} else {
			if (pSearcher->closeFind != Searcher::CloseFind::closePrevent) {
				Close();
			}
		}
		return true;
	} else if (control == IDMARKALL) {
		Next(true, false);
		return true;
	} else if (control == IDFINDWHAT) {
		if (subCommand == CBN_EDITCHANGE) {
			NextIncremental(changingEdit);
			return true;
		} else if (subCommand == CBN_SELCHANGE) {
			NextIncremental(changingCombo);
			return true;
		}
	} else {
		pSearcher->FlagFromCmd(control) = !pSearcher->FlagFromCmd(control);
		NextIncremental(changingEdit);
		CheckButtons();
	}
	return false;
}

void FindStrip::CheckButtons() {
	entered++;
	CheckButton(wCheckWord, pSearcher->wholeWord);
	CheckButton(wCheckCase, pSearcher->matchCase);
	CheckButton(wCheckRE, pSearcher->regExp);
	CheckButton(wCheckWrap, pSearcher->wrapFind);
	CheckButton(wCheckBE, pSearcher->unSlash);
	CheckButton(wCheckUp, pSearcher->reverseFind);
	entered--;
}

void FindStrip::Show() {
	pSearcher->failedfind = false;
	Focus();
	pSearcher->SetCaretAsStart();
	SetComboFromMemory(wText, pSearcher->memFinds);
	::SetWindowText(HwndOf(wText), GUI::StringFromUTF8(pSearcher->findWhat).c_str());
	::SendMessage(HwndOf(wText), CB_SETEDITSEL, 0, MAKELPARAM(0, -1));
	CheckButtons();
	pSearcher->ScrollEditorIfNeeded();
	MarkIncremental();
}

void ReplaceStrip::Creation() {
	SearchStripBase::Creation();

	lineHeight = 23;

	wStaticFind = CreateText(textFindPrompt);

	wText = CreateWindowEx(0, TEXT("ComboBox"), TEXT(""),
		WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | CBS_DROPDOWN | CBS_AUTOHSCROLL,
		50, 2, 300, 80,
		Hwnd(), HmenuID(IDFINDWHAT), ::GetModuleHandle(NULL), 0);
	SetFontHandle(wText, fontText);
	wText.Show();

	GUI::Rectangle rcCombo = wText.GetPosition();
	lineHeight = rcCombo.Height() + space + 1;

	wStaticReplace = CreateText(textReplacePrompt);

	wReplace = CreateWindowEx(0, TEXT("ComboBox"), TEXT(""),
		WS_CHILD | WS_TABSTOP | CBS_DROPDOWN | CBS_AUTOHSCROLL,
		50, 2, 300, 80,
		Hwnd(), HmenuID(IDREPLACEWITH), ::GetModuleHandle(NULL), 0);
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
	SearchStripBase::Destruction();
}

int ReplaceStrip::Lines() const {
	return 2;
}

void ReplaceStrip::Size() {
	if (!visible)
		return;
	Strip::Size();

	int widthCaption = Maximum(WidthControl(wStaticFind), WidthControl(wStaticReplace));

	GUI::Rectangle rcLine = LineArea(0);

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

	rcLine = LineArea(1);

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

static bool IsSameOrChild(GUI::Window &wParent, HWND wChild) {
	HWND hwnd = HwndOf(wParent);
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
	HMENU menu = static_cast<HMENU>(popup.GetID());
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
		pSearcher->ReplaceOnce(incrementalBehaviour == simple);
		NextIncremental(changingEdit);	// Show not found colour if no more matches.
	} else if ((cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL)) {
		//~ replacements = pSciTEWin->ReplaceAll(cmd == IDREPLACEINSEL);
		pSearcher->ReplaceAll(cmd == IDREPLACEINSEL);
		NextIncremental(changingEdit);	// Show not found colour if no more matches.
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
		case CBN_EDITCHANGE:
			NextIncremental(changingEdit);
			return true;
		case CBN_SELCHANGE:
			NextIncremental(changingCombo);
			return true;
		default:
			return false;
		}

	case IDOK:
	case IDREPLACEALL:
	case IDREPLACE:
	case IDREPLACEINSEL:
		HandleReplaceCommand(control);
		return true;

	case IDWHOLEWORD:
	case IDMATCHCASE:
	case IDREGEXP:
	case IDUNSLASH:
	case IDWRAP:
	case IDM_WHOLEWORD:
	case IDM_MATCHCASE:
	case IDM_REGEXP:
	case IDM_WRAPAROUND:
	case IDM_UNSLASH:
		pSearcher->FlagFromCmd(control) = !pSearcher->FlagFromCmd(control);
		NextIncremental(changingEdit);
		break;

	default:
		return false;
	}
	CheckButtons();
	return false;
}

void ReplaceStrip::CheckButtons() {
	entered++;
	CheckButton(wCheckWord, pSearcher->wholeWord);
	CheckButton(wCheckCase, pSearcher->matchCase);
	CheckButton(wCheckRE, pSearcher->regExp);
	CheckButton(wCheckWrap, pSearcher->wrapFind);
	CheckButton(wCheckBE, pSearcher->unSlash);
	entered--;
}

void ReplaceStrip::Show() {
	pSearcher->failedfind = false;
	Focus();
	pSearcher->SetCaretAsStart();
	SetComboFromMemory(wText, pSearcher->memFinds);
	::SetWindowText(HwndOf(wText), GUI::StringFromUTF8(pSearcher->findWhat).c_str());
	::SendMessage(HwndOf(wText), CB_SETEDITSEL, 0, MAKELPARAM(0, -1));
	SetComboFromMemory(wReplace, pSearcher->memReplaces);

	CheckButtons();

	if (pSearcher->FindHasText() != 0 && pSearcher->focusOnReplace) {
		::SetFocus(HwndOf(wReplace));
	}

	pSearcher->ScrollEditorIfNeeded();
	MarkIncremental();
}

void UserStrip::Creation() {
	Strip::Creation();
	// Combo boxes automatically size to a reasonable height so create a temporary and measure
	HWND wComboTest = ::CreateWindowEx(0, TEXT("ComboBox"), TEXT("Aby"),
		WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | CBS_DROPDOWN | CBS_AUTOHSCROLL,
		50, 2, 300, 80,
		Hwnd(), 0, ::GetModuleHandle(NULL), 0);
	SetWindowFont(wComboTest, fontText, 0);
	RECT rc;
	::GetWindowRect(wComboTest, &rc);
	::DestroyWindow(wComboTest);
	lineHeight = rc.bottom - rc.top + 3;
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
				::SendMessage(HwndOf(ctl->w),
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
				rcSize.bottom = rcSize.top + lineHeight - 3;
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

int UserStrip::Lines() const {
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
	size_t controlID=0;
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
					60 * control, line * lineHeight + space, puc->widthDesired, lineHeight - 3,
					Hwnd(), HmenuID(controlID), ::GetModuleHandle(NULL), 0);
				break;

			case UserControl::ucCombo:
				puc->widthDesired = 100;
				puc->fixedWidth = false;
				puc->w = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("ComboBox"), puc->text.c_str(),
					WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | CBS_DROPDOWN | CBS_AUTOHSCROLL,
					60 * control, line * lineHeight + space, puc->widthDesired, 180,
					Hwnd(), HmenuID(controlID), ::GetModuleHandle(NULL), 0);
				break;

			case UserControl::ucButton:
			case UserControl::ucDefaultButton:
				puc->widthDesired = WidthText(fontText, puc->text.c_str()) +
					2 * ::GetSystemMetrics(SM_CXEDGE) +
					2 * WidthText(fontText, TEXT(" "));
				puc->w = ::CreateWindowEx(0, TEXT("Button"), puc->text.c_str(),
					WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS |
					((puc->controlType == UserControl::ucDefaultButton) ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
					60 * control, line * lineHeight + space, puc->widthDesired, lineHeight-1,
					Hwnd(), HmenuID(controlID), ::GetModuleHandle(NULL), 0);
				break;

			default:
				puc->widthDesired = WidthText(fontText, puc->text.c_str());
				puc->w = ::CreateWindowEx(0, TEXT("Static"), puc->text.c_str(),
					WS_CHILD | WS_CLIPSIBLINGS | ES_RIGHT,
					60 * control, line * lineHeight + space, puc->widthDesired, lineHeight - 5,
					Hwnd(), HmenuID(controlID), ::GetModuleHandle(NULL), 0);
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
				ComboBox_AddString(combo, i->c_str());
			}
		}
	}
}

std::string UserStrip::GetValue(int control) {
	UserControl *ctl = FindControl(control);
	if (ctl) {
		return ControlText(ctl->w);
	}
	return "";
}
