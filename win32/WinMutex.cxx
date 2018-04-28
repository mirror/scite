// SciTE - Scintilla based Text Editor
/** @file WinMutex.cxx
 ** Define mutex
 **/
// SciTE & Scintilla copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// Copyright 2007 by Neil Hodgson <neilh@scintilla.org>, from April White <april_white@sympatico.ca>
// The License.txt file describes the conditions under which this software may be distributed.

// http://www.microsoft.com/msj/0797/win320797.aspx

#include <windows.h>
#include "Mutex.h"

class WinMutex : public Mutex {
private:
	CRITICAL_SECTION cs;
	virtual void Lock() { ::EnterCriticalSection(&cs); }
	virtual void Unlock() { ::LeaveCriticalSection(&cs); }
	WinMutex() { ::InitializeCriticalSection(&cs); }
	// Deleted so WinMutex objects can not be copied.
	WinMutex(const WinMutex &) = delete;
	WinMutex(WinMutex &&) = delete;
	void operator=(const WinMutex &) = delete;
	void operator=(WinMutex &&) = delete;
	virtual ~WinMutex() { ::DeleteCriticalSection(&cs); }
	friend class Mutex;
};

Mutex *Mutex::Create() {
   return new WinMutex();
}
