// SciTE - Scintilla based Text Editor
/** @file SciTEWin.cxx
 ** Main code for the Windows version of the editor.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <time.h>

#include "SciTEWin.h"

#ifndef NO_FILER
#include "DirectorExtension.h"
#endif

#ifdef STATIC_BUILD
const char appName[] = "Sc1";
#else
const char appName[] = "SciTE";
#endif

HINSTANCE SciTEWin::hInstance = 0;
char *SciTEWin::className = NULL;
char *SciTEWin::classNameInternal = NULL;
SciTEWin *SciTEWin::app = NULL;

SciTEWin::SciTEWin(Extension *ext) : SciTEBase(ext) {
	app = this;
	cmdShow = 0;
	heightBar = 7;
	fontTabs = 0;

	winPlace.length = 0;

	openWhat[0] = '\0';
	memset(&fr, 0, sizeof(fr));
	filterDefault = 1;

	// Read properties resource into propsEmbed
	// The embedded properties file is to allow distributions to be sure
	// that they have a sensible default configuration even if the properties
	// files are missing. Also allows a one file distribution of Sc1.EXE.
	propsEmbed.Clear();
	propsEmbed.Set("PLAT_WIN", "1");
	HRSRC handProps = ::FindResource(hInstance, "Embedded", "Properties");
	if (handProps) {
		DWORD size = ::SizeofResource(hInstance, handProps);
		HGLOBAL hmem = ::LoadResource(hInstance, handProps);
		if (hmem) {
			const void *pv = ::LockResource(hmem);
			if (pv) {
				propsEmbed.ReadFromMemory(
				    reinterpret_cast<const char *>(pv), size, 0);
			}
		}
		::FreeResource(handProps);
	}

	ReadGlobalPropFile();
	/// Need to copy properties to variables before setting up Window
	SetPropertiesInitial();
	ReadAbbrevPropFile();

	hDevMode = 0;
	hDevNames = 0;
	::ZeroMemory(&pagesetupMargin, sizeof(pagesetupMargin));

	hHH = 0;
	hMM = 0;
}

SciTEWin::~SciTEWin() {
	if (hDevMode)
		::GlobalFree(hDevMode);
	if (hDevNames)
		::GlobalFree(hDevNames);
	if (hHH)
		::FreeLibrary(hHH);
	if (hMM)
		::FreeLibrary(hMM);
	if (fontTabs)
		::DeleteObject(fontTabs);
}

uptr_t SciTEWin::GetInstance() {
	return reinterpret_cast<uptr_t>(hInstance);
}

void SciTEWin::Register(HINSTANCE hInstance_) {
	const char resourceName[] = "SciTE";

	hInstance = hInstance_;

	WNDCLASS wndclass;

	// Register the frame window
	className = "SciTEWindow";
	wndclass.style = 0;
	wndclass.lpfnWndProc = SciTEWin::TWndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = sizeof(SciTEWin*);
	wndclass.hInstance = hInstance;
	wndclass.hIcon = ::LoadIcon(hInstance, resourceName);
	wndclass.hCursor = NULL;
	wndclass.hbrBackground = NULL;
	wndclass.lpszMenuName = resourceName;
	wndclass.lpszClassName = className;
	if (!::RegisterClass(&wndclass))
		exit(FALSE);

	// Register the window that holds the two Scintilla edit windows and the separator
	classNameInternal = "SciTEWindowContent";
	wndclass.lpfnWndProc = SciTEWin::IWndProc;
	wndclass.lpszMenuName = 0;
	wndclass.lpszClassName = classNameInternal;
	if (!::RegisterClass(&wndclass))
		exit(FALSE);
}

static void GetSciTEPath(char *path, unsigned int lenPath, char *home) {
	*path = '\0';
	if (home) {
		strncpy(path, home, lenPath);
	} else {
		::GetModuleFileName(0, path, lenPath);
		// Remove the SciTE.exe
		char *lastSlash = strrchr(path, pathSepChar);
		if (lastSlash)
			*lastSlash = '\0';
	}
	path[lenPath - 1] = '\0';
	ChopTerminalSlash(path);
}

void SciTEWin::GetDefaultDirectory(char *directory, size_t size) {
	char *home = getenv("SciTE_HOME");
	GetSciTEPath(directory, size, home);
}

bool SciTEWin::GetSciteDefaultHome(char *path, unsigned int lenPath) {
	*path = '\0';
	char *home = getenv("SciTE_HOME");
	GetSciTEPath(path, lenPath, home);
	return true;
}

bool SciTEWin::GetSciteUserHome(char *path, unsigned int lenPath) {
	*path = '\0';
	char *home = getenv("SciTE_HOME");
	if (!home)
		home = getenv("USERPROFILE");
	GetSciTEPath(path, lenPath, home);
	return true;
}

bool SciTEWin::GetDefaultPropertiesFileName(char *pathDefaultProps,
        char *pathDefaultDir, unsigned int lenPath) {
	if (!GetSciteDefaultHome(pathDefaultDir, lenPath))
		return false;
	strncpy(pathDefaultProps, pathDefaultDir, lenPath);
	strncat(pathDefaultProps, pathSepString, lenPath);
	strncat(pathDefaultProps, propGlobalFileName, lenPath);
	return true;
}

bool SciTEWin::GetAbbrevPropertiesFileName(char *pathAbbrevProps,
        char *pathDefaultDir, unsigned int lenPath) {
	if (!GetSciteDefaultHome(pathDefaultDir, lenPath))
		return false;
	strncpy(pathAbbrevProps, pathDefaultDir, lenPath);
	strncat(pathAbbrevProps, pathSepString, lenPath);
	strncat(pathAbbrevProps, propAbbrevFileName, lenPath);
	return true;
}

bool SciTEWin::GetUserPropertiesFileName(char *pathUserProps,
        char *pathUserDir, unsigned int lenPath) {
	if (!GetSciteUserHome(pathUserDir, lenPath))
		return false;
	strncpy(pathUserProps, pathUserDir, lenPath);
	strncat(pathUserProps, pathSepString, lenPath);
	strncat(pathUserProps, propUserFileName, lenPath);
	return true;
}

// Help command lines contain topic!path
void SciTEWin::ExecuteOtherHelp(const char *cmd) {
	char *topic = StringDup(cmd);
	char *path = strchr(topic, '!');
	if (topic && path) {
		*path = '\0';
		path++;	// After the !
		::WinHelp(MainHWND(),
		        path,
		        HELP_KEY,
		        reinterpret_cast<unsigned long>(topic));
	}
	if (topic) {
		delete []topic;
	}
}

// HH_AKLINK not in mingw headers
struct XHH_AKLINK {
	long cbStruct;
	BOOL fReserved;
	const char *pszKeywords;
	char *pszUrl;
	char *pszMsgText;
	char *pszMsgTitle;
	char *pszWindow;
	BOOL fIndexOnFail;
};

// Help command lines contain topic!path
void SciTEWin::ExecuteHelp(const char *cmd) {
	if (!hHH)
		hHH = ::LoadLibrary("HHCTRL.OCX");

	if (hHH) {
		char *topic = StringDup(cmd);
		char *path = strchr(topic, '!');
		if (topic && path) {
			*path = '\0';
			path++;	// After the !
			typedef HWND (WINAPI *HelpFn) (HWND, const char *, UINT, DWORD);
			HelpFn fnHHA = (HelpFn)::GetProcAddress(hHH, "HtmlHelpA");
			if (fnHHA) {
				XHH_AKLINK ak;
				ak.cbStruct = sizeof(ak);
				ak.fReserved = FALSE;
				ak.pszKeywords = topic;
				ak.pszUrl = NULL;
				ak.pszMsgText = NULL;
				ak.pszMsgTitle = NULL;
				ak.pszWindow = NULL;
				ak.fIndexOnFail = TRUE;
				fnHHA(NULL,
				      path,
				      0x000d,        	// HH_KEYWORD_LOOKUP
				      reinterpret_cast<DWORD>(&ak)
				     );
			}
		}
		delete []topic;
	}
}

void SciTEWin::CopyAsRTF() {
	CharacterRange cr = GetSelection();
	char *fileNameTemp = tmpnam(0);
	if (fileNameTemp) {
		SaveToRTF(fileNameTemp, cr.cpMin, cr.cpMax);
		FILE *fp = fopen(fileNameTemp, fileRead);
		if (fp) {
			fseek(fp, 0, SEEK_END);
			int len = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			HGLOBAL hand = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, len + 1);
			if (hand) {
				::OpenClipboard(MainHWND());
				::EmptyClipboard();
				char *ptr = static_cast<char *>(::GlobalLock(hand));
				fread(ptr, 1, len, fp);
				ptr[len] = '\0';
				::GlobalUnlock(hand);
				::SetClipboardData(::RegisterClipboardFormat(CF_RTF), hand);
				::CloseClipboard();
			}
			fclose(fp);
		}
		unlink(fileNameTemp);
	}
}

void SciTEWin::FullScreenToggle() {
	HWND wTaskBar = FindWindow("Shell_TrayWnd", "");
	fullScreen = !fullScreen;
	if (fullScreen) {
		::SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
		::SystemParametersInfo(SPI_SETWORKAREA, 0, 0, SPIF_SENDCHANGE);
		::ShowWindow(wTaskBar, SW_HIDE);

		winPlace.length = sizeof(winPlace);
		::GetWindowPlacement(MainHWND(), &winPlace);
		int topStuff = ::GetSystemMetrics(SM_CYMENU) +
			       ::GetSystemMetrics(SM_CYEDGE);
		if (props.GetInt("full.screen.hides.menu"))
			topStuff += ::GetSystemMetrics(SM_CYCAPTION);
		::SetWindowPos(MainHWND(), HWND_TOP,
			       -::GetSystemMetrics(SM_CXSIZEFRAME) - 1,
			       -topStuff - 2,
			       ::GetSystemMetrics(SM_CXSCREEN) +
			       2 * ::GetSystemMetrics(SM_CXSIZEFRAME) + 2,
			       ::GetSystemMetrics(SM_CYSCREEN) + topStuff +
			       ::GetSystemMetrics(SM_CYSIZEFRAME) + 3,
			       0);
	} else {
		::ShowWindow(wTaskBar, SW_SHOW);
		if (winPlace.length) {
			::SystemParametersInfo(SPI_SETWORKAREA, 0, &rcWorkArea, 0);
			if (winPlace.showCmd == SW_SHOWMAXIMIZED) {
				::ShowWindow(MainHWND(), SW_RESTORE);
				::ShowWindow(MainHWND(), SW_SHOWMAXIMIZED);
			} else {
				::SetWindowPlacement(MainHWND(), &winPlace);
			}
		}
	}
	::SetForegroundWindow(MainHWND());
	CheckMenus();
}

HWND SciTEWin::MainHWND() {
	return reinterpret_cast<HWND>(wSciTE.GetID());
}

void SciTEWin::Command(WPARAM wParam, LPARAM lParam) {
	int cmdID = ControlIDOfCommand(wParam);
	switch (cmdID) {

	case IDM_ACTIVATE:
		Activate(lParam);
		break;

	case IDM_FINISHEDEXECUTE:
		{
			executing = false;
			CheckMenus();
			for (int icmd = 0; icmd < commandMax; icmd++) {
				jobQueue[icmd].Clear();
			}
			commandCurrent = 0;
			CheckReload();
		}
		break;

	case IDM_ONTOP:
		topMost = (topMost ? false : true);
		::SetWindowPos(MainHWND(), (topMost ? HWND_TOPMOST : HWND_NOTOPMOST ), 0, 0, 0, 0, SWP_NOMOVE + SWP_NOSIZE);
		CheckAMenuItem(IDM_ONTOP, topMost);
		break;

	case IDM_FULLSCREEN:
		FullScreenToggle();
		break;

	default:
		SciTEBase::MenuCommand(cmdID);
	}
}

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
bool MakeLongPath(const char* shortPath, char* longPath) {
	// when we have pfnGetLong, we assume it never changes as kernel32 is always loaded
	static DWORD (STDAPICALLTYPE* pfnGetLong)(const char* lpszShortPath, char* lpszLongPath, DWORD cchBuffer) = NULL;
	static bool kernelTried = FALSE;
	bool ok = FALSE;

	if (!kernelTried) {
		HMODULE hModule;
		kernelTried = true;
		hModule = ::GetModuleHandleA("KERNEL32");
		//assert(hModule != NULL); // must not call FreeLibrary on such handle

		// attempt to get GetLongPathName (implemented in Win98/2000 only!)
		(FARPROC&)pfnGetLong = ::GetProcAddress(hModule, "GetLongPathNameA");
	}

	// the kernel GetLongPathName proc is faster and (hopefully) more reliable
	if (pfnGetLong != NULL) {
		// call kernel proc
		ok = (pfnGetLong)(shortPath, longPath, _MAX_PATH) != 0;
	} else {
		char short_path[_MAX_PATH];  // copy, so we can modify it
		char* tok;

		*longPath = '\0';

		lstrcpyn(short_path, shortPath, _MAX_PATH);

		for (;;) {
			tok = strtok(short_path, "\\");
			if (tok == NULL)
				break;

			if ((strlen(shortPath) > 3) &&
			        (shortPath[0] == '\\') && (shortPath[1] == '\\')) {
				// UNC, skip first seps
				strcat(longPath, "\\\\");
				strcat(longPath, tok);
				strcat(longPath, "\\");

				tok = strtok(NULL, "\\");
				if (tok == NULL)
					break;
			}
			strcat(longPath, tok);

			bool isDir = false;

			for (;;) {
				WIN32_FIND_DATA fd;
				HANDLE hfind;
				char* tokend;

				tok = strtok(NULL, "\\");
				if (tok == NULL)
					break;

				strcat(longPath, "\\");
				tokend = longPath + strlen(longPath);

				// temporary add short component
				strcpy(tokend, tok);

				hfind = ::FindFirstFile(longPath, &fd);
				if (hfind == INVALID_HANDLE_VALUE)
					break;

				isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

				// finally add long component we got
				strcpy(tokend, fd.cFileName);

				::FindClose(hfind);
			}
			ok = tok == NULL;

			if (ok && isDir)
				strcat(longPath, "\\");

			break;
		}
	}

	if (!ok) {
		lstrcpyn(longPath, shortPath, _MAX_PATH);
	}
	return ok;
}

void SciTEWin::FixFilePath() {
	char longPath[_MAX_PATH];
	// first try MakeLongPath which corrects the path and the case of filename too
	if (MakeLongPath(fullPath, longPath)) {
		strcpy(fullPath, longPath);
		char *cpDirEnd = strrchr(fullPath, pathSepChar);
		if (cpDirEnd) {
			strcpy(fileName, cpDirEnd + 1);
			strcpy(dirName, fullPath);
			dirName[cpDirEnd - fullPath] = '\0';
		}
	} else {
		// On Windows file comparison is done case insensitively so the user can
		// enter scite.cxx and still open this file, SciTE.cxx. To ensure that the file
		// is saved with correct capitalisation FindFirstFile is used to find out the
		// real name of the file.
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind = ::FindFirstFile(fullPath, &FindFileData);
		if (hFind != INVALID_HANDLE_VALUE) {	// FindFirstFile found the file
			char *cpDirEnd = strrchr(fullPath, pathSepChar);
			if (cpDirEnd) {
				strcpy(fileName, FindFileData.cFileName);
				strcpy(dirName, fullPath);
				dirName[cpDirEnd - fullPath] = '\0';
				strcpy(fullPath, dirName);
				strcat(fullPath, pathSepString);
				strcat(fullPath, fileName);
			}
			::FindClose(hFind);
		}
	}
}

/**
 * Take a filename or relative path and put it at the end of the current path.
 * If the path is absolute, return the same path.
 */
