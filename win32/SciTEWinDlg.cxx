// SciTE - Scintilla based Text Editor
/** @file SciTEWinDlg.cxx
 ** Dialog code for the Windows version of the editor.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include "SciTEWin.h"

/**
 * Flash the given window for the asked @a duration to visually warn the user.
 */
void FlashThisWindow(
    HWND hWnd, 		///< Window to flash handle.
    int duration) {	///< Duration of the flash state.

	HDC hDC;

	hDC = ::GetDC(hWnd);
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
void PlayThisSound(
    const char *sound, 	///< Path to a .wav file or string with a frequency value.
    int duration, 		///< If @a sound is a frequency, gives the duration of the sound.
    HMODULE &hMM) {		///< Multimedia DLL handle.

	bool bPlayOK = false;
	int soundFreq;
	if (*sound == '\0') {
		soundFreq = -1;	// No sound at all
	}
	else {
		soundFreq = atoi(sound);	// May be a frequency, not a filename
	}

	if (soundFreq == 0) {	// sound is probably a path
		if (!hMM) {
			// Load the DLL only if requested by the user
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
		// soundFreq and duration are not used on Win9x
		// On those systems, PC will either use the default sound event or emit a standard speaker sound
		::Beep(soundFreq, duration);
	}

}

void SciTEWin::WarnUser(int warnID) {
	SString warning;
	char *warn, *next;
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
	next = GetNextPropItem(warn, flashDuration, 10);
	next = GetNextPropItem(next, sound, _MAX_PATH);
	GetNextPropItem(next, soundDuration, 10);
	delete []warn;

	int flashLen = atoi(flashDuration);
	if (flashLen) {
		FlashThisWindow(wEditor.GetID(), flashLen);
	}
	PlayThisSound(sound, atoi(soundDuration), hMM);
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
int PASCAL DefaultDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {

	case WM_INITDIALOG:
		{
			HWND wsci = GetDlgItem(hDlg, IDABOUTSCINTILLA);
			SetAboutMessage(wsci, lParam ? "Sc1  " : "SciTE");
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
	if (lpProc == NULL)
		lpProc = reinterpret_cast<DLGPROC>(DefaultDlg);

	int result;
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

#define MULTISELECTOPEN

bool SciTEWin::OpenDialog() {

#ifdef MULTISELECTOPEN
	char openName[2048]; // maximum common dialog buffer size (says mfc..)
#else
	char openName[MAX_PATH];
#endif
	*openName = '\0';

	OPENFILENAME ofn = {
	    sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	ofn.hwndOwner = wSciTE.GetID();
	ofn.hInstance = hInstance;
	ofn.lpstrFile = openName;
	ofn.nMaxFile = sizeof(openName);
	char *filter = 0;
	SString openFilter = props.GetExpanded("open.filter");
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

#ifdef MULTISELECTOPEN
	if (buffers.size > 1) {
		ofn.Flags |=
		    OFN_EXPLORER |
		    OFN_PATHMUSTEXIST |
		    OFN_ALLOWMULTISELECT;
	}
#endif
	if (::GetOpenFileName(&ofn)) {
		filterDefault = ofn.nFilterIndex;
		delete []filter;
		//Platform::DebugPrintf("Open: <%s>\n", openName);
#ifdef MULTISELECTOPEN
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
			int len = path.length();
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
#else
		Open(openName);
#endif
	} else {
		delete []filter;
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
			if (cpDot != NULL)
				strcpy(cpDot, ext);
			else
				strcat(saveName, ext);
		}
		OPENFILENAME ofn = {
		    sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};
		ofn.hwndOwner = wSciTE.GetID();
		ofn.hInstance = hInstance;
		ofn.lpstrFile = saveName;
		ofn.nMaxFile = sizeof(saveName);
		ofn.lpstrTitle = title;
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

void SciTEWin::SaveAsHTML() {
	SString path = ChooseSaveName("Save File As HTML", 
		"Web (.html;.htm)\0*.html;*.htm\0", ".html");
	if (path.length()) {
		SaveToHTML(path.c_str());
	}
}

void SciTEWin::SaveAsRTF() {
	SString path = ChooseSaveName("Save File As RTF", 
		"RTF (.rtf)\0*.rtf\0", ".rtf");
	if (path.length()) {
		SaveToRTF(path.c_str());
	}
}

void SciTEWin::SaveAsPDF() {
	SString path = ChooseSaveName("Save File As PDF", 
		"PDF (.pdf)\0*.pdf\0", ".pdf");
	if (path.length()) {
		SaveToPDF(path.c_str());
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
		return ;
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
	SString headerOrFooter;
	headerOrFooter.assign("", MAX_PATH + 100);	// Usually the path, date et page number

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
	di.lpszDocName = windowName;
	di.lpszOutput = 0;
	di.lpszDatatype = 0;
	di.fwType = 0;
	if (::StartDoc(hdc, &di) < 0) {
		MessageBox(wSciTE.GetID(), "Can not start printer document.", 0, MB_OK);
		return ;
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
				::SetTextColor(hdc, sdHeader.fore.AsLong());
				::SetBkColor(hdc, sdHeader.back.AsLong());
				::SelectObject(hdc, fontHeader);
				UINT ta = ::SetTextAlign(hdc, TA_BOTTOM);
				RECT rcw = {frPrint.rc.left, frPrint.rc.top - headerLineHeight - headerLineHeight / 2,
				            frPrint.rc.right, frPrint.rc.top - headerLineHeight / 2};
				rcw.bottom = rcw.top + headerLineHeight;
				::ExtTextOut(hdc, frPrint.rc.left + 5, frPrint.rc.top - headerLineHeight / 2,
				             ETO_OPAQUE, &rcw, sHeader.c_str(), sHeader.length(), NULL);
				::SetTextAlign(hdc, ta);
				HPEN pen = ::CreatePen(0, 1, sdHeader.fore.AsLong());
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
				::SetTextColor(hdc, sdFooter.fore.AsLong());
				::SetBkColor(hdc, sdFooter.back.AsLong());
				::SelectObject(hdc, fontFooter);
				UINT ta = ::SetTextAlign(hdc, TA_TOP);
				RECT rcw = {frPrint.rc.left, frPrint.rc.bottom + footerLineHeight / 2,
				            frPrint.rc.right, frPrint.rc.bottom + footerLineHeight + footerLineHeight / 2};
				::ExtTextOut(hdc, frPrint.rc.left + 5, frPrint.rc.bottom + footerLineHeight / 2,
				             ETO_OPAQUE, &rcw, sFooter.c_str(), sFooter.length(), NULL);
				::SetTextAlign(hdc, ta);
				HPEN pen = ::CreatePen(0, 1, sdFooter.fore.AsLong());
				HPEN penOld = static_cast<HPEN>(::SelectObject(hdc, pen));
				::SetBkColor(hdc, sdFooter.fore.AsLong());
				::MoveToEx(hdc, frPrint.rc.left, frPrint.rc.bottom + footerLineHeight / 4, NULL);
				::LineTo(hdc, frPrint.rc.right, frPrint.rc.bottom + footerLineHeight / 4);
				::SelectObject(hdc, penOld);
				::DeleteObject(pen);
			}

#ifdef DEBUG_PRINT
			// Print physical margins
			MoveToEx(hdc, frPrint.rcPage.left, frPrint.rcPage.top, NULL);
			LineTo(hdc, frPrint.rcPage.right, frPrint.rcPage.top);
			LineTo(hdc, frPrint.rcPage.right, frPrint.rcPage.bottom);
			LineTo(hdc, frPrint.rcPage.left, frPrint.rcPage.bottom);
			LineTo(hdc, frPrint.rcPage.left, frPrint.rcPage.top);
			// Print setup margins
			MoveToEx(hdc, frPrint.rc.left, frPrint.rc.top, NULL);
			LineTo(hdc, frPrint.rc.right, frPrint.rc.top);
			LineTo(hdc, frPrint.rc.right, frPrint.rc.bottom);
			LineTo(hdc, frPrint.rc.left, frPrint.rc.bottom);
			LineTo(hdc, frPrint.rc.left, frPrint.rc.top);
#endif

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

	pdlg.hwndOwner = wSciTE.GetID();
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
		return ;

	pagesetupMargin.left = pdlg.rtMargin.left;
	pagesetupMargin.top = pdlg.rtMargin.top;
	pagesetupMargin.right = pdlg.rtMargin.right;
	pagesetupMargin.bottom = pdlg.rtMargin.bottom;

	hDevMode = pdlg.hDevMode;
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
		sci = reinterpret_cast<SciTEWin *>(lParam);
		::SetDlgItemText(hDlg, IDFINDWHAT, sci->findWhat);
		FillComboFromMemory(wFindWhat, sci->memFinds);
		if (sci->wholeWord)
			::SendMessage(wWholeWord, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->matchCase)
			::SendMessage(wMatchCase, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->regExp)
			::SendMessage(wRegExp, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->wrapFind)
			::SendMessage(wWrap, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->unSlash)
			::SendMessage(wUnSlash, BM_SETCHECK, BST_CHECKED, 0);
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
			sci->regExp = BST_CHECKED ==
			              ::SendMessage(wRegExp, BM_GETCHECK, 0, 0);
			sci->wrapFind = BST_CHECKED ==
			              ::SendMessage(wWrap, BM_GETCHECK, 0, 0);
			sci->unSlash = BST_CHECKED ==
			               ::SendMessage(wUnSlash, BM_GETCHECK, 0, 0);
			sci->reverseFind = BST_CHECKED ==
			                   ::SendMessage(wUp, BM_GETCHECK, 0, 0);
			sci->wFindReplace = 0;
			EndDialog(hDlg, IDOK);
			sci->FindNext(sci->reverseFind);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL SciTEWin::HandleReplaceCommand(int cmd) {
	if (!wFindReplace.GetID())
		return TRUE;
	HWND wWholeWord = ::GetDlgItem(wFindReplace.GetID(), IDWHOLEWORD);
	HWND wMatchCase = ::GetDlgItem(wFindReplace.GetID(), IDMATCHCASE);
	HWND wRegExp = ::GetDlgItem(wFindReplace.GetID(), IDREGEXP);
	HWND wWrap = ::GetDlgItem(wFindReplace.GetID(), IDWRAP);
	HWND wUnSlash = ::GetDlgItem(wFindReplace.GetID(), IDUNSLASH);

	if ((cmd == IDOK) || (cmd == IDREPLACE) || (cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL)) {
		::GetDlgItemText(wFindReplace.GetID(), IDFINDWHAT, findWhat, sizeof(findWhat));
		props.Set("find.what", findWhat);
		memFinds.Insert(findWhat);
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
	if ((cmd == IDREPLACE) || (cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL)) {
		::GetDlgItemText(wFindReplace.GetID(), IDREPLACEWITH, replaceWhat, sizeof(replaceWhat));
		memReplaces.Insert(replaceWhat);
	}

	if (cmd == IDOK) {
		FindNext(reverseFind);
	} else if (cmd == IDREPLACE) {
		if (havefound) {
			ReplaceOnce();
		} else {
			FindNext(reverseFind);
		}
	} else if ((cmd == IDREPLACEALL) || (cmd == IDREPLACEINSEL)) {
		ReplaceAll(cmd == IDREPLACEINSEL);
	}

	return TRUE;
}

BOOL CALLBACK SciTEWin::ReplaceDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static SciTEWin *sci;
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
		sci = reinterpret_cast<SciTEWin *>(lParam);
		::SetDlgItemText(hDlg, IDFINDWHAT, sci->findWhat);
		FillComboFromMemory(wFindWhat, sci->memFinds);
		::SetDlgItemText(hDlg, IDREPLACEWITH, sci->replaceWhat);
		FillComboFromMemory(wReplaceWith, sci->memReplaces);
		if (sci->wholeWord)
			::SendMessage(wWholeWord, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->matchCase)
			::SendMessage(wMatchCase, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->regExp)
			::SendMessage(wRegExp, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->wrapFind)
			::SendMessage(wWrap, BM_SETCHECK, BST_CHECKED, 0);
		if (sci->unSlash)
			::SendMessage(wUnSlash, BM_SETCHECK, BST_CHECKED, 0);
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
		return ;
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
		return ;
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
	lineNo[0] = GetCurrentLineNumber();
	lineNo[1] = SendEditor(SCI_GETLINECOUNT, 0, 0L);
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
			GotoLineEnsureVisible(lineNo[0]-1);
			//}
		}

	}
	SetFocus(wEditor.GetID());
}

BOOL CALLBACK SciTEWin::TabSizeDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static IndentationSettings *pSettings;

	switch (message) {

	case WM_INITDIALOG:
		pSettings = reinterpret_cast<IndentationSettings *>(lParam);
		SendDlgItemMessage(hDlg, IDTABSIZE, EM_LIMITTEXT, 2, 1);
		char tmp[3];
		if (pSettings->tabSize > 99)
			pSettings->tabSize = 99;
		sprintf(tmp, "%d", pSettings->tabSize);
		SendDlgItemMessage(hDlg, IDTABSIZE, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(tmp));

		SendDlgItemMessage(hDlg, IDINDENTSIZE, EM_LIMITTEXT, 2, 1);
		if (pSettings->indentSize > 99)
			pSettings->indentSize = 99;
		sprintf(tmp, "%d", pSettings->indentSize);
		SendDlgItemMessage(hDlg, IDINDENTSIZE, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(tmp));

		CheckDlgButton(hDlg, IDUSETABS, pSettings->useTabs);
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
			pSettings->tabSize = static_cast<int>(GetDlgItemInt(hDlg, IDTABSIZE, &bOK, FALSE));
			pSettings->indentSize = static_cast<int>(GetDlgItemInt(hDlg, IDINDENTSIZE, &bOK, FALSE));
			pSettings->useTabs = static_cast<bool>(IsDlgButtonChecked(hDlg, IDUSETABS));
			//			if (!bOK)
			//				pTabSize = 0;
			EndDialog(hDlg, IDOK);
			return TRUE;
		}
	}

	return FALSE;
}

void SciTEWin::TabSizeDialog() {
	IndentationSettings settings;

	settings.tabSize = SendEditor(SCI_GETTABWIDTH);
	settings.indentSize = SendEditor(SCI_GETINDENT);
	settings.useTabs = SendEditor(SCI_GETUSETABS);
	if (DoDialog(hInstance, "TabSize", wSciTE.GetID(),
	             reinterpret_cast<DLGPROC>(TabSizeDlg),
	             reinterpret_cast<DWORD>(&settings)) == IDOK) {
		if (settings.tabSize > 0)
			SendEditor(SCI_SETTABWIDTH, settings.tabSize);
		if (settings.indentSize > 0)
			SendEditor(SCI_SETINDENT, settings.indentSize);
		SendEditor(SCI_SETUSETABS, settings.useTabs);
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

void SciTEWin::AboutDialogWithBuild(int staticBuild) {
	DoDialog(hInstance, "About", wSciTE.GetID(), NULL, staticBuild);
	::SetFocus(wEditor.GetID());
}

