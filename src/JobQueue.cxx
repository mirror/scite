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

#include <string>
#include <vector>

#include "Scintilla.h"

#include "GUI.h"

#include "SString.h"
#include "FilePath.h"

#include "SciTE.h"

#include "Mutex.h"
#include "JobQueue.h"

void JobQueue::ClearJobs() {
	for (int ic = 0; ic < commandMax; ic++) {
		jobQueue[ic].Clear();
	}
	commandCurrent = 0;
}

void JobQueue::AddCommand(const SString &command, const FilePath &directory, JobSubsystem jobType, const SString &input, int flags) {
	if ((commandCurrent < commandMax) && (command.length())) {
		if (commandCurrent == 0)
			jobUsesOutputPane = false;
		jobQueue[commandCurrent] = Job(command, directory, jobType, input, flags);
		commandCurrent++;
		if (jobType == jobCLI)
			jobUsesOutputPane = true;
		// For jobExtension, the Trace() method shows output pane on demand.
	}
}