void SciTEWin::AbsolutePath(char *absPath, const char *relativePath, int size) {
	// The runtime libraries for GCC and Visual C++ give different results for _fullpath
	// so use the OS.
	LPTSTR fileBit = 0;
	::GetFullPathName(relativePath, size, absPath, &fileBit);
	//Platform::DebugPrintf("AbsolutePath: <%s> -> <%s>\n", relativePath, absPath);
}

/**
 * Run a command with redirected input and output streams
 * so the output can be put in a window.
 * It is based upon several usenet posts and a knowledge base article.
 */
void SciTEWin::ProcessExecute() {
	DWORD exitcode = 0;
	if (scrollOutput)
		SendOutput(SCI_GOTOPOS, SendOutput(SCI_GETTEXTLENGTH));
	int originalEnd = SendOutput(SCI_GETCURRENTPOS);
	bool seenOutput = false;

	for (int icmd = 0; icmd < commandCurrent && icmd < commandMax && exitcode == 0; icmd++) {

		time_t timeStart = time(0);

		if (jobQueue[icmd].jobType == jobShell) {
			ShellExec(jobQueue[icmd].command, jobQueue[icmd].directory);
			continue;
		}

		if (jobQueue[icmd].jobType == jobExtension) {
			if (extender)
				extender->OnExecute(jobQueue[icmd].command.c_str());
			continue;
		}

		if (jobQueue[icmd].jobType == jobHelp) {
			ExecuteHelp(jobQueue[icmd].command.c_str());
			continue;
		}

		if (jobQueue[icmd].jobType == jobOtherHelp) {
			ExecuteOtherHelp(jobQueue[icmd].command.c_str());
			continue;
		}

		OSVERSIONINFO osv = {sizeof(OSVERSIONINFO), 0, 0, 0, 0, ""};
		::GetVersionEx(&osv);
		bool windows95 = osv.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;

		SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), 0, 0};
		char buffer[16384];
		//Platform::DebugPrintf("Execute <%s>\n", command);
		OutputAppendStringSynchronised(">");
		OutputAppendStringSynchronised(jobQueue[icmd].command.c_str());
		OutputAppendStringSynchronised("\n");

		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = NULL;

		SECURITY_DESCRIPTOR sd;
		// If NT make a real security thing to allow inheriting handles
		if (!windows95) {
			::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
			::SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = &sd;
		}

		HANDLE hPipeWrite = NULL;
		HANDLE hPipeRead = NULL;
		// Create pipe for output redirection
		// read handle, write handle, security attributes,  number of bytes reserved for pipe - 0 default
		::CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0);

		//Platform::DebugPrintf("2Execute <%s>\n");
		// Create pipe for input redirection. In this code, you do not
		// redirect the output of the child process, but you need a handle
		// to set the hStdInput field in the STARTUP_INFO struct. For safety,
		// you should not set the handles to an invalid handle.

		HANDLE hWrite2 = NULL;
		HANDLE hRead2 = NULL;
		// read handle, write handle, security attributes,  number of bytes reserved for pipe - 0 default
		::CreatePipe(&hRead2, &hWrite2, &sa, 0);

		::SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, 0);
		::SetHandleInformation(hRead2, HANDLE_FLAG_INHERIT, 0);

		//Platform::DebugPrintf("3Execute <%s>\n");
		// Make child process use hPipeWrite as standard out, and make
		// sure it does not show on screen.
		STARTUPINFO si = {
		    sizeof(STARTUPINFO), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		if (jobQueue[icmd].jobType == jobCLI)
			si.wShowWindow = SW_HIDE;
		else
			si.wShowWindow = SW_SHOW;
		si.hStdInput = hWrite2;
		si.hStdOutput = hPipeWrite;
		si.hStdError = hPipeWrite;

		char startDirectory[_MAX_PATH];
		startDirectory[0] = '\0';
		AbsolutePath(startDirectory, jobQueue[icmd].directory.c_str(), _MAX_PATH);

		PROCESS_INFORMATION pi = {0, 0, 0, 0};

		bool worked = ::CreateProcess(
		                  NULL,
		                  const_cast<char *>(jobQueue[icmd].command.c_str()),
		                  NULL, NULL,
		                  TRUE, 0,
		                  NULL,
		                  startDirectory[0] ? startDirectory : NULL,
		                  &si, &pi);

		if (!worked) {
			OutputAppendStringSynchronised(">Failed to CreateProcess\n");
		}

		// Now that this has been inherited, close it to be safe.
		::CloseHandle(hPipeWrite);

		// These are no longer needed
		::CloseHandle(hWrite2);
		::CloseHandle(hRead2);

		bool completed = !worked;
		DWORD timeDetectedDeath = 0;
		while (!completed) {

			Sleep(100L);

			DWORD bytesRead = 0;
			DWORD bytesAvail = 0;
			if (!::PeekNamedPipe(hPipeRead, buffer,
				                sizeof(buffer), &bytesRead, &bytesAvail, NULL)) {
				bytesAvail = 0;
			}

			if (bytesAvail > 0) {
				int bTest = ::ReadFile(hPipeRead, buffer,
				                       sizeof(buffer), &bytesRead, NULL);

				if (bTest && bytesRead) {
					if (!seenOutput) {
						MakeOutputVisible();
						seenOutput = true;
					}
					// Display the data
					OutputAppendStringSynchronised(buffer, bytesRead);
					::UpdateWindow(MainHWND());
				} else {
					completed = true;
				}
			} else {
				DWORD dwExitCode = STILL_ACTIVE;
				if (::GetExitCodeProcess(pi.hProcess, &dwExitCode)) {
					if (STILL_ACTIVE != dwExitCode) {
						if (windows95) {
							// Process is dead, but wait a second in case there is some output in transit
							if (timeDetectedDeath == 0) {
								timeDetectedDeath = ::GetTickCount();
							} else {
								if ((::GetTickCount() - timeDetectedDeath) >
										static_cast<unsigned int>(props.GetInt("win95.death.delay", 500))) {
									completed = true;    // It's a dead process
								}
							}
						} else {	// NT, so dead already
							completed = true;
						}
					}
				}
			}

			if (cancelFlag) {
				::TerminateProcess(pi.hProcess, 1);
				completed = true;
			}
		}

		if (worked) {
			::WaitForSingleObject(pi.hProcess, INFINITE);
			::GetExitCodeProcess(pi.hProcess, &exitcode);
			if (isBuilding) {
				// The build command is first command in a sequence so it is only built if
				// that command succeeds not if a second returns after document is modified.
				isBuilding = false;
				if (exitcode == 0)
					isBuilt = true;
			}
			SString sExitMessage(exitcode);
			sExitMessage.insert(0, ">Exit code: ");
			if (timeCommands) {
				sExitMessage += "    Time: ";
				sExitMessage += SString(time(0) - timeStart);
			}
			sExitMessage.append("\n");
			OutputAppendStringSynchronised(sExitMessage.c_str());
			::CloseHandle(pi.hProcess);
			::CloseHandle(pi.hThread);
			WarnUser(warnExecuteOK);
		} else {
			WarnUser(warnExecuteKO);
		}
		::CloseHandle(hPipeRead);
	}

	// Move selection back to beginning of this run so that F4 will go
	// to first error of this run.
	if (scrollOutput && returnOutputToCommand)
		SendOutputEx(SCI_GOTOPOS, originalEnd, 0, false);
	returnOutputToCommand = true;
	::SendMessage(MainHWND(), WM_COMMAND, IDM_FINISHEDEXECUTE, 0);
}

