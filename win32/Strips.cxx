// SciTE - Scintilla based Text Editor
/** @file Strips.cxx
 ** Implementation of UI strips.
 **/
// Copyright 2013 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include "SciTEWin.h"

void *PointerFromWindow(HWND hWnd) noexcept {
	return reinterpret_cast<void *>(::GetWindowLongPtr(hWnd, 0));
}

void SetWindowPointer(HWND hWnd, void *ptr) noexcept {
	::SetWindowLongPtr(hWnd, 0, reinterpret_cast<LONG_PTR>(ptr));
}

void *SetWindowPointerFromCreate(HWND hWnd, LPARAM lParam) noexcept {
	LPCREATESTRUCT pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
	void *ptr = pcs->lpCreateParams;
	SetWindowPointer(hWnd, ptr);
	return ptr;
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
	constexpr size_t maxClassNameLength = 256+1;	// +1 for NUL
	GUI::gui_char className[maxClassNameLength];
	if (::GetClassNameW(hWnd, className, maxClassNameLength))
		return GUI::gui_string(className);
	else
		return GUI::gui_string();
}

void ComboBoxAppend(HWND hWnd, const GUI::gui_string &gs) noexcept {
	ComboBox_AddString(hWnd, gs.c_str());
}

namespace {

HINSTANCE ApplicationInstance() noexcept {
	return ::GetModuleHandle(nullptr);
}

void SetFontHandle(const GUI::Window &w, HFONT hfont) noexcept {
	SetWindowFont(HwndOf(w), hfont, 0);
}

void CheckButton(const GUI::Window &wButton, bool checked) noexcept {
	Button_SetCheck(HwndOf(wButton), checked ? BST_CHECKED : BST_UNCHECKED);
}

SIZE SizeButton(const GUI::Window &wButton) noexcept {
	SIZE sz = { 0, 0 };
	// Push buttons can be measured with BCM_GETIDEALSIZE.
	::SendMessage(HwndOf(wButton),
		      BCM_GETIDEALSIZE, 0, reinterpret_cast<LPARAM>(&sz));
	return sz;
}

SIZE SizeText(HFONT hfont, const GUI::gui_char *text) noexcept {
	HDC hdcMeasure = ::CreateCompatibleDC({});
	HFONT hfontOriginal = SelectFont(hdcMeasure, hfont);
	RECT rcText = {0, 0, 2000, 2000};
	::DrawText(hdcMeasure, text, -1, &rcText, DT_CALCRECT);
	SelectFont(hdcMeasure, hfontOriginal);
	::DeleteDC(hdcMeasure);
	return SIZE{ rcText.right - rcText.left, rcText.bottom - rcText.top, };
}

int WidthText(HFONT hfont, const GUI::gui_char *text) noexcept {
	return SizeText(hfont, text).cx;
}

int WidthControl(GUI::Window &w) {
	const GUI::Rectangle rc = w.GetPosition();
	return rc.Width();
}

GUI::gui_string ControlGText(GUI::Window w) {
	return TextOfWindow(HwndOf(w));
}

std::string ControlText(GUI::Window w) {
	const GUI::gui_string gsText = ControlGText(w);
	return GUI::UTF8FromString(gsText);
}

std::string ComboSelectionText(GUI::Window w) {
	HWND combo = HwndOf(w);
	const int selection = ComboBox_GetCurSel(combo);
	if (selection != CB_ERR) {
		const int len = ComboBox_GetLBTextLen(combo, selection);
		GUI::gui_string itemText(len+1, L'\0');
		const int lenActual = ComboBox_GetLBText(combo, selection, &itemText[0]);
		if (lenActual != CB_ERR) {
			itemText.pop_back(); // Remove NUL
			return GUI::UTF8FromString(itemText);
		}
	}
	return std::string();
}

enum class ComboSelection { all, atEnd };

void SetComboText(GUI::Window w, const std::string &s, ComboSelection selection) {
	HWND combo = HwndOf(w);
	GUI::gui_string text = GUI::StringFromUTF8(s);
	ComboBox_SetText(combo, text.c_str());
	if (selection == ComboSelection::all) {
		ComboBox_SetEditSel(combo, 0, -1);
	} else {
		const size_t textLength = text.length();
		ComboBox_SetEditSel(combo, textLength, textLength);
	}
}

void SetComboFromMemory(GUI::Window w, const ComboMemory &mem) {
	HWND combo = HwndOf(w);
	ComboBox_ResetContent(combo);
	for (int i = 0; i < mem.Length(); i++) {
		GUI::gui_string gs = GUI::StringFromUTF8(mem.At(i));
		ComboBoxAppend(combo, gs);
	}
}

}

