// SciTE - Scintilla based Text Editor
/** @file SciTEWinDlg.cxx
 ** Dialog code for the Windows version of the editor.
 **/
// Copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include "SciTEWin.h"
// need this header for SHBrowseForFolder
#include <shlobj.h>

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
			hMM = ::LoadLibrary(TEXT("WINMM.DLL"));
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

static SciTEWin *Caller(HWND hDlg, UINT message, LPARAM lParam) {
	if (message == WM_INITDIALOG) {
		::SetWindowLongPtr(hDlg, DWLP_USER, lParam);
	}
	return reinterpret_cast<SciTEWin*>(::GetWindowLongPtr(hDlg, DWLP_USER));
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

bool SciTEWin::DialogHandled(GUI::WindowID id, MSG *pmsg) {
	if (id) {
		if (	::IsDialogMessageW(reinterpret_cast<HWND>(id), pmsg))
			return true;
	}
	return false;
}

bool SciTEWin::ModelessHandler(MSG *pmsg) {
	if (DialogHandled(wFindReplace.GetID(), pmsg)) {
		return true;
	}
	if (DialogHandled(wFindInFiles.GetID(), pmsg)) {
		return true;
	}
	if (wParameters.GetID()) {
		// Allow commands, such as Ctrl+1 to be active while the Parameters dialog is
		// visible so that a group of commands can be easily run with differing parameters.
		bool menuKey = (pmsg->message == WM_KEYDOWN) &&
		               (pmsg->wParam != VK_TAB) &&
		               (pmsg->wParam != VK_ESCAPE) &&
		               (pmsg->wParam != VK_RETURN) &&
		               (pmsg->wParam < 'A' || pmsg->wParam > 'Z') &&
		               (IsKeyDown(VK_CONTROL) || !IsKeyDown(VK_MENU));
		if (!menuKey && DialogHandled(wParameters.GetID(), pmsg))
			return true;
	}
	if ((pmsg->message == WM_KEYDOWN) || (pmsg->message == WM_SYSKEYDOWN)) {
		if (searchStrip.KeyDown(pmsg->wParam))
			return true;
		if (findStrip.KeyDown(pmsg->wParam))
			return true;
		if (replaceStrip.KeyDown(pmsg->wParam))
			return true;
		if (userStrip.KeyDown(pmsg->wParam))
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
int SciTEWin::DoDialog(HINSTANCE hInst, const TCHAR *resName, HWND hWnd, DLGPROC lpProc) {
	int result = static_cast<int>(
		::DialogBoxParam(hInst, resName, hWnd, lpProc, reinterpret_cast<LPARAM>(this)));

	if (result == -1) {
		GUI::gui_string errorNum = GUI::StringFromInteger(::GetLastError());
		GUI::gui_string msg = LocaliseMessage("Failed to create dialog box: ^0.", errorNum.c_str());
		::MessageBoxW(hWnd, msg.c_str(), appName, MB_OK | MB_SETFOREGROUND);
	}

	return result;
}


GUI::gui_string SciTEWin::DialogFilterFromProperty(const GUI::gui_char *filterProperty) {
	GUI::gui_string filter = filterProperty;
	if (filter.length()) {
		std::replace(filter.begin(), filter.end(), '|', '\0');
		size_t start = 0;
		while (start < filter.length()) {
			const GUI::gui_char *filterName = filter.c_str() + start;
			if (*filterName == '#') {
				size_t next = start + wcslen(filter.c_str() + start) + 1;
				next += wcslen(filter.c_str() + next) + 1;
				filter.erase(start, next - start);
			} else {
				GUI::gui_string localised = localiser.Text(GUI::UTF8FromString(filterName).c_str(), false);
				if (localised.size()) {
					filter.erase(start, wcslen(filterName));
					filter.insert(start, localised.c_str());
				}
				start += wcslen(filter.c_str() + start) + 1;
				start += wcslen(filter.c_str() + start) + 1;
			}
		}
	}
	return filter;
}

bool SciTEWin::OpenDialog(FilePath directory, const GUI::gui_char *filter) {
	enum {maxBufferSize=2048};

	GUI::gui_string openFilter = DialogFilterFromProperty(filter);

	if (!openWhat[0]) {
		wcscpy(openWhat, localiser.Text("Custom Filter").c_str());
		openWhat[wcslen(openWhat) + 1] = '\0';
	}

	bool succeeded = false;
	GUI::gui_char openName[maxBufferSize]; // maximum common dialog buffer size (says mfc..)
	openName[0] = '\0';

	OPENFILENAMEW ofn = {
	       sizeof(ofn), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	ofn.hwndOwner = MainHWND();
	ofn.hInstance = hInstance;
	ofn.lpstrFile = openName;
	ofn.nMaxFile = maxBufferSize;
	ofn.lpstrFilter = openFilter.c_str();
	ofn.lpstrCustomFilter = openWhat;
	ofn.nMaxCustFilter = ELEMENTS(openWhat);
	ofn.nFilterIndex = filterDefault;
	GUI::gui_string translatedTitle = localiser.Text("Open File");
	ofn.lpstrTitle = translatedTitle.c_str();
	if (props.GetInt("open.dialog.in.file.directory")) {
		ofn.lpstrInitialDir = directory.AsInternal();
	}
	ofn.Flags = OFN_HIDEREADONLY;

	if (buffers.size > 1) {
		ofn.Flags |=
		    OFN_EXPLORER |
		    OFN_PATHMUSTEXIST |
		    OFN_NOCHANGEDIR |
		    OFN_ALLOWMULTISELECT;
	}
	if (::GetOpenFileNameW(&ofn)) {
		succeeded = true;
		filterDefault = ofn.nFilterIndex;
		// if single selection then have path+file
		if (wcslen(openName) > static_cast<size_t>(ofn.nFileOffset)) {
			Open(openName);
		} else {
			FilePath directory(openName);
			GUI::gui_char *p = openName + wcslen(openName) + 1;
			while (*p) {
				// make path+file, add it to the list
				Open(FilePath(directory, FilePath(p)));
				// goto next char pos after \0
				p += wcslen(p) + 1;
			}
		}
	}
	return succeeded;
}

FilePath SciTEWin::ChooseSaveName(FilePath directory, const char *title, const GUI::gui_char *filter, const char *ext) {
	FilePath path;
	if (0 == dialogsOnScreen) {
		GUI::gui_char saveName[MAX_PATH] = GUI_TEXT("");
		FilePath savePath = SaveName(ext);
		if (!savePath.IsUntitled()) {
			wcscpy(saveName, savePath.AsInternal());
		}
		OPENFILENAMEW ofn = {
		                       sizeof(ofn), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		                   };
		ofn.hwndOwner = MainHWND();
		ofn.hInstance = hInstance;
		ofn.lpstrFile = saveName;
		ofn.nMaxFile = ELEMENTS(saveName);
		GUI::gui_string translatedTitle = localiser.Text(title);
		ofn.lpstrTitle = translatedTitle.c_str();
		ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		ofn.lpstrFilter = filter;
		ofn.lpstrInitialDir = directory.AsInternal();

		dialogsOnScreen++;
		if (::GetSaveFileNameW(&ofn)) {
			path = saveName;
		}
		dialogsOnScreen--;
	}
	return path;
}

bool SciTEWin::SaveAsDialog() {
	GUI::gui_string saveFilter = DialogFilterFromProperty(
		GUI::StringFromUTF8(props.GetExpanded("save.filter").c_str()).c_str());
	FilePath path = ChooseSaveName(filePath.Directory(), "Save File", saveFilter.c_str());
	if (path.IsSet()) {
		return SaveIfNotOpen(path, false);
	}
	return false;
}

void SciTEWin::SaveACopy() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Save a Copy");
	if (path.IsSet()) {
		SaveBuffer(path, sfNone);
	}
}

void SciTEWin::SaveAsHTML() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Export File As HTML",
	                              GUI_TEXT("Web (.html;.htm)\0*.html;*.htm\0"), ".html");
	if (path.IsSet()) {
		SaveToHTML(path);
	}
}

void SciTEWin::SaveAsRTF() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Export File As RTF",
	                              GUI_TEXT("RTF (.rtf)\0*.rtf\0"), ".rtf");
	if (path.IsSet()) {
		SaveToRTF(path);
	}
}