void ExecThread(void *ptw) {
	SciTEWin *tw = reinterpret_cast<SciTEWin *>(ptw);
	tw->ProcessExecute();
}

struct ShellErr {
	DWORD code;
	const char *descr;
};

void SciTEWin::ShellExec(const SString &cmd, const SString &dir) {
	char *mycmd;

	// guess if cmd is an executable, if this succeeds it can
	// contain spaces without enclosing it with "
	char *mycmdcopy = StringDup(cmd.c_str());
	strlwr(mycmdcopy);

	char *mycmd_end = NULL;
	char *myparams = NULL;

	char *s = strstr(mycmdcopy, ".exe");
	if (s == NULL)
		s = strstr(mycmdcopy, ".cmd");
	if (s == NULL)
		s = strstr(mycmdcopy, ".bat");
	if (s == NULL)
		s = strstr(mycmdcopy, ".com");
	if ((s != NULL) && ((*(s + 4) == '\0') || (*(s + 4) == ' '))) {
		int len_mycmd = s - mycmdcopy + 4;
		delete []mycmdcopy;
		mycmdcopy = StringDup(cmd.c_str());
		mycmd = mycmdcopy;
		mycmd_end = mycmdcopy + len_mycmd;
	} else {
		delete []mycmdcopy;
		mycmdcopy = StringDup(cmd.c_str());
		if (*mycmdcopy != '"') {
			// get next space to separate cmd and parameters
			mycmd_end = strchr(mycmdcopy, ' ');
			mycmd = mycmdcopy;
		} else {
			// the cmd is surrounded by ", so it can contain spaces, but we must
			// strip the " for ShellExec
			mycmd = mycmdcopy + 1;
			char *s = strchr(mycmdcopy + 1, '"');
			if (s != NULL) {
				*s = '\0';
				mycmd_end = s + 1;
			}
		}
	}

	if ((mycmd_end != NULL) && (*mycmd_end != '\0')) {
		*mycmd_end = '\0';
		// test for remaining params after cmd, they may be surrounded by " but
		// we give them as-is to ShellExec
		++mycmd_end;
		while (*mycmd_end == ' ')
			++mycmd_end;

		if (*mycmd_end != '\0')
			myparams = mycmd_end;
	}

	DWORD rc = reinterpret_cast<DWORD>(
	               ::ShellExecute(
	                   MainHWND(),        // parent wnd for msgboxes during app start
	                   NULL,         // cmd is open
	                   mycmd,        // file to open
	                   myparams,        // parameters
	                   dir.c_str(),        // launch directory
	                   SW_SHOWNORMAL)); //default show cmd

	if (rc > 32) {
		// it worked!
		delete []mycmdcopy;
		return ;
	}

	const int numErrcodes = 15;
	static const ShellErr field[numErrcodes] = {
	    { 0, "The operating system is out of memory or resources." },
	    { ERROR_FILE_NOT_FOUND, "The specified file was not found." },
	    { ERROR_PATH_NOT_FOUND, "The specified path was not found." },
	    { ERROR_BAD_FORMAT, "The .exe file is invalid (non-Win32\256 .exe or error in .exe image)." },
	    { SE_ERR_ACCESSDENIED, "The operating system denied access to the specified file." },
	    { SE_ERR_ASSOCINCOMPLETE, "The file name association is incomplete or invalid." },
	    { SE_ERR_DDEBUSY, "The DDE transaction could not be completed because other DDE transactions were being processed." },
	    { SE_ERR_DDEFAIL, "The DDE transaction failed." },
	    { SE_ERR_DDETIMEOUT, "The DDE transaction could not be completed because the request timed out." },
	    { SE_ERR_DLLNOTFOUND, "The specified dynamic-link library was not found." },
	    { SE_ERR_FNF, "The specified file was not found." },
	    { SE_ERR_NOASSOC, "There is no application associated with the given file name extension." },
	    { SE_ERR_OOM, "There was not enough memory to complete the operation." },
	    { SE_ERR_PNF, "The specified path was not found." },
	    { SE_ERR_SHARE, "A sharing violation occurred." },
	};

	int i;
	for (i = 0; i < numErrcodes; ++i) {
		if (field[i].code == rc)
			break;
	}

	SString errormsg("Error while launching:\n\"");
	errormsg += mycmdcopy;
	if (myparams != NULL) {
		errormsg += "\" with Params:\n\"";
		errormsg += myparams;
	}
	errormsg += "\"\n";
	if (i < numErrcodes) {
		errormsg += field[i].descr;
	} else {
		errormsg += "Unknown error code:";
		errormsg += SString(rc).c_str();
	}
	::MessageBox(MainHWND(), errormsg.c_str(), appName, MB_OK);

	delete []mycmdcopy;
}