LRESULT PASCAL BaseWin::StWndProc(
	HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	if (iMessage == WM_CREATE) {
		// Pointer to BaseWin passed with WM_CREATE so remember in window pointer
		BaseWin *basePassed = static_cast<BaseWin *>(SetWindowPointerFromCreate(hWnd, lParam));
		basePassed->SetID(hWnd);
	}
	// Find C++ object associated with window.
	BaseWin *base = static_cast<BaseWin *>(::PointerFromWindow(hWnd));
	// base will be zero if WM_CREATE not seen yet
	if (base) {
		return base->WndProc(iMessage, wParam, lParam);
	} else {
		return ::DefWindowProc(hWnd, iMessage, wParam, lParam);
	}
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
	{nullptr, 0, 0},
};

GUI::Window Strip::CreateText(const char *text) {
	GUI::gui_string localised = localiser->Text(text);
	const int width = WidthText(fontText, localised.c_str()) + 4;
	GUI::Window w;
	w.SetID(::CreateWindowEx(0, TEXT("Static"), localised.c_str(),
				 WS_CHILD | WS_CLIPSIBLINGS | SS_RIGHT,
				 2, 2, width, 21,
				 Hwnd(), HmenuID(0), ::ApplicationInstance(), nullptr));
	SetFontHandle(w, fontText);
	w.Show();
	return w;
}

#define PACKVERSION(major,minor) MAKELONG(minor,major)

static DWORD GetVersion(LPCTSTR lpszDllName) noexcept {
	DWORD dwVersion = 0;
	HINSTANCE hinstDll = ::LoadLibrary(lpszDllName);
	if (hinstDll) {
		DLLGETVERSIONPROC pDllGetVersion = reinterpret_cast<DLLGETVERSIONPROC>(
				::GetProcAddress(hinstDll, "DllGetVersion"));

		if (pDllGetVersion) {
			DLLVERSIONINFO dvi;
			::ZeroMemory(&dvi, sizeof(dvi));
			dvi.cbSize = sizeof(dvi);

			const HRESULT hr = (*pDllGetVersion)(&dvi);
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
		const int checkSize = ::GetSystemMetrics(SM_CXMENUCHECK);
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
				 Hwnd(), HmenuID(ident), ::ApplicationInstance(), nullptr));
	if (check) {
		int resNum = IDBM_WORD;
		switch (ident) {
		case IDWHOLEWORD:
			resNum = IDBM_WORD;
			break;
		case IDMATCHCASE:
			resNum = IDBM_CASE;
			break;
		case IDREGEXP:
			resNum = IDBM_REGEX;
			break;
		case IDUNSLASH:
			resNum = IDBM_BACKSLASH;
			break;
		case IDWRAP:
			resNum = IDBM_AROUND;
			break;
		case IDDIRECTIONUP:
			resNum = IDBM_UP;
			break;
		}

		const UINT flags = (GetVersion(TEXT("COMCTL32")) >= PACKVERSION(6, 0)) ?
				   (LR_DEFAULTSIZE) : (LR_DEFAULTSIZE|LR_LOADMAP3DCOLORS);
		HBITMAP bm = static_cast<HBITMAP>(::LoadImage(
				::ApplicationInstance(), MAKEINTRESOURCE(resNum + resDifference), IMAGE_BITMAP,
				bmpDimension, bmpDimension, flags));

		::SendMessage(HwndOf(w),
			      BM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(bm));
	}
	SetFontHandle(w, fontText);
	w.Show();
	if (!check) {
		const SIZE sz = SizeButton(w);
		if (sz.cx > 0) {
			const GUI::Rectangle rc(0, 0, sz.cx + 2 * WidthText(fontText, TEXT(" ")), sz.cy);
			w.SetPosition(rc);
		}
	}
	TOOLINFO toolInfo {};
	toolInfo.cbSize = sizeof(toolInfo);
	toolInfo.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
	toolInfo.hinst = ::ApplicationInstance();
	toolInfo.hwnd = Hwnd();
	toolInfo.uId = reinterpret_cast<UINT_PTR>(w.GetID());
	toolInfo.lpszText = LPSTR_TEXTCALLBACK;
	::GetClientRect(Hwnd(), &toolInfo.rect);
	::SendMessageW(HwndOf(wToolTip), TTM_ADDTOOLW,
		       0, reinterpret_cast<LPARAM>(&toolInfo));
	::SendMessage(HwndOf(wToolTip), TTM_ACTIVATE, TRUE, 0);
	return w;
}

void Strip::Tab(bool forwards) noexcept {
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
				    TOOLTIPS_CLASSW, nullptr,
				    WS_POPUP | TTS_ALWAYSTIP,
				    CW_USEDEFAULT, CW_USEDEFAULT,
				    CW_USEDEFAULT, CW_USEDEFAULT,
				    Hwnd(), NULL, ::ApplicationInstance(), nullptr);

	SetTheme();
}

