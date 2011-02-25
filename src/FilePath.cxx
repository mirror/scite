// SciTE - Scintilla based Text Editor
/** @file FilePath.cxx
 ** Encapsulate a file path.
 **/
// Copyright 1998-2005 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <string>
#include <vector>
#include <algorithm>

#if defined(__unix__)

#include <unistd.h>

#if defined(GTK)
#include <gtk/gtk.h>
#endif

#include <dirent.h>
#include <errno.h>

#else

#include <io.h>

#undef _WIN32_WINNT
#define _WIN32_WINNT  0x0500
#ifdef _MSC_VER
// windows.h, et al, use a lot of nameless struct/unions - can't fix it, so allow it
#pragma warning(disable: 4201)
#endif
#include <windows.h>
#ifdef _MSC_VER
// okay, that's done, don't allow it in our code
#pragma warning(default: 4201)
// Turn off MS dislike of POSIX
#pragma warning(disable: 4996)
#endif
#include <commctrl.h>

// For chdir
#ifdef _MSC_VER
#include <direct.h>
#endif
#ifdef __DMC__
#include <dir.h>
#endif
#ifdef __MINGW32_VERSION
#include <direct.h>
#endif

#endif

#include "Scintilla.h"

#include "GUI.h"

#include "SString.h"
#include "FilePath.h"

#if defined(__unix__)
const GUI::gui_char pathSepString[] = "/";
const GUI::gui_char pathSepChar = '/';
const GUI::gui_char listSepString[] = ":";
const GUI::gui_char configFileVisibilityString[] = ".";
const GUI::gui_char fileRead[] = "rb";
const GUI::gui_char fileWrite[] = "wb";
#endif
#ifdef WIN32
// Windows
const GUI::gui_char pathSepString[] = GUI_TEXT("\\");
const GUI::gui_char pathSepChar = '\\';
const GUI::gui_char listSepString[] = GUI_TEXT(";");
const GUI::gui_char configFileVisibilityString[] = GUI_TEXT("");
const GUI::gui_char fileRead[] = GUI_TEXT("rb");
const GUI::gui_char fileWrite[] = GUI_TEXT("wb");
#endif

FilePath::FilePath(const GUI::gui_char *fileName_) : fileName(fileName_ ? fileName_ : GUI_TEXT("")) {}

FilePath::FilePath(const GUI::gui_string &fileName_) : fileName(fileName_) {}

FilePath::FilePath(FilePath const &directory, FilePath const &name) {
	Set(directory, name);
}

void FilePath::Set(const GUI::gui_char *fileName_) {
	fileName = fileName_;
}

void FilePath::Set(FilePath const &other) {
	fileName = other.fileName;
}

void FilePath::Set(FilePath const &directory, FilePath const &name) {
	if (name.IsAbsolute()) {
		fileName = name.fileName;
	} else {
		fileName = directory.fileName;
		if (fileName.size() && (fileName[fileName.size()-1] != pathSepChar))
			fileName += pathSepString;
		fileName += name.fileName;
	}
}

void FilePath::SetDirectory(FilePath directory) {
	FilePath curName(*this);
	Set(directory, curName);
}

void FilePath::Init() {
	fileName = GUI_TEXT("");
}

bool FilePath::SameNameAs(const GUI::gui_char *other) const {
#ifdef WIN32
	return CSTR_EQUAL == CompareString(LOCALE_SYSTEM_DEFAULT, NORM_IGNORECASE,
		fileName.c_str(), -1, other, -1);
#else
	return fileName == other;
#endif
}

bool FilePath::SameNameAs(const FilePath &other) const {
	return SameNameAs(other.fileName.c_str());
}

bool FilePath::IsSet() const {
	return fileName.length() > 0;
}

bool FilePath::IsUntitled() const {
	size_t dirEnd = fileName.rfind(pathSepChar);
	return (dirEnd == GUI::gui_string::npos) || (!fileName[dirEnd+1]);
}

bool FilePath::IsAbsolute() const {
	if (fileName.length() == 0)
		return false;
#ifdef __unix__
	if (fileName[0] == '/')
		return true;
#endif
#ifdef WIN32
	if (fileName[0] == pathSepChar || fileName[1] == ':')	// UNC path or drive separator
		return true;
#endif

	return false;
}

bool FilePath::IsRoot() const {
#ifdef WIN32
	if ((fileName[0] == pathSepChar) && (fileName[1] == pathSepChar) && (fileName.find(pathSepString, 2) == GUI::gui_string::npos))
        return true; // UNC path like \\server
	return (fileName.length() == 3) && (fileName[1] == ':') && (fileName[2] == pathSepChar);
#else
	return fileName == "/";
#endif
}

