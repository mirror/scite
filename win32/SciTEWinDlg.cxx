// SciTE - Scintilla based Text Editor
/** @file SciTEWinDlg.cxx
 ** Dialog code for the Windows version of the editor.
 **/
// Copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include "SciTEWin.h"
// need this header for SHBrowseForFolder
#include <Shlobj.h>

/**
 * Flash the given window for the asked @a duration to visually warn the user.
 */
static void FlashThisWindow(
    HWND hWnd,    		///< Window to flash handle.
    int duration) {	///< Duration of the flash state.

	HDC hDC = ::GetDC(hWnd);
	if (hDC != NULL) {
		RECT rc;
		::GetClientRect(hWnd, &rc);
		::FillRect(hDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
		::Sleep(duration);
	}
	::ReleaseDC(hWnd, hDC);
	::InvalidateRect(hWnd, NULL, true);
}

/**
 * Play the given sound, loading if needed the corresponding DLL function.
 */
static void PlayThisSound(
    const char *sound,    	///< Path to a .wav file or string with a frequency value.
    int duration,    		///< If @a sound is a frequency, gives the duration of the sound.
    HMODULE &hMM) {		///< Multimedia DLL handle.

	bool bPlayOK = false;
	int soundFreq;
	if (!sound || *sound == '\0') {
		soundFreq = -1;	// No sound at all
	} else {
		soundFreq = atoi(sound);	// May be a frequency, not a filename
	}

	if (soundFreq == 0) {	// sound is probably a path
		if (!hMM) {
			// Load the DLL only if needed (may be slow on some systems)
			hMM = ::LoadLibrary("WINMM.DLL");
		}

		if (hMM) {
			typedef BOOL (WINAPI *MMFn) (LPCSTR, HMODULE, DWORD);
			MMFn fnMM = (MMFn)::GetProcAddress(hMM, "PlaySoundA");
			if (fnMM) {
				bPlayOK = fnMM(sound, NULL, SND_ASYNC | SND_FILENAME);
			}
		}
	}
	if (!bPlayOK && soundFreq >= 0) {	// The sound could no be played, or user gave a frequency
		// Will use the speaker to generate a sound
		if (soundFreq < 37 || soundFreq > 32767) {
			soundFreq = 440;
		}
		if (duration < 50) {
			duration = 50;
		}
		if (duration > 5000) {	// Don't play too long...
			duration = 5000;
		}
		// soundFreq and duration are not used on Win9x.
		// On those systems, PC will either use the default sound event or
		// emit a standard speaker sound.
		::Beep(soundFreq, duration);
	}
}

// Handle Unicode controls (assume strings to be UTF-8 on Windows NT)

// This is a reasonable buffer size for dialog box text conversions
#define CTL_TEXT_BUF 512

SString GetDlgItemText2(HWND hDlg, int idItem) {
	SString result = "";
	WCHAR wsz[CTL_TEXT_BUF];
	char	msz[CTL_TEXT_BUF];

	if (IsWindowUnicode(GetDlgItem(hDlg, idItem))) {
		if (GetDlgItemTextW(hDlg, idItem, wsz, CTL_TEXT_BUF)) {
			if (WideCharToMultiByte(CP_UTF8, 0, wsz, -1, msz, CTL_TEXT_BUF, NULL, NULL))
				result = msz;
		}
	}
	else {
		if (GetDlgItemTextA(hDlg, idItem, msz, CTL_TEXT_BUF))
			result = msz;
	}

	//MessageBox(GetFocus(),result.c_str(),"GetDlgItemText2():out",0);

	return result;
}

BOOL SetDlgItemText2(HWND hDlg, int idItem, LPCSTR pmsz) {
	BOOL bSuccess = FALSE;
	WCHAR wsz[CTL_TEXT_BUF];

	if (!pmsz || *pmsz == 0)
		return SetDlgItemTextA(hDlg, idItem, "");

	//MessageBox(GetFocus(),pmsz,"SetDlgItemText2():in",0);

	if (IsWindowUnicode(GetDlgItem(hDlg, idItem))) {
		if (MultiByteToWideChar(CP_UTF8, 0, pmsz, -1, wsz, CTL_TEXT_BUF)) {
			bSuccess = SetDlgItemTextW(hDlg, idItem, wsz);
		}
	}
	else {
		bSuccess = SetDlgItemTextA(hDlg, idItem, pmsz);
	}

	return bSuccess;
}

static SciTEWin *Caller(HWND hDlg, UINT message, LPARAM lParam) {
	if (message == WM_INITDIALOG) {
		SetWindowLong(hDlg, DWL_USER, lParam);
	}
	return reinterpret_cast<SciTEWin*>(GetWindowLong(hDlg, DWL_USER));
}

void SciTEWin::WarnUser(int warnID) {
	SString warning;
	char *warn;
	char flashDuration[10], sound[_MAX_PATH], soundDuration[10];

	switch (warnID) {
	case warnFindWrapped:
		warning = props.Get("warning.findwrapped");
		break;
	case warnNotFound:
		warning = props.Get("warning.notfound");
		break;
	case warnWrongFile:
		warning = props.Get("warning.wrongfile");
		break;
	case warnExecuteOK:
		warning = props.Get("warning.executeok");
		break;
	case warnExecuteKO:
		warning = props.Get("warning.executeko");
		break;
	case warnNoOtherBookmark:
		warning = props.Get("warning.nootherbookmark");
		break;
	default:
		warning = "";
		break;
	}
	warn = StringDup(warning.c_str());
	const char *next = GetNextPropItem(warn, flashDuration, 10);
	next = GetNextPropItem(next, sound, _MAX_PATH);
	GetNextPropItem(next, soundDuration, 10);
	delete []warn;

	int flashLen = atoi(flashDuration);
	if (flashLen) {
		FlashThisWindow(reinterpret_cast<HWND>(wEditor.GetID()), flashLen);
	}
	PlayThisSound(sound, atoi(soundDuration), hMM);
}

bool SciTEWin::ModelessHandler(MSG *pmsg) {
	if (wFindReplace.GetID()) {
		if (::IsDialogMessage(reinterpret_cast<HWND>(wFindReplace.GetID()), pmsg))
			return true;
	}
	if (wFindIncrement.GetID()) {
		if (::IsDialogMessage(reinterpret_cast<HWND>(wFindIncrement.GetID()), pmsg))
			return true;
	}
	if (wParameters.GetID()) {
		bool menuKey = (pmsg->message == WM_KEYDOWN) &&
		               (pmsg->wParam != VK_TAB) &&
		               (pmsg->wParam != VK_ESCAPE) &&
		               (pmsg->wParam != VK_RETURN) &&
		               (Platform::IsKeyDown(VK_CONTROL) || !Platform::IsKeyDown(VK_MENU));
		if (!menuKey && ::IsDialogMessage(reinterpret_cast<HWND>(wParameters.GetID()), pmsg))
			return true;
	}
	if (pmsg->message == WM_KEYDOWN || pmsg->message == WM_SYSKEYDOWN) {
		if (KeyDown(pmsg->wParam))
			return true;
	} else if (pmsg->message == WM_KEYUP) {
		if (KeyUp(pmsg->wParam))
			return true;
	}

	return false;
}

//  DoDialog is a bit like something in PC Magazine May 28, 1991, page 357
int SciTEWin::DoDialog(HINSTANCE hInst, const char *resName, HWND hWnd, DLGPROC lpProc) {
	int result = ::DialogBoxParam(hInst, resName, hWnd, lpProc, reinterpret_cast<LPARAM>(this));

	if (result == -1) {
		SString errorNum(::GetLastError());
		SString msg = LocaliseMessage("Failed to create dialog box: ^0.", errorNum.c_str());
		MessageBox(hWnd, msg.c_str(), appName, MB_OK | MB_SETFOREGROUND);
	}

	return result;
}

bool SciTEWin::OpenDialog(const char *filter) {

	char openName[2048]; // maximum common dialog buffer size (says mfc..)
	*openName = '\0';

	OPENFILENAME ofn = {
	                       sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	                   };
	ofn.hwndOwner = MainHWND();
	ofn.hInstance = hInstance;
	ofn.lpstrFile = openName;
	ofn.nMaxFile = sizeof(openName);
	SString openFilter;
	if (filter)
		openFilter = filter;
	else
		openFilter = props.GetExpanded("open.filter");
	if (openFilter.length()) {
		openFilter.substitute('|', '\0');
		size_t start = 0;
		while (start < openFilter.length()) {
			const char *filterName = openFilter.c_str() + start;
			SString localised = LocaliseString(filterName, false);
			if (localised.length()) {
				openFilter.remove(start, strlen(filterName));
				openFilter.insert(start, localised.c_str());
			}
			start += strlen(openFilter.c_str() + start) + 1;
			start += strlen(openFilter.c_str() + start) + 1;
		}
	}
	ofn.lpstrFilter = openFilter.c_str();
	if (!openWhat[0]) {
		strcpy(openWhat, LocaliseString("Custom Filter").c_str());
		openWhat[strlen(openWhat) + 1] = '\0';
	}
	ofn.lpstrCustomFilter = openWhat;
	ofn.nMaxCustFilter = sizeof(openWhat);
	ofn.nFilterIndex = filterDefault;
	SString translatedTitle = LocaliseString("Open File");
	ofn.lpstrTitle = translatedTitle.c_str();
	if (props.GetInt("open.dialog.in.file.directory")) {
		ofn.lpstrInitialDir = dirName;
	}
	ofn.Flags = OFN_HIDEREADONLY;

	if (buffers.size > 1) {
		ofn.Flags |=
		    OFN_EXPLORER |
		    OFN_PATHMUSTEXIST |
		    OFN_ALLOWMULTISELECT;
	}
	if (::GetOpenFileName(&ofn)) {
		filterDefault = ofn.nFilterIndex;
		//Platform::DebugPrintf("Open: <%s>\n", openName);
		// find char pos after first Delimiter
		char* p = openName;
		while (*p != '\0')
			++p;
		++p;
		// if single selection then have path+file
		if ((p - openName) > ofn.nFileOffset) {
			Open(openName);
		} else {
			SString path(openName);
			size_t len = path.length();
			if ((len > 0) && (path[len - 1] != '\\'))
				path += "\\";
			while (*p != '\0') {
				// make path+file, add it to the list
				SString file = path;
				file += p;
				Open(file.c_str());
				// goto next char pos after \0
				while (*p != '\0')
					++p;
				++p;
			}
		}
	} else {
		return false;
	}
	return true;
}

SString SciTEWin::ChooseSaveName(const char *title, const char *filter, const char *ext) {
	SString path;
	if (0 == dialogsOnScreen) {
		char saveName[MAX_PATH] = "";
		strcpy(saveName, fileName);
		if (ext) {
			char *cpDot = strrchr(saveName, '.');
			int keepExt = props.GetInt("export.keep.ext", 0);
			if (cpDot != NULL) {
				if (keepExt == 0) {
					strcpy(cpDot, ext);
				} else if (keepExt == 1) {
					strcat(saveName, ext);
				} else if (keepExt == 2) {
					*cpDot = '_';
					strcat(saveName, ext);
				}
			} else {
				strcat(saveName, ext);
			}
		}
		OPENFILENAME ofn = {
		                       sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		                   };
		ofn.hwndOwner = MainHWND();
		ofn.hInstance = hInstance;
		ofn.lpstrFile = saveName;
		ofn.nMaxFile = sizeof(saveName);
		SString translatedTitle = LocaliseString(title);
		ofn.lpstrTitle = translatedTitle.c_str();
		ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
		ofn.lpstrFilter = filter;
		ofn.lpstrInitialDir = dirName;

		dialogsOnScreen++;
		if (::GetSaveFileName(&ofn)) {
			path = saveName;
		}
		dialogsOnScreen--;
	}
	return path;
}

bool SciTEWin::SaveAsDialog() {
	SString path = ChooseSaveName("Save File");
	if (path.length()) {
		//Platform::DebugPrintf("Save: <%s>\n", openName);
		SetFileName(path.c_str(), false); // don't fix case
		Save();
		ReadProperties();

		// In case extension was changed
		SendEditor(SCI_COLOURISE, 0, -1);
		wEditor.InvalidateAll();
		if (extender)
			extender->OnSave(fullPath);
		return true;
	}
	return false;
}

void SciTEWin::SaveACopy() {
	SString path = ChooseSaveName("Save a Copy");
	if (path.length()) {
		SaveBuffer(path.c_str());
	}
}

void SciTEWin::SaveAsHTML() {
	SString path = ChooseSaveName("Export File As HTML",
	                              "Web (.html;.htm)\0*.html;*.htm\0", ".html");
	if (path.length()) {
		SaveToHTML(path.c_str());
	}
}

void SciTEWin::SaveAsRTF() {
	SString path = ChooseSaveName("Export File As RTF",
	                              "RTF (.rtf)\0*.rtf\0", ".rtf");
	if (path.length()) {
		SaveToRTF(path.c_str());
	}
}

void SciTEWin::SaveAsPDF() {
	SString path = ChooseSaveName("Export File As PDF",
	                              "PDF (.pdf)\0*.pdf\0", ".pdf");
	if (path.length()) {
		SaveToPDF(path.c_str());
	}
}

void SciTEWin::SaveAsTEX() {
	SString path = ChooseSaveName("Export File As TeX",
	                              "TeX (.tex)\0*.tex\0", ".tex");
	if (path.length()) {
		SaveToTEX(path.c_str());
	}
}

void SciTEWin::SaveAsXML() {
	SString path = ChooseSaveName("Export File As XML",
	                              "TeX (.xml)\0*.xml\0", ".xml");
	if (path.length()) {
		SaveToXML(path.c_str());
	}
}

void SciTEWin::LoadSessionDialog() {
	char openName[MAX_PATH] = "\0";
	OPENFILENAME ofn = {
	                       sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	                   };
	ofn.hwndOwner = MainHWND();
	ofn.hInstance = hInstance;
	ofn.lpstrFile = openName;
	ofn.nMaxFile = sizeof(openName);
	ofn.lpstrFilter = "Session (.ses)\0*.ses\0";
	SString translatedTitle = LocaliseString("Load Session");
	ofn.lpstrTitle = translatedTitle.c_str();
	ofn.Flags = OFN_HIDEREADONLY;
	if (::GetOpenFileName(&ofn))
		LoadSession(openName);
}

void SciTEWin::SaveSessionDialog() {
	char saveName[MAX_PATH] = "\0";
	strcpy(saveName, "SciTE.ses");
	OPENFILENAME ofn = {
			       sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			   };
	ofn.hwndOwner = MainHWND();
	ofn.hInstance = hInstance;
	ofn.lpstrFile = saveName;
	ofn.nMaxFile = sizeof(saveName);
	SString translatedTitle = LocaliseString("Save Current Session");
	ofn.lpstrTitle = translatedTitle.c_str();
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	ofn.lpstrFilter = "Session (.ses)\0*.ses\0";
	if (::GetSaveFileName(&ofn)) {
		SaveSession(saveName);
	}
}

/**
 * Display the Print dialog (if @a showDialog asks it),
 * allowing it to choose what to print on which printer.
 * If OK, print the user choice, with optionally defined header and footer.
 */
void SciTEWin::Print(
    bool showDialog) {	///< false if must print silently (using default settings).

	PRINTDLG pdlg = {
	                    sizeof(PRINTDLG), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	                };
	pdlg.hwndOwner = MainHWND();
	pdlg.hInstance = hInstance;
	pdlg.Flags = PD_USEDEVMODECOPIES | PD_ALLPAGES | PD_RETURNDC;
	pdlg.nFromPage = 1;
	pdlg.nToPage = 1;
	pdlg.nMinPage = 1;
	pdlg.nMaxPage = 0xffffU; // We do not know how many pages in the
	// document until the printer is selected and the paper size is known.
	pdlg.nCopies = 1;
	pdlg.hDC = 0;
	pdlg.hDevMode = hDevMode;
	pdlg.hDevNames = hDevNames;

	// See if a range has been selected
	CharacterRange crange = GetSelection();
	int startPos = crange.cpMin;
	int endPos = crange.cpMax;

	if (startPos == endPos) {
		pdlg.Flags |= PD_NOSELECTION;
	} else {
		pdlg.Flags |= PD_SELECTION;
	}
	if (!showDialog) {
		// Don't display dialog box, just use the default printer and options
		pdlg.Flags |= PD_RETURNDEFAULT;
	}
	if (!::PrintDlg(&pdlg)) {
		return;
	}

	hDevMode = pdlg.hDevMode;
	hDevNames = pdlg.hDevNames;

	HDC hdc = pdlg.hDC;

	PRectangle rectMargins, rectPhysMargins;
	Point ptPage;
	Point ptDpi;

	// Get printer resolution
	ptDpi.x = GetDeviceCaps(hdc, LOGPIXELSX);    // dpi in X direction
	ptDpi.y = GetDeviceCaps(hdc, LOGPIXELSY);    // dpi in Y direction

	// Start by getting the physical page size (in device units).
	ptPage.x = GetDeviceCaps(hdc, PHYSICALWIDTH);   // device units
	ptPage.y = GetDeviceCaps(hdc, PHYSICALHEIGHT);  // device units

	// Get the dimensions of the unprintable
	// part of the page (in device units).
	rectPhysMargins.left = GetDeviceCaps(hdc, PHYSICALOFFSETX);
	rectPhysMargins.top = GetDeviceCaps(hdc, PHYSICALOFFSETY);

	// To get the right and lower unprintable area,
	// we take the entire width and height of the paper and
	// subtract everything else.
	rectPhysMargins.right = ptPage.x						// total paper width
	                        - GetDeviceCaps(hdc, HORZRES) // printable width
	                        - rectPhysMargins.left;				// left unprintable margin

	rectPhysMargins.bottom = ptPage.y						// total paper height
	                         - GetDeviceCaps(hdc, VERTRES)	// printable height
	                         - rectPhysMargins.top;				// right unprintable margin

	// At this point, rectPhysMargins contains the widths of the
	// unprintable regions on all four sides of the page in device units.

	// Take in account the page setup given by the user (if one value is not null)
	if (pagesetupMargin.left != 0 || pagesetupMargin.right != 0 ||
	        pagesetupMargin.top != 0 || pagesetupMargin.bottom != 0) {
		PRectangle rectSetup;

		// Convert the hundredths of millimeters (HiMetric) or
		// thousandths of inches (HiEnglish) margin values
		// from the Page Setup dialog to device units.
		// (There are 2540 hundredths of a mm in an inch.)

		char localeInfo[3];
		GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, localeInfo, 3);

		if (localeInfo[0] == '0') {	// Metric system. '1' is US System
			rectSetup.left = MulDiv (pagesetupMargin.left, ptDpi.x, 2540);
			rectSetup.top = MulDiv (pagesetupMargin.top, ptDpi.y, 2540);
			rectSetup.right	= MulDiv(pagesetupMargin.right, ptDpi.x, 2540);
			rectSetup.bottom	= MulDiv(pagesetupMargin.bottom, ptDpi.y, 2540);
		} else {
			rectSetup.left	= MulDiv(pagesetupMargin.left, ptDpi.x, 1000);
			rectSetup.top	= MulDiv(pagesetupMargin.top, ptDpi.y, 1000);
			rectSetup.right	= MulDiv(pagesetupMargin.right, ptDpi.x, 1000);
			rectSetup.bottom	= MulDiv(pagesetupMargin.bottom, ptDpi.y, 1000);
		}

		// Dont reduce margins below the minimum printable area
		rectMargins.left	= Platform::Maximum(rectPhysMargins.left, rectSetup.left);
		rectMargins.top	= Platform::Maximum(rectPhysMargins.top, rectSetup.top);
		rectMargins.right	= Platform::Maximum(rectPhysMargins.right, rectSetup.right);
		rectMargins.bottom	= Platform::Maximum(rectPhysMargins.bottom, rectSetup.bottom);
	} else {
		rectMargins.left	= rectPhysMargins.left;
		rectMargins.top	= rectPhysMargins.top;
		rectMargins.right	= rectPhysMargins.right;
		rectMargins.bottom	= rectPhysMargins.bottom;
	}

	// rectMargins now contains the values used to shrink the printable
	// area of the page.

	// Convert device coordinates into logical coordinates
	DPtoLP(hdc, (LPPOINT) &rectMargins, 2);
	DPtoLP(hdc, (LPPOINT)&rectPhysMargins, 2);

	// Convert page size to logical units and we're done!
	DPtoLP(hdc, (LPPOINT) &ptPage, 1);

	SString headerFormat = props.Get("print.header.format");
	SString footerFormat = props.Get("print.footer.format");

	TEXTMETRIC tm;
	SString headerOrFooter;	// Usually the path, date and page number

	SString headerStyle = props.Get("print.header.style");
	StyleDefinition sdHeader(headerStyle.c_str());

	int headerLineHeight = ::MulDiv(
	                           (sdHeader.specified & StyleDefinition::sdSize) ? sdHeader.size : 9,
	                           ptDpi.y, 72);
	HFONT fontHeader = ::CreateFont(headerLineHeight,
	                                0, 0, 0,
	                                sdHeader.bold ? FW_BOLD : FW_NORMAL,
	                                sdHeader.italics,
	                                sdHeader.underlined,
	                                0, 0, 0,
	                                0, 0, 0,
	                                (sdHeader.specified & StyleDefinition::sdFont) ? sdHeader.font.c_str() : "Arial");
	::SelectObject(hdc, fontHeader);
	::GetTextMetrics(hdc, &tm);
	headerLineHeight = tm.tmHeight + tm.tmExternalLeading;

	SString footerStyle = props.Get("print.footer.style");
	StyleDefinition sdFooter(footerStyle.c_str());

	int footerLineHeight = ::MulDiv(
	                           (sdFooter.specified & StyleDefinition::sdSize) ? sdFooter.size : 9,
	                           ptDpi.y, 72);
	HFONT fontFooter = ::CreateFont(footerLineHeight,
	                                0, 0, 0,
	                                sdFooter.bold ? FW_BOLD : FW_NORMAL,
	                                sdFooter.italics,
	                                sdFooter.underlined,
	                                0, 0, 0,
	                                0, 0, 0,
	                                (sdFooter.specified & StyleDefinition::sdFont) ? sdFooter.font.c_str() : "Arial");
	::SelectObject(hdc, fontFooter);
	::GetTextMetrics(hdc, &tm);
	footerLineHeight = tm.tmHeight + tm.tmExternalLeading;

	DOCINFO di = {sizeof(DOCINFO), 0, 0, 0, 0};
	di.lpszDocName = windowName.c_str();
	di.lpszOutput = 0;
	di.lpszDatatype = 0;
	di.fwType = 0;
	if (::StartDoc(hdc, &di) < 0) {
		SString msg = LocaliseMessage("Can not start printer document.");
		WindowMessageBox(wSciTE, msg, MB_OK);
		return;
	}

	LONG lengthDoc = SendEditor(SCI_GETLENGTH);
	LONG lengthDocMax = lengthDoc;
	LONG lengthPrinted = 0;

	// Requested to print selection
	if (pdlg.Flags & PD_SELECTION) {
		if (startPos > endPos) {
			lengthPrinted = endPos;
			lengthDoc = startPos;
		} else {
			lengthPrinted = startPos;
			lengthDoc = endPos;
		}

		if (lengthPrinted < 0)
			lengthPrinted = 0;
		if (lengthDoc > lengthDocMax)
			lengthDoc = lengthDocMax;
	}

	// We must substract the physical margins from the printable area
	RangeToFormat frPrint;
	frPrint.hdc = hdc;
	frPrint.hdcTarget = hdc;
	frPrint.rc.left = rectMargins.left - rectPhysMargins.left;
	frPrint.rc.top = rectMargins.top - rectPhysMargins.top;
	frPrint.rc.right = ptPage.x - rectMargins.right - rectPhysMargins.left;
	frPrint.rc.bottom = ptPage.y - rectMargins.bottom - rectPhysMargins.top;
	frPrint.rcPage.left = 0;
	frPrint.rcPage.top = 0;
	frPrint.rcPage.right = ptPage.x - rectPhysMargins.left - rectPhysMargins.right - 1;
	frPrint.rcPage.bottom = ptPage.y - rectPhysMargins.top - rectPhysMargins.bottom - 1;
	if (headerFormat.size()) {
		frPrint.rc.top += headerLineHeight + headerLineHeight / 2;
	}
	if (footerFormat.size()) {
		frPrint.rc.bottom -= footerLineHeight + footerLineHeight / 2;
	}
	// Print each page
	int pageNum = 1;
	bool printPage;
	PropSet propsPrint;
	propsPrint.superPS = &props;
	SetFileProperties(propsPrint);

	while (lengthPrinted < lengthDoc) {
		printPage = (!(pdlg.Flags & PD_PAGENUMS) ||
		             (pageNum >= pdlg.nFromPage) && (pageNum <= pdlg.nToPage));

		char pageString[32];
		sprintf(pageString, "%0d", pageNum);
		propsPrint.Set("CurrentPage", pageString);

		if (printPage) {
			::StartPage(hdc);

			if (headerFormat.size()) {
				SString sHeader = propsPrint.GetExpanded("print.header.format");
				::SetTextColor(hdc, sdHeader.ForeAsLong());
				::SetBkColor(hdc, sdHeader.BackAsLong());
				::SelectObject(hdc, fontHeader);
				UINT ta = ::SetTextAlign(hdc, TA_BOTTOM);
				RECT rcw = {frPrint.rc.left, frPrint.rc.top - headerLineHeight - headerLineHeight / 2,
				            frPrint.rc.right, frPrint.rc.top - headerLineHeight / 2};
				rcw.bottom = rcw.top + headerLineHeight;
				::ExtTextOut(hdc, frPrint.rc.left + 5, frPrint.rc.top - headerLineHeight / 2,
				             ETO_OPAQUE, &rcw, sHeader.c_str(),
				             static_cast<int>(sHeader.length()), NULL);
				::SetTextAlign(hdc, ta);
				HPEN pen = ::CreatePen(0, 1, sdHeader.ForeAsLong());
				HPEN penOld = static_cast<HPEN>(::SelectObject(hdc, pen));
				::MoveToEx(hdc, frPrint.rc.left, frPrint.rc.top - headerLineHeight / 4, NULL);
				::LineTo(hdc, frPrint.rc.right, frPrint.rc.top - headerLineHeight / 4);
				::SelectObject(hdc, penOld);
				::DeleteObject(pen);
			}
		}

		frPrint.chrg.cpMin = lengthPrinted;
		frPrint.chrg.cpMax = lengthDoc;

		lengthPrinted = SendEditor(SCI_FORMATRANGE,
		                           printPage,
		                           reinterpret_cast<LPARAM>(&frPrint));

		if (printPage) {
			if (footerFormat.size()) {
				SString sFooter = propsPrint.GetExpanded("print.footer.format");
				::SetTextColor(hdc, sdFooter.ForeAsLong());
				::SetBkColor(hdc, sdFooter.BackAsLong());
				::SelectObject(hdc, fontFooter);
				UINT ta = ::SetTextAlign(hdc, TA_TOP);
				RECT rcw = {frPrint.rc.left, frPrint.rc.bottom + footerLineHeight / 2,
				            frPrint.rc.right, frPrint.rc.bottom + footerLineHeight + footerLineHeight / 2};
				::ExtTextOut(hdc, frPrint.rc.left + 5, frPrint.rc.bottom + footerLineHeight / 2,
				             ETO_OPAQUE, &rcw, sFooter.c_str(),
				             static_cast<int>(sFooter.length()), NULL);
				::SetTextAlign(hdc, ta);
				HPEN pen = ::CreatePen(0, 1, sdFooter.ForeAsLong());
				HPEN penOld = static_cast<HPEN>(::SelectObject(hdc, pen));
				::SetBkColor(hdc, sdFooter.ForeAsLong());
				::MoveToEx(hdc, frPrint.rc.left, frPrint.rc.bottom + footerLineHeight / 4, NULL);
				::LineTo(hdc, frPrint.rc.right, frPrint.rc.bottom + footerLineHeight / 4);
				::SelectObject(hdc, penOld);
				::DeleteObject(pen);
			}

			::EndPage(hdc);
		}
		pageNum++;

		if ((pdlg.Flags & PD_PAGENUMS) && (pageNum > pdlg.nToPage))
			break;
	}

	SendEditor(SCI_FORMATRANGE, FALSE, 0);

	::EndDoc(hdc);
	::DeleteDC(hdc);
	if (fontHeader) {
		::DeleteObject(fontHeader);
	}
	if (fontFooter) {
		::DeleteObject(fontFooter);
	}
}

