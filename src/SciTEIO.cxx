// SciTE - Scintilla based Text Editor
// SciTESave.cxx - manage input and output with the system
// Copyright 1998-2000 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
//#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>	// For time_t

#include "Platform.h"

#if PLAT_GTK

#include <unistd.h>

#endif

#if PLAT_WIN
// For getcwd and chdir
#ifdef _MSC_VER
#include <direct.h>
#endif
#ifdef __BORLANDC__
#include <dir.h>
#endif

#endif

#include "SciTE.h"
#include "PropSet.h"
#include "Accessor.h"
#include "WindowAccessor.h"
//#include "KeyWords.h"
//#include "ScintillaWidget.h"
#include "Scintilla.h"
//#include "SciLexer.h"
#include "Extender.h"
#include "SciTEBase.h"

void SciTEBase::FixFilePath() {
	// Only used on Windows to fix the case of file names
}

void SciTEBase::SetFileName(const char *openName, bool fixCase) {
	if (openName[0] == '\"') {
		char pathCopy[MAX_PATH + 1];
		pathCopy[0] = '\0';
		strncpy(pathCopy, openName + 1, MAX_PATH);
		pathCopy[MAX_PATH] = '\0';
		if (pathCopy[strlen(pathCopy) - 1] == '\"')
			pathCopy[strlen(pathCopy) - 1] = '\0';
		AbsolutePath(fullPath, pathCopy, MAX_PATH);
	} else if (openName[0]) {
		AbsolutePath(fullPath, openName, MAX_PATH);
	} else {
		fullPath[0] = '\0';
	}

	char *cpDirEnd = strrchr(fullPath, pathSepChar);
	if (cpDirEnd) {
		strcpy(fileName, cpDirEnd + 1);
		strcpy(dirName, fullPath);
		dirName[cpDirEnd - fullPath] = '\0';
		//Platform::DebugPrintf("SetFileName: <%s> <%s>\n", fileName, dirName);
	}
	else {
		strcpy(fileName, fullPath);
		getcwd(dirName, sizeof(dirName));
		//Platform::DebugPrintf("Working directory: <%s>\n", dirName);
		strcpy(fullPath, dirName);
		strcat(fullPath, pathSepString);
		strcat(fullPath, fileName);
	}
	if (fixCase)
		FixFilePath();
	char fileBase[MAX_PATH];
	strcpy(fileBase, fileName);
	char *extension = strrchr(fileBase, '.');
	if (extension) {
		*extension = '\0';
		strcpy(fileExt, extension + 1);
	} else { // No extension
		fileExt[0] = '\0';
	}

	chdir(dirName);

	ReadLocalPropFile();

	props.Set("FilePath", fullPath);
	props.Set("FileDir", dirName);
	props.Set("FileName", fileBase);
	props.Set("FileExt", fileExt);
	props.Set("FileNameExt", fileName);

	SetWindowName();
	if (buffers.buffers)
		buffers.buffers[buffers.current].fileName = fullPath;
	BuffersMenu();
}

bool SciTEBase::Exists(const char *dir, const char *path, char *testPath) {
	char copyPath[MAX_PATH];
	if (dir) {
		if ((strlen(dir) + strlen(pathSepString) + strlen(path) + 1) > MAX_PATH)
			return false;
		strcpy(copyPath, dir);
		strcat(copyPath, pathSepString);
		strcat(copyPath, path);
	} else {
		strcpy(copyPath, path);
	}
#if PLAT_GTK
	if(path[0] == '/')// absolute path
		strcpy(copyPath, path);
#endif
	FILE *fp = fopen(copyPath, "rb");
	if (!fp)
		return false;
	fclose(fp);
	AbsolutePath(testPath, copyPath, MAX_PATH);
	return true;
}

time_t GetModTime(const char *fullPath) {
	struct stat statusFile;
	if (stat(fullPath, &statusFile) != -1)
		return statusFile.st_mtime;
	else
		return 0;
}

void SciTEBase::CountLineEnds(int &linesCR, int &linesLF, int &linesCRLF) {
	linesCR = 0;
	linesLF = 0;
	linesCRLF = 0;
	int lengthDoc = LengthDocument();
	char chPrev = ' ';
	WindowAccessor acc(wEditor.GetID(), props);
	for (int i = 0; i < lengthDoc; i++) {
		char ch = acc[i];
		char chNext = acc.SafeGetCharAt(i+1);
		if (ch == '\r') {
			if (chNext == '\n')
				linesCRLF++;
			else 
				linesCR++;
		} else if (ch == '\n') {
			if (chPrev != '\r') {
				linesLF++;
			}
		}
		chPrev = ch;
	}
}