int FilePath::RootLength() {
#ifdef WIN32
	return 3;
#else
	return 1;
#endif
}

const GUI::gui_char *FilePath::AsInternal() const {
	return fileName.c_str();
}

std::string FilePath::AsUTF8() const {
	return GUI::UTF8FromString(fileName);
}

FilePath FilePath::Name() const {
	size_t dirEnd = fileName.rfind(pathSepChar);
	if (dirEnd != GUI::gui_string::npos)
		return fileName.substr(dirEnd + 1);
	else
		return fileName;
}

FilePath FilePath::BaseName() const {
	size_t dirEnd = fileName.rfind(pathSepChar);
	size_t extStart = fileName.rfind('.');
	if (dirEnd != GUI::gui_string::npos) {
		if (extStart > dirEnd) {
			return FilePath(fileName.substr(dirEnd + 1, extStart - dirEnd - 1));
		} else {
			return FilePath(fileName.substr(dirEnd + 1));
		}
	} else if (extStart) {
		return FilePath(fileName.substr(0, extStart));
	} else {
		return fileName;
	}
}

FilePath FilePath::Extension() const {
	size_t dirEnd = fileName.rfind(pathSepChar);
	size_t extStart = fileName.rfind('.');
	if ((extStart != GUI::gui_string::npos) && (extStart > dirEnd))
		return fileName.substr(extStart + 1);
	else
		return GUI_TEXT("");
}

FilePath FilePath::Directory() const {
	if (IsRoot()) {
		return FilePath(fileName.c_str());
	} else {
		size_t lenDirectory = fileName.rfind(pathSepChar);
		if (lenDirectory != GUI::gui_string::npos) {
			if (lenDirectory < static_cast<size_t>(RootLength())) {
				lenDirectory = static_cast<size_t>(RootLength());
			}
			return FilePath(fileName.substr(0, lenDirectory));
		} else {
			return FilePath();
		}
	}
}

#ifdef _WIN32

// Substitute functions that take wchar_t arguments but have the same name
// as char functions so that the compiler will choose the right form.

static size_t strlen(const wchar_t *str) {
	return wcslen(str);
}

static wchar_t *strcpy(wchar_t *strDestination, const wchar_t *strSource) {
	return wcscpy(strDestination, strSource);
}

static int strcmp(const wchar_t *a, const wchar_t *b) {
	return wcscmp(a,b);
}

static wchar_t *strchr(wchar_t *str, wchar_t c) {
	return wcschr(str, c);
}

static wchar_t *strrchr(wchar_t *str, wchar_t c) {
	return wcsrchr(str, c);
}

static wchar_t *getcwd(wchar_t *buffer, int maxlen) {
	return _wgetcwd(buffer, maxlen);
}

static int chdir(const wchar_t *dirname) {
	return _wchdir(dirname);
}

static FILE *fopen(const wchar_t *filename, const wchar_t *mode) {
	return _wfopen(filename, mode);
}

static int unlink(const wchar_t *filename) {
	return _wunlink(filename);
}

static int access(const wchar_t *path, int mode) {
	return _waccess(path, mode);
}

#if defined(_MSC_VER) && (_MSC_VER > 1310)
static int stat(const wchar_t *path, struct _stat64i32 *buffer) {
	return _wstat(path, buffer);
}
#else
static int stat(const wchar_t *path, struct _stat *buffer) {
	return _wstat(path, buffer);
}
#endif

#endif

static GUI::gui_char *split(GUI::gui_char*& s, GUI::gui_char c) {
	GUI::gui_char *t = s;
	if (s && (s = strchr(s, c)) != NULL)
		* s++ = '\0';
	return t;
}

FilePath FilePath::NormalizePath() const {
	GUI::gui_char *path = new GUI::gui_char[fileName.length() + 1];
	strcpy(path, AsInternal());
#ifdef WIN32
	// Convert unix path separators to Windows
	for (GUI::gui_char *cp = path; *cp; cp++) {
		if (*cp == '/')
			*cp = pathSepChar;
	}
#endif
	GUI::gui_char *absPath = new GUI::gui_char[fileName.length() + 1];
	GUI::gui_char *cur = absPath;
	*cur = '\0';
	GUI::gui_char *tmp = path;
	if (*tmp == pathSepChar) {
		*cur++ = pathSepChar;
		*cur = '\0';
		tmp++;
	}
	GUI::gui_char *part;
	while ((part = split(tmp, pathSepChar)) != NULL) {
		GUI::gui_char *last;
		if (strcmp(part, GUI_TEXT(".")) == 0)
			;
		else if (strcmp(part, GUI_TEXT("..")) == 0 && (last = strrchr(absPath, pathSepChar)) != NULL) {
			if (last > absPath)
				cur = last;
			else
				cur = last + 1;
			*cur = '\0';
		} else {
			if (cur > absPath && *(cur - 1) != pathSepChar)
				*cur++ = pathSepChar;
			strcpy(cur, part);
			cur += strlen(part);
		}
	}
	FilePath ret(absPath);
	delete []path;
	delete []absPath;
	return ret;
}