void SciTEWin::PrintSetup() {
	PAGESETUPDLG pdlg = {
	                        sizeof(PAGESETUPDLG), 0, 0, 0, 0, {0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, 0, 0, 0, 0, 0, 0
	                    };

	pdlg.hwndOwner = MainHWND();
	pdlg.hInstance = hInstance;

	if (pagesetupMargin.left != 0 || pagesetupMargin.right != 0 ||
	        pagesetupMargin.top != 0 || pagesetupMargin.bottom != 0) {
		pdlg.Flags = PSD_MARGINS;

		pdlg.rtMargin.left = pagesetupMargin.left;
		pdlg.rtMargin.top = pagesetupMargin.top;
		pdlg.rtMargin.right = pagesetupMargin.right;
		pdlg.rtMargin.bottom = pagesetupMargin.bottom;
	}

	pdlg.hDevMode = hDevMode;
	pdlg.hDevNames = hDevNames;

	if (!PageSetupDlg(&pdlg))
		return;

	pagesetupMargin.left = pdlg.rtMargin.left;
	pagesetupMargin.top = pdlg.rtMargin.top;
	pagesetupMargin.right = pdlg.rtMargin.right;
	pagesetupMargin.bottom = pdlg.rtMargin.bottom;

	hDevMode = pdlg.hDevMode;
	hDevNames = pdlg.hDevNames;
}

// IsWindowsNT() introduced because IsNT() did not work
extern bool IsWindowsNT();

static void FillComboFromMemory(HWND combo, const ComboMemory &mem, bool useTop = false) {
	if (IsWindowUnicode(combo)) {
		for (int i = 0; i < mem.Length(); i++) {
			//Platform::DebugPrintf("Combo[%0d] = %s\n", i, mem.At(i).c_str());
			WCHAR wszBuf[CTL_TEXT_BUF];
			MultiByteToWideChar(CP_UTF8, 0, mem.At(i).c_str(), -1, wszBuf,
			                    CTL_TEXT_BUF);
			::SendMessageW(combo, CB_ADDSTRING, 0,
			               reinterpret_cast<LPARAM>(wszBuf));
		}
	} else {
		for (int i = 0; i < mem.Length(); i++) {
			//Platform::DebugPrintf("Combo[%0d] = %s\n", i, mem.At(i).c_str());
			::SendMessage(combo, CB_ADDSTRING, 0,
			              reinterpret_cast<LPARAM>(mem.At(i).c_str()));
		}
	}
	if (useTop) {
		::SendMessage(combo, CB_SETCURSEL, 0, 0);
	}
}

static void FillComboFromProps(HWND combo, PropSet &props) {
	char *key;
	char *val;
	if (props.GetFirst(&key, &val)) {
		::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(key));
		while (props.GetNext(&key, &val)) {
			::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(key));
		}
	}
}