void SciTEWin::Execute() {
	SciTEBase::Execute();

	_beginthread(ExecThread, 1024 * 1024, reinterpret_cast<void *>(this));
}

void SciTEWin::StopExecute() {
	::InterlockedExchange(&cancelFlag, 1L);
}

void SciTEWin::AddCommand(const SString &cmd, const SString &dir, JobSubsystem jobType, bool forceQueue) {
	if (cmd.length()) {
		if ((jobType == jobShell) && !forceQueue) {
			ShellExec(cmd, dir);
		} else {
			SciTEBase::AddCommand(cmd, dir, jobType, forceQueue);
		}
	}
}

void SciTEWin::QuitProgram() {
	if (SaveIfUnsureAll() != IDCANCEL) {
		if (fullScreen)	// Ensure tray visible on exit
			FullScreenToggle();
		::PostQuitMessage(0);
		wSciTE.Destroy();
	}
}

void SciTEWin::CreateUI() {
	// Pass 'this' pointer in lpParam of CreateWindow().
	int left = props.GetInt("position.left", CW_USEDEFAULT);
	int top = props.GetInt("position.top", CW_USEDEFAULT);
	int width = props.GetInt("position.width", CW_USEDEFAULT);
	int height = props.GetInt("position.height", CW_USEDEFAULT);
	if (width == -1 || height == -1) {
		cmdShow = SW_MAXIMIZE;
		width = CW_USEDEFAULT;
		height = CW_USEDEFAULT;
	}
	wSciTE = ::CreateWindowEx(
	             0,
	             className,
	             windowName.c_str(),
	             WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
	             WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
	             WS_CLIPCHILDREN,
	             left, top, width, height,
	             NULL,
	             NULL,
	             hInstance,
	             reinterpret_cast<LPSTR>(this));
	if (!wSciTE.Created())
		exit(FALSE);

	LocaliseMenus();
	SString pageSetup = props.Get("print.margins");
	char val[32];
	char *ps = StringDup(pageSetup.c_str());
	const char *next = GetNextPropItem(ps, val, 32);
	pagesetupMargin.left = atol(val);
	next = GetNextPropItem(next, val, 32);
	pagesetupMargin.right = atol(val);
	next = GetNextPropItem(next, val, 32);
	pagesetupMargin.top = atol(val);
	GetNextPropItem(next, val, 32);
	pagesetupMargin.bottom = atol(val);
	delete []ps;

	UIAvailable();
}