/**
 * Take a filename or relative path and put it at the end of the current path.
 * If the path is absolute, return the same path.
 */
FilePath FilePath::AbsolutePath() const {
#ifdef WIN32
	// The runtime libraries for GCC and Visual C++ give different results for _fullpath
	// so use the OS.
	GUI::gui_char absPath[2000];
	absPath[0] = '\0';
	GUI::gui_char *fileBit = 0;
	::GetFullPathNameW(AsInternal(), sizeof(absPath)/sizeof(absPath[0]), absPath, &fileBit);
	return FilePath(absPath);
#else
	if (IsAbsolute()) {
		return NormalizePath();
	} else {
		return FilePath(GetWorkingDirectory(), *this).NormalizePath();
	}
#endif
}

// Only used on Windows to fix the case of file names

FilePath FilePath::GetWorkingDirectory() {
	GUI::gui_char dir[MAX_PATH + 1];
	dir[0] = '\0';
	if (getcwd(dir, MAX_PATH)) {
		dir[MAX_PATH] = '\0';
		// In Windows, getcwd returns a trailing backslash
		// when the CWD is at the root of a disk, so remove it
		size_t endOfPath = strlen(dir) - 1;
		if (dir[endOfPath] == pathSepChar) {
			dir[endOfPath] = '\0';
		}
	}
	return FilePath(dir);
}

bool FilePath::SetWorkingDirectory() const {
	return chdir(AsInternal()) == 0;
}

void FilePath::List(FilePathSet &directories, FilePathSet &files) {
#ifdef WIN32
	FilePath wildCard(*this, GUI_TEXT("*.*"));
	bool complete = false;
	WIN32_FIND_DATAW findFileData;
	HANDLE hFind = ::FindFirstFileW(wildCard.AsInternal(), &findFileData);
	if (hFind != INVALID_HANDLE_VALUE) {
		while (!complete) {
			if ((strcmp(findFileData.cFileName, GUI_TEXT(".")) != 0) && (strcmp(findFileData.cFileName, GUI_TEXT("..")) != 0)) {
				if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					directories.Append(FilePath(AsInternal(), findFileData.cFileName));
				} else {
					files.Append(FilePath(AsInternal(), findFileData.cFileName));
				}
			}
			if (!::FindNextFileW(hFind, &findFileData)) {
				complete = true;
			}
		}
		::FindClose(hFind);
	}
#else
	errno = 0;
	DIR *dp = opendir(AsInternal());
	if (dp == NULL) {
		//~ fprintf(stderr, "%s: cannot open for reading: %s\n", AsInternal(), strerror(errno));
		return;
	}
	struct dirent *ent;
	while ((ent = readdir(dp)) != NULL) {
		if ((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)) {
			FilePath pathFull(AsInternal(), ent->d_name);
			if (pathFull.IsDirectory()) {
				directories.Append(pathFull);
			} else {
				files.Append(pathFull);
			}
		}
	}

	if (errno == 0) {
		closedir(dp);
	}
#endif
}

FILE *FilePath::Open(const GUI::gui_char *mode) const {
	if (IsSet()) {
		return fopen(fileName.c_str(), mode);
	} else {
		return NULL;
	}
}

void FilePath::Remove() const {
	unlink(AsInternal());
}

#ifndef R_OK
// Microsoft does not define the constants used to call access
#define R_OK 4
#endif

time_t FilePath::ModifiedTime() const {
	if (IsUntitled())
		return 0;
	if (access(AsInternal(), R_OK) == -1)
		return 0;
#ifdef _WIN32
#if defined(_MSC_VER) && (_MSC_VER > 1310)
	struct _stat64i32 statusFile;
#else
	struct _stat statusFile;
#endif
#else
	struct stat statusFile;
#endif
	if (stat(AsInternal(), &statusFile) != -1)
		return statusFile.st_mtime;
	else
		return 0;
}

int FilePath::GetFileLength() const {
	int size = -1;
	if (IsSet()) {
		FILE *fp = Open(fileRead);
		if (fp) {
			fseek(fp, 0, SEEK_END);
			size = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			fclose(fp);
		}
	}
	return size;
}

bool FilePath::Exists() const {
	bool ret = false;
	if (IsSet()) {
		FILE *fp = Open(fileRead);
		if (fp) {
			ret = true;
			fclose(fp);
		}
	}
	return ret;
}