void SciTEBase::Open(const char *file, bool initialCmdLine) {

	InitialiseBuffers();

	if (!file) {
		MessageBox(wSciTE.GetID(), "Bad file", appName, MB_OK);
	}

	int index = buffers.GetDocumentByName(file);
	if (index >= 0) {
		SetDocumentAt(index);
		DeleteFileStackMenu();
		SetFileStackMenu();
		return ;
	}

	if (buffers.size == buffers.length) {
		AddFileToStack(fullPath, GetSelection(), GetCurrentScrollPosition());
		ClearDocument();
	} else {
		New();
	}

	//Platform::DebugPrintf("Opening %s\n", file);
	SetFileName(file);
	overrideExtension = "";
	ReadProperties();
	UpdateBuffersCurrent();

	if (initialCmdLine) {
		if (props.GetInt("save.recent", 0))
			LoadRecentMenu();
	}
	
	if (fileName[0]) {
		SendEditor(SCI_CANCEL);
		SendEditor(SCI_SETUNDOCOLLECTION, 0);

		fileModTime = GetModTime(fullPath);

		FILE *fp = fopen(fullPath, "rb");
		if (fp || initialCmdLine) {
			if (fp) {
				char data[blockSize];
				int lenFile = fread(data, 1, sizeof(data), fp);
				while (lenFile > 0) {
					SendEditorString(SCI_ADDTEXT, lenFile, data);
					lenFile = fread(data, 1, sizeof(data), fp);
				}
				fclose(fp);
				int linesCR;
				int linesLF;
				int linesCRLF;
				CountLineEnds(linesCR, linesLF, linesCRLF);
				if (props.GetInt("eol.auto")) {
					if ((linesLF > linesCR) && (linesLF > linesCRLF))
						SendEditor(SCI_SETEOLMODE, SC_EOL_LF);
					if ((linesCR > linesLF) && (linesCR > linesCRLF))
						SendEditor(SCI_SETEOLMODE, SC_EOL_CR);
					if ((linesCRLF > linesLF) && (linesCRLF > linesCR))
						SendEditor(SCI_SETEOLMODE, SC_EOL_CRLF);
				}
			}
		} else {
			char msg[MAX_PATH + 100];
			strcpy(msg, "Could not open file \"");
			strcat(msg, fullPath);
			strcat(msg, "\".");
			MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
		}
		SendEditor(SCI_SETUNDOCOLLECTION, 1);
		// Flick focus to the output window and back to
		// ensure palette realised correctly.
		SetFocus(wOutput.GetID());
		SetFocus(wEditor.GetID());
		SendEditor(SCI_EMPTYUNDOBUFFER);
		SendEditor(SCI_SETSAVEPOINT);
		if (props.GetInt("fold.on.open") > 0) {
			FoldAll();
		}
		SendEditor(SCI_GOTOPOS, 0);
	}
	Redraw();
	RemoveFileFromStack(fullPath);
	DeleteFileStackMenu();
	SetFileStackMenu();
	SetWindowName();
	if (extender)
		extender->OnOpen();
}

void SciTEBase::Revert() {
	FILE *fp = fopen(fullPath, "rb");

	if (fp) {
		SendEditor(SCI_CANCEL);
		SendEditor(SCI_CLEARALL);
		fileModTime = GetModTime(fullPath);
		char data[blockSize];
		int lenFile = fread(data, 1, sizeof(data), fp);
		while (lenFile > 0) {
			SendEditorString(SCI_ADDTEXT, lenFile, data);
			lenFile = fread(data, 1, sizeof(data), fp);
		}
		fclose(fp);
		SendEditor(SCI_SETUNDOCOLLECTION, 1);
		// Flick focus to the output window and back to
		// ensure palette realised correctly.
		SendEditor(SCI_SETSAVEPOINT);
		SendEditor(SCI_GOTOPOS, 0);
		Redraw();
	} else {
		char msg[MAX_PATH + 100];
		strcpy(msg, "Could not revert file \"");
		strcat(msg, fullPath);
		strcat(msg, "\".");
		MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
	}
}

int SciTEBase::SaveIfUnsure(bool forceQuestion) {
	if (isDirty) {
		if (props.GetInt("are.you.sure", 1) ||
		        IsUntitledFileName(fullPath) ||
		        forceQuestion) {
			char msg[MAX_PATH + 100];
			if (fileName[0]) {
				strcpy(msg, "Save changes to \"");
				strcat(msg, fullPath);
				strcat(msg, "\"?");
			} else {
				strcpy(msg, "Save changes to (Untitled)?");
			}
			dialogsOnScreen++;
			int decision = MessageBox(wSciTE.GetID(), msg, appName, MB_YESNOCANCEL);
			dialogsOnScreen--;
			if (decision == IDYES) {
				if (!Save())
					decision = IDCANCEL;
			}
			return decision;
		} else {
			if (!Save())
				return IDCANCEL;
		}
	}
	return IDYES;
}