static SString GetItemText(HWND hDlg, int id) {
	HWND wT = ::GetDlgItem(hDlg, id);
	int len = ::GetWindowTextLength(wT);
	SBuffer itemText(len);
	if (len > 0) {
		::GetDlgItemText(hDlg, id, itemText.ptr(), len + 1);
	}
	return SString(itemText);
}

BOOL SciTEWin::FindMessage(HWND hDlg, UINT message, WPARAM wParam) {
	// Avoid getting dialog items before set up or during tear down.
	if (WM_SETFONT == message || WM_NCDESTROY == message)
		return FALSE;
	HWND wFindWhat = ::GetDlgItem(hDlg, IDFINDWHAT);
	HWND wWholeWord = ::GetDlgItem(hDlg, IDWHOLEWORD);
	HWND wMatchCase = ::GetDlgItem(hDlg, IDMATCHCASE);
	HWND wRegExp = ::GetDlgItem(hDlg, IDREGEXP);
	HWND wWrap = ::GetDlgItem(hDlg, IDWRAP);
	HWND wUnSlash = ::GetDlgItem(hDlg, IDUNSLASH);
	HWND wUp = ::GetDlgItem(hDlg, IDDIRECTIONUP);
	HWND wDown = ::GetDlgItem(hDlg, IDDIRECTIONDOWN);

	switch (message) {

	case WM_INITDIALOG:
		LocaliseDialog(hDlg);
		SetDlgItemText2(hDlg, IDFINDWHAT, findWhat.c_str());
		FillComboFromMemory(wFindWhat, memFinds);
		if (wholeWord)
			::SendMessage(wWholeWord, BM_SETCHECK, BST_CHECKED, 0);
		if (matchCase)
			::SendMessage(wMatchCase, BM_SETCHECK, BST_CHECKED, 0);
		if (regExp)
			::SendMessage(wRegExp, BM_SETCHECK, BST_CHECKED, 0);
		if (wrapFind)
			::SendMessage(wWrap, BM_SETCHECK, BST_CHECKED, 0);
		if (unSlash)
			::SendMessage(wUnSlash, BM_SETCHECK, BST_CHECKED, 0);
		if (reverseFind) {
			::SendMessage(wUp, BM_SETCHECK, BST_CHECKED, 0);
		} else {
			::SendMessage(wDown, BM_SETCHECK, BST_CHECKED, 0);
		}
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			wFindReplace.Destroy();
			wFindReplace = 0;
			return FALSE;
		} else if ( (ControlIDOfCommand(wParam) == IDOK) ||
		            (ControlIDOfCommand(wParam) == IDMARKALL) ) {
			findWhat = GetDlgItemText2(hDlg, IDFINDWHAT);
			props.Set("find.what", findWhat.c_str());
			memFinds.Insert(findWhat.c_str());
			wholeWord = BST_CHECKED ==
			            ::SendMessage(wWholeWord, BM_GETCHECK, 0, 0);
			matchCase = BST_CHECKED ==
			            ::SendMessage(wMatchCase, BM_GETCHECK, 0, 0);
			regExp = BST_CHECKED ==
			         ::SendMessage(wRegExp, BM_GETCHECK, 0, 0);
			wrapFind = BST_CHECKED ==
			           ::SendMessage(wWrap, BM_GETCHECK, 0, 0);
			unSlash = BST_CHECKED ==
			          ::SendMessage(wUnSlash, BM_GETCHECK, 0, 0);
			reverseFind = BST_CHECKED ==
			              ::SendMessage(wUp, BM_GETCHECK, 0, 0);
			::EndDialog(hDlg, IDOK);
			wFindReplace.Destroy();
			wFindReplace = 0;
			if (ControlIDOfCommand(wParam) == IDMARKALL){
				MarkAll();
			}
			FindNext(reverseFind);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::FindDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->FindMessage(hDlg, message, wParam);
}

BOOL SciTEWin::HandleReplaceCommand(int cmd) {
	if (!wFindReplace.GetID())
		return TRUE;
	HWND hwndFR = reinterpret_cast<HWND>(wFindReplace.GetID());
	HWND wWholeWord = ::GetDlgItem(hwndFR, IDWHOLEWORD);
	HWND wMatchCase = ::GetDlgItem(hwndFR, IDMATCHCASE);
	HWND wRegExp = ::GetDlgItem(hwndFR, IDREGEXP);
	HWND wWrap = ::GetDlgItem(hwndFR, IDWRAP);
	HWND wUnSlash = ::GetDlgItem(hwndFR, IDUNSLASH);

	if ((cmd == IDOK) || (cmd == IDREPLACE) || (cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL) || (cmd == IDREPLACEINBUF)) {
		findWhat = GetDlgItemText2(hwndFR, IDFINDWHAT);
		props.Set("find.what", findWhat.c_str());
		memFinds.Insert(findWhat.c_str());
		wholeWord = BST_CHECKED ==
		            ::SendMessage(wWholeWord, BM_GETCHECK, 0, 0);
		matchCase = BST_CHECKED ==
		            ::SendMessage(wMatchCase, BM_GETCHECK, 0, 0);
		regExp = BST_CHECKED ==
		         ::SendMessage(wRegExp, BM_GETCHECK, 0, 0);
		wrapFind = BST_CHECKED ==
		           ::SendMessage(wWrap, BM_GETCHECK, 0, 0);
		unSlash = BST_CHECKED ==
		          ::SendMessage(wUnSlash, BM_GETCHECK, 0, 0);
	}
	if ((cmd == IDREPLACE) || (cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL) || (cmd == IDREPLACEINBUF)) {
		replaceWhat = GetDlgItemText2(hwndFR, IDREPLACEWITH);
		memReplaces.Insert(replaceWhat.c_str());
	}

	int replacements = 0;
	if (cmd == IDOK) {
		FindNext(reverseFind);	// Find next
	} else if (cmd == IDREPLACE) {
		if (havefound) {
			ReplaceOnce();
		} else {
			CharacterRange crange = GetSelection();
			SetSelection(crange.cpMin, crange.cpMin);
			FindNext(reverseFind);
			if (havefound) {
				ReplaceOnce();
			}
		}
	} else if ((cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL)) {
		replacements = ReplaceAll(cmd == IDREPLACEINSEL);
	} else if (cmd == IDREPLACEINBUF) {
		replacements = ReplaceInBuffers();
	}
	char replDone[10];
	sprintf(replDone, "%d", replacements);
	::SetDlgItemText(hwndFR, IDREPLDONE, replDone);

	return TRUE;
}

BOOL SciTEWin::ReplaceMessage(HWND hDlg, UINT message, WPARAM wParam) {
	// Avoid getting dialog items before set up or during tear down.
	if (WM_SETFONT == message || WM_NCDESTROY == message)
		return FALSE;
	HWND wFindWhat = ::GetDlgItem(hDlg, IDFINDWHAT);
	HWND wReplaceWith = ::GetDlgItem(hDlg, IDREPLACEWITH);
	HWND wWholeWord = ::GetDlgItem(hDlg, IDWHOLEWORD);
	HWND wMatchCase = ::GetDlgItem(hDlg, IDMATCHCASE);
	HWND wRegExp = ::GetDlgItem(hDlg, IDREGEXP);
	HWND wWrap = ::GetDlgItem(hDlg, IDWRAP);
	HWND wUnSlash = ::GetDlgItem(hDlg, IDUNSLASH);

	switch (message) {

	case WM_INITDIALOG:
		LocaliseDialog(hDlg);
		SetDlgItemText2(hDlg, IDFINDWHAT, findWhat.c_str());
		FillComboFromMemory(wFindWhat, memFinds);
		SetDlgItemText2(hDlg, IDREPLACEWITH, replaceWhat.c_str());
		FillComboFromMemory(wReplaceWith, memReplaces);
		if (wholeWord)
			::SendMessage(wWholeWord, BM_SETCHECK, BST_CHECKED, 0);
		if (matchCase)
			::SendMessage(wMatchCase, BM_SETCHECK, BST_CHECKED, 0);
		if (regExp)
			::SendMessage(wRegExp, BM_SETCHECK, BST_CHECKED, 0);
		if (wrapFind)
			::SendMessage(wWrap, BM_SETCHECK, BST_CHECKED, 0);
		if (unSlash)
			::SendMessage(wUnSlash, BM_SETCHECK, BST_CHECKED, 0);
		::SetDlgItemText(hDlg, IDREPLDONE, "0");
		if (findWhat.length() != 0 && props.GetInt("find.replacewith.focus", 1)) {
			::SetFocus(wReplaceWith);
			return FALSE;
		}
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			props.Set("Replacements", "");
			UpdateStatusBar(false);
			::EndDialog(hDlg, IDCANCEL);
			wFindReplace.Destroy();
			wFindReplace = 0;
			return FALSE;
		} else {
			return HandleReplaceCommand(ControlIDOfCommand(wParam));
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::ReplaceDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->ReplaceMessage(hDlg, message, wParam);
}

BOOL SciTEWin::IncrementFindMessage(HWND hDlg, UINT message, WPARAM wParam) {
	// Avoid getting dialog items before set up or during tear down.
	if (WM_SETFONT == message || WM_NCDESTROY == message)
		return FALSE;
	HWND wFindWhat = ::GetDlgItem(hDlg, IDC_INCFINDTEXT);

	switch (message) {

	case WM_INITDIALOG:{
		wFindIncrement = hDlg;
		LocaliseDialog(hDlg);
		SetWindowLong(hDlg, GWL_STYLE, WS_TABSTOP || GetWindowLong(hDlg, GWL_STYLE));
		SetDlgItemText2(hDlg, IDC_INCFINDTEXT, ""); //findWhat.c_str()
		SetFocus(hDlg);

		PRectangle aRect = wFindIncrement.GetPosition();
		PRectangle aTBRect = wStatusBar.GetPosition();
		PRectangle aNewRect = aTBRect;
		aNewRect.top = aNewRect.bottom - (aRect.bottom - aRect.top);
		aNewRect.right = aNewRect.left + aRect.right - aRect.left;
		//~ RegisterHotKey(hDlg,1,0,VK_F3);
		//~ RegisterHotKey(hDlg,2,MOD_SHIFT,VK_F3);

		wFindIncrement.SetPosition(aNewRect);


		return TRUE;
	}

	case WM_SETFOCUS:
		return 0;

	case WM_HOTKEY:
		if (wParam == 1)
			FindNext(false,false);
		if (wParam == 2)
			FindNext(true,false);
		break;

	case WM_CLOSE:
		//~ UnregisterHotKey(hDlg,1);
		//~ UnregisterHotKey(hDlg,2);
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			props.Set("Replacements", "");
			UpdateStatusBar(false);
			::EndDialog(hDlg, IDCANCEL);
			wFindIncrement.Destroy();
			wFindIncrement = 0;
			return FALSE;
		} else if (((ControlIDOfCommand(wParam) == IDC_INCFINDTEXT) && ((wParam >> 16) == 0x0300))
			|| (ControlIDOfCommand(wParam) == IDC_INCFINDBTNOK)) {
			SString ffLastWhat;
			ffLastWhat = findWhat;
			findWhat = GetDlgItemText2(hDlg, IDC_INCFINDTEXT);

			if (ControlIDOfCommand(wParam) != IDC_INCFINDBTNOK) {
				CharacterRange cr = GetSelection();
				if (ffLastWhat.length()) {
					SetSelection(cr.cpMin - ffLastWhat.length(), cr.cpMin - ffLastWhat.length());
				}
			}
			wholeWord = false;
			FindNext(false, false);
			if ((!havefound) && (ffLastWhat.length() == findWhat.length()-1) &&
				strncmp(findWhat.c_str(), ffLastWhat.c_str(), ffLastWhat.length()) == 0) {
				findWhat = ffLastWhat;
				SetDlgItemText2(hDlg, IDC_INCFINDTEXT, findWhat.c_str());
				SendMessage(wFindWhat, EM_SETSEL, ffLastWhat.length(), ffLastWhat.length());
			}
			return FALSE;
		}
	}

	return FALSE;
}


BOOL CALLBACK SciTEWin::FindIncrementDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->IncrementFindMessage(hDlg, message, wParam);
}