bool FilePath::IsDirectory() const {
#ifdef _WIN32
#if defined(_MSC_VER) && (_MSC_VER > 1310)
	struct _stat64i32 statusFile;
#else
	struct _stat statusFile;
#endif
#else
	struct stat statusFile;
#endif
	if (stat(AsInternal(), &statusFile) != -1)
#ifdef WIN32
		return statusFile.st_mode & _S_IFDIR;
#else
		return statusFile.st_mode & S_IFDIR;
#endif
	else
		return false;
}

#ifdef _WIN32
void Lowercase(GUI::gui_string &s) {
	int chars = ::LCMapString(LOCALE_SYSTEM_DEFAULT, LCMAP_LOWERCASE, s.c_str(), s.size()+1, NULL, 0);
	std::vector<wchar_t> vc(chars);
	::LCMapString(LOCALE_SYSTEM_DEFAULT, LCMAP_LOWERCASE, s.c_str(), s.size()+1, &vc[0], chars);
	s = &vc[0];
}
#endif

static bool EndMatches(GUI::gui_string const &s, GUI::gui_string const &end) {
	return (s.size() >= end.size()) &&
		(std::equal(s.begin() + s.size() - end.size(), s.end(), end.begin()));
}

bool FilePath::Matches(const GUI::gui_char *pattern) const {
	GUI::gui_string pat(pattern);
	GUI::gui_string nameCopy(Name().fileName);
#ifdef _WIN32
	Lowercase(pat);
	Lowercase(nameCopy);
#endif
	std::replace(pat.begin(), pat.end(), ' ', '\0');
	size_t start = 0;
	while (start < pat.length()) {
		const GUI::gui_char *patElement = pat.c_str() + start;
		if (patElement[0] == '*') {
			if (EndMatches(nameCopy, patElement + 1)) {
				return true;
			}
		} else {
			if (nameCopy == patElement) {
				return true;
			}
		}
		start += strlen(patElement) + 1;
	}
	return false;
}

#ifdef WIN32
/**
 * Makes a long path from a given, possibly short path/file.
 *
 * The short path/file must exist, and if it is a file it must be fully specified
 * otherwise the function fails.
 *
 * sizeof @a longPath buffer must be a least _MAX_PATH
 * @returns true on success, and the long path in @a longPath buffer,
 * false on failure, and copies the @a shortPath arg to the @a longPath buffer.
 */
static bool MakeLongPath(const GUI::gui_char* shortPath, GUI::gui_char* longPath) {

	bool ok = ::GetLongPathNameW(shortPath, longPath, _MAX_PATH) != 0;

	if (!ok) {
		wcsncpy(longPath, shortPath, _MAX_PATH);
	}
	return ok;
}
#endif

void FilePath::FixName() {
#ifdef WIN32
	// Only used on Windows to use long file names and fix the case of file names
	GUI::gui_char longPath[_MAX_PATH];
	// first try MakeLongPath which corrects the path and the case of filename too
	if (MakeLongPath(AsInternal(), longPath)) {
		Set(longPath);
	} else {
		// On Windows file comparison is done case insensitively so the user can
		// enter scite.cxx and still open this file, SciTE.cxx. To ensure that the file
		// is saved with correct capitalisation FindFirstFile is used to find out the
		// real name of the file.
		WIN32_FIND_DATAW FindFileData;
		HANDLE hFind = ::FindFirstFileW(AsInternal(), &FindFileData);
		FilePath dir = Directory();
		if (hFind != INVALID_HANDLE_VALUE) {	// FindFirstFile found the file
			Set(dir, FindFileData.cFileName);
			::FindClose(hFind);
		}
	}
#endif
}

FilePathSet::FilePathSet(int size_) {
	size = size_;
	body = new FilePath[size];
	lengthBody = 0;
}

FilePathSet::FilePathSet(const FilePathSet &other) {
	size = other.size;
	lengthBody = other.lengthBody;
	body = new FilePath[size];
	for (size_t i = 0; i < lengthBody; i++) {
		body[i] = other.body[i];
	}
}

FilePathSet::~FilePathSet() {
	delete []body;
	body = NULL;
	size = 0;
	lengthBody = 0;
}

FilePath FilePathSet::At(size_t pos) const {
	return body[pos];
}

void FilePathSet::Append(FilePath fp) {
	if (lengthBody >= size) {
		size *= 2;
		FilePath *bodyNew = new FilePath[size];
		for (size_t i = 0; i < lengthBody; i++) {
			bodyNew[i] = body[i];
		}
		delete []body;
		body = bodyNew;
	}
	body[lengthBody++] = fp;
}

size_t FilePathSet::Length() const {
	return lengthBody;
}