SString SciTEWin::ProcessArgs(const char *cmdLine) {
	// Break up the command line into individual arguments and strip double quotes
	// from each argument creating a string with each argument separated by '\n'
	SString args;
	const char *startArg = cmdLine;
	while (*startArg) {
		while (isspace(*startArg)) {
			startArg++;
		}
		const char *endArg = startArg;
		if (*startArg == '"') {	// Opening double-quote
			startArg++;
			endArg = startArg;
			while (*endArg && *endArg != '\"') {
				endArg++;
			}
		} else {	// No double-quote, end of argument on first space
			while (*endArg && !isspace(*endArg)) {
				endArg++;
			}
		}
		SString arg(startArg, 0, endArg - startArg);
		args.appendwithseparator(arg.c_str(), '\n');
		startArg = endArg;	// On a space or a double-quote, or on the end of the command line
		if (*startArg) {
			startArg++;
		}
	}

	return args;
}

void SciTEWin::Run(const char *cmdLine) {
	if (props.GetInt("check.if.already.open")) {
		HWND hAnother = ::FindWindow("SciTEWindow", NULL);
		if (hAnother) {
			::SetForegroundWindow(hAnother);
			COPYDATASTRUCT cds;
			cds.dwData = 0;
			// Send 2 messages - first the CWD, then the real 
			// command-line. (restoring the cwd could be  done, 
			// but keeping it to the last file opened can also 
			// be useful)
			TCHAR cwdCmd[MAX_PATH+7]; // 7 for "-cwd:" and 2x'"'
			strcpy(cwdCmd, "\"-cwd:");
			getcwd(cwdCmd+strlen(cwdCmd), MAX_PATH);
			strcat(cwdCmd, "\"");
			// defeat the "\" mangling - convert "\" to "/"
			for (char *temp=cwdCmd;*temp;temp++)
				if (*temp=='\\')
					*temp = '/';
			cds.cbData = strlen(cwdCmd)+1;
			cds.lpData = static_cast<void *>(cwdCmd);
			::SendMessage(hAnother, WM_COPYDATA, 0,
				reinterpret_cast<LPARAM>(&cds));
			// now the commandline itself.
			cds.cbData = strlen(cmdLine)+1;
			cds.lpData = static_cast<void *>(
				const_cast<char *>(cmdLine));
			::SendMessage(hAnother, WM_COPYDATA, 0,
				reinterpret_cast<LPARAM>(&cds));
			exit(0);
		}
	}

	SString args = ProcessArgs(cmdLine);

	bool performPrint = ProcessCommandLine(args, 0);

	CreateUI();

	if (performPrint) {
		Print(false);
		::PostQuitMessage(0);
	}

	SizeSubWindows();
	wSciTE.Show();
	if (cmdShow)	// assume SW_MAXIMIZE only
		::ShowWindow(MainHWND(), cmdShow);

	// Open all files given on command line.
	// The filenames containing spaces must be enquoted.
	// In case of not using buffers they get closed immediately except
	// the last one, but they move to the MRU file list
	ProcessCommandLine(args, 1);
	Redraw();
}

