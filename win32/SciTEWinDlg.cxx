// SciTE - Scintilla based Text Editor
// SciTEWinDlg.cxx - dialog code for the Windows version of the editor
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include "SciTEWin.h"

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
		OPENFILENAME ofn = {
		    sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};
		ofn.hwndOwner = wSciTE.GetID();
		ofn.hInstance = hInstance;
		ofn.lpstrFile = openName;
		ofn.nMaxFile = sizeof(openName);
		ofn.lpstrTitle = "Save File";
		ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
		ofn.lpstrInitialDir = dirName;

		dialogsOnScreen++;
		choseOK = ::GetSaveFileName(&ofn);
		if (choseOK) {
			//Platform::DebugPrintf("Save: <%s>\n", openName);
			SetFileName(openName, false); // don't fix case
			Save();
			ReadProperties();
			// In case extension was changed
			SendEditor(SCI_COLOURISE, 0, -1);
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
		OPENFILENAME ofn = {
		    sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};
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

void SciTEWin::SaveAsRTF() {
	if (0 == dialogsOnScreen) {
		char saveName[MAX_PATH] = "\0";
		strcpy(saveName, fileName);
		char *cpDot = strrchr(saveName, '.');
		if (cpDot != NULL)
			strcpy(cpDot, ".rtf");
		else
			strcat(saveName, ".rtf");
		OPENFILENAME ofn = {
		    sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};
		ofn.hwndOwner = wSciTE.GetID();
		ofn.hInstance = hInstance;
		ofn.lpstrFile = saveName;
		ofn.nMaxFile = sizeof(saveName);
		ofn.lpstrTitle = "Save File As RTF";
		ofn.Flags = OFN_HIDEREADONLY;

		ofn.lpstrFilter = "RTF (.rtf)\0*.rtf\0";

		dialogsOnScreen++;
		if (::GetSaveFileName(&ofn)) {
			//Platform::DebugPrintf("Save As RTF: <%s>\n", saveName);
			SaveToRTF(saveName);
		}
		dialogsOnScreen--;
	}
}

void SciTEWin::SaveAsPDF() {
	if (0 == dialogsOnScreen) {
		char saveName[MAX_PATH] = "\0";
		strcpy(saveName, fileName);
		char *cpDot = strchr(saveName, '.');
		if (cpDot != NULL)
			strcpy(cpDot, ".pdf");
		else
			strcat(saveName, ".pdf");
		OPENFILENAME ofn = {
		    sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};
		ofn.hwndOwner = wSciTE.GetID();
		ofn.hInstance = hInstance;
		ofn.lpstrFile = saveName;
		ofn.nMaxFile = sizeof(saveName);
		ofn.lpstrTitle = "Save File As PDF";
		ofn.Flags = OFN_HIDEREADONLY;

		ofn.lpstrFilter = "PDF (.pdf)\0*.pdf\0";

		dialogsOnScreen++;
		if (::GetSaveFileName(&ofn)) {
			Platform::DebugPrintf("Save As PDF: <%s>\n", saveName);
			SaveToPDF(saveName);
		}
		dialogsOnScreen--;
	}
}

void SciTEWin::Print(bool showDialog) {

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
	int startPos = 0;
	int endPos = 0;

	if (SendEditor(EM_GETSEL,
	               reinterpret_cast<WPARAM>(&startPos),
	               reinterpret_cast<LPARAM>(&endPos)) == 0)
		pdlg.Flags |= PD_NOSELECTION;
	if (!showDialog)
		pdlg.Flags |= PD_RETURNDEFAULT;
	if (!::PrintDlg(&pdlg)) {
		return ;
	}

	hDevMode = pdlg.hDevMode;
	hDevNames = pdlg.hDevNames;

	HDC hdc = pdlg.hDC;

	PRectangle rectMargins;
	Point ptPage;

	// Start by getting the dimensions of the unprintable
	// part of the page (in device units).

	rectMargins.left = GetDeviceCaps(hdc, PHYSICALOFFSETX);
	rectMargins.top = GetDeviceCaps(hdc, PHYSICALOFFSETY);

	// To get the right and lower unprintable area, we need to take
	// the entire width and height of the paper and
	// subtract everything else.

	// Get the physical page size (in device units).
	ptPage.x = GetDeviceCaps(hdc, PHYSICALWIDTH);   // device units
	ptPage.y = GetDeviceCaps(hdc, PHYSICALHEIGHT);  // device units

	rectMargins.right = ptPage.x                       // total paper width
	                    - GetDeviceCaps(hdc, HORZRES) // printable width
	                    - rectMargins.left;           // left unprtable margin

	rectMargins.bottom = ptPage.y                       // total paper height
	                     - GetDeviceCaps(hdc, VERTRES) // printable ht
	                     - rectMargins.top;            // rt unprtable margin

	// At this point, rectMargins contains the widths of the
	// unprintable regions on all four sides of the page in device units.

	if (pagesetupMargin.left != 0 || pagesetupMargin.right != 0 ||
	        pagesetupMargin.top != 0 || pagesetupMargin.bottom != 0) {
		PRectangle rectSetup;
		Point ptDpi;

		// Convert the HiMetric margin values from the Page Setup dialog
		// to device units and subtract the unprintable part we just
		// calculated. (2540 tenths of a mm in an inch)

		ptDpi.x = GetDeviceCaps(hdc, LOGPIXELSX);    // dpi in X direction
		ptDpi.y = GetDeviceCaps(hdc, LOGPIXELSY);    // dpi in Y direction

		rectSetup.left = MulDiv (pagesetupMargin.left, ptDpi.x, 2540);
		rectSetup.top = MulDiv (pagesetupMargin.top, ptDpi.y, 2540);
		rectSetup.right = ptPage.x - MulDiv (pagesetupMargin.right, ptDpi.x, 2540);
		rectSetup.bottom = ptPage.y - MulDiv (pagesetupMargin.bottom, ptDpi.y, 2540);

		// Dont reduce margins below the minimum printable area
		rectMargins.left = Platform::Maximum(rectMargins.left, rectSetup.left);
		rectMargins.top = Platform::Maximum(rectMargins.top, rectSetup.top);
		rectMargins.right = Platform::Minimum(rectMargins.right, rectSetup.right);
		rectMargins.bottom = Platform::Minimum(rectMargins.bottom, rectSetup.bottom);
	}

	// rectMargins now contains the values used to shrink the printable
	// area of the page.

	// Convert to logical units
	DPtoLP(hdc, (LPPOINT) &rectMargins, 2);

	// Convert page size to logical units and we're done!
	DPtoLP(hdc, (LPPOINT) &ptPage, 1);

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
			lengthDoc = startPos;
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

		RangeToFormat frPrint;
		frPrint.hdc = hdc;
		frPrint.hdcTarget = hdc;
		frPrint.rc.left = rectMargins.left;
		frPrint.rc.top = rectMargins.top;
		frPrint.rc.right = ptPage.x - (rectMargins.right + rectMargins.left);
		frPrint.rc.bottom = ptPage.y - (rectMargins.bottom + rectMargins.top);
		frPrint.rcPage.left = 0;
		frPrint.rcPage.top = 0;
		frPrint.rcPage.right = ptPage.x;
		frPrint.rcPage.bottom = ptPage.y;
		frPrint.chrg.cpMin = lengthPrinted;
		frPrint.chrg.cpMax = lengthDoc;

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
	if (WM_SETFONT == message)  // Avoid getting dialog items before set up
		return FALSE;
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
			sci->FindNext(sci->reverseFind);
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
		FindNext(reverseFind);
	} else if (cmd == IDREPLACE) {
		if (havefound) {
			SendEditorString(EM_REPLACESEL, 0, replaceWhat);
			havefound = false;
		}
		FindNext(reverseFind);
	} else if (cmd == IDREPLACEALL) {
		ReplaceAll();
	}

	return TRUE;
}

BOOL CALLBACK SciTEWin::ReplaceDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static SciTEWin *sci;
	if (WM_SETFONT == message)  // Avoid getting dialog items before set up
		return FALSE;
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
	lineNo[0] = SendEditor(EM_LINEFROMCHAR, static_cast<WPARAM>( -1), 0L) + 1;
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