void SciTEWin::SaveAsPDF() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Export File As PDF",
	                              GUI_TEXT("PDF (.pdf)\0*.pdf\0"), ".pdf");
	if (path.IsSet()) {
		SaveToPDF(path);
	}
}

void SciTEWin::SaveAsTEX() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Export File As LaTeX",
	                              GUI_TEXT("TeX (.tex)\0*.tex\0"), ".tex");
	if (path.IsSet()) {
		SaveToTEX(path);
	}
}

void SciTEWin::SaveAsXML() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Export File As XML",
	                              GUI_TEXT("XML (.xml)\0*.xml\0"), ".xml");
	if (path.IsSet()) {
		SaveToXML(path);
	}
}

void SciTEWin::LoadSessionDialog() {
	GUI::gui_char openName[MAX_PATH] = GUI_TEXT("");
	OPENFILENAMEW ofn = {
	                       sizeof(ofn), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	                   };
	ofn.hwndOwner = MainHWND();
	ofn.hInstance = hInstance;
	ofn.lpstrFile = openName;
	ofn.nMaxFile = ELEMENTS(openName);
	ofn.lpstrFilter = GUI_TEXT("Session (.session)\0*.session\0");
	GUI::gui_string translatedTitle = localiser.Text("Load Session");
	ofn.lpstrTitle = translatedTitle.c_str();
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	if (::GetOpenFileNameW(&ofn)) {
		LoadSessionFile(openName);
		RestoreSession();
	}
}

void SciTEWin::SaveSessionDialog() {
	GUI::gui_char saveName[MAX_PATH] = GUI_TEXT("\0");
	wcscpy(saveName, GUI_TEXT("SciTE.session"));
	OPENFILENAMEW ofn = {
			       sizeof(ofn), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			   };
	ofn.hwndOwner = MainHWND();
	ofn.hInstance = hInstance;
	ofn.lpstrDefExt = GUI_TEXT("session");
	ofn.lpstrFile = saveName;
	ofn.nMaxFile = ELEMENTS(saveName);
	GUI::gui_string translatedTitle = localiser.Text("Save Current Session");
	ofn.lpstrTitle = translatedTitle.c_str();
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	ofn.lpstrFilter = GUI_TEXT("Session (.session)\0*.session\0");
	if (::GetSaveFileNameW(&ofn)) {
		SaveSessionFile(saveName);
	}
}

static void DeleteFontObject(HFONT &font) {
	if (font) {
		::DeleteObject(font);
		font = 0;
	}
}

/**
 * Display the Print dialog (if @a showDialog asks it),
 * allowing it to choose what to print on which printer.
 * If OK, print the user choice, with optionally defined header and footer.
 */