int SciTEBase::SaveIfUnsureAll(bool forceQuestion) {
	UpdateBuffersCurrent();	// Ensure isDirty copied
	for (int i = 0; i < buffers.length; i++) {
		if (buffers.buffers[i].isDirty) {
			SetDocumentAt(i);
			if (SaveIfUnsure(forceQuestion) == IDCANCEL)
				return IDCANCEL;
		}
	}
	if (props.GetInt("save.recent", 0))
		SaveRecentStack();
	// Definitely going to exit now, so delete all documents
	// Set editor back to initial document
	SendEditor(SCI_SETDOCPOINTER, 0, buffers.buffers[0].doc);
	// Release all the extra documents
	for (int j = 0; j < buffers.size; j++) {
		if (buffers.buffers[j].doc)
			SendEditor(SCI_RELEASEDOCUMENT, 0, buffers.buffers[j].doc);
	}
	// Initial document will be deleted when editor deleted
	return IDYES;
}

int SciTEBase::SaveIfUnsureForBuilt() {
	if (isDirty) {
		if (props.GetInt("are.you.sure.for.build", 0))
			return SaveIfUnsure(true);

		Save();
	}
	return IDYES;
}

int StripTrailingSpaces(char *data, int ds, bool lastBlock) {
	int lastRead = 0;
	char *w = data;

	for (int i = 0; i < ds; i++) {
		char ch = data[i];
		if ((ch == ' ') || (ch == '\t')) {
			// Swallow those spaces
		}
		else if ((ch == '\r') || (ch == '\n')) {
			*w++ = ch;
			lastRead = i + 1;
		} else {
			while (lastRead < i) {
				*w++ = data[lastRead++];
			}
			*w++ = ch;
			lastRead = i + 1;
		}
	}
	// If a non-final block, then preserve whitespace at end of block as it may be significant.
	if (!lastBlock) {
		while (lastRead < ds) {
			*w++ = data[lastRead++];
		}
	}
	return w - data;
}

// Returns false only if cancelled
bool SciTEBase::Save() {
	if (fileName[0]) {

		if (props.GetInt("save.deletes.first")) {
			unlink(fullPath);
		}

		//Platform::DebugPrintf("Saving <%s><%s>\n", fileName, fullPath);
		//DWORD dwStart = timeGetTime();
		FILE *fp = fopen(fullPath, "wb");
		if (fp) {
			char data[blockSize + 1];
			int lengthDoc = LengthDocument();
			for (int i = 0; i < lengthDoc; i += blockSize) {
				int grabSize = lengthDoc - i;
				if (grabSize > blockSize)
					grabSize = blockSize;
				GetRange(wEditor, i, i + grabSize, data);
				if (props.GetInt("strip.trailing.spaces"))
					grabSize = StripTrailingSpaces(
					               data, grabSize, grabSize != blockSize);
				fwrite(data, grabSize, 1, fp);
			}
			fclose(fp);

			//MoveFile(fullPath, fullPath);

			//DWORD dwEnd = timeGetTime();
			//Platform::DebugPrintf("Saved file=%d\n", dwEnd - dwStart);
			fileModTime = GetModTime(fullPath);
			SendEditor(SCI_SETSAVEPOINT);
			char fileNameLowered[MAX_PATH];
			strcpy(fileNameLowered, fileName);
			lowerCaseString(fileNameLowered);
			if (0 != strstr(fileNameLowered, ".properties")) {
				ReadGlobalPropFile();
				ReadLocalPropFile();
				ReadProperties();
				SetWindowName();
				BuffersMenu();
				Redraw();
			}
		} else {
			char msg[200];
			strcpy(msg, "Could not save file \"");
			strcat(msg, fullPath);
			strcat(msg, "\".");
			dialogsOnScreen++;
			MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
			dialogsOnScreen--;
		}
		return true;
	} else {
		return SaveAs();
	}
}

bool SciTEBase::SaveAs(char *file) {
	if (file && *file) {
		SetFileName(file);
		Save();
		ReadProperties();
		SendEditor(SCI_CLEARDOCUMENTSTYLE);
		SendEditor(SCI_COLOURISE, 0, -1);
		Redraw();
		SetWindowName();
		BuffersMenu();
		return true;
	} else {
		return SaveAsDialog();
	}
}

