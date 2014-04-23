// SciTE - Scintilla based Text Editor
/** @file GTKMutex.cxx
 ** Define mutex
 **/
// SciTE & Scintilla copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// Copyright 2007 by Neil Hodgson <neilh@scintilla.org>, from April White <april_white@sympatico.ca>
// The License.txt file describes the conditions under which this software may be distributed.

// http://www.microsoft.com/msj/0797/win320797.aspx

#include <glib.h>

#include "Mutex.h"

class GTKMutex : public Mutex {
private:
	GMutex m;
	virtual void Lock() {
		g_mutex_lock(&m);
	}
	virtual void Unlock() {
		g_mutex_unlock(&m);
	}
	GTKMutex() {
		g_mutex_init(&m);
	}
	virtual ~GTKMutex() {
		g_mutex_clear(&m);
	}
	friend class Mutex;
};

Mutex *Mutex::Create() {
   return new GTKMutex();
}
