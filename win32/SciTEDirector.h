// SciTE - Scintilla based Text Editor
// SciTEDirector.h - defines interface between SciTE and a 'director' program 
// such as a Project Manager.
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.
/** @file **/
#ifndef NEWSSS
// These are values that go in the dwData field of the COPYDATASTRUCT
// From Director to SciTE or from SciTE to Director
#define SCD_IDENTIFY 1000
#define SCD_CLOSING 1001

// From Director to SciTE
#define SCD_OPEN 2000

// From SciTE to Director
#define SCD_OPENED 3000
#define SCD_SAVED 3001

#else

#define SCD_IDENTIFY 1000
#define SCD_OPEN 1001
#define SCD_CLOSING 1002

#define SCD_OPENED 2000
#define SCD_SAVED 2001
#endif