#define RTF_HEADEROPEN "{\\rtf1\\ansi\\deff0\\deftab720"
#define RTF_FONTDEFOPEN "{\\fonttbl"
#define RTF_FONTDEF "{\\f%d\\fnil\\fcharset0 %s;}"
#define RTF_FONTDEFCLOSE "}"
#define RTF_COLORDEFOPEN "{\\colortbl"
#define RTF_COLORDEF "\\red%d\\green%d\\blue%d;"
#define RTF_COLORDEFCLOSE "}"
#define RTF_HEADERCLOSE "\n"
#define RTF_BODYOPEN ""
#define RTF_BODYCLOSE "}"

#define RTF_SETFONTFACE "\\f"
#define RTF_SETFONTSIZE "\\fs"
#define RTF_SETCOLOR "\\cf"
#define RTF_SETBACKGROUND "\\highlight"
#define RTF_BOLD_ON "\\b"
#define RTF_BOLD_OFF "\\b0"
#define RTF_ITALIC_ON "\\i"
#define RTF_ITALIC_OFF "\\i0"
#define RTF_UNDERLINE_ON "\\ul"
#define RTF_UNDERLINE_OFF "\\ulnone"
#define RTF_STRIKE_ON "\\i"
#define RTF_STRIKE_OFF "\\strike0"

#define RTF_EOLN "\\par\n"
#define RTF_TAB "\\tab "

#define MAX_STYLEDEF 128
#define MAX_FONTDEF 64
#define MAX_COLORDEF 8
#define RTF_FONTFACE "Courier New"
#define RTF_COLOR "#000000"

int GetHexChar(char ch) { // 'H'
	return ch > '9' ? (ch | 0x20) - 'a' + 10 : ch - '0';
}

int GetHexByte(const char *hexbyte) { // "HH"
	return (GetHexChar(*hexbyte) << 4) | GetHexChar(hexbyte[1]);
}

int GetRTFHighlight(const char *rgb) { // "#RRGGBB"
	static int highlights[][3] = {
	    { 0x00, 0x00, 0x00 },   // highlight1  0;0;0       black
	    { 0x00, 0x00, 0xFF },   // highlight2  0;0;255     blue
	    { 0x00, 0xFF, 0xFF },   // highlight3  0;255;255   cyan
	    { 0x00, 0xFF, 0x00 },   // highlight4  0;255;0     green
	    { 0xFF, 0x00, 0xFF },   // highlight5  255;0;255   violet
	    { 0xFF, 0x00, 0x00 },   // highlight6  255;0;0     red
	    { 0xFF, 0xFF, 0x00 },   // highlight7  255;255;0   yellow
	    { 0xFF, 0xFF, 0xFF },   // highlight8  255;255;255 white
	    { 0x00, 0x00, 0x80 },   // highlight9  0;0;128     dark blue
	    { 0x00, 0x80, 0x80 },   // highlight10 0;128;128   dark cyan
	    { 0x00, 0x80, 0x00 },   // highlight11 0;128;0     dark green
	    { 0x80, 0x00, 0x80 },   // highlight12 128;0;128   dark violet
	    { 0x80, 0x00, 0x00 },   // highlight13 128;0;0     brown
	    { 0x80, 0x80, 0x00 },   // highlight14 128;128;0   khaki
	    { 0x80, 0x80, 0x80 },   // highlight15 128;128;128 dark grey
	    { 0xC0, 0xC0, 0xC0 },   // highlight16 192;192;192 grey
	};
	int maxdelta = 3 * 255 + 1, delta, index = -1;
	int r = GetHexByte (rgb + 1), g = GetHexByte (rgb + 3), b = GetHexByte (rgb + 5);
	for (unsigned int i = 0; i < sizeof(highlights) / sizeof(*highlights); i++) {
		delta = abs (r - *highlights[i]) +
		        abs (g - highlights[i][1]) +
		        abs (b - highlights[i][2]);
		if (delta < maxdelta) {
			maxdelta = delta;
			index = i;
		}
	}
	return index + 1;
}