void SciTEWin::FindIncrement() {
	if (wFindIncrement.Created())
		return;

	memset(&fr, 0, sizeof(fr));
	fr.lStructSize = sizeof(fr);
	fr.hwndOwner = MainHWND();
	fr.hInstance = hInstance;
	fr.Flags = 0;
	if (!reverseFind)
		fr.Flags |= FR_DOWN;
	findWhat.clear();
	fr.lpstrFindWhat = const_cast<char *>(findWhat.c_str());
	fr.wFindWhatLen = static_cast<WORD>(findWhat.length() + 1);

	replacing = false;
	//DoDialog(hInstance,
	//		 MAKEINTRESOURCE(IDD_FIND2),
	//		 MainHWND(),
	//		 reinterpret_cast<DLGPROC>(FindIncrementDlg));
	if (IsWindowsNT()) {
		DialogBoxParamW(hInstance, (LPCWSTR)MAKEINTRESOURCE(IDD_FIND2),
		                MainHWND(), reinterpret_cast<DLGPROC>(FindIncrementDlg),
		                reinterpret_cast<LPARAM>(this));
	} else {
		DialogBoxParamA(hInstance, MAKEINTRESOURCE(IDD_FIND2),
		                MainHWND(), reinterpret_cast<DLGPROC>(FindIncrementDlg),
		                reinterpret_cast<LPARAM>(this));
	}
	WindowSetFocus(wEditor);
}