void SciTEWin::Paint(Surface *surfaceWindow, PRectangle) {
	PRectangle rcInternal = GetClientRectangle();
	//surfaceWindow->FillRectangle(rcInternal, Colour(0xff,0x80,0x80));

	int heightClient = rcInternal.Height();
	int widthClient = rcInternal.Width();

	int heightEditor = heightClient - heightOutput - heightBar;
	int yBorder = heightEditor;
	int xBorder = widthClient - heightOutput - heightBar;
	for (int i = 0; i < heightBar; i++) {
		if (i == 1)
			surfaceWindow->PenColour(GetSysColor(COLOR_3DHIGHLIGHT));
		else if (i == heightBar - 2)
			surfaceWindow->PenColour(GetSysColor(COLOR_3DSHADOW));
		else if (i == heightBar - 1)
			surfaceWindow->PenColour(GetSysColor(COLOR_3DDKSHADOW));
		else
			surfaceWindow->PenColour(GetSysColor(COLOR_3DFACE));
		if (splitVertical) {
			surfaceWindow->MoveTo(xBorder + i, 0);
			surfaceWindow->LineTo(xBorder + i, heightClient);
		} else {
			surfaceWindow->MoveTo(0, yBorder + i);
			surfaceWindow->LineTo(widthClient, yBorder + i);
		}
	}
}

void SciTEWin::AboutDialog() {
#ifdef STATIC_BUILD
	AboutDialogWithBuild(1);
#else
	AboutDialogWithBuild(0);
#endif
}

void SciTEWin::DropFiles(HDROP hdrop) {
	// If drag'n'drop inside the SciTE window but outside
	// Scintilla, hdrop is null, and an exception is generated!
	if (hdrop) {
		int filesDropped = ::DragQueryFile(hdrop, 0xffffffff, NULL, 0);
		for (int i = 0; i < filesDropped; ++i) {
			char pathDropped[MAX_PATH];
			::DragQueryFile(hdrop, i, pathDropped, sizeof(pathDropped));
			if (!Open(pathDropped)) {
				break;
			}
		}
		::DragFinish(hdrop);
		// Put SciTE to forefront
		// May not work for Win2k, but OK for lower versions
		// Note: how to drop a file to an iconic window?
		// Actually, it is the Send To command that generates a drop.
		if (::IsIconic(MainHWND())) {
			::ShowWindow(MainHWND(), SW_RESTORE);
		}
		::SetForegroundWindow(MainHWND());
	}
}

void SciTEWin::MinimizeToTray() {
	char n[64] = "SciTE";
	NOTIFYICONDATA nid;
	memset(&nid, 0, sizeof(nid));
	nid.cbSize = sizeof(nid);
	nid.hWnd = MainHWND();
	nid.uID = 1;
	nid.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP;
	nid.uCallbackMessage = SCITE_TRAY;
	nid.hIcon  = static_cast<HICON>(
		::LoadImage(hInstance, "SCITE", IMAGE_ICON, 16, 16, LR_DEFAULTSIZE));
	strcpy(nid.szTip,n);
	::ShowWindow(MainHWND(), SW_MINIMIZE);
	if (::Shell_NotifyIcon(NIM_ADD, &nid)){
		::ShowWindow(MainHWND(), SW_HIDE);
	}
}

void SciTEWin::RestoreFromTray() {
	NOTIFYICONDATA nid;
	memset(&nid, 0, sizeof(nid));
	nid.cbSize = sizeof(nid);
	nid.hWnd = MainHWND();
	nid.uID = 1;
	::ShowWindow(MainHWND(), SW_SHOW);
	::Sleep(100);
	::Shell_NotifyIcon(NIM_DELETE, &nid);
}

LRESULT SciTEWin::CopyData(COPYDATASTRUCT *pcds) {
	if (pcds) {
		if (props.GetInt("minimize.to.tray")) {
			RestoreFromTray();
		}
		const char *text = static_cast<char *>(pcds->lpData);
		if (text && (strlen(text) > 0)) {
			SString args = ProcessArgs(text);
			ProcessCommandLine(args, 0);
			ProcessCommandLine(args, 1);
		}
		::FlashWindow(MainHWND(), FALSE);
	}
	return TRUE;
}