void GetRTFStyleChange(char *delta, char *last, const char *current) { // \f0\fs20\cf0\highlight0\b0\i0
	int lastLen = strlen(last), offset = 2, lastOffset, currentOffset, len;
	*delta = '\0';
	// font face
	lastOffset = offset + 1;
	while (last[lastOffset] != '\\')
		lastOffset++;
	currentOffset = offset + 1;
	while (current[currentOffset] != '\\')
		currentOffset++;
	if (lastOffset != currentOffset ||   // change
	        strncmp(last + offset, current + offset, lastOffset - offset)) {
		if (lastOffset != currentOffset) {
			memmove (last + currentOffset, last + lastOffset, lastLen - lastOffset + 1);
			lastLen += currentOffset - lastOffset;
		}
		len = currentOffset - offset;
		memcpy(last + offset, current + offset, len);
		strcat (delta, RTF_SETFONTFACE);
		lastOffset = strlen(delta);
		memcpy(delta + lastOffset, last + offset, len);
		delta[lastOffset + len] = '\0';
	}
	offset = currentOffset + 3;
	// size
	lastOffset = offset + 1;
	while (last[lastOffset] != '\\')
		lastOffset++;
	currentOffset = offset + 1;
	while (current[currentOffset] != '\\')
		currentOffset++;
	if (lastOffset != currentOffset ||   // change
	        strncmp(last + offset, current + offset, lastOffset - offset)) {
		if (lastOffset != currentOffset) {
			memmove (last + currentOffset, last + lastOffset, lastLen - lastOffset + 1);
			lastLen += currentOffset - lastOffset;
		}
		len = currentOffset - offset;
		memcpy(last + offset, current + offset, len);
		strcat (delta, RTF_SETFONTSIZE);
		lastOffset = strlen(delta);
		memcpy(delta + lastOffset, last + offset, len);
		delta[lastOffset + len] = '\0';
	}
	offset = currentOffset + 3;
	// color
	lastOffset = offset + 1;
	while (last[lastOffset] != '\\')
		lastOffset++;
	currentOffset = offset + 1;
	while (current[currentOffset] != '\\')
		currentOffset++;
	if (lastOffset != currentOffset ||   // change
	        strncmp(last + offset, current + offset, lastOffset - offset)) {
		if (lastOffset != currentOffset) {
			memmove (last + currentOffset, last + lastOffset, lastLen - lastOffset + 1);
			lastLen += currentOffset - lastOffset;
		}
		len = currentOffset - offset;
		memcpy(last + offset, current + offset, len);
		strcat (delta, RTF_SETCOLOR);
		lastOffset = strlen(delta);
		memcpy(delta + lastOffset, last + offset, len);
		delta[lastOffset + len] = '\0';
	}
	offset = currentOffset + 10;
	// background
	lastOffset = offset + 1;
	while (last[lastOffset] != '\\')
		lastOffset++;
	currentOffset = offset + 1;
	while (current[currentOffset] != '\\')
		currentOffset++;
	if (lastOffset != currentOffset ||   // change
	        strncmp(last + offset, current + offset, lastOffset - offset)) {
		if (lastOffset != currentOffset) {
			memmove (last + currentOffset, last + lastOffset, lastLen - lastOffset + 1);
			lastLen += currentOffset - lastOffset;
		}
		len = currentOffset - offset;
		memcpy(last + offset, current + offset, len);
		strcat (delta, RTF_SETBACKGROUND);
		lastOffset = strlen(delta);
		memcpy(delta + lastOffset, last + offset, len);
		delta[lastOffset + len] = '\0';
	}
	offset = currentOffset + 2;
	// bold
	if (last[offset] != current[offset]) {
		if (current[offset] == '\\') { // turn on
			memmove (last + offset, last + offset + 1, lastLen-- - offset);
			strcat (delta, RTF_BOLD_ON);
			offset += 2;
		} else { // turn off
			memmove (last + offset + 1, last + offset, ++lastLen - offset);
			last[offset] = '0';
			strcat (delta, RTF_BOLD_OFF);
			offset += 3;
		}
	} else
		offset += current[offset] == '\\' ? 2 : 3;
	// italic
	if (last[offset] != current[offset]) {
		if (current[offset] == '\\') { // turn on
			memmove (last + offset, last + offset + 1, lastLen-- - offset);
			strcat (delta, RTF_ITALIC_ON);
		} else { // turn off
			memmove (last + offset + 1, last + offset, ++lastLen - offset);
			last[offset] = '0';
			strcat (delta, RTF_ITALIC_OFF);
		}
	}
	if (*delta) {
		lastOffset = strlen(delta);
		delta[lastOffset] = ' ';
		delta[lastOffset + 1] = '\0';
	}
}