void SciTEWin::Find() {
	if (wFindIncrement.Created())
		return;
	if (wFindReplace.Created())
		return;
	SelectionIntoFind();

	memset(&fr, 0, sizeof(fr));
	fr.lStructSize = sizeof(fr);
	fr.hwndOwner = MainHWND();
	fr.hInstance = hInstance;
	fr.Flags = 0;
	if (!reverseFind)
		fr.Flags |= FR_DOWN;
	fr.lpstrFindWhat = const_cast<char *>(findWhat.c_str());
	fr.wFindWhatLen = static_cast<WORD>(findWhat.length() + 1);

	if (IsWindowsNT()) {
		wFindReplace = ::CreateDialogParamW(hInstance,
		                                    (LPCWSTR)MAKEINTRESOURCE(IDD_FIND),
		                                    MainHWND(),
		                                    reinterpret_cast<DLGPROC>(FindDlg),
		                                    reinterpret_cast<LPARAM>(this));
	} else {
		wFindReplace = ::CreateDialogParamA(hInstance,
		                                    MAKEINTRESOURCE(IDD_FIND),
		                                    MainHWND(),
		                                    reinterpret_cast<DLGPROC>(FindDlg),
		                                    reinterpret_cast<LPARAM>(this));
	}
	wFindReplace.Show();

	replacing = false;
}

