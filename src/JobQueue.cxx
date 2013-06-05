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
#include <set>
#include <map>
#include <algorithm>

#include "Scintilla.h"

#include "GUI.h"

#include "SString.h"
#include "StringHelpers.h"
#include "FilePath.h"
#include "PropSetFile.h"
#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"

JobSubsystem SubsystemFromChar(char c) {
	if (c == '1')
		return jobGUI;
	else if (c == '2')
		return jobShell;
	else if (c == '3')
		return jobExtension;
	else if (c == '4')
		return jobHelp;
	else if (c == '5')
		return jobOtherHelp;
	else if (c == '7')
		return jobImmediate;
	return jobCLI;
}

JobMode::JobMode(PropSetFile &props, int item, const char *fileNameExt) : jobType(jobCLI), saveBefore(0), isFilter(false), flags(0) {
	bool quiet = false;
	int repSel = 0;
	bool groupUndo = false;

	const std::string itemSuffix = StdStringFromInteger(item) + ".";
	std::string propName = std::string("command.mode.") + itemSuffix;
	std::string modeVal(props.GetNewExpand(propName.c_str(), fileNameExt).c_str());

	modeVal.erase(std::remove(modeVal.begin(), modeVal.end(),' '), modeVal.end());
	std::vector<std::string> modes = StringSplit(modeVal, ',');
	for (std::vector<std::string>::iterator it=modes.begin(); it != modes.end(); ++it) {

		std::vector<std::string> optValue = StringSplit(*it, ':');

		const std::string opt = optValue[0];
		const std::string value = (optValue.size() > 1) ? optValue[1] : std::string();

		if (opt == "subsystem" && !value.empty()) {
			if (value[0] == '0' || value == "console")
				jobType = jobCLI;
			else if (value[0] == '1' || value == "windows")
				jobType = jobGUI;
			else if (value[0] == '2' || value == "shellexec")
				jobType = jobShell;
			else if (value[0] == '3' || value == "lua" || value == "director")
				jobType = jobExtension;
			else if (value[0] == '4' || value == "htmlhelp")
				jobType = jobHelp;
			else if (value[0] == '5' || value == "winhelp")
				jobType = jobOtherHelp;
			else if (value[0] == '7' || value == "immediate")
				jobType = jobImmediate;
		}

		if (opt == "quiet") {
			if (value.empty() || value[0] == '1' || value == "yes")
				quiet = true;
			else if (value[0] == '0' || value == "no")
				quiet = false;
		}

		if (opt == "savebefore") {
			if (value.empty() || value[0] == '1' || value == "yes")
				saveBefore = 1;
			else if (value[0] == '0' || value == "no")
				saveBefore = 2;
			else if (value == "prompt")
				saveBefore = 0;
		}

		if (opt == "filter") {
			if (value.empty() || value[0] == '1' || value == "yes")
				isFilter = true;
			else if (value[0] == '0' || value == "no")
				isFilter = false;
		}

		if (opt == "replaceselection") {
			if (value.empty() || value[0] == '1' || value == "yes")
				repSel = 1;
			else if (value[0] == '0' || value == "no")
				repSel = 0;
			else if (value == "auto")
				repSel = 2;
		}

		if (opt == "groupundo") {
			if (value.empty() || value[0] == '1' || value == "yes")
				groupUndo = true;
			else if (value[0] == '0' || value == "no")
				groupUndo = false;
		}
	}

	// The mode flags also have classic properties with similar effect.
	// If the classic property is specified, it overrides the mode.
	// To see if the property is absent (as opposed to merely evaluating
	// to nothing after variable expansion), use GetWild for the
	// existence check.  However, for the value check, use getNewExpand.

	propName = "command.save.before.";
	propName += itemSuffix;
	if (props.GetWild(propName.c_str(), fileNameExt).length())
		saveBefore = props.GetNewExpand(propName.c_str(), fileNameExt).value();

	propName = "command.is.filter.";
	propName += itemSuffix;
	if (props.GetWild(propName.c_str(), fileNameExt).length())
		isFilter = (props.GetNewExpand(propName.c_str(), fileNameExt)[0] == '1');

	propName = "command.subsystem.";
	propName += itemSuffix;
	if (props.GetWild(propName.c_str(), fileNameExt).length()) {
		SString subsystemVal = props.GetNewExpand(propName.c_str(), fileNameExt);
		jobType = SubsystemFromChar(subsystemVal[0]);
	}

	propName = "command.input.";
	propName += itemSuffix;
	if (props.GetWild(propName.c_str(), fileNameExt).length()) {
		input = props.GetNewExpand(propName.c_str(), fileNameExt);
		flags |= jobHasInput;
	}

	propName = "command.quiet.";
	propName += itemSuffix;
	if (props.GetWild(propName.c_str(), fileNameExt).length())
		quiet = (props.GetNewExpand(propName.c_str(), fileNameExt).value() == 1);
	if (quiet)
		flags |= jobQuiet;

	propName = "command.replace.selection.";
	propName += itemSuffix;
	if (props.GetWild(propName.c_str(), fileNameExt).length())
		repSel = props.GetNewExpand(propName.c_str(), fileNameExt).value();

	if (repSel == 1)
		flags |= jobRepSelYes;
	else if (repSel == 2)
		flags |= jobRepSelAuto;

	if (groupUndo)
		flags |= jobGroupUndo;
}

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