void SciTEBase::SaveToRTF(const char *saveName) {
	SendEditor(SCI_COLOURISE, 0, -1);
	int tabSize = props.GetInt("export.rtf.tabsize", props.GetInt("tabsize"));
	int wysiwyg = props.GetInt("export.rtf.wysiwyg", 1);
	SString fontFace = props.GetExpanded("export.rtf.font.face");
	int fontSize = props.GetInt("export.rtf.font.size", 10 << 1);
	int tabs = props.GetInt("export.rtf.tabs", 0);
	if (tabSize == 0)
		tabSize = 4;
	if (!fontFace.length())
		fontFace = RTF_FONTFACE;
	FILE *fp = fopen(saveName, "wt");
	if (fp) {
		char styles[STYLE_DEFAULT + 1][MAX_STYLEDEF];
		char fonts[STYLE_DEFAULT + 1][MAX_FONTDEF];
		char colors[STYLE_DEFAULT + 1][MAX_COLORDEF];
		char lastStyle[MAX_STYLEDEF], deltaStyle[MAX_STYLEDEF];
		int fontCount = 1, colorCount = 1, i;
		fputs(RTF_HEADEROPEN RTF_FONTDEFOPEN, fp);
		strncpy(*fonts, fontFace.c_str(), MAX_FONTDEF);
		fprintf(fp, RTF_FONTDEF, 0, fontFace.c_str());
		strncpy(*colors, "#000000", MAX_COLORDEF);
		for (int istyle = 0; istyle <= STYLE_DEFAULT; istyle++) {
			char key[200];
			sprintf(key, "style.*.%0d", istyle);
			char *valdef = StringDup(props.GetExpanded(key).c_str());
			sprintf(key, "style.%s.%0d", language.c_str(), istyle);
			char *val = StringDup(props.GetExpanded(key).c_str());
			SString family;
			SString fore;
			SString back;
			bool italics = false;
			bool bold = false;
			int size = 0;
			if ((valdef && *valdef) || (val && *val)) {
				if (valdef && *valdef) {
					char *opt = valdef;
					while (opt) {
						char *cpComma = strchr(opt, ',');
						if (cpComma)
							*cpComma = '\0';
						char *colon = strchr(opt, ':');
						if (colon)
							*colon++ = '\0';
						if (0 == strcmp(opt, "italics"))
							italics = true;
						if (0 == strcmp(opt, "notitalics"))
							italics = false;
						if (0 == strcmp(opt, "bold"))
							bold = true;
						if (0 == strcmp(opt, "notbold"))
							bold = false;
						if (0 == strcmp(opt, "font"))
							family = colon;
						if (0 == strcmp(opt, "fore"))
							fore = colon;
						if (0 == strcmp(opt, "back"))
							back = colon;
						if (0 == strcmp(opt, "size"))
							size = atoi(colon);
						if (cpComma)
							opt = cpComma + 1;
						else
							opt = 0;
					}
				}
				if (val && *val) {
					char *opt = val;
					while (opt) {
						char *cpComma = strchr(opt, ',');
						if (cpComma)
							*cpComma = '\0';
						char *colon = strchr(opt, ':');
						if (colon)
							*colon++ = '\0';
						if (0 == strcmp(opt, "italics"))
							italics = true;
						if (0 == strcmp(opt, "notitalics"))
							italics = false;
						if (0 == strcmp(opt, "bold"))
							bold = true;
						if (0 == strcmp(opt, "notbold"))
							bold = false;
						if (0 == strcmp(opt, "font"))
							family = colon;
						if (0 == strcmp(opt, "fore"))
							fore = colon;
						if (0 == strcmp(opt, "back"))
							back = colon;
						if (0 == strcmp(opt, "size"))
							size = atoi(colon);
						if (cpComma)
							opt = cpComma + 1;
						else
							opt = 0;
					}
				}
				if (wysiwyg && family.length()) {
					for (i = 0; i < fontCount; i++)
						if (!strcasecmp(family.c_str(), fonts[i]))
							break;
					if (i >= fontCount) {
						strncpy(fonts[fontCount++], family.c_str(), MAX_FONTDEF);
						fprintf(fp, RTF_FONTDEF, i, family.c_str());
					}
					sprintf(lastStyle, RTF_SETFONTFACE "%d", i);
				} else
					strcpy(lastStyle, RTF_SETFONTFACE "0");
				sprintf(lastStyle + strlen(lastStyle), RTF_SETFONTSIZE "%d",
				        wysiwyg && size ? size << 1 : fontSize);
				if (fore.length()) {
					for (i = 0; i < colorCount; i++)
						if (!strcasecmp(fore.c_str(), colors[i]))
							break;
					if (i >= colorCount)
						strncpy(colors[colorCount++], fore.c_str(), MAX_COLORDEF);
					sprintf(lastStyle + strlen(lastStyle), RTF_SETCOLOR "%d", i);
				} else
					strcat(lastStyle, RTF_SETCOLOR "0");
				sprintf(lastStyle + strlen(lastStyle), RTF_SETBACKGROUND "%d",
				        back.length() ? GetRTFHighlight(back.c_str()) : 0);
				strcat(lastStyle, bold ? RTF_BOLD_ON : RTF_BOLD_OFF);
				strcat(lastStyle, italics ? RTF_ITALIC_ON : RTF_ITALIC_OFF);
				strncpy(styles[istyle], lastStyle, MAX_STYLEDEF);
			} else
				sprintf(styles[istyle], RTF_SETFONTFACE "0" RTF_SETFONTSIZE "%d"
				        RTF_SETCOLOR "0" RTF_SETBACKGROUND "0"
				        RTF_BOLD_OFF RTF_ITALIC_OFF, fontSize);
			if (val)
				delete []val;
			if (valdef)
				delete []valdef;
		}
		fputs(RTF_FONTDEFCLOSE RTF_COLORDEFOPEN, fp);
		for (i = 0; i < colorCount; i++) {
			fprintf(fp, RTF_COLORDEF, GetHexByte(colors[i] + 1),
			        GetHexByte(colors[i] + 3), GetHexByte(colors[i] + 5));
		}
		fprintf(fp, RTF_COLORDEFCLOSE RTF_HEADERCLOSE RTF_BODYOPEN RTF_SETFONTFACE "0"
		        RTF_SETFONTSIZE "%d" RTF_SETCOLOR "0 ", fontSize);
		sprintf(lastStyle, RTF_SETFONTFACE "0" RTF_SETFONTSIZE "%d"
		        RTF_SETCOLOR "0" RTF_SETBACKGROUND "0"
		        RTF_BOLD_OFF RTF_ITALIC_OFF, fontSize);
		int lengthDoc = LengthDocument();
		bool prevCR = false;
		int styleCurrent = -1;
		WindowAccessor acc(wEditor.GetID(), props);
		for (i = 0; i < lengthDoc; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);
			if (style != styleCurrent) {
				GetRTFStyleChange(deltaStyle, lastStyle, styles[style]);
				if (*deltaStyle)
					fputs(deltaStyle, fp);
				styleCurrent = style;
			}
			if (ch == '{')
				fputs("\\{", fp);
			else if (ch == '}')
				fputs("\\}", fp);
			else if (ch == '\\')
				fputs("\\", fp);
			else if (ch == '\t') {
				if (tabs)
					fputs(RTF_TAB, fp);
				else
					for (int itab = 0; itab < tabSize; itab++)
						fputc(' ', fp);
			} else if (ch == '\n') {
				if (!prevCR)
					fputs(RTF_EOLN, fp);
			} else if (ch == '\r')
				fputs(RTF_EOLN, fp);
			else
				fputc(ch, fp);
			prevCR = ch == '\r';
		}
		fputs(RTF_BODYCLOSE, fp);
		fclose(fp);
	} else {
		char msg[200];
		strcpy(msg, "Could not save file \"");
		strcat(msg, fullPath);
		strcat(msg, "\".");
		dialogsOnScreen++;
		MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
		dialogsOnScreen--;
	}
}