static bool KeyMatch(SString sKey, int keyval, int modifiers) {
	if (keyval == 0x11)
		return false;
	if (keyval == 0x10)
		return false;
	if (0 == sKey.length()) 
		return false;
	int modsInKey = 0;
	if (sKey.contains("Ctrl+")) {
		modsInKey |= SCMOD_CTRL;
		sKey.remove("Ctrl+");
	}
	if (sKey.contains("Shift+")) {
		modsInKey |= SCMOD_SHIFT;
		sKey.remove("Shift+");
	}
	if (sKey.contains("Alt+")) {
		modsInKey |= SCMOD_ALT;
		sKey.remove("Alt+");
	}
	if (modifiers != modsInKey)
		return false;
	if ((sKey.length() == 1) && (modsInKey & SCMOD_CTRL)) {
		char keySought = sKey[0];
		return keySought == keyval;
	}
	if ((sKey.length() > 1) && (sKey[0] == 'F') && (isdigit(sKey[1]))) {
		sKey.remove("F");
		int keyNum = sKey.value();
		if (keyNum == (keyval - VK_F1 + 1))
			return true;
	}
	return false;
}

LRESULT SciTEWin::KeyDown(WPARAM wParam) {
	// Look through lexer menu
	int modifiers =
		(Platform::IsKeyDown(VK_SHIFT) ? SCMOD_SHIFT : 0) |
		(Platform::IsKeyDown(VK_CONTROL) ? SCMOD_CTRL : 0) |
		(Platform::IsKeyDown(VK_MENU) ? SCMOD_ALT : 0);
	for (int j=0; j<languageItems; j++) {
		if (KeyMatch(languageMenu[j].menuKey, wParam, modifiers)) {
			SciTEBase::MenuCommand(IDM_LANGUAGE + j);
			return 1l;
		}
	}
	return 0l;
}

LRESULT SciTEWin::WndProc(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	//Platform::DebugPrintf("start wnd proc %x %x\n",iMessage, MainHWND());
	switch (iMessage) {

	case WM_CREATE:
		Creation();
		break;

	case WM_COMMAND:
		Command(wParam, lParam);
		break;

	case WM_SYSCOMMAND:
		if ((wParam == SC_MINIMIZE) && props.GetInt("minimize.to.tray")) {
			MinimizeToTray();
			return 0;
		}
		return ::DefWindowProc(MainHWND(), iMessage, wParam, lParam);

	case SCITE_TRAY:
		if (lParam == WM_LBUTTONDBLCLK) {
			RestoreFromTray();
			::ShowWindow(MainHWND(), SW_RESTORE);
			::FlashWindow(MainHWND(), FALSE);
		}
		break;

	case WM_NOTIFY:
		Notify(reinterpret_cast<SCNotification *>(lParam));
		break;

	case WM_KEYDOWN:
		return KeyDown(wParam);

	case WM_KEYUP:
		//Platform::DebugPrintf("keyup %d %x %x\n",iMessage, wParam, lParam);
		break;

	case WM_SIZE:
		//Platform::DebugPrintf("size %d %x %x\n",iMessage, wParam, lParam);
		if (wParam != 1)
			SizeSubWindows();
		break;

	case WM_GETMINMAXINFO: {
			MINMAXINFO *pmmi = reinterpret_cast<MINMAXINFO *>(lParam);
			if (fullScreen) {
				// Last constants for both x and y are just fiddles - don't know why they are needed
				pmmi->ptMaxSize.x = ::GetSystemMetrics(SM_CXSCREEN) +
				                    2 * ::GetSystemMetrics(SM_CXSIZEFRAME) +
				                    2;
				pmmi->ptMaxSize.y = ::GetSystemMetrics(SM_CYSCREEN) +
				                    ::GetSystemMetrics(SM_CYCAPTION) +
				                    ::GetSystemMetrics(SM_CYMENU) +
				                    2 * ::GetSystemMetrics(SM_CYSIZEFRAME) +
				                    3;
				pmmi->ptMaxTrackSize.x = pmmi->ptMaxSize.x;
				pmmi->ptMaxTrackSize.y = pmmi->ptMaxSize.y;
				return 0;
			} else {
				return ::DefWindowProc(MainHWND(), iMessage, wParam, lParam);
			}
		}

	case WM_INITMENU:
		CheckMenus();
		break;

	case WM_CLOSE:
		QuitProgram();
		return 0;

	case WM_DESTROY:
		break;

	case WM_SETTINGCHANGE:
		//Platform::DebugPrintf("** Setting Changed\n");
		SendEditor(WM_SETTINGCHANGE, wParam, lParam);
		SendOutput(WM_SETTINGCHANGE, wParam, lParam);
		break;

	case WM_SYSCOLORCHANGE:
		//Platform::DebugPrintf("** Color Changed\n");
		SendEditor(WM_SYSCOLORCHANGE, wParam, lParam);
		SendOutput(WM_SYSCOLORCHANGE, wParam, lParam);
		break;

	case WM_PALETTECHANGED:
		//Platform::DebugPrintf("** Palette Changed\n");
		if (wParam != reinterpret_cast<WPARAM>(MainHWND())) {
			SendEditor(WM_PALETTECHANGED, wParam, lParam);
			//SendOutput(WM_PALETTECHANGED, wParam, lParam);
		}

		break;

	case WM_QUERYNEWPALETTE:
		//Platform::DebugPrintf("** Query palette\n");
		SendEditor(WM_QUERYNEWPALETTE, wParam, lParam);
		//SendOutput(WM_QUERYNEWPALETTE, wParam, lParam);
		return TRUE;

	case WM_ACTIVATEAPP:
		SendEditor(SCI_HIDESELECTION, !wParam);
		// Do not want to display dialog yet as may be in middle of system mouse capture
		::PostMessage(MainHWND(), WM_COMMAND, IDM_ACTIVATE, wParam);
		break;

	case WM_ACTIVATE:
		::SetFocus(reinterpret_cast<HWND>(wEditor.GetID()));
		break;

	case WM_DROPFILES:
		DropFiles(reinterpret_cast<HDROP>(wParam));
		break;

	case WM_COPYDATA:
		return CopyData(reinterpret_cast<COPYDATASTRUCT *>(lParam));

	default:
		//Platform::DebugPrintf("default wnd proc %x %d %d\n",iMessage, wParam, lParam);
		return ::DefWindowProc(MainHWND(), iMessage, wParam, lParam);
	}
	//Platform::DebugPrintf("end wnd proc\n");
	return 0l;
}

