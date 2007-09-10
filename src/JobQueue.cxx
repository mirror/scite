// SciTE - Scintilla based Text Editor
/** @file JobQueue.cxx
 ** Define job queue
 **/
// SciTE & Scintilla copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// Copyright 2007 by Neil Hodgson <neilh@scintilla.org>, from April White <april_white@sympatico.ca>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

#include "SciTE.h"
#include "PropSet.h"

#include "Scintilla.h"
#include "FilePath.h"

#include "Mutex.h"
#include "JobQueue.h"