void Strip::Destruction() noexcept {
	if (fontText)
		::DeleteObject(fontText);
	fontText = {};
#ifdef THEME_AVAILABLE
	if (hTheme)
		::CloseThemeData(hTheme);
#endif
	hTheme = {};
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
			HWND wChild = GetFirstChild(Hwnd());
			while (wChild) {
				const GUI::gui_string className = ClassNameOfWindow(wChild);
				if ((className == TEXT("Button")) || (className == TEXT("Static"))) {
					const std::string caption = GUI::UTF8FromString(TextOfWindow(wChild));
					for (int i=0; caption[i]; i++) {
						if ((caption[i] == L'&') && (MakeUpperCase(caption[i+1]) == static_cast<int>(key))) {
							if (className == TEXT("Button")) {
								::SendMessage(wChild, BM_CLICK, 0, 0);
							} else {	// Static caption
								wChild = GetNextSibling(wChild);
								::SetFocus(wChild);
							}
							return true;
						}
					}
				}
				wChild = GetNextSibling(wChild);
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

namespace {

constexpr RECT RECTFromRectangle(GUI::Rectangle r) noexcept {
	RECT rc = { r.left, r.top, r.right, r.bottom };
	return rc;
}

}

void Strip::Paint(HDC hDC) {
	const GUI::Rectangle rcStrip = GetClientPosition();
	const RECT rc = RECTFromRectangle(rcStrip);
	HBRUSH hbrFace = CreateSolidBrush(::GetSysColor(COLOR_3DFACE));
	::FillRect(hDC, &rc, hbrFace);
	::DeleteObject(hbrFace);

	if (HasClose()) {
		// Draw close box
		RECT rcClose = RECTFromRectangle(CloseArea());
		if (hTheme) {
#ifdef THEME_AVAILABLE
			int closeAppearence = CBS_NORMAL;
			if (closeState == csOver) {
				closeAppearence = CBS_HOT;
			} else if (closeState == csClickedOver) {
				closeAppearence = CBS_PUSHED;
			}
			::DrawThemeBackground(hTheme, hDC, WP_SMALLCLOSEBUTTON, closeAppearence,
					      &rcClose, nullptr);
#endif
		} else {
			int closeAppearence = 0;
			if (closeState == csOver) {
				closeAppearence = DFCS_HOT;
			} else if (closeState == csClickedOver) {
				closeAppearence = DFCS_PUSHED;
			}

			DrawFrameControl(hDC, &rcClose, DFC_CAPTION,
					 DFCS_CAPTIONCLOSE | closeAppearence);
		}
	}
}

bool Strip::HasClose() const noexcept {
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
		return GUI::Rectangle(-1, -1, -1, -1);
	}
}

GUI::Rectangle Strip::LineArea(int line) {
	GUI::Rectangle rcLine = GetPosition();

	rcLine.right -= rcLine.left;

	rcLine.left = space;
	rcLine.top = space + line * lineHeight;
	rcLine.right -= space;
	rcLine.bottom = rcLine.top + lineHeight - space;

	if (HasClose())
		rcLine.right -= closeSize.cx + space;	// Allow for close box and gap

	return rcLine;
}

int Strip::Lines() const noexcept {
	return 1;
}

void Strip::InvalidateClose() {
	const RECT rc = RECTFromRectangle(CloseArea());
	::InvalidateRect(Hwnd(), &rc, TRUE);
}

bool Strip::MouseInClose(GUI::Point pt) {
	const GUI::Rectangle rcClose = CloseArea();
	return rcClose.Contains(pt);
}

void Strip::TrackMouse(GUI::Point pt) {
	const stripCloseState closeStateStart = closeState;
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
		TRACKMOUSEEVENT tme {};
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = Hwnd();
		TrackMouseEvent(&tme);
	}
	if (closeStateStart != closeState)
		InvalidateClose();
}

void Strip::SetTheme() noexcept {
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
		const HRESULT hr = ::GetThemePartSize(hTheme, NULL, WP_SMALLCLOSEBUTTON, CBS_NORMAL,
						      nullptr, TS_TRUE, &closeSize);
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

static bool HideKeyboardCues() noexcept {
	BOOL b=FALSE;
	if (::SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &b, 0) == 0)
		return FALSE;
	return !b;
}

LRESULT Strip::EditColour(HWND hwnd, HDC hdc) noexcept {
	return ::DefWindowProc(Hwnd(), WM_CTLCOLOREDIT, reinterpret_cast<WPARAM>(hdc),
			       reinterpret_cast<LPARAM>(hwnd));
}