void SciTEBase::SaveToHTML(const char *saveName) {
	SendEditor(SCI_COLOURISE, 0, -1);
	int tabSize = props.GetInt("tabsize");
	if (tabSize == 0)
		tabSize = 4;
	int wysiwyg = props.GetInt("export.html.wysiwyg", 1);
	int tabs = props.GetInt("export.html.tabs", 0);
	FILE *fp = fopen(saveName, "wt");
	if (fp) {
		int styleCurrent = 0;
		fputs("<HTML>\n", fp);
		fputs("<HEAD>\n", fp);
		fputs("<STYLE>\n", fp);
		SString colour;
		for (int istyle = 0; istyle <= STYLE_DEFAULT; istyle++) {
			char key[200];
			sprintf(key, "style.*.%0d", istyle);
			char *valdef = StringDup(props.GetExpanded(key).c_str());
			sprintf(key, "style.%s.%0d", language.c_str(), istyle);
			char *val = StringDup(props.GetExpanded(key).c_str());
			SString family;
			SString fore;
			SString back;
			bool italics = false;
			bool bold = false;
			int size = 0;
			if ((valdef && *valdef) || (val && *val)) {
				if (istyle == STYLE_DEFAULT)
					fprintf(fp, "SPAN {\n");
				else
					fprintf(fp, ".S%0d {\n", istyle);
				if (valdef && *valdef) {
					char *opt = valdef;
					while (opt) {
						char *cpComma = strchr(opt, ',');
						if (cpComma)
							*cpComma = '\0';
						char *colon = strchr(opt, ':');
						if (colon)
							*colon++ = '\0';
						if (0 == strcmp(opt, "italics"))
							italics = true;
						if (0 == strcmp(opt, "notitalics"))
							italics = false;
						if (0 == strcmp(opt, "bold"))
							bold = true;
						if (0 == strcmp(opt, "notbold"))
							bold = false;
						if (0 == strcmp(opt, "font"))
							family = colon;
						if (0 == strcmp(opt, "fore"))
							fore = colon;
						if (0 == strcmp(opt, "back"))
							back = colon;
						if (0 == strcmp(opt, "size"))
							size = atoi(colon);
						if (cpComma)
							opt = cpComma + 1;
						else
							opt = 0;
					}
				}
				if (val && *val) {
					char *opt = val;
					while (opt) {
						char *cpComma = strchr(opt, ',');
						if (cpComma)
							*cpComma = '\0';
						char *colon = strchr(opt, ':');
						if (colon)
							*colon++ = '\0';
						if (0 == strcmp(opt, "italics"))
							italics = true;
						if (0 == strcmp(opt, "notitalics"))
							italics = false;
						if (0 == strcmp(opt, "bold"))
							bold = true;
						if (0 == strcmp(opt, "notbold"))
							bold = false;
						if (0 == strcmp(opt, "font"))
							family = colon;
						if (0 == strcmp(opt, "fore"))
							fore = colon;
						if (0 == strcmp(opt, "back"))
							back = colon;
						if (0 == strcmp(opt, "size"))
							size = atoi(colon);
						if (cpComma)
							opt = cpComma + 1;
						else
							opt = 0;
					}
				}
				if (italics)
					fprintf(fp, "\tfont-style: italic;\n");
				if (bold)
					fprintf(fp, "\tfont-weight: bold;\n");
				if (wysiwyg && family.length())
					fprintf(fp, "\tfont-family: %s;\n", family.c_str());
				if (fore.length())
					fprintf(fp, "\tcolor: %s;\n", fore.c_str());
				if (back.length())
					fprintf(fp, "\tbackground: %s;\n", back.c_str());
				if (wysiwyg && size)
					fprintf(fp, "\tfont-size: %0dpt;\n", size);
				fprintf(fp, "}\n");
			}
			if (val)
				delete []val;
			if (valdef)
				delete []valdef;
		}
		fputs("</STYLE>\n", fp);
		fputs("</HEAD>\n", fp);
		fputs("<BODY>\n", fp);
		if (wysiwyg)
			fputs("<SPAN class=S0>", fp);
		else
			fputs("<PRE class=S0>", fp);
		int lengthDoc = LengthDocument();
		bool prevCR = false;
		WindowAccessor acc(wEditor.GetID(), props);
		for (int i = 0; i < lengthDoc; i++) {
			char ch = acc[i];
			int style = acc.StyleAt(i);
			if (style != styleCurrent) {
				if (wysiwyg || styleCurrent != 0)
					fputs("</SPAN>", fp);
				if (wysiwyg || style != 0)
					fprintf(fp, "<SPAN class=S%0d>", style);
				styleCurrent = style;
			}
			if (ch == ' ') {
				if (wysiwyg)
					fputs("&nbsp;", fp);
				else
					fputc(' ', fp);
			} else if (ch == '\t') {
				if (wysiwyg) {
					for (int itab = 0; itab < tabSize; itab++)
						fputs("&nbsp;", fp);
				} else {
					if (tabs) {
						fputc(ch, fp);
					} else {
						for (int itab = 0; itab < tabSize; itab++)
							fputc(' ', fp);
					}
				}
			} else if (ch == '\r') {
				if (wysiwyg)
					fputs("<BR>\n", fp);
				else
					fputc('\n', fp);
			} else if (ch == '\n') {
				if (!prevCR)
					fputs("<BR>\n", fp);
			} else if (ch == '<') {
				fputs("&lt;", fp);
			} else if (ch == '>') {
				fputs("&gt;", fp);
			} else if (ch == '&') {
				fputs("&amp;", fp);
			} else {
				fputc(ch, fp);
			}
			prevCR = ch == '\r';
		}
		if (wysiwyg)
			fputs("</SPAN>", fp);
		else
			fputs("</PRE>", fp);
		fputs("</BODY>\n</HTML>\n", fp);
		fclose(fp);
	} else {
		char msg[200];
		strcpy(msg, "Could not save file \"");
		strcat(msg, fullPath);
		strcat(msg, "\".");
		dialogsOnScreen++;
		MessageBox(wSciTE.GetID(), msg, appName, MB_OK);
		dialogsOnScreen--;
	}
}