LRESULT PASCAL SciTEWin::TWndProc(
    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	//Platform::DebugPrintf("W:%x M:%d WP:%x L:%x\n", hWnd, iMessage, wParam, lParam);

	// Find C++ object associated with window.
	SciTEWin *scite = reinterpret_cast<SciTEWin *>(GetWindowLong(hWnd, 0));
	// scite will be zero if WM_CREATE not seen yet
	if (scite == 0) {
		if (iMessage == WM_CREATE) {
			LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			scite = reinterpret_cast<SciTEWin *>(cs->lpCreateParams);
			scite->wSciTE = hWnd;
			::SetWindowLong(hWnd, 0, reinterpret_cast<LONG>(scite));
			return scite->WndProc(iMessage, wParam, lParam);
		} else
			return ::DefWindowProc(hWnd, iMessage, wParam, lParam);
	} else
		return scite->WndProc(iMessage, wParam, lParam);
}

LRESULT SciTEWin::WndProcI(UINT iMessage, WPARAM wParam, LPARAM lParam) {
	switch (iMessage) {

	case WM_COMMAND:
		Command(wParam, lParam);
		break;

	case WM_NOTIFY:
		Notify(reinterpret_cast<SCNotification *>(lParam));
		break;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			::BeginPaint(reinterpret_cast<HWND>(wContent.GetID()), &ps);
			Surface *surfaceWindow = Surface::Allocate();
			if (surfaceWindow) {
				surfaceWindow->Init(ps.hdc);
				PRectangle rcPaint(ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
				Paint(surfaceWindow, rcPaint);
				surfaceWindow->Release();
				delete surfaceWindow;
			}
			::EndPaint(reinterpret_cast<HWND>(wContent.GetID()), &ps);
			return 0;
		}

	case WM_LBUTTONDOWN:
		ptStartDrag = Point::FromLong(lParam);
		capturedMouse = true;
		heightOutputStartDrag = heightOutput;
		::SetCapture(reinterpret_cast<HWND>(wContent.GetID()));
		//Platform::DebugPrintf("Click %x %x\n", wParam, lParam);
		break;

	case WM_MOUSEMOVE:
		if (capturedMouse) {
			MoveSplit(Point::FromLong(lParam));
		}
		break;

	case WM_LBUTTONUP:
		if (capturedMouse) {
			MoveSplit(Point::FromLong(lParam));
			capturedMouse = false;
			::ReleaseCapture();
		}
		break;

	case WM_SETCURSOR:
		if (ControlIDOfCommand(lParam) == HTCLIENT) {
			Point ptCursor;
			::GetCursorPos(reinterpret_cast<POINT *>(&ptCursor));
			Point ptClient = ptCursor;
			::ScreenToClient(MainHWND(), reinterpret_cast<POINT *>(&ptClient));
			if ((ptClient.y > (visHeightTools + visHeightTab)) && (ptClient.y < visHeightTools + visHeightTab + visHeightEditor)) {
				PRectangle rcScintilla = wEditor.GetPosition();
				PRectangle rcOutput = wOutput.GetPosition();
				if (!rcScintilla.Contains(ptCursor) && !rcOutput.Contains(ptCursor)) {
					wSciTE.SetCursor(splitVertical ? Window::cursorHoriz : Window::cursorVert);
					return TRUE;
				}
			}
		}
		return ::DefWindowProc(MainHWND(), iMessage, wParam, lParam);

	default:
		//Platform::DebugPrintf("default wnd proc %x %d %d\n",iMessage, wParam, lParam);
		return ::DefWindowProc(reinterpret_cast<HWND>(wContent.GetID()), 
			iMessage, wParam, lParam);
	}
	//Platform::DebugPrintf("end wnd proc\n");
	return 0l;
}

LRESULT PASCAL SciTEWin::IWndProc(
    HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	//Platform::DebugPrintf("W:%x M:%d WP:%x L:%x\n", hWnd, iMessage, wParam, lParam);

	// Find C++ object associated with window.
	SciTEWin *scite = reinterpret_cast<SciTEWin *>(GetWindowLong(hWnd, 0));
	// scite will be zero if WM_CREATE not seen yet
	if (scite == 0) {
		if (iMessage == WM_CREATE) {
			LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			scite = reinterpret_cast<SciTEWin *>(cs->lpCreateParams);
			scite->wContent = hWnd;
			::SetWindowLong(hWnd, 0, reinterpret_cast<LONG>(scite));
			return scite->WndProcI(iMessage, wParam, lParam);
		} else
			return ::DefWindowProc(hWnd, iMessage, wParam, lParam);
	} else
		return scite->WndProcI(iMessage, wParam, lParam);
}

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpszCmdLine, int) {

#ifdef LUA_SCRIPTING
	LuaExtension luaExtender;
	Extension *extender = &luaExtender;
#else
#ifndef NO_FILER
	DirectorExtension director;
	Extension *extender = &director;
#else
	Extension *extender = 0;
#endif
#endif
	//Platform::DebugPrintf("Command line is \n%s\n<<", lpszCmdLine);

	HACCEL hAccTable = ::LoadAccelerators(hInstance, "ACCELS");

	SciTEWin::Register(hInstance);
#ifdef STATIC_BUILD
	Scintilla_RegisterClasses(hInstance);
#else
	HMODULE hmod = ::LoadLibrary("SciLexer.DLL");
	if (hmod == NULL)
		::MessageBox(NULL, "The Scintilla DLL could not be loaded.  SciTE will now close", "Error loading Scintilla", MB_OK | MB_ICONERROR);
#endif

	MSG msg;
	msg.wParam = 0; {
		SciTEWin MainWind(extender);
		MainWind.Run(lpszCmdLine);
		bool going = true;
		while (going) {
			going = ::GetMessage(&msg, NULL, 0, 0);
			if (going) {
				if (!MainWind.ModelessHandler(&msg)) {
					if (::TranslateAccelerator(reinterpret_cast<HWND>(MainWind.GetID()), hAccTable, &msg) == 0) {
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
				}
			}
		}
	}

#ifndef STATIC_BUILD
	::FreeLibrary(hmod);
#endif

	return msg.wParam;
}