LRESULT Strip::CustomDraw(NMHDR *pnmh) noexcept {
	const int btnStyle = GetWindowStyle(pnmh->hwndFrom);
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
		const bool checked = Button_GetCheck(pnmh->hwndFrom) == BST_CHECKED;

		int buttonAppearence = checked ? TS_CHECKED : TS_NORMAL;
		if (pcd->uItemState & CDIS_SELECTED)
			buttonAppearence = TS_PRESSED;
		else if (pcd->uItemState & CDIS_HOT)
			buttonAppearence = checked ? TS_HOTCHECKED : TS_HOT;
		HBRUSH hbrFace = CreateSolidBrush(::GetSysColor(COLOR_3DFACE));
		::FillRect(pcd->hdc, &pcd->rc, hbrFace);
		::DeleteObject(hbrFace);
		::DrawThemeBackground(hThemeButton, pcd->hdc, TP_BUTTON, buttonAppearence,
				      &pcd->rc, nullptr);

		RECT rcButton = pcd->rc;
		rcButton.bottom--;
		const HRESULT hr = ::GetThemeBackgroundContentRect(hThemeButton, pcd->hdc, TP_BUTTON,
				   buttonAppearence, &pcd->rc, &rcButton);
		if (!SUCCEEDED(hr)) {
			return CDRF_DODEFAULT;
		}

		HBITMAP hBitmap = reinterpret_cast<HBITMAP>(::SendMessage(
					  pnmh->hwndFrom, BM_GETIMAGE, IMAGE_BITMAP, 0));

		// Retrieve the bitmap dimensions
		BITMAPINFO rbmi = BITMAPINFO();
		rbmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		::GetDIBits(pcd->hdc, hBitmap, 0, 0, nullptr, &rbmi, DIB_RGB_COLORS);

		constexpr DWORD colourTransparent = RGB(0xC0, 0xC0, 0xC0);

		// Offset from button edge to contents.
		const int xOffset = ((rcButton.right - rcButton.left) - rbmi.bmiHeader.biWidth) / 2 + 1;
		const int yOffset = ((rcButton.bottom - rcButton.top) - rbmi.bmiHeader.biHeight) / 2;

		HDC hdcBM = ::CreateCompatibleDC({});
		HBITMAP hbmOriginal = SelectBitmap(hdcBM, hBitmap);
		::TransparentBlt(pcd->hdc, xOffset, yOffset,
				 rbmi.bmiHeader.biWidth, rbmi.bmiHeader.biHeight,
				 hdcBM, 0, 0, rbmi.bmiHeader.biWidth, rbmi.bmiHeader.biHeight, colourTransparent);
		SelectBitmap(hdcBM, hbmOriginal);
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

	case WM_SETCURSOR: {
			HWND hWnd = reinterpret_cast<HWND>(wParam);
			const GUI::gui_string className = ClassNameOfWindow(hWnd);
			if (className == TEXT("Edit") || className == TEXT("ComboBox")) {
				return ::DefWindowProc(Hwnd(), iMessage, wParam, lParam);
			} else {
				::SetCursor(::LoadCursor(NULL, IDC_ARROW));
				return 0;
			}
		}

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
			if (pnmh->code == NM_CUSTOMDRAW) {
				return CustomDraw(pnmh);
			} else if (pnmh->code == TTN_GETDISPINFO) {
				NMTTDISPINFOW *pnmtdi = reinterpret_cast<LPNMTTDISPINFO>(lParam);
				const int idButton = static_cast<int>(
							     (pnmtdi->uFlags & TTF_IDISHWND) ?
							     ::GetDlgCtrlID(reinterpret_cast<HWND>(pnmtdi->hdr.idFrom)) : pnmtdi->hdr.idFrom);
				for (const SearchOption &so : toggles) {
					if (so.id == idButton) {
						GUI::gui_string localised = localiser->Text(so.label);
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

	return 0;
}

void Strip::AddToPopUp(GUI::Menu &popup, const char *label, int cmd, bool checked) const {
	const GUI::gui_string localised = localiser->Text(label);
	HMENU menu = static_cast<HMENU>(popup.GetID());
	if (localised.empty())
		::AppendMenu(menu, MF_SEPARATOR, 0, TEXT(""));
	else
		::AppendMenu(menu, MF_STRING | (checked ? MF_CHECKED : 0), cmd, localised.c_str());
}

void Strip::ShowPopup() {
}

void BackgroundStrip::Creation() {
	Strip::Creation();
	lineHeight = SizeText(fontText, GUI_TEXT("\u00C5Ay")).cy + space * 2 + 1;

	wExplanation = ::CreateWindowEx(0, TEXT("Static"), TEXT(""),
					WS_CHILD | WS_CLIPSIBLINGS,
					2, 2, 100, 21,
					Hwnd(), HmenuID(0), ::ApplicationInstance(), nullptr);
	wExplanation.Show();
	SetFontHandle(wExplanation, fontText);

	wProgress = ::CreateWindowEx(0, PROGRESS_CLASS, TEXT(""),
				     WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
				     2, 2, 100, 21,
				     Hwnd(), HmenuID(0), ::ApplicationInstance(), nullptr);
}

void BackgroundStrip::Destruction() noexcept {
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
	const GUI::Rectangle rcArea = LineArea(0);

	constexpr int progWidth = 200;

	GUI::Rectangle rcProgress = rcArea;
	rcProgress.right = rcProgress.left + progWidth;
	wProgress.SetPosition(rcProgress);

	GUI::Rectangle rcExplanation = rcArea;
	rcExplanation.left += progWidth + 8;
	rcExplanation.top -= 1;
	rcExplanation.bottom += 1;
	wExplanation.SetPosition(rcExplanation);

	::InvalidateRect(Hwnd(), nullptr, TRUE);
}

bool BackgroundStrip::HasClose() const noexcept {
	return false;
}

void BackgroundStrip::Focus() noexcept {
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
	return 0;
}

void BackgroundStrip::SetProgress(const GUI::gui_string &explanation, size_t size, size_t progress) {
	if (explanation != ControlGText(wExplanation)) {
		::SetWindowTextW(HwndOf(wExplanation), explanation.c_str());
	}
	// Scale values by 1000 as PBM_SETRANGE32 limited to 32-bit
	constexpr int scaleProgress = 1000;
	::SendMessage(HwndOf(wProgress), PBM_SETRANGE32, 0, size/scaleProgress);
	::SendMessage(HwndOf(wProgress), PBM_SETPOS, progress/scaleProgress, 0);
}

static constexpr COLORREF colourNoMatch = RGB(0xff, 0x66, 0x66);

void SearchStripBase::Creation() {
	Strip::Creation();

	hbrNoMatch = CreateSolidBrush(colourNoMatch);
}

void SearchStripBase::Destruction() noexcept {
	::DeleteObject(hbrNoMatch);
	hbrNoMatch = {};
	Strip::Destruction();
}

LRESULT SearchStripBase::NoMatchColour(HDC hdc) noexcept {
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
			       Hwnd(), HmenuID(IDC_INCFINDTEXT), ::ApplicationInstance(), nullptr);
	wText.Show();

	SetFontHandle(wText, fontText);

	wButton = CreateButton(textFindNext, IDC_INCFINDBTNOK);

	const GUI::Rectangle rcButton = wButton.GetPosition();
	lineHeight = rcButton.Height() + space + 1;
}

void SearchStrip::Destruction() noexcept {
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
	const GUI::Rectangle rcArea = LineArea(0);

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

	::InvalidateRect(Hwnd(), nullptr, TRUE);
}

void SearchStrip::Paint(HDC hDC) {
	Strip::Paint(hDC);
}

void SearchStrip::Focus() noexcept {
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
	const int control = ControlIDOfWParam(wParam);
	const int subCommand = static_cast<int>(wParam >> 16);
	if (((control == IDC_INCFINDTEXT) && (subCommand == EN_CHANGE)) ||
			(control == IDC_INCFINDBTNOK)) {
		Next(control != IDC_INCFINDBTNOK);
		return true;
	}
	return false;
}

LRESULT SearchStrip::EditColour(HWND hwnd, HDC hdc) noexcept {
	if (GetDlgItem(static_cast<HWND>(GetID()), IDC_INCFINDTEXT) == hwnd) {
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
	return 0;
}

LRESULT FindReplaceStrip::EditColour(HWND hwnd, HDC hdc) noexcept {
	if (GetDlgItem(static_cast<HWND>(GetID()), IDFINDWHAT) == ::GetParent(hwnd)) {
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

void FindReplaceStrip::SetIncrementalBehaviour(int behaviour) noexcept {
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
			       Hwnd(), HmenuID(IDFINDWHAT), ::ApplicationInstance(), nullptr);
	SetFontHandle(wText, fontText);
	wText.Show();

	const GUI::Rectangle rcCombo = wText.GetPosition();
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

void FindStrip::Destruction() noexcept {
	SearchStripBase::Destruction();
}

void FindStrip::Size() {
	if (!visible)
		return;
	Strip::Size();
	const GUI::Rectangle rcArea = LineArea(0);

	GUI::Rectangle rcButton = rcArea;
	rcButton.top -= 1;
	rcButton.bottom += 1;

	const int checkWidth = rcButton.Height() - 2;	// Using height to make square
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

	::InvalidateRect(Hwnd(), nullptr, TRUE);
}

void FindStrip::Paint(HDC hDC) {
	Strip::Paint(hDC);
}

void FindStrip::Focus() noexcept {
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
				if (pSearcher->closeFind == Searcher::CloseFind::closePrevent) {
					Next(false, IsKeyDown(VK_SHIFT));
				} else {
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
	if (markAll) {
		pSearcher->MarkAll(Searcher::markWithBookMarks);
	}
	const bool found = pSearcher->FindNext(pSearcher->reverseFind ^ invertDirection) >= 0;
	if (pSearcher->ShouldClose(found)) {
		Close();
	} else {
		SetComboFromMemory(wText, pSearcher->memFinds);
		SetComboText(wText, pSearcher->findWhat, ComboSelection::atEnd);
	}
}

void FindStrip::ShowPopup() {
	GUI::Menu popup;
	popup.CreatePopUp();
	for (int i=SearchOption::tWord; i<=SearchOption::tUp; i++) {
		AddToPopUp(popup, toggles[i].label, toggles[i].cmd, pSearcher->FlagFromCmd(toggles[i].cmd));
	}
	const GUI::Rectangle rcButton = wButton.GetPosition();
	const GUI::Point pt(rcButton.left, rcButton.bottom);
	popup.Show(pt, *this);
}

bool FindStrip::Command(WPARAM wParam) {
	if (entered)
		return false;
	const int control = ControlIDOfWParam(wParam);
	const int subCommand = static_cast<int>(wParam >> 16);
	if (control == IDOK) {
		if (incrementalBehaviour == simple) {
			Next(false, false);
		} else {
			if (pSearcher->closeFind == Searcher::CloseFind::closePrevent) {
				Next(false, false);
			} else {
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

void FindStrip::CheckButtons() noexcept {
	entered++;
	CheckButton(wCheckWord, pSearcher->wholeWord);
	CheckButton(wCheckCase, pSearcher->matchCase);
	CheckButton(wCheckRE, pSearcher->regExp);
	CheckButton(wCheckWrap, pSearcher->wrapFind);
	CheckButton(wCheckBE, pSearcher->unSlash);
	CheckButton(wCheckUp, pSearcher->reverseFind);
	entered--;
}

void FindStrip::ShowStrip() {
	pSearcher->failedfind = false;
	Focus();
	pSearcher->SetCaretAsStart();
	SetComboFromMemory(wText, pSearcher->memFinds);
	SetComboText(wText, pSearcher->findWhat, ComboSelection::all);
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
			       Hwnd(), HmenuID(IDFINDWHAT), ::ApplicationInstance(), nullptr);
	SetFontHandle(wText, fontText);
	wText.Show();

	const GUI::Rectangle rcCombo = wText.GetPosition();
	lineHeight = rcCombo.Height() + space + 1;

	wStaticReplace = CreateText(textReplacePrompt);

	wReplace = CreateWindowEx(0, TEXT("ComboBox"), TEXT(""),
				  WS_CHILD | WS_TABSTOP | CBS_DROPDOWN | CBS_AUTOHSCROLL,
				  50, 2, 300, 80,
				  Hwnd(), HmenuID(IDREPLACEWITH), ::ApplicationInstance(), nullptr);
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

void ReplaceStrip::Destruction() noexcept {
	SearchStripBase::Destruction();
}

int ReplaceStrip::Lines() const noexcept {
	return 2;
}

void ReplaceStrip::Size() {
	if (!visible)
		return;
	Strip::Size();

	const int widthCaption = std::max(WidthControl(wStaticFind), WidthControl(wStaticReplace));

	GUI::Rectangle rcLine = LineArea(0);

	const int widthButtons = std::max(WidthControl(wButtonFind), WidthControl(wButtonReplace));
	const int widthLastButtons = std::max(WidthControl(wButtonReplaceAll), WidthControl(wButtonReplaceInSelection));

	GUI::Rectangle rcButton = rcLine;
	rcButton.top -= 1;

	const int checkWidth = rcButton.Height() - 2;	// Using height to make square

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

	::InvalidateRect(Hwnd(), nullptr, TRUE);
}

void ReplaceStrip::Paint(HDC hDC) {
	Strip::Paint(hDC);
}

void ReplaceStrip::Focus() noexcept {
	::SetFocus(HwndOf(wText));
}

static bool IsSameOrChild(const GUI::Window &wParent, HWND wChild) noexcept {
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

void ReplaceStrip::ShowPopup() {
	GUI::Menu popup;
	popup.CreatePopUp();
	for (int i=SearchOption::tWord; i<=SearchOption::tWrap; i++) {
		AddToPopUp(popup, toggles[i].label, toggles[i].cmd, pSearcher->FlagFromCmd(toggles[i].cmd));
	}
	const GUI::Rectangle rcButton = wCheckWord.GetPosition();
	const GUI::Point pt(rcButton.left, rcButton.bottom);
	popup.Show(pt, *this);
}

void ReplaceStrip::HandleReplaceCommand(int cmd, bool reverseFind) {
	pSearcher->SetFind(ControlText(wText).c_str());
	SetComboFromMemory(wText, pSearcher->memFinds);
	SetComboText(wText, pSearcher->findWhat, ComboSelection::atEnd);
	if (cmd != IDOK) {
		pSearcher->SetReplace(ControlText(wReplace).c_str());
		SetComboFromMemory(wReplace, pSearcher->memReplaces);
		SetComboText(wReplace, pSearcher->replaceWhat, ComboSelection::atEnd);
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
	const int control = ControlIDOfWParam(wParam);
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

void ReplaceStrip::CheckButtons() noexcept {
	entered++;
	CheckButton(wCheckWord, pSearcher->wholeWord);
	CheckButton(wCheckCase, pSearcher->matchCase);
	CheckButton(wCheckRE, pSearcher->regExp);
	CheckButton(wCheckWrap, pSearcher->wrapFind);
	CheckButton(wCheckBE, pSearcher->unSlash);
	entered--;
}

void ReplaceStrip::ShowStrip() {
	pSearcher->failedfind = false;
	Focus();
	pSearcher->SetCaretAsStart();
	SetComboFromMemory(wText, pSearcher->memFinds);
	SetComboText(wText, pSearcher->findWhat, ComboSelection::all);
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
					   Hwnd(), 0, ::ApplicationInstance(), nullptr);
	SetWindowFont(wComboTest, fontText, 0);
	RECT rc;
	::GetWindowRect(wComboTest, &rc);
	::DestroyWindow(wComboTest);
	lineHeight = rc.bottom - rc.top + 3;
}

void UserStrip::Destruction() noexcept {
	psd.reset();
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

	// Calculate widths of buttons.
	for (std::vector<UserControl> &line : psd->controls) {
		for (UserControl &ctl : line) {
			if (ctl.controlType == UserControl::ucButton) {
				const SIZE sz = SizeButton(ctl.w);
				if (sz.cx > 0) {
					ctl.widthDesired = sz.cx + 2 * WidthText(fontText, TEXT(" "));
				}
			}
		}
	}

	psd->CalculateColumnWidths(rcArea.Width());

	// Position each control.
	int top = rcArea.top;
	for (std::vector<UserControl> &line : psd->controls) {
		int left = rcArea.left;
		size_t column = 0;
		for (UserControl &ctl : line) {
			ctl.widthAllocated = psd->widths[column].widthAllocated;

			GUI::Rectangle rcSize = ctl.w.GetClientPosition();
			int topWithFix = top;
			if (ctl.controlType == UserControl::ucButton)
				topWithFix--;
			if (ctl.controlType == UserControl::ucStatic)
				topWithFix += 3;
			if (ctl.controlType == UserControl::ucEdit)
				rcSize.bottom = rcSize.top + lineHeight - 3;
			if (ctl.controlType == UserControl::ucCombo)
				rcSize.bottom = rcSize.top + 180;
			const GUI::Rectangle rcControl(left, topWithFix, left + ctl.widthAllocated, topWithFix + rcSize.Height());
			ctl.w.SetPosition(rcControl);
			left += ctl.widthAllocated + 4;

			column++;
		}
		top += lineHeight;
	}

	::InvalidateRect(Hwnd(), nullptr, TRUE);
}

bool UserStrip::HasClose() const noexcept {
	return psd && psd->hasClose;
}

void UserStrip::Focus() noexcept {
	for (const std::vector<UserControl> &line : psd->controls) {
		for (const UserControl &ctl : line) {
			if (ctl.controlType != UserControl::ucStatic) {
				::SetFocus(HwndOf(ctl.w));
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
			for (const std::vector<UserControl> &line : psd->controls) {
				for (const UserControl &ctl : line) {
					if (ctl.controlType == UserControl::ucDefaultButton) {
						extender->OnUserStrip(ctl.item, scClicked);
						return true;
					}
				}
			}
		}
	}

	return false;
}

static StripCommand NotificationToStripCommand(int notification) noexcept {
	switch (notification) {
	case BN_CLICKED:
		return scClicked;
	case EN_CHANGE:
	case CBN_EDITCHANGE:
		return scChange;
	case EN_UPDATE:
		return scUnknown;
	case CBN_SETFOCUS:
	case EN_SETFOCUS:
		return scFocusIn;
	case CBN_KILLFOCUS:
	case EN_KILLFOCUS:
		return scFocusOut;
	default:
		return scUnknown;
	}
}

bool UserStrip::Command(WPARAM wParam) {
	if (entered)
		return false;
	const int control = ControlIDOfWParam(wParam);
	const int notification = HIWORD(wParam);
	if (extender) {
		const StripCommand sc = NotificationToStripCommand(notification);
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
	return 0;
}

int UserStrip::Lines() const noexcept {
	return psd ? static_cast<int>(psd->controls.size()) : 1;
}

void UserStrip::SetDescription(const char *description) {
	entered++;
	GUI::gui_string sDescription = GUI::StringFromUTF8(description);
	const bool resetting = psd != nullptr;
	if (psd) {
		for (std::vector<UserControl> &line : psd->controls) {
			for (UserControl &ctl : line) {
				ctl.w.Destroy();
			}
		}
	}
	psd.reset(new StripDefinition(sDescription));
	// Create all the controls but with arbitrary initial positions which will be fixed up later.
	size_t controlID=0;
	int top = space;
	for (std::vector<UserControl> &line : psd->controls) {
		int left = 0;
		for (UserControl &ctl : line) {
			switch (ctl.controlType) {
			case UserControl::ucEdit:
				ctl.widthDesired = 100;
				ctl.fixedWidth = false;
				ctl.w = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"), ctl.text.c_str(),
							 WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | ES_AUTOHSCROLL,
							 left, top, ctl.widthDesired, lineHeight - 3,
							 Hwnd(), HmenuID(controlID), ::ApplicationInstance(), nullptr);
				break;

			case UserControl::ucCombo:
				ctl.widthDesired = 100;
				ctl.fixedWidth = false;
				ctl.w = ::CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("ComboBox"), ctl.text.c_str(),
							 WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
							 left, top, ctl.widthDesired, 180,
							 Hwnd(), HmenuID(controlID), ::ApplicationInstance(), nullptr);
				break;

			case UserControl::ucButton:
			case UserControl::ucDefaultButton:
				ctl.widthDesired = WidthText(fontText, ctl.text.c_str()) +
						   2 * ::GetSystemMetrics(SM_CXEDGE) +
						   2 * WidthText(fontText, TEXT(" "));
				ctl.w = ::CreateWindowEx(0, TEXT("Button"), ctl.text.c_str(),
							 WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS |
							 ((ctl.controlType == UserControl::ucDefaultButton) ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
							 left, top, ctl.widthDesired, lineHeight-1,
							 Hwnd(), HmenuID(controlID), ::ApplicationInstance(), nullptr);
				break;

			default:
				ctl.widthDesired = WidthText(fontText, ctl.text.c_str());
				ctl.w = ::CreateWindowEx(0, TEXT("Static"), ctl.text.c_str(),
							 WS_CHILD | WS_CLIPSIBLINGS | ES_RIGHT,
							 left, top, ctl.widthDesired, lineHeight - 5,
							 Hwnd(), HmenuID(controlID), ::ApplicationInstance(), nullptr);
				break;
			}
			ctl.w.Show();
			SetFontHandle(ctl.w, fontText);
			controlID++;
			left += 60;
		}
		top += lineHeight;
	}
	if (resetting)
		Size();
	entered--;
	Focus();
}

void UserStrip::SetExtender(Extension *extender_) noexcept {
	extender = extender_;
}

void UserStrip::SetSciTE(SciTEWin *pSciTEWin_) noexcept {
	pSciTEWin = pSciTEWin_;
}

UserControl *UserStrip::FindControl(int control) {
	return psd->FindControl(control);
}

void UserStrip::Set(int control, const char *value) {
	const UserControl *ctl = FindControl(control);
	if (ctl) {
		GUI::gui_string sValue = GUI::StringFromUTF8(value);
		::SetWindowTextW(HwndOf(ctl->w), sValue.c_str());
	}
}

void UserStrip::SetList(int control, const char *value) {
	const UserControl *ctl = FindControl(control);
	if (ctl) {
		if (ctl->controlType == UserControl::ucCombo) {
			const GUI::gui_string sValue = GUI::StringFromUTF8(value);
			const std::vector<GUI::gui_string> listValues = ListFromString(sValue);
			HWND combo = HwndOf(ctl->w);
			ComboBox_ResetContent(combo);
			for (const GUI::gui_string &gs : listValues) {
				ComboBoxAppend(combo, gs);
			}
		}
	}
}

std::string UserStrip::GetValue(int control) {
	const UserControl *ctl = FindControl(control);
	if (ctl) {
		return ControlText(ctl->w);
	}
	return "";
}
