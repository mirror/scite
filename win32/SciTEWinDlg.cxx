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
		SetWindowLongPtr(hDlg, DWLP_USER, lParam);
	}
	return reinterpret_cast<SciTEWin*>(GetWindowLongPtr(hDlg, DWLP_USER));
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

bool DialogHandled(WindowID id, MSG *pmsg) {
	if (id) {
		if (::IsDialogMessage(reinterpret_cast<HWND>(id), pmsg))
			return true;
	}
	return false;
}

bool SciTEWin::ModelessHandler(MSG *pmsg) {
	if (DialogHandled(wFindReplace.GetID(), pmsg)) {
		return true;
	}
	if (DialogHandled(wFindIncrement.GetID(), pmsg)) {
		return true;
	}
	if (DialogHandled(wFindInFiles.GetID(), pmsg)) {
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
		::MessageBox(hWnd, msg.c_str(), appName, MB_OK | MB_SETFOREGROUND);
	}

	return result;
}

bool SciTEWin::OpenDialog(FilePath directory, const char *filter) {
	enum {maxBufferSize=2048};

	SString openFilter = filter;
	if (openFilter.length()) {
		openFilter.substitute('|', '\0');
		size_t start = 0;
		while (start < openFilter.length()) {
			const char *filterName = openFilter.c_str() + start;
			if (*filterName == '#') {
				size_t next = start + strlen(openFilter.c_str() + start) + 1;
				next += strlen(openFilter.c_str() + next) + 1;
				openFilter.remove(start, next - start);
			} else {
				SString localised = localiser.Text(filterName, false);
				if (localised.length()) {
					openFilter.remove(start, strlen(filterName));
					openFilter.insert(start, localised.c_str());
				}
				start += strlen(openFilter.c_str() + start) + 1;
				start += strlen(openFilter.c_str() + start) + 1;
			}
		}
	}

	if (!openWhat[0]) {
		strcpy(openWhat, localiser.Text("Custom Filter").c_str());
		openWhat[strlen(openWhat) + 1] = '\0';
	}

	bool succeeded = false;
	char openName[maxBufferSize]; // maximum common dialog buffer size (says mfc..)
	openName[0] = '\0';

	OPENFILENAMEA ofn = {
	       sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	ofn.hwndOwner = MainHWND();
	ofn.hInstance = hInstance;
	ofn.lpstrFile = openName;
	ofn.nMaxFile = maxBufferSize;
	ofn.lpstrFilter = openFilter.c_str();
	ofn.lpstrCustomFilter = openWhat;
	ofn.nMaxCustFilter = sizeof(openWhat);
	ofn.nFilterIndex = filterDefault;
	SString translatedTitle = localiser.Text("Open File");
	ofn.lpstrTitle = translatedTitle.c_str();
	if (props.GetInt("open.dialog.in.file.directory")) {
		ofn.lpstrInitialDir = directory.AsFileSystem();
	}
	ofn.Flags = OFN_HIDEREADONLY;

	if (buffers.size > 1) {
		ofn.Flags |=
		    OFN_EXPLORER |
		    OFN_PATHMUSTEXIST |
		    OFN_ALLOWMULTISELECT;
	}
	if (::GetOpenFileName(&ofn)) {
		succeeded = true;
		filterDefault = ofn.nFilterIndex;
		// if single selection then have path+file
		if (strlen(openName) > static_cast<size_t>(ofn.nFileOffset)) {
			Open(openName);
		} else {
			FilePath directory(openName);
			char *p = openName + strlen(openName) + 1;
			while (*p) {
				// make path+file, add it to the list
				Open(FilePath(directory, FilePath(p)));
				// goto next char pos after \0
				p += strlen(p) + 1;
			}
		}
	}
	return succeeded;
}

FilePath SciTEWin::ChooseSaveName(FilePath directory, const char *title, const char *filter, const char *ext) {
	FilePath path;
	if (0 == dialogsOnScreen) {
		char saveName[MAX_PATH] = "";
		FilePath savePath = SaveName(ext);
		if (!savePath.IsUntitled()) {
			strcpy(saveName, savePath.AsFileSystem());
		}
		OPENFILENAME ofn = {
		                       sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		                   };
		ofn.hwndOwner = MainHWND();
		ofn.hInstance = hInstance;
		ofn.lpstrFile = saveName;
		ofn.nMaxFile = sizeof(saveName);
		SString translatedTitle = localiser.Text(title);
		ofn.lpstrTitle = translatedTitle.c_str();
		ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
		ofn.lpstrFilter = filter;
		ofn.lpstrInitialDir = directory.AsFileSystem();

		dialogsOnScreen++;
		if (::GetSaveFileName(&ofn)) {
			path = saveName;
		}
		dialogsOnScreen--;
	}
	return path;
}

bool SciTEWin::SaveAsDialog() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Save File");
	if (path.IsSet()) {
		//Platform::DebugPrintf("Save: <%s>\n", openName);
		SetFileName(path, false); // don't fix case
		Save();
		ReadProperties();

		// In case extension was changed
		SendEditor(SCI_COLOURISE, 0, -1);
		wEditor.InvalidateAll();
		if (extender)
			extender->OnSave(filePath.AsFileSystem());
		return true;
	}
	return false;
}