void SciTEWin::Print(
    bool showDialog) {	///< false if must print silently (using default settings).

	RemoveFindMarks();
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
	Sci_CharacterRange crange = GetSelection();
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

	GUI::Rectangle rectMargins, rectPhysMargins;
	GUI::Point ptPage;
	GUI::Point ptDpi;

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
		GUI::Rectangle rectSetup;

		// Convert the hundredths of millimeters (HiMetric) or
		// thousandths of inches (HiEnglish) margin values
		// from the Page Setup dialog to device units.
		// (There are 2540 hundredths of a mm in an inch.)

		TCHAR localeInfo[3];
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
		rectMargins.left	= Maximum(rectPhysMargins.left, rectSetup.left);
		rectMargins.top	= Maximum(rectPhysMargins.top, rectSetup.top);
		rectMargins.right	= Maximum(rectPhysMargins.right, rectSetup.right);
		rectMargins.bottom	= Maximum(rectPhysMargins.bottom, rectSetup.bottom);
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
	HFONT fontHeader = ::CreateFontA(headerLineHeight,
	                                0, 0, 0,
	                                sdHeader.weight,
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
	HFONT fontFooter = ::CreateFontA(footerLineHeight,
	                                0, 0, 0,
	                                sdFooter.weight,
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
		::DeleteDC(hdc);
		DeleteFontObject(fontHeader);
		DeleteFontObject(fontFooter);
		GUI::gui_string msg = LocaliseMessage("Can not start printer document.");
		WindowMessageBox(wSciTE, msg, MB_OK);
		return;
	}

	LONG lengthDoc = wEditor.Call(SCI_GETLENGTH);
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
	Sci_RangeToFormat frPrint;
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
	PropSetFile propsPrint;
	propsPrint.superPS = &props;
	SetFileProperties(propsPrint);

	while (lengthPrinted < lengthDoc) {
		printPage = (!(pdlg.Flags & PD_PAGENUMS) ||
		             ((pageNum >= pdlg.nFromPage) && (pageNum <= pdlg.nToPage)));

		char pageString[32];
		sprintf(pageString, "%0d", pageNum);
		propsPrint.Set("CurrentPage", pageString);

		if (printPage) {
			::StartPage(hdc);

			if (headerFormat.size()) {
				GUI::gui_string sHeader = GUI::StringFromUTF8(propsPrint.GetExpanded("print.header.format").c_str());
				::SetTextColor(hdc, sdHeader.ForeAsLong());
				::SetBkColor(hdc, sdHeader.BackAsLong());
				::SelectObject(hdc, fontHeader);
				UINT ta = ::SetTextAlign(hdc, TA_BOTTOM);
				RECT rcw = {frPrint.rc.left, frPrint.rc.top - headerLineHeight - headerLineHeight / 2,
				            frPrint.rc.right, frPrint.rc.top - headerLineHeight / 2};
				rcw.bottom = rcw.top + headerLineHeight;
				::ExtTextOutW(hdc, frPrint.rc.left + 5, frPrint.rc.top - headerLineHeight / 2,
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

		lengthPrinted = wEditor.Call(SCI_FORMATRANGE,
		                           printPage,
		                           reinterpret_cast<LPARAM>(&frPrint));

		if (printPage) {
			if (footerFormat.size()) {
				GUI::gui_string sFooter = GUI::StringFromUTF8(propsPrint.GetExpanded("print.footer.format").c_str());
				::SetTextColor(hdc, sdFooter.ForeAsLong());
				::SetBkColor(hdc, sdFooter.BackAsLong());
				::SelectObject(hdc, fontFooter);
				UINT ta = ::SetTextAlign(hdc, TA_TOP);
				RECT rcw = {frPrint.rc.left, frPrint.rc.bottom + footerLineHeight / 2,
				            frPrint.rc.right, frPrint.rc.bottom + footerLineHeight + footerLineHeight / 2};
				::ExtTextOutW(hdc, frPrint.rc.left + 5, frPrint.rc.bottom + footerLineHeight / 2,
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

	wEditor.Call(SCI_FORMATRANGE, FALSE, 0);

	::EndDoc(hdc);
	::DeleteDC(hdc);
	DeleteFontObject(fontHeader);
	DeleteFontObject(fontFooter);
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

class Dialog {
	HWND hDlg;
public:

	Dialog(HWND hDlg_) : hDlg(hDlg_) {
	}

	HWND Item(int id) {
		return ::GetDlgItem(hDlg, id);
	}

	void Enable(int id, bool enable) {
		::EnableWindow(Item(id), enable);
	}

	GUI::gui_string ItemTextG(int id) {
		HWND wT = Item(id);
		int len = ::GetWindowTextLengthW(wT) + 1;
		std::vector<GUI::gui_char> itemText(len);
		if (::GetDlgItemTextW(hDlg, id, &itemText[0], len)) {
			return GUI::gui_string(&itemText[0]);
		} else {
			return GUI::gui_string();
		}
	}

	void SetItemText(int id, const GUI::gui_char *s) {
		::SetDlgItemTextW(hDlg, id, s);
	}

	// Handle Unicode controls (assume strings to be UTF-8 on Windows NT)

	SString ItemTextU(int id) {
		SString s = GUI::UTF8FromString(ItemTextG(id).c_str()).c_str();
		return s;
	}

	void SetItemTextU(int id, const SString &s) {
		SetItemText(id, GUI::StringFromUTF8(s.c_str()).c_str());
	}

	void SetCheck(int id, bool value) {
		::SendMessage(::GetDlgItem(hDlg, id), BM_SETCHECK,
			value ? BST_CHECKED : BST_UNCHECKED, 0);
	}

	bool Checked(int id) {
		return BST_CHECKED == ::SendMessage(::GetDlgItem(hDlg, id), BM_GETCHECK, 0, 0);
	}

	void FillComboFromMemory(int id, const ComboMemory &mem, bool useTop = false) {
		HWND combo = Item(id);
		::SendMessage(combo, CB_RESETCONTENT, 0, 0);
		for (int i = 0; i < mem.Length(); i++) {
			GUI::gui_string gs = GUI::StringFromUTF8(mem.At(i).c_str());
			::SendMessageW(combo, CB_ADDSTRING, 0,
				       reinterpret_cast<LPARAM>(gs.c_str()));
		}
		if (useTop) {
			::SendMessage(combo, CB_SETCURSEL, 0, 0);
		}
	}

};

static void FillComboFromProps(HWND combo, PropSetFile &props) {
	const char *key;
	const char *val;
	if (props.GetFirst(key, val)) {
		GUI::gui_string wkey = GUI::StringFromUTF8(key);
		::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wkey.c_str()));
		while (props.GetNext(key, val)) {
			wkey = GUI::StringFromUTF8(key);
			::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wkey.c_str()));
		}
	}
}

void SciTEWin::UserStripShow(const char *description) {
	userStrip.visible = *description != 0;
	if (userStrip.visible) {
		userStrip.SetSciTE(this);
		userStrip.SetExtender(extender);
		userStrip.SetDescription(description);
	}
	SizeSubWindows();
}

void SciTEWin::UserStripSet(int control, const char *value) {
	userStrip.Set(control, value);
}

void SciTEWin::UserStripSetList(int control, const char *value) {
	userStrip.SetList(control, value);
}

const char *SciTEWin::UserStripValue(int control) {
	std::string val = userStrip.GetValue(control);
	char *ret = new char[val.size() + 1];
	strcpy(ret, val.c_str());
	return ret;
}

void SciTEWin::UserStripClosed() {
	SizeSubWindows();
}

void SciTEWin::ShowBackgroundProgress(const GUI::gui_string &explanation, int size, int progress) {
	backgroundStrip.visible = !explanation.empty();
	SizeSubWindows();
	if (backgroundStrip.visible)
		backgroundStrip.SetProgress(explanation, size, progress);
}

class DialogFindReplace : public Dialog, public SearchUI  {
	bool advanced;
public:
	DialogFindReplace(HWND hDlg_, bool advanced_) :
		Dialog(hDlg_), advanced(advanced_) {
	}
	void GrabFields();
	void FillFields();
};

void DialogFindReplace::GrabFields() {
	pSearcher->SetFind(ItemTextU(IDFINDWHAT).c_str());
	if (pSearcher->replacing) {
		pSearcher->SetReplace(ItemTextU(IDREPLACEWITH).c_str());
	}
	pSearcher->wholeWord = Checked(IDWHOLEWORD);
	pSearcher->matchCase = Checked(IDMATCHCASE);
	pSearcher->regExp = Checked(IDREGEXP);
	pSearcher->wrapFind = Checked(IDWRAP);
	pSearcher->unSlash = Checked(IDUNSLASH);
	if (!pSearcher->replacing) {
		pSearcher->reverseFind = Checked(IDDIRECTIONUP);
	}
	if (advanced) {
		pSearcher->findInStyle = Checked(IDFINDINSTYLE);
		pSearcher->findStyle = atoi(ItemTextU(IDFINDSTYLE).c_str());
	}
}

void DialogFindReplace::FillFields() {
	FillComboFromMemory(IDFINDWHAT, pSearcher->memFinds);
	SetItemTextU(IDFINDWHAT, pSearcher->findWhat);
	if (pSearcher->replacing) {
		FillComboFromMemory(IDREPLACEWITH, pSearcher->memReplaces);
		SetItemTextU(IDREPLACEWITH, pSearcher->replaceWhat);
		SetItemText(IDREPLDONE, GUI_TEXT("0"));
	}
	SetCheck(IDWHOLEWORD, pSearcher->wholeWord);
	SetCheck(IDMATCHCASE, pSearcher->matchCase);
	SetCheck(IDREGEXP, pSearcher->regExp);
	SetCheck(IDWRAP, pSearcher->wrapFind);
	SetCheck(IDUNSLASH, pSearcher->unSlash);
	if (!pSearcher->replacing) {
		SetCheck(pSearcher->reverseFind ? IDDIRECTIONUP : IDDIRECTIONDOWN, true);
	}
	if (advanced) {
		SetCheck(IDFINDINSTYLE, pSearcher->findInStyle);
	}
}

BOOL SciTEWin::FindMessage(HWND hDlg, UINT message, WPARAM wParam) {
	// Avoid getting dialog items before set up or during tear down.
	if (!(WM_INITDIALOG == message || WM_CLOSE == message || WM_COMMAND == message))
		return FALSE;
	DialogFindReplace dlg(hDlg, FindReplaceAdvanced());
	dlg.SetSearcher(this);

	switch (message) {

	case WM_INITDIALOG:
		LocaliseDialog(hDlg);
		dlg.FillFields();
		if (FindReplaceAdvanced()) {
			dlg.SetCheck(IDFINDSTYLE, findInStyle);
			dlg.Enable(IDFINDSTYLE, findInStyle);
			::SendMessage(dlg.Item(IDFINDSTYLE), EM_LIMITTEXT, 3, 1);
			::SetDlgItemInt(hDlg, IDFINDSTYLE, wEditor.Call(SCI_GETSTYLEAT, wEditor.Call(SCI_GETCURRENTPOS)), FALSE);
		}
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfWParam(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			wFindReplace.Destroy();
			return FALSE;
		} else if ( (ControlIDOfWParam(wParam) == IDOK) ||
		            (ControlIDOfWParam(wParam) == IDMARKALL) ) {
			dlg.GrabFields();
			if (closeFind) {
				::EndDialog(hDlg, IDOK);
				wFindReplace.Destroy();
			}
			if (ControlIDOfWParam(wParam) == IDMARKALL){
				MarkAll();
			}
			FindNext(reverseFind ^ IsKeyDown(VK_SHIFT));
			return TRUE;
		} else if (ControlIDOfWParam(wParam) == IDFINDINSTYLE) {
			if (FindReplaceAdvanced()) {
				findInStyle = dlg.Checked(IDFINDINSTYLE);
				dlg.Enable(IDFINDSTYLE, findInStyle);
			}
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::FindDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->FindMessage(hDlg, message, wParam);
}

BOOL SciTEWin::HandleReplaceCommand(int cmd, bool reverseFind) {
	if (!wFindReplace.GetID())
		return TRUE;
	HWND hwndFR = reinterpret_cast<HWND>(wFindReplace.GetID());
	DialogFindReplace dlg(hwndFR, FindReplaceAdvanced());
	dlg.SetSearcher(this);

	if ((cmd == IDOK) || (cmd == IDREPLACE) || (cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL) || (cmd == IDREPLACEINBUF)) {
		dlg.GrabFields();
	}

	int replacements = 0;
	if (cmd == IDOK) {
		FindNext(reverseFind);
	} else if (cmd == IDREPLACE) {
		ReplaceOnce();
	} else if ((cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL)) {
		replacements = ReplaceAll(cmd == IDREPLACEINSEL);
	} else if (cmd == IDREPLACEINBUF) {
		replacements = ReplaceInBuffers();
	}
	GUI::gui_string replDone = GUI::StringFromInteger(replacements);
	dlg.SetItemText(IDREPLDONE, replDone.c_str());

	return TRUE;
}

BOOL SciTEWin::ReplaceMessage(HWND hDlg, UINT message, WPARAM wParam) {
	// Avoid getting dialog items before set up or during tear down.
	if (!(WM_INITDIALOG == message || WM_CLOSE == message || WM_COMMAND == message))
		return FALSE;
	DialogFindReplace dlg(hDlg, FindReplaceAdvanced());
	dlg.SetSearcher(this);

	switch (message) {

	case WM_INITDIALOG:
		LocaliseDialog(hDlg);
		dlg.FillFields();
		if (FindReplaceAdvanced()) {
			dlg.Enable(IDFINDSTYLE, findInStyle);
			::SetDlgItemInt(hDlg, IDFINDSTYLE, wEditor.Call(SCI_GETSTYLEAT, wEditor.Call(SCI_GETCURRENTPOS)), FALSE);
		}
		if (findWhat.length() != 0 && props.GetInt("find.replacewith.focus", 1)) {
			::SetFocus(::GetDlgItem(hDlg, IDREPLACEWITH));
			return FALSE;
		}
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfWParam(wParam) == IDCANCEL) {
			props.Set("Replacements", "");
			UpdateStatusBar(false);
			::EndDialog(hDlg, IDCANCEL);
			wFindReplace.Destroy();
			return FALSE;
		} else if (ControlIDOfWParam(wParam) == IDFINDINSTYLE) {
			if (FindReplaceAdvanced()) {
				findInStyle = dlg.Checked(IDFINDINSTYLE);
				dlg.Enable(IDFINDSTYLE, findInStyle);
			}
			return TRUE;
		} else {
			return HandleReplaceCommand(ControlIDOfWParam(wParam), IsKeyDown(VK_SHIFT));
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::ReplaceDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->ReplaceMessage(hDlg, message, wParam);
}

void SciTEWin::UIClosed() {
	SciTEBase::UIClosed();
	props.Set("Replacements", "");
	if (!searchStrip.visible)
		bands[SciTEWin::bandSearch].visible = false;
	if (!findStrip.visible)
		bands[SciTEWin::bandFind].visible = false;
	if (!replaceStrip.visible)
		bands[SciTEWin::bandReplace].visible = false;
	UpdateStatusBar(false);
	SizeSubWindows();
	WindowSetFocus(wEditor);
}

void SciTEWin::FindIncrement() {
	if (findStrip.visible)
		findStrip.Close();
	if (replaceStrip.visible)
		replaceStrip.Close();
	searchStrip.visible = !searchStrip.visible;
	SizeSubWindows();
	if (searchStrip.visible) {
		searchStrip.Focus();
	} else {
		WindowSetFocus(wEditor);
	}
}

bool SciTEWin::FindReplaceAdvanced() {
	return props.GetInt("find.replace.advanced");
}

void SciTEWin::Find() {
	if (wFindReplace.Created()) {
		if (!replacing) {
			SelectionIntoFind();
			HWND hDlg = reinterpret_cast<HWND>(wFindReplace.GetID());
			Dialog dlg(hDlg);
			dlg.SetItemTextU(IDFINDWHAT, findWhat);
			::SetFocus(hDlg);
		}
		return;
	}
	SelectionIntoFind();

	if (props.GetInt("find.use.strip")) {
		if (searchStrip.visible)
			searchStrip.Close();
		if (replaceStrip.visible)
			replaceStrip.Close();
		findStrip.visible = true;
		SizeSubWindows();
		findStrip.Show();
	} else {
		if (searchStrip.visible || replaceStrip.visible)
			return;

		replacing = false;

		int dialog_id = FindReplaceAdvanced() ? IDD_FIND_ADV : IDD_FIND;

		wFindReplace = ::CreateDialogParamW(hInstance,
											(LPCWSTR)MAKEINTRESOURCE(dialog_id),
											MainHWND(),
											reinterpret_cast<DLGPROC>(FindDlg),
											reinterpret_cast<LPARAM>(this));
		wFindReplace.Show();
	}
}

// Set a call back with the handle after init to set the path.
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/reference/callbackfunctions/browsecallbackproc.asp

static int __stdcall BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM, LPARAM pData) {
	if (uMsg == BFFM_INITIALIZED) {
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
	}
	return 0;
}

void SciTEWin::PerformGrep() {
	SelectionIntoProperties();

	SString findInput;
	long flags = 0;
	if (props.Get("find.input").length()) {
		findInput = props.GetNewExpand("find.input");
		flags += jobHasInput;
	}

	SString findCommand = props.GetNewExpand("find.command");
	if (findCommand == "") {
		// Call InternalGrep in a new thread
		// searchParams is "(w|~)(c|~)(d|~)(b|~)\0files\0text"
		// A "w" indicates whole word, "c" case sensitive, "d" dot directories, "b" binary files
		SString searchParams;
		searchParams.append(wholeWord ? "w" : "~");
		searchParams.append(matchCase ? "c" : "~");
		searchParams.append(props.GetInt("find.in.dot") ? "d" : "~");
		searchParams.append(props.GetInt("find.in.binary") ? "b" : "~");
		searchParams.append("\0", 1);
		searchParams.append(props.Get("find.files").c_str());
		searchParams.append("\0", 1);
		searchParams.append(props.Get("find.what").c_str());
		AddCommand(searchParams, props.Get("find.directory"), jobGrep, findInput, flags);
	} else {
		AddCommand(findCommand,
			   props.Get("find.directory"),
			   jobCLI, findInput, flags);
	}
	if (jobQueue.commandCurrent > 0) {
		Execute();
	}
}

void SciTEWin::FillCombos(Dialog &dlg) {
	dlg.FillComboFromMemory(IDFINDWHAT, memFinds, true);
	dlg.FillComboFromMemory(IDFILES, memFiles, true);
	dlg.FillComboFromMemory(IDDIRECTORY, memDirectory, true);
}

BOOL SciTEWin::GrepMessage(HWND hDlg, UINT message, WPARAM wParam) {
	if (WM_SETFONT == message || WM_NCDESTROY == message)
		return FALSE;
	Dialog dlg(hDlg);

	switch (message) {

	case WM_INITDIALOG:
		LocaliseDialog(hDlg);
		FillCombos(dlg);
		dlg.SetItemTextU(IDFINDWHAT, props.Get("find.what"));
		dlg.SetItemTextU(IDDIRECTORY, props.Get("find.directory"));
		if (props.GetNewExpand("find.command") == "") {
			// Empty means use internal that can respond to flags
			dlg.SetCheck(IDWHOLEWORD, wholeWord);
			dlg.SetCheck(IDMATCHCASE, matchCase);
		} else {
			dlg.Enable(IDWHOLEWORD, false);
			dlg.Enable(IDMATCHCASE, false);
		}
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfWParam(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			wFindInFiles.Destroy();
			return FALSE;

		} else if (ControlIDOfWParam(wParam) == IDOK) {
			if (jobQueue.IsExecuting()) {
				GUI::gui_string msgBuf = LocaliseMessage("Job is currently executing. Wait until it finishes.");
				WindowMessageBox(wFindInFiles, msgBuf, MB_OK | MB_ICONWARNING);
				return FALSE;
			}
			findWhat = dlg.ItemTextU(IDFINDWHAT);
			props.Set("find.what", findWhat.c_str());
			memFinds.Insert(findWhat.c_str());

			SString files = dlg.ItemTextU(IDFILES);
			props.Set("find.files", files.c_str());
			memFiles.Insert(files.c_str());

			SString directory = dlg.ItemTextU(IDDIRECTORY);
			props.Set("find.directory", directory.c_str());
			memDirectory.Insert(directory.c_str());

			wholeWord = dlg.Checked(IDWHOLEWORD);
			matchCase = dlg.Checked(IDMATCHCASE);

			FillCombos(dlg);

			PerformGrep();
			if (props.GetInt("find.in.files.close.on.find", 1)) {
				::EndDialog(hDlg, IDOK);
				wFindInFiles.Destroy();
				return TRUE;
			} else {
				return FALSE;
			}
		} else if (ControlIDOfWParam(wParam) == IDDOTDOT) {
			FilePath directory(dlg.ItemTextG(IDDIRECTORY));
			directory = directory.Directory();
			dlg.SetItemText(IDDIRECTORY, directory.AsInternal());

		} else if (ControlIDOfWParam(wParam) == IDBROWSE) {

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
				TCHAR szDisplayName[MAX_PATH] = TEXT("");
				info.pszDisplayName = szDisplayName;
				GUI::gui_string title = localiser.Text("Select a folder to search from");
				info.lpszTitle = title.c_str();
				info.ulFlags = 0;
				info.lpfn = BrowseCallbackProc;
				GUI::gui_string directory = dlg.ItemTextG(IDDIRECTORY);
				if (!EndsWith(directory, pathSepString)) {
					directory += pathSepString;
				}
				info.lParam = reinterpret_cast<LPARAM>(directory.c_str());

				// Execute the browsing dialog.
				LPITEMIDLIST pidl = ::SHBrowseForFolder(&info);

				// pidl will be null if they cancel the browse dialog.
				// pidl will be not null when they select a folder.
				if (pidl) {
					// Try to convert the pidl to a display string.
					// Return is true if success.
					TCHAR szDir[MAX_PATH];
					if (::SHGetPathFromIDList(pidl, szDir)) {
						// Set edit control to the directory path.
						dlg.SetItemText(IDDIRECTORY, szDir);
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
	if (wFindInFiles.Created()) {
		HWND hDlg = reinterpret_cast<HWND>(wFindInFiles.GetID());
		Dialog dlg(hDlg);
		dlg.SetItemTextU(IDFINDWHAT, findWhat);
		::SetFocus(hDlg);
		return;
	}
	props.Set("find.what", findWhat.c_str());
	FilePath findInDir = filePath.Directory();
	props.Set("find.directory", findInDir.AsUTF8().c_str());
	wFindInFiles = ::CreateDialogParam(hInstance, TEXT("Grep"), MainHWND(),
		reinterpret_cast<DLGPROC>(GrepDlg), reinterpret_cast<sptr_t>(this));
	wFindInFiles.Show();
}

void SciTEWin::Replace() {
	if (wFindReplace.Created()) {
		if (replacing) {
			SelectionIntoFind(false);
			HWND hDlg = reinterpret_cast<HWND>(wFindReplace.GetID());
			Dialog dlg(hDlg);
			dlg.SetItemTextU(IDFINDWHAT, findWhat);
			::SetFocus(hDlg);
		}
		return;
	}
	SelectionIntoFind(false); // don't strip EOL at end of selection

	if (props.GetInt("replace.use.strip")) {
		if (searchStrip.visible)
			searchStrip.Close();
		if (findStrip.visible)
			findStrip.Close();
		replaceStrip.visible = true;
		SizeSubWindows();
		replaceStrip.Show();
		havefound = false;
	} else {
		if (searchStrip.visible || findStrip.visible)
			return;
		int dialog_id = (!props.GetInt("find.replace.advanced") ? IDD_REPLACE : IDD_REPLACE_ADV);

		replacing = true;
		havefound = false;

		wFindReplace = ::CreateDialogParamW(hInstance,
											(LPCWSTR)MAKEINTRESOURCE(dialog_id),
											MainHWND(),
											reinterpret_cast<DLGPROC>(ReplaceDlg),
											reinterpret_cast<sptr_t>(this));
		wFindReplace.Show();
	}
}

void SciTEWin::FindReplace(bool replace) {
	replacing = replace;
}

void SciTEWin::DestroyFindReplace() {
	if (wFindReplace.Created()) {
		::EndDialog(reinterpret_cast<HWND>(wFindReplace.GetID()), IDCANCEL);
		wFindReplace.Destroy();
	}
}

BOOL SciTEWin::GoLineMessage(HWND hDlg, UINT message, WPARAM wParam) {
	switch (message) {

	case WM_INITDIALOG: {
			int position = wEditor.Call(SCI_GETCURRENTPOS);
			int lineNumber = wEditor.Call(SCI_LINEFROMPOSITION, position) + 1;
			int lineStart = wEditor.Call(SCI_POSITIONFROMLINE, lineNumber - 1);
			int characterOnLine = 1;
			while (position > lineStart) {
				position = wEditor.Call(SCI_POSITIONBEFORE, position);
				characterOnLine++;
			}

			LocaliseDialog(hDlg);
			::SendDlgItemMessage(hDlg, IDGOLINE, EM_LIMITTEXT, 10, 1);
			::SendDlgItemMessage(hDlg, IDGOLINECHAR, EM_LIMITTEXT, 10, 1);
			::SetDlgItemInt(hDlg, IDCURRLINE, lineNumber, FALSE);
			::SetDlgItemInt(hDlg, IDCURRLINECHAR, characterOnLine, FALSE);
			::SetDlgItemInt(hDlg, IDLASTLINE, wEditor.Call(SCI_GETLINECOUNT), FALSE);
                }
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfWParam(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			return FALSE;
		} else if (ControlIDOfWParam(wParam) == IDOK) {
			BOOL bHasLine;
			int lineNumber = static_cast<int>(
			                     ::GetDlgItemInt(hDlg, IDGOLINE, &bHasLine, FALSE));
			BOOL bHasChar;
			int characterOnLine = static_cast<int>(
			                     ::GetDlgItemInt(hDlg, IDGOLINECHAR, &bHasChar, FALSE));

			if (bHasLine || bHasChar) {
				if (!bHasLine)
					lineNumber = wEditor.Call(SCI_LINEFROMPOSITION, wEditor.Call(SCI_GETCURRENTPOS)) + 1;

				GotoLineEnsureVisible(lineNumber - 1);

				if (bHasChar && characterOnLine > 1 && lineNumber <= wEditor.Call(SCI_GETLINECOUNT)) {
					// Constrain to the requested line
					int lineStart = wEditor.Call(SCI_POSITIONFROMLINE, lineNumber - 1);
					int lineEnd = wEditor.Call(SCI_GETLINEENDPOSITION, lineNumber - 1);

					int position = lineStart;
					while (--characterOnLine && position < lineEnd)
						position = wEditor.Call(SCI_POSITIONAFTER, position);

					wEditor.Call(SCI_GOTOPOS, position);
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
	DoDialog(hInstance, TEXT("GoLine"), MainHWND(), reinterpret_cast<DLGPROC>(GoLineDlg));
	WindowSetFocus(wEditor);
}

BOOL SciTEWin::AbbrevMessage(HWND hDlg, UINT message, WPARAM wParam) {
	Dialog dlg(hDlg);
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
		if (ControlIDOfWParam(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			return FALSE;
		} else if (ControlIDOfWParam(wParam) == IDOK) {
			SString sAbbrev = dlg.ItemTextU(IDABBREV);
			strncpy(abbrevInsert, sAbbrev.c_str(), sizeof(abbrevInsert));
			abbrevInsert[sizeof(abbrevInsert) - 1] = '\0';
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
	bool success = (DoDialog(hInstance, TEXT("InsAbbrev"), MainHWND(),
		reinterpret_cast<DLGPROC>(AbbrevDlg)) == IDOK);
	WindowSetFocus(wEditor);
	return success;
}

BOOL SciTEWin::TabSizeMessage(HWND hDlg, UINT message, WPARAM wParam) {
	switch (message) {

	case WM_INITDIALOG: {
			LocaliseDialog(hDlg);
			::SendDlgItemMessage(hDlg, IDTABSIZE, EM_LIMITTEXT, 2, 1);
			int tabSize = wEditor.Call(SCI_GETTABWIDTH);
			if (tabSize > 99)
				tabSize = 99;
			char tmp[3];
			sprintf(tmp, "%d", tabSize);
			::SetDlgItemTextA(hDlg, IDTABSIZE, tmp);

			::SendDlgItemMessage(hDlg, IDINDENTSIZE, EM_LIMITTEXT, 2, 1);
			int indentSize = wEditor.Call(SCI_GETINDENT);
			if (indentSize > 99)
				indentSize = 99;
			sprintf(tmp, "%d", indentSize);
			::SetDlgItemTextA(hDlg, IDINDENTSIZE, tmp);

			::CheckDlgButton(hDlg, IDUSETABS, wEditor.Call(SCI_GETUSETABS));
			return TRUE;
		}

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfWParam(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			return FALSE;
		} else if ((ControlIDOfWParam(wParam) == IDCONVERT) ||
			(ControlIDOfWParam(wParam) == IDOK)) {
			BOOL bOK;
			int tabSize = static_cast<int>(::GetDlgItemInt(hDlg, IDTABSIZE, &bOK, FALSE));
			if (tabSize > 0)
				wEditor.Call(SCI_SETTABWIDTH, tabSize);
			int indentSize = static_cast<int>(::GetDlgItemInt(hDlg, IDINDENTSIZE, &bOK, FALSE));
			if (indentSize > 0)
				wEditor.Call(SCI_SETINDENT, indentSize);
			bool useTabs = static_cast<bool>(::IsDlgButtonChecked(hDlg, IDUSETABS));
			wEditor.Call(SCI_SETUSETABS, useTabs);
			if (ControlIDOfWParam(wParam) == IDCONVERT) {
				ConvertIndentation(tabSize, useTabs);
			}
			::EndDialog(hDlg, ControlIDOfWParam(wParam));
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CALLBACK SciTEWin::TabSizeDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	return Caller(hDlg, message, lParam)->TabSizeMessage(hDlg, message, wParam);
}

void SciTEWin::TabSizeDialog() {
	DoDialog(hInstance, TEXT("TabSize"), MainHWND(), reinterpret_cast<DLGPROC>(TabSizeDlg));
	WindowSetFocus(wEditor);
}

bool SciTEWin::ParametersOpen() {
	return wParameters.Created();
}

void SciTEWin::ParamGrab() {
	if (wParameters.Created()) {
		HWND hDlg = reinterpret_cast<HWND>(wParameters.GetID());
		Dialog dlg(hDlg);
		for (int param = 0; param < maxParam; param++) {
			std::string paramVal = GUI::UTF8FromString(dlg.ItemTextG(IDPARAMSTART + param));
			SString paramText(param + 1);
			props.Set(paramText.c_str(), paramVal.c_str());
		}
		UpdateStatusBar(true);
	}
}

BOOL SciTEWin::ParametersMessage(HWND hDlg, UINT message, WPARAM wParam) {
	switch (message) {

	case WM_INITDIALOG: {
			LocaliseDialog(hDlg);
			wParameters = hDlg;
			Dialog dlg(hDlg);
			if (modalParameters) {
				GUI::gui_string sCommand = GUI::StringFromUTF8(parameterisedCommand.c_str());
				dlg.SetItemText(IDCMD, sCommand.c_str());
			}
			for (int param = 0; param < maxParam; param++) {
				SString paramText(param + 1);
				SString paramTextVal = props.Get(paramText.c_str());
				GUI::gui_string sVal = GUI::StringFromUTF8(paramTextVal.c_str());
				dlg.SetItemText(IDPARAMSTART + param, sVal.c_str());
			}
		}
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfWParam(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			if (!modalParameters) {
				wParameters.Destroy();
			}
			return FALSE;
		} else if (ControlIDOfWParam(wParam) == IDOK) {
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
		                   TEXT("PARAMETERS"),
		                   MainHWND(),
		                   reinterpret_cast<DLGPROC>(ParametersDlg)) == IDOK;
		wParameters = 0;
		WindowSetFocus(wEditor);
	} else {
		::CreateDialogParam(hInstance,
		                    TEXT("PARAMETERSNONMODAL"),
		                    MainHWND(),
		                    reinterpret_cast<DLGPROC>(ParametersDlg),
		                    reinterpret_cast<LPARAM>(this));
		wParameters.Show();
	}

	return success;
}

int SciTEWin::WindowMessageBox(GUI::Window &w, const GUI::gui_string &msg, int style) {
	dialogsOnScreen++;
	int ret = ::MessageBoxW(reinterpret_cast<HWND>(w.GetID()), msg.c_str(), appName, style | MB_SETFOREGROUND);
	dialogsOnScreen--;
	return ret;
}

void SciTEWin::FindMessageBox(const SString &msg, const SString *findItem) {
	if (findItem == 0) {
		GUI::gui_string msgBuf = LocaliseMessage(msg.c_str());
		WindowMessageBox(wFindReplace.Created() ? wFindReplace : wSciTE, msgBuf, MB_OK | MB_ICONWARNING);
	} else {
		GUI::gui_string findThing = GUI::StringFromUTF8(findItem->c_str());
		GUI::gui_string msgBuf = LocaliseMessage(msg.c_str(), findThing.c_str());
		WindowMessageBox(wFindReplace.Created() ? wFindReplace : wSciTE, msgBuf, MB_OK | MB_ICONWARNING);
	}
}

LRESULT CALLBACK CreditsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_GETDLGCODE)
		return DLGC_STATIC | DLGC_WANTARROWS | DLGC_WANTCHARS;

	WNDPROC lpPrevWndProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (lpPrevWndProc)
		return ::CallWindowProc(lpPrevWndProc, hwnd, uMsg, wParam, lParam);

	return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL SciTEWin::AboutMessage(HWND hDlg, UINT message, WPARAM wParam) {
	switch (message) {

	case WM_INITDIALOG: {
		LocaliseDialog(hDlg);
		GUI::ScintillaWindow ss;
		HWND hwndCredits = ::GetDlgItem(hDlg, IDABOUTSCINTILLA);
		LONG_PTR subclassedProc = ::SetWindowLongPtr(hwndCredits, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CreditsWndProc));
		::SetWindowLongPtr(hwndCredits, GWLP_USERDATA, subclassedProc);
		ss.SetID(hwndCredits);
		SetAboutMessage(ss, staticBuild ? "Sc1  " : "SciTE");
		}
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfWParam(wParam) == IDOK) {
			::EndDialog(hDlg, IDOK);
			return TRUE;
		} else if (ControlIDOfWParam(wParam) == IDCANCEL) {
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
	DoDialog(hInstance, TEXT("About"), MainHWND(),
	         reinterpret_cast<DLGPROC>(AboutDlg));
	WindowSetFocus(wEditor);
}
