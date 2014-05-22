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
#if GLIB_CHECK_VERSION(2,31,0)
	GMutex m;
#endif
	GMutex *pm;
	virtual void Lock() {
		g_mutex_lock(pm);
	}
	virtual void Unlock() {
		g_mutex_unlock(pm);
	}
	GTKMutex() {
#if GLIB_CHECK_VERSION(2,31,0)
		pm = &m;
		g_mutex_init(pm);
#else
		pm = g_mutex_new();
#endif
	}
	virtual ~GTKMutex() {
#if GLIB_CHECK_VERSION(2,31,0)
		g_mutex_clear(pm);
#else
		g_mutex_free(pm);
#endif
	}
	friend class Mutex;
};

Mutex *Mutex::Create() {
   return new GTKMutex();
}