// Set a call back with the handle after init to set the path.
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/reference/callbackfunctions/browsecallbackproc.asp

static int __stdcall BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM, LPARAM pData) {
	if (uMsg == BFFM_INITIALIZED) {
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
	}
	return 0;
}

BOOL SciTEWin::GrepMessage(HWND hDlg, UINT message, WPARAM wParam) {
	HWND hFindWhat;
	HWND hFiles;
	HWND hDirectory;

	switch (message) {

	case WM_INITDIALOG:
		LocaliseDialog(hDlg);
		::SetDlgItemText(hDlg, IDFINDWHAT, props.Get("find.what").c_str());
		hFindWhat = ::GetDlgItem(hDlg, IDFINDWHAT);
		FillComboFromMemory(hFindWhat, memFinds);
		hFiles = ::GetDlgItem(hDlg, IDFILES);
		FillComboFromMemory(hFiles, memFiles, true);
		::SetDlgItemText(hDlg, IDDIRECTORY, props.Get("find.directory").c_str());
		hDirectory = ::GetDlgItem(hDlg, IDDIRECTORY);
		FillComboFromMemory(hDirectory, memDirectory);
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			return FALSE;

		} else if (ControlIDOfCommand(wParam) == IDOK) {
			findWhat = GetItemText(hDlg, IDFINDWHAT);
			props.Set("find.what", findWhat.c_str());
			memFinds.Insert(findWhat.c_str());

			SString files = GetItemText(hDlg, IDFILES);
			props.Set("find.files", files.c_str());
			memFiles.Insert(files.c_str());

			SString directory = GetItemText(hDlg, IDDIRECTORY);
			props.Set("find.directory", directory.c_str());
			memDirectory.Insert(directory.c_str());

			::EndDialog(hDlg, IDOK);
			return TRUE;

		} else if (ControlIDOfCommand(wParam) == IDBROWSE) {

			// This code was copied and slightly modifed from:
			// http://www.bcbdev.com/faqs/faq62.htm

			// SHBrowseForFolder returns a PIDL. The memory for the PIDL is
			// allocated by the shell. Eventually, we will need to free this
			// memory, so we need to get a pointer to the shell malloc COM
			// object that will free the PIDL later on.
			LPMALLOC pShellMalloc = 0;
			if (::SHGetMalloc(&pShellMalloc) == NO_ERROR) {
				// If we were able to get the shell malloc object,
				// then proceed by initializing the BROWSEINFO stuct
				BROWSEINFO info;
				memset(&info, 0, sizeof(info));
				info.hwndOwner = hDlg;
				info.pidlRoot = NULL;
				char szDisplayName[MAX_PATH];
				info.pszDisplayName = szDisplayName;
				SString title = LocaliseString("Select a folder to search from");
				info.lpszTitle = title.c_str();
				info.ulFlags = 0;
				info.lpfn = BrowseCallbackProc;
				SString directory = GetItemText(hDlg, IDDIRECTORY);
				info.lParam = reinterpret_cast<LPARAM>(directory.c_str());

				// Execute the browsing dialog.
				LPITEMIDLIST pidl = ::SHBrowseForFolder(&info);

				// pidl will be null if they cancel the browse dialog.
				// pidl will be not null when they select a folder.
				if (pidl) {
					// Try to convert the pidl to a display string.
					// Return is true if success.
					char szDir[MAX_PATH];
					if (::SHGetPathFromIDList(pidl, szDir)) {
						// Set edit control to the directory path.
						::SetDlgItemText(hDlg, IDDIRECTORY, szDir);
					}
					pShellMalloc->Free(pidl);
				}
				pShellMalloc->Release();
			}
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::GrepDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->GrepMessage(hDlg, message, wParam);
}

void SciTEWin::FindInFiles() {
	SelectionIntoFind();
	props.Set("find.what", findWhat.c_str());
	char findInDir[1024];
	GetDocumentDirectory(findInDir, sizeof(findInDir));
	props.Set("find.directory", findInDir);
	if (DoDialog(hInstance, "Grep", MainHWND(), reinterpret_cast<DLGPROC>(GrepDlg)) == IDOK) {
		//Platform::DebugPrintf("asked to find %s %s %s\n", props.Get("find.what"), props.Get("find.files"), props.Get("find.directory"));
		SelectionIntoProperties();

		SString findInput;
		long flags = 0;
		if (props.Get("find.input").length()) {
			findInput = props.GetNewExpand("find.input");
			flags += jobHasInput;
		}

		AddCommand(props.GetNewExpand("find.command"),
		           props.Get("find.directory"),
		           jobCLI, findInput, flags);
		if (commandCurrent > 0)
			Execute();
	}
}

void SciTEWin::Replace() {
	if (wFindReplace.Created())
		return;
	SelectionIntoFind(false); // don't strip EOL at end of selection

	memset(&fr, 0, sizeof(fr));
	fr.lStructSize = sizeof(fr);
	fr.hwndOwner = MainHWND();
	fr.hInstance = hInstance;
	fr.Flags = FR_REPLACE;
	fr.lpstrFindWhat = const_cast<char *>(findWhat.c_str());
	fr.lpstrReplaceWith = const_cast<char *>(replaceWhat.c_str());
	fr.wFindWhatLen = static_cast<WORD>(findWhat.length() + 1);
	fr.wReplaceWithLen = static_cast<WORD>(replaceWhat.length() + 1);

	if (IsWindowsNT()) {
		wFindReplace = ::CreateDialogParamW(hInstance,
		                                    (LPCWSTR)MAKEINTRESOURCE(IDD_REPLACE),
		                                    MainHWND(),
		                                    reinterpret_cast<DLGPROC>(ReplaceDlg),
		                                    reinterpret_cast<sptr_t>(this));
	} else {
		wFindReplace = ::CreateDialogParamA(hInstance,
		                                    MAKEINTRESOURCE(IDD_REPLACE),
		                                    MainHWND(),
		                                    reinterpret_cast<DLGPROC>(ReplaceDlg),
		                                    reinterpret_cast<sptr_t>(this));
	}
	wFindReplace.Show();

	replacing = true;
	havefound = false;
}

void SciTEWin::FindReplace(bool replace) {
	replacing = replace;
}

void SciTEWin::DestroyFindReplace() {
	if (wFindReplace.Created()) {
		::EndDialog(reinterpret_cast<HWND>(wFindReplace.GetID()), IDCANCEL);
		wFindReplace.Destroy();
		wFindReplace = 0;
	}
}

BOOL SciTEWin::GoLineMessage(HWND hDlg, UINT message, WPARAM wParam) {
	switch (message) {

	case WM_INITDIALOG: {
			int position = SendEditor(SCI_GETCURRENTPOS);
			int lineNumber = SendEditor(SCI_LINEFROMPOSITION, position) + 1;
			int lineStart = SendEditor(SCI_POSITIONFROMLINE, lineNumber - 1);
			int characterOnLine = 1;
			while (position > lineStart) {
				position = SendEditor(SCI_POSITIONBEFORE, position);
				characterOnLine++;
			}

			LocaliseDialog(hDlg);
			::SendDlgItemMessage(hDlg, IDGOLINE, EM_LIMITTEXT, 10, 1);
			::SendDlgItemMessage(hDlg, IDGOLINECHAR, EM_LIMITTEXT, 10, 1);
			::SetDlgItemInt(hDlg, IDCURRLINE, lineNumber, FALSE);
			::SetDlgItemInt(hDlg, IDCURRLINECHAR, characterOnLine, FALSE);
			::SetDlgItemInt(hDlg, IDLASTLINE, SendEditor(SCI_GETLINECOUNT), FALSE);
                }
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			return FALSE;
		} else if (ControlIDOfCommand(wParam) == IDOK) {
			BOOL bHasLine;
			int lineNumber = static_cast<int>(
			                     ::GetDlgItemInt(hDlg, IDGOLINE, &bHasLine, FALSE));
			BOOL bHasChar;
			int characterOnLine = static_cast<int>(
			                     ::GetDlgItemInt(hDlg, IDGOLINECHAR, &bHasChar, FALSE));

			if (bHasLine || bHasChar) {
				if (!bHasLine)
					lineNumber = SendEditor(SCI_LINEFROMPOSITION, SendEditor(SCI_GETCURRENTPOS)) + 1;

				GotoLineEnsureVisible(lineNumber - 1);

				if (bHasChar && characterOnLine > 1 && lineNumber <= SendEditor(SCI_GETLINECOUNT)) {
					// Constrain to the requested line
					int lineStart = SendEditor(SCI_POSITIONFROMLINE, lineNumber - 1);
					int lineEnd = SendEditor(SCI_GETLINEENDPOSITION, lineNumber - 1);

					int position = lineStart;
					while (--characterOnLine && position < lineEnd)
						position = SendEditor(SCI_POSITIONAFTER, position);

					SendEditor(SCI_GOTOPOS, position);
				}
			}
			::EndDialog(hDlg, IDOK);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::GoLineDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->GoLineMessage(hDlg, message, wParam);
}

void SciTEWin::GoLineDialog() {
	DoDialog(hInstance, "GoLine", MainHWND(), reinterpret_cast<DLGPROC>(GoLineDlg));
	WindowSetFocus(wEditor);
}

BOOL SciTEWin::AbbrevMessage(HWND hDlg, UINT message, WPARAM wParam) {
	HWND hAbbrev = ::GetDlgItem(hDlg, IDABBREV);
	switch (message) {

	case WM_INITDIALOG:
		LocaliseDialog(hDlg);
		FillComboFromProps(hAbbrev, propsAbbrev);
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			return FALSE;
		} else if (ControlIDOfCommand(wParam) == IDOK) {
			::GetDlgItemText(hDlg, IDABBREV, abbrevInsert, sizeof(abbrevInsert));
			::EndDialog(hDlg, IDOK);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::AbbrevDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->AbbrevMessage(hDlg, message, wParam);
}

bool SciTEWin::AbbrevDialog() {
	bool success = (DoDialog(hInstance, "InsAbbrev", MainHWND(), reinterpret_cast<DLGPROC>(AbbrevDlg)) == IDOK);
	WindowSetFocus(wEditor);
	return success;
}

BOOL SciTEWin::TabSizeMessage(HWND hDlg, UINT message, WPARAM wParam) {
	switch (message) {

	case WM_INITDIALOG: {
			LocaliseDialog(hDlg);
			::SendDlgItemMessage(hDlg, IDTABSIZE, EM_LIMITTEXT, 2, 1);
			int tabSize = SendEditor(SCI_GETTABWIDTH);
			if (tabSize > 99)
				tabSize = 99;
			char tmp[3];
			sprintf(tmp, "%d", tabSize);
			::SetDlgItemText(hDlg, IDTABSIZE, tmp);

			::SendDlgItemMessage(hDlg, IDINDENTSIZE, EM_LIMITTEXT, 2, 1);
			int indentSize = SendEditor(SCI_GETINDENT);
			if (indentSize > 99)
				indentSize = 99;
			sprintf(tmp, "%d", indentSize);
			::SetDlgItemText(hDlg, IDINDENTSIZE, tmp);

			::CheckDlgButton(hDlg, IDUSETABS, SendEditor(SCI_GETUSETABS));
			return TRUE;
		}

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			return FALSE;
		} else if (ControlIDOfCommand(wParam) == IDOK) {
			BOOL bOK;
			int tabSize = static_cast<int>(::GetDlgItemInt(hDlg, IDTABSIZE, &bOK, FALSE));
			if (tabSize > 0)
				SendEditor(SCI_SETTABWIDTH, tabSize);
			int indentSize = static_cast<int>(::GetDlgItemInt(hDlg, IDINDENTSIZE, &bOK, FALSE));
			if (indentSize > 0)
				SendEditor(SCI_SETINDENT, indentSize);
			bool useTabs = static_cast<bool>(::IsDlgButtonChecked(hDlg, IDUSETABS));
			SendEditor(SCI_SETUSETABS, useTabs);
			::EndDialog(hDlg, IDOK);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::TabSizeDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->TabSizeMessage(hDlg, message, wParam);
}

void SciTEWin::TabSizeDialog() {
	DoDialog(hInstance, "TabSize", MainHWND(), reinterpret_cast<DLGPROC>(TabSizeDlg));
	WindowSetFocus(wEditor);
}

void SciTEWin::ParamGrab() {
	if (wParameters.Created()) {
		HWND hDlg = reinterpret_cast<HWND>(wParameters.GetID());
		for (int param = 0; param < maxParam; param++) {
			char paramVal[200];
			::GetDlgItemText(hDlg, IDPARAMSTART + param, paramVal, sizeof(paramVal));
			SString paramText(param + 1);
			props.Set(paramText.c_str(), paramVal);
		}
		UpdateStatusBar(true);
	}
}

BOOL SciTEWin::ParametersMessage(HWND hDlg, UINT message, WPARAM wParam) {
	switch (message) {

	case WM_INITDIALOG: {
			LocaliseDialog(hDlg);
			wParameters = hDlg;
			if (modalParameters) {
				::SetDlgItemText(hDlg, IDCMD, parameterisedCommand.c_str());
			}
			for (int param = 0; param < maxParam; param++) {
				SString paramText(param + 1);
				SString paramTextVal = props.Get(paramText.c_str());
				::SetDlgItemText(hDlg, IDPARAMSTART + param, paramTextVal.c_str());
			}
		}
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			if (!modalParameters) {
				wParameters.Destroy();
			}
			return FALSE;
		} else if (ControlIDOfCommand(wParam) == IDOK) {
			ParamGrab();
			::EndDialog(hDlg, IDOK);
			if (!modalParameters) {
				wParameters.Destroy();
			}
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::ParametersDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->ParametersMessage(hDlg, message, wParam);
}

bool SciTEWin::ParametersDialog(bool modal) {
	if (wParameters.Created()) {
		ParamGrab();
		if (!modal) {
			wParameters.Destroy();
		}
		return true;
	}
	bool success = false;
	modalParameters = modal;
	if (modal) {
		success = DoDialog(hInstance,
		                   "PARAMETERS",
		                   MainHWND(),
		                   reinterpret_cast<DLGPROC>(ParametersDlg)) == IDOK;
		wParameters = 0;
		WindowSetFocus(wEditor);
	} else {
		::CreateDialogParam(hInstance,
		                    "PARAMETERSNONMODAL",
		                    MainHWND(),
		                    reinterpret_cast<DLGPROC>(ParametersDlg),
		                    reinterpret_cast<LPARAM>(this));
		wParameters.Show();
	}

	return success;
}

int SciTEWin::WindowMessageBox(Window &w, const SString &msg, int style) {
	dialogsOnScreen++;
	int ret = ::MessageBox(reinterpret_cast<HWND>(w.GetID()), msg.c_str(), appName, style | MB_SETFOREGROUND);
	dialogsOnScreen--;
	return ret;
}

BOOL SciTEWin::AboutMessage(HWND hDlg, UINT message, WPARAM wParam) {
	switch (message) {

	case WM_INITDIALOG:
		LocaliseDialog(hDlg);
		SetAboutMessage(::GetDlgItem(hDlg, IDABOUTSCINTILLA),
		                staticBuild ? "Sc1  " : "SciTE");
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDOK) {
			::EndDialog(hDlg, IDOK);
			return TRUE;
		} else if (ControlIDOfCommand(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			return FALSE;
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::AboutDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->AboutMessage(hDlg, message, wParam);
}

void SciTEWin::AboutDialogWithBuild(int staticBuild_) {
	staticBuild = staticBuild_;
	DoDialog(hInstance, "About", MainHWND(),
	         reinterpret_cast<DLGPROC>(AboutDlg));
	WindowSetFocus(wEditor);
}