void SciTEWin::SaveACopy() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Save a Copy");
	if (path.IsSet()) {
		SaveBuffer(path);
	}
}

void SciTEWin::SaveAsHTML() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Export File As HTML",
	                              "Web (.html;.htm)\0*.html;*.htm\0", ".html");
	if (path.IsSet()) {
		SaveToHTML(path);
	}
}

void SciTEWin::SaveAsRTF() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Export File As RTF",
	                              "RTF (.rtf)\0*.rtf\0", ".rtf");
	if (path.IsSet()) {
		SaveToRTF(path);
	}
}

void SciTEWin::SaveAsPDF() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Export File As PDF",
	                              "PDF (.pdf)\0*.pdf\0", ".pdf");
	if (path.IsSet()) {
		SaveToPDF(path);
	}
}

void SciTEWin::SaveAsTEX() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Export File As LaTeX",
	                              "TeX (.tex)\0*.tex\0", ".tex");
	if (path.IsSet()) {
		SaveToTEX(path);
	}
}

void SciTEWin::SaveAsXML() {
	FilePath path = ChooseSaveName(filePath.Directory(), "Export File As XML",
	                              "XML (.xml)\0*.xml\0", ".xml");
	if (path.IsSet()) {
		SaveToXML(path);
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
	ofn.lpstrFilter = "Session (.session)\0*.session\0";
	SString translatedTitle = localiser.Text("Load Session");
	ofn.lpstrTitle = translatedTitle.c_str();
	ofn.Flags = OFN_HIDEREADONLY;
	if (::GetOpenFileName(&ofn))
		LoadSession(openName);
}

void SciTEWin::SaveSessionDialog() {
	char saveName[MAX_PATH] = "\0";
	strcpy(saveName, "SciTE.session");
	OPENFILENAME ofn = {
			       sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			   };
	ofn.hwndOwner = MainHWND();
	ofn.hInstance = hInstance;
	ofn.lpstrDefExt = "session";
	ofn.lpstrFile = saveName;
	ofn.nMaxFile = sizeof(saveName);
	SString translatedTitle = localiser.Text("Save Current Session");
	ofn.lpstrTitle = translatedTitle.c_str();
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	ofn.lpstrFilter = "Session (.session)\0*.session\0";
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
	PropSetFile propsPrint;
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

// This is a reasonable buffer size for dialog box text conversions
#define CTL_TEXT_BUF 512

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

	SString ItemText(int id) {
		HWND wT = Item(id);
		int len = ::GetWindowTextLength(wT);
		SBuffer itemText(len);
		if (len > 0) {
			::GetDlgItemText(hDlg, id, itemText.ptr(), len + 1);
		}
		return SString(itemText);
	}

	void SetItemText(int id, const char *s) {
		::SetDlgItemText(hDlg, id, s);
	}

	// Handle Unicode controls (assume strings to be UTF-8 on Windows NT)

	SString ItemTextU(int id) {
		SString result = "";
		char	msz[CTL_TEXT_BUF];

		if (::IsWindowUnicode(Item(id))) {
			WCHAR wsz[CTL_TEXT_BUF];
			if (::GetDlgItemTextW(hDlg, id, wsz, CTL_TEXT_BUF)) {
				if (::WideCharToMultiByte(CP_UTF8, 0, wsz, -1, msz, CTL_TEXT_BUF, NULL, NULL))
					result = msz;
			}
		} else {
			if (::GetDlgItemTextA(hDlg, id, msz, CTL_TEXT_BUF))
				result = msz;
		}

		return result;
	}

	BOOL SetItemTextU(int id, LPCSTR pmsz) {
		BOOL bSuccess = FALSE;
		WCHAR wsz[CTL_TEXT_BUF];

		if (!pmsz || *pmsz == 0)
			return ::SetDlgItemTextA(hDlg, id, "");

		if (::IsWindowUnicode(Item(id))) {
			if (::MultiByteToWideChar(CP_UTF8, 0, pmsz, -1, wsz, CTL_TEXT_BUF)) {
				bSuccess = ::SetDlgItemTextW(hDlg, id, wsz);
			}
		} else {
			bSuccess = ::SetDlgItemTextA(hDlg, id, pmsz);
		}

		return bSuccess;
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
		if (::IsWindowUnicode(combo)) {
			for (int i = 0; i < mem.Length(); i++) {
				//Platform::DebugPrintf("Combo[%0d] = %s\n", i, mem.At(i).c_str());
				WCHAR wszBuf[CTL_TEXT_BUF];
				::MultiByteToWideChar(CP_UTF8, 0, mem.At(i).c_str(), -1, wszBuf,
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

};

static void FillComboFromProps(HWND combo, PropSetFile &props) {
	char *key;
	char *val;
	if (props.GetFirst(&key, &val)) {
		::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(key));
		while (props.GetNext(&key, &val)) {
			::SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(key));
		}
	}
}

BOOL SciTEWin::FindMessage(HWND hDlg, UINT message, WPARAM wParam) {
	// Avoid getting dialog items before set up or during tear down.
	if (WM_SETFONT == message || WM_NCDESTROY == message)
		return FALSE;
	Dialog dlg(hDlg);

	switch (message) {

	case WM_INITDIALOG:
		LocaliseDialog(hDlg);
		dlg.FillComboFromMemory(IDFINDWHAT, memFinds);
		dlg.SetItemTextU(IDFINDWHAT, findWhat.c_str());
		::SendDlgItemMessage(hDlg, IDFINDWHAT, CB_LIMITTEXT, CTL_TEXT_BUF - 1, 0);
		dlg.SetCheck(IDWHOLEWORD, wholeWord);
		dlg.SetCheck(IDMATCHCASE, matchCase);
		dlg.SetCheck(IDREGEXP, regExp);
		dlg.SetCheck(IDWRAP, wrapFind);
		dlg.SetCheck(IDUNSLASH, unSlash);
		dlg.SetCheck(reverseFind ? IDDIRECTIONUP : IDDIRECTIONDOWN, true);
		if (FindReplaceAdvanced()) {
			dlg.SetCheck(IDFINDSTYLE, findInStyle);
			dlg.Enable(IDFINDSTYLE, findInStyle);
			::SendMessage(dlg.Item(IDFINDSTYLE), EM_LIMITTEXT, 3, 1);
			::SetDlgItemInt(hDlg, IDFINDSTYLE, SendEditor(SCI_GETSTYLEAT, SendEditor(SCI_GETCURRENTPOS)), FALSE);
		}
		return TRUE;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			wFindReplace.Destroy();
			return FALSE;
		} else if ( (ControlIDOfCommand(wParam) == IDOK) ||
		            (ControlIDOfCommand(wParam) == IDMARKALL) ) {
			findWhat = dlg.ItemTextU(IDFINDWHAT);
			props.Set("find.what", findWhat.c_str());
			memFinds.Insert(findWhat.c_str());
			wholeWord = dlg.Checked(IDWHOLEWORD);
			matchCase = dlg.Checked(IDMATCHCASE);
			regExp = dlg.Checked(IDREGEXP);
			wrapFind = dlg.Checked(IDWRAP);
			unSlash = dlg.Checked(IDUNSLASH);
			if (FindReplaceAdvanced()) {
				findInStyle = dlg.Checked(IDFINDINSTYLE);
				findStyle = atoi(dlg.ItemTextU(IDFINDSTYLE).c_str());
			}
			reverseFind = dlg.Checked(IDDIRECTIONUP);
			::EndDialog(hDlg, IDOK);
			wFindReplace.Destroy();
			if (ControlIDOfCommand(wParam) == IDMARKALL){
				MarkAll();
			}
			FindNext(reverseFind);
			return TRUE;
		} else if (ControlIDOfCommand(wParam) == IDFINDINSTYLE) {
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

BOOL SciTEWin::HandleReplaceCommand(int cmd) {
	if (!wFindReplace.GetID())
		return TRUE;
	HWND hwndFR = reinterpret_cast<HWND>(wFindReplace.GetID());
	Dialog dlg(hwndFR);

	if ((cmd == IDOK) || (cmd == IDREPLACE) || (cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL) || (cmd == IDREPLACEINBUF)) {
		findWhat = dlg.ItemTextU(IDFINDWHAT);
		props.Set("find.what", findWhat.c_str());
		memFinds.Insert(findWhat.c_str());
		wholeWord = dlg.Checked(IDWHOLEWORD);
		matchCase = dlg.Checked(IDMATCHCASE);
		regExp = dlg.Checked(IDREGEXP);
		wrapFind = dlg.Checked(IDWRAP);
		unSlash = dlg.Checked(IDUNSLASH);
		if (FindReplaceAdvanced()) {
			findInStyle = dlg.Checked(IDFINDINSTYLE);
			findStyle = atoi(dlg.ItemTextU(IDFINDSTYLE).c_str());
		}
	}
	if ((cmd == IDREPLACE) || (cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL) || (cmd == IDREPLACEINBUF)) {
		replaceWhat = dlg.ItemTextU(IDREPLACEWITH);
		memReplaces.Insert(replaceWhat.c_str());
	}

	int replacements = 0;
	if (cmd == IDOK) {
		FindNext(false);
	} else if (cmd == IDREPLACE) {
		if (havefound) {
			ReplaceOnce();
		} else {
			CharacterRange crange = GetSelection();
			SetSelection(crange.cpMin, crange.cpMin);
			FindNext(false);
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
	dlg.SetItemText(IDREPLDONE, replDone);

	return TRUE;
}

BOOL SciTEWin::ReplaceMessage(HWND hDlg, UINT message, WPARAM wParam) {
	// Avoid getting dialog items before set up or during tear down.
	if (WM_SETFONT == message || WM_NCDESTROY == message)
		return FALSE;
	Dialog dlg(hDlg);
	HWND wReplaceWith = ::GetDlgItem(hDlg, IDREPLACEWITH);

	switch (message) {

	case WM_INITDIALOG:
		LocaliseDialog(hDlg);
		dlg.FillComboFromMemory(IDFINDWHAT, memFinds);
		dlg.SetItemTextU(IDFINDWHAT, findWhat.c_str());
		::SendDlgItemMessage(hDlg, IDFINDWHAT, CB_LIMITTEXT, CTL_TEXT_BUF - 1, 0);
		dlg.FillComboFromMemory(IDREPLACEWITH, memReplaces);
		dlg.SetItemTextU(IDREPLACEWITH, replaceWhat.c_str());
		::SendDlgItemMessage(hDlg, IDREPLACEWITH, CB_LIMITTEXT, CTL_TEXT_BUF - 1, 0);
		dlg.SetCheck(IDWHOLEWORD, wholeWord);
		dlg.SetCheck(IDMATCHCASE, matchCase);
		dlg.SetCheck(IDREGEXP, regExp);
		dlg.SetCheck(IDWRAP, wrapFind);
		dlg.SetCheck(IDUNSLASH, unSlash);
		if (FindReplaceAdvanced()) {
			dlg.SetCheck(IDFINDSTYLE, findInStyle);
			dlg.Enable(IDFINDSTYLE, findInStyle);
			::SetDlgItemInt(hDlg, IDFINDSTYLE, SendEditor(SCI_GETSTYLEAT, SendEditor(SCI_GETCURRENTPOS)), FALSE);
		}
		dlg.SetItemText(IDREPLDONE, "0");
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
			return FALSE;
		} else if (ControlIDOfCommand(wParam) == IDFINDINSTYLE) {
			if (FindReplaceAdvanced()) {
				findInStyle = dlg.Checked(IDFINDINSTYLE);
				dlg.Enable(IDFINDSTYLE, findInStyle);
			}
			return TRUE;
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
	// Prevent reentrance when setting text
	static bool entered = false;
	if (entered)
		return FALSE;

	// Avoid getting dialog items before set up or during tear down.
	if (WM_SETFONT == message || WM_NCDESTROY == message)
		return FALSE;

	Dialog dlg(hDlg);

	switch (message) {

	case WM_INITDIALOG:{
		wFindIncrement = hDlg;
		LocaliseDialog(hDlg);
		SetWindowLong(hDlg, GWL_STYLE, WS_TABSTOP || GetWindowLong(hDlg, GWL_STYLE));
		dlg.SetItemTextU(IDC_INCFINDTEXT, ""); //findWhat.c_str()
		SetFocus(hDlg);

		PRectangle aRect = wFindIncrement.GetPosition();
		PRectangle aTBRect = wStatusBar.GetPosition();
		PRectangle aNewRect = aTBRect;
		aNewRect.top = aNewRect.bottom - (aRect.bottom - aRect.top);
		aNewRect.right = aNewRect.left + aRect.right - aRect.left;
		wFindIncrement.SetPosition(aNewRect);

		return TRUE;
	}

	case WM_SETFOCUS:
		return 0;

	case WM_CLOSE:
		::SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
		break;

	case WM_COMMAND:
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			props.Set("Replacements", "");
			UpdateStatusBar(false);
			::EndDialog(hDlg, IDCANCEL);
			wFindIncrement.Destroy();
			return FALSE;
		} else if (((ControlIDOfCommand(wParam) == IDC_INCFINDTEXT) && ((wParam >> 16) == 0x0300))
			|| (ControlIDOfCommand(wParam) == IDC_INCFINDBTNOK)) {
			SString ffLastWhat;
			ffLastWhat = findWhat;
			findWhat = dlg.ItemTextU(IDC_INCFINDTEXT);

			if (ControlIDOfCommand(wParam) != IDC_INCFINDBTNOK) {
				CharacterRange cr = GetSelection();
				if (ffLastWhat.length()) {
					SetSelection(cr.cpMin - ffLastWhat.length(), cr.cpMin - ffLastWhat.length());
				}
			}
			wholeWord = false;
			FindNext(false, false);
 			if ((!havefound) &&
				strncmp(findWhat.c_str(), ffLastWhat.c_str(), ffLastWhat.length()) == 0) {
				// Could not find string with added character so revert to previous value.
				findWhat = ffLastWhat;
				entered = true;
				dlg.SetItemTextU(IDC_INCFINDTEXT, findWhat.c_str());
				SendMessage(dlg.Item(IDC_INCFINDTEXT), EM_SETSEL, ffLastWhat.length(), ffLastWhat.length());
				entered = false;
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
	if (wFindIncrement.Created()) {
		wFindIncrement.Destroy();
		return;
	}

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
	if (isWindowsNT) {
		::CreateDialogParamW(hInstance,
		                                    (LPCWSTR)MAKEINTRESOURCE(IDD_FIND2),
		                                    MainHWND(),
		                                    reinterpret_cast<DLGPROC>(FindIncrementDlg),
		                                    reinterpret_cast<LPARAM>(this));
	} else {
		::CreateDialogParamA(hInstance,
		                                    MAKEINTRESOURCE(IDD_FIND2),
		                                    MainHWND(),
		                                    reinterpret_cast<DLGPROC>(FindIncrementDlg),
		                                    reinterpret_cast<LPARAM>(this));
	}
	wFindIncrement.Show();
}

bool SciTEWin::FindReplaceAdvanced() {
	return props.GetInt("find.replace.advanced");
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
	int dialog_id = FindReplaceAdvanced() ? IDD_FIND_ADV : IDD_FIND;

	if (isWindowsNT) {
		wFindReplace = ::CreateDialogParamW(hInstance,
		                                    (LPCWSTR)MAKEINTRESOURCE(dialog_id),
		                                    MainHWND(),
		                                    reinterpret_cast<DLGPROC>(FindDlg),
		                                    reinterpret_cast<LPARAM>(this));
	} else {
		wFindReplace = ::CreateDialogParamA(hInstance,
		                                    MAKEINTRESOURCE(dialog_id),
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
		dlg.SetItemText(IDFINDWHAT, props.Get("find.what").c_str());
		dlg.SetItemText(IDDIRECTORY, props.Get("find.directory").c_str());
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
		if (ControlIDOfCommand(wParam) == IDCANCEL) {
			::EndDialog(hDlg, IDCANCEL);
			wFindInFiles.Destroy();
			return FALSE;

		} else if (ControlIDOfCommand(wParam) == IDOK) {
			findWhat = dlg.ItemText(IDFINDWHAT);
			props.Set("find.what", findWhat.c_str());
			memFinds.Insert(findWhat.c_str());

			SString files = dlg.ItemText(IDFILES);
			props.Set("find.files", files.c_str());
			memFiles.Insert(files.c_str());

			SString directory = dlg.ItemText(IDDIRECTORY);
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
		} else if (ControlIDOfCommand(wParam) == IDDOTDOT) {
			FilePath directory(dlg.ItemText(IDDIRECTORY).c_str());
			directory = directory.Directory();
			dlg.SetItemText(IDDIRECTORY, directory.AsInternal());

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
				SString title = localiser.Text("Select a folder to search from");
				info.lpszTitle = title.c_str();
				info.ulFlags = 0;
				info.lpfn = BrowseCallbackProc;
				SString directory = dlg.ItemText(IDDIRECTORY);
				if (!directory.endswith(pathSepString)) {
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
					char szDir[MAX_PATH];
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
	if (wFindInFiles.Created())
		return;
	SelectionIntoFind();
	props.Set("find.what", findWhat.c_str());
	FilePath findInDir = filePath.Directory();
	props.Set("find.directory", findInDir.AsFileSystem());
	wFindInFiles = ::CreateDialogParam(hInstance, "Grep", MainHWND(),
		reinterpret_cast<DLGPROC>(GrepDlg), reinterpret_cast<sptr_t>(this));
	wFindInFiles.Show();
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
	int dialog_id = (!props.GetInt("find.replace.advanced") ? IDD_REPLACE : IDD_REPLACE_ADV);

	if (isWindowsNT) {
		wFindReplace = ::CreateDialogParamW(hInstance,
		                                    (LPCWSTR)MAKEINTRESOURCE(dialog_id),
		                                    MainHWND(),
		                                    reinterpret_cast<DLGPROC>(ReplaceDlg),
		                                    reinterpret_cast<sptr_t>(this));
	} else {
		wFindReplace = ::CreateDialogParamA(hInstance,
		                                    MAKEINTRESOURCE(dialog_id),
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
		} else if ((ControlIDOfCommand(wParam) == IDCONVERT) ||
			(ControlIDOfCommand(wParam) == IDOK)) {
			BOOL bOK;
			int tabSize = static_cast<int>(::GetDlgItemInt(hDlg, IDTABSIZE, &bOK, FALSE));
			if (tabSize > 0)
				SendEditor(SCI_SETTABWIDTH, tabSize);
			int indentSize = static_cast<int>(::GetDlgItemInt(hDlg, IDINDENTSIZE, &bOK, FALSE));
			if (indentSize > 0)
				SendEditor(SCI_SETINDENT, indentSize);
			bool useTabs = static_cast<bool>(::IsDlgButtonChecked(hDlg, IDUSETABS));
			SendEditor(SCI_SETUSETABS, useTabs);
			if (ControlIDOfCommand(wParam) == IDCONVERT) {
				ConvertIndentation(tabSize, useTabs);
			}
			::EndDialog(hDlg, ControlIDOfCommand(wParam));
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

bool SciTEWin::ParametersOpen() {
	return wParameters.Created();
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
			Dialog dlg(hDlg);
			if (modalParameters) {
				dlg.SetItemText(IDCMD, parameterisedCommand.c_str());
			}
			for (int param = 0; param < maxParam; param++) {
				SString paramText(param + 1);
				SString paramTextVal = props.Get(paramText.c_str());
				dlg.SetItemText(IDPARAMSTART + param, paramTextVal.c_str());
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

void SciTEWin::FindMessageBox(const SString &msg, const SString *findItem) {

	if (findItem == 0) {
		SString msgBuf = LocaliseMessage(msg.c_str());
		WindowMessageBox(wFindReplace.Created() ? wFindReplace : wSciTE, msgBuf, MB_OK | MB_ICONWARNING);
	} else {
		if (isWindowsNT) {

			SString sFormat = localiser.Text(msg.c_str());
			SString sPart1 = sFormat.substr(0, sFormat.search("^0", 0));
			SString sPart2 = sFormat.substr(sFormat.search("^0", 0));
			sPart2 = sPart2.substr(2); // skip initial ^0

			WCHAR wszPart1[256];
			::MultiByteToWideChar(CP_ACP, 0, sPart1.c_str(), -1, wszPart1, 256);

			WCHAR wszPart2[256];
			::MultiByteToWideChar(CP_ACP, 0, sPart2.c_str(), -1, wszPart2, 256);

			WCHAR wszFindItem[CTL_TEXT_BUF];
			::MultiByteToWideChar(CP_UTF8, 0, findItem->c_str(), -1, wszFindItem, CTL_TEXT_BUF);

			WCHAR wszAppName[64];
			::MultiByteToWideChar(CP_ACP, 0, appName, -1, wszAppName, 64);

			WCHAR wszOutput[1024];
			::lstrcpyW(wszOutput, wszPart1);
			::lstrcatW(wszOutput, wszFindItem);
			::lstrcatW(wszOutput, wszPart2);
			::MessageBoxW(
			    wFindReplace.Created() ? (HWND)wFindReplace.GetID() : (HWND)wSciTE.GetID(),
			    wszOutput, wszAppName, MB_OK | MB_ICONWARNING);
		} else {
			SString msgBuf = LocaliseMessage(msg.c_str(), findItem->c_str());
			WindowMessageBox(wFindReplace.Created() ? wFindReplace : wSciTE, msgBuf, MB_OK | MB_ICONWARNING);
		}
	}
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
