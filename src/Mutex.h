// SciTE - Scintilla based Text Editor
/** @file Mutex.h
 ** Define mutex
 **/
// SciTE & Scintilla copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// Copyright 2007 by Neil Hodgson <neilh@scintilla.org>, from April White <april_white@sympatico.ca>
// The License.txt file describes the conditions under which this software may be distributed.

// TODO: see http://www.codeproject.com/threads/cppsyncstm.asp

#ifndef MUTEX_H
#define MUTEX_H

class Mutex {
public:
	virtual void Lock() = 0;
	virtual void Unlock() = 0;
	virtual ~Mutex() {}
	static Mutex *Create();
};

class Lock {
	Mutex *mute;
public:
	Lock(Mutex *mute_) : mute(mute_) {
		mute->Lock();
	}
	~Lock() {
		mute->Unlock();
	}
};

#endif
