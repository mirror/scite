// SciTE - Scintilla based Text Editor
/** @file GTKMutex.cxx
 ** Define mutex
 **/
// SciTE & Scintilla copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// Copyright 2007 by Neil Hodgson <neilh@scintilla.org>, from April White <april_white@sympatico.ca>
// The License.txt file describes the conditions under which this software may be distributed.

// http://www.microsoft.com/msj/0797/win320797.aspx

#include "Mutex.h"

class GTKMutex : public Mutex {
private:
	virtual void Lock() {}
	virtual void Unlock() {}
	GTKMutex() {}
	virtual ~GTKMutex() {}
	friend class Mutex;
};

Mutex *Mutex::Create() {
   return new GTKMutex();
}
