// SciTE - Scintilla based Text Editor
/** @file JobQueue.h
 ** Define job queue
 **/
// SciTE & Scintilla copyright 1998-2003 by Neil Hodgson <neilh@scintilla.org>
// Copyright 2007 by Neil Hodgson <neilh@scintilla.org>, from April White <april_white@sympatico.ca>
// The License.txt file describes the conditions under which this software may be distributed.

// TODO: see http://www.codeproject.com/threads/cppsyncstm.asp

#ifndef JOBQUEUE_H
#define JOBQUEUE_H

enum JobSubsystem {
    jobCLI = 0, jobGUI = 1, jobShell = 2, jobExtension = 3, jobHelp = 4, jobOtherHelp = 5, jobGrep = 6};

enum JobFlags {
    jobForceQueue = 1,
    jobHasInput = 2,
    jobQuiet = 4,
    // 8 reserved for jobVeryQuiet
    jobRepSelMask = 48,
    jobRepSelYes = 16,
    jobRepSelAuto = 32,
    jobGroupUndo = 64
};

class Job {
public:
	SString command;
	FilePath directory;
	JobSubsystem jobType;
	SString input;
	int flags;

	Job() {
		Clear();
	}

	Job(const SString &command_, const FilePath &directory_, JobSubsystem jobType_, const SString &input_, int flags_)
		: command(command_), directory(directory_), jobType(jobType_), input(input_), flags(flags_) {
	}

	void Clear() {
		command = "";
		directory.Init();
		jobType = jobCLI;
		input = "";
		flags = 0;
	}
};

class JobQueue {
public:
	Mutex *mutex;
	bool clearBeforeExecute;
	bool isBuilding;
	bool isBuilt;
	bool executing;
	enum { commandMax = 2 };
	int commandCurrent;
	Job jobQueue[commandMax];
	bool jobUsesOutputPane;
	long cancelFlag;
	bool timeCommands;

	JobQueue() {
		mutex = Mutex::Create();
		clearBeforeExecute = false;
		isBuilding = false;
		isBuilt = false;
		executing = false;
		commandCurrent = 0;
		jobUsesOutputPane = false;
		cancelFlag = 0L;
		timeCommands = false;
	}

	~JobQueue() {
		delete mutex;
		mutex = 0;
	}

	bool TimeCommands() const {
		Lock lock(mutex);
		return timeCommands;
	}

	bool ClearBeforeExecute() const {
		Lock lock(mutex);
		return clearBeforeExecute;
	}

	bool ShowOutputPane() const {
		Lock lock(mutex);
		return jobUsesOutputPane;
	}

	bool IsExecuting() const {
		Lock lock(mutex);
		return executing;
	}

	void SetExecuting(bool state) {
		Lock lock(mutex);
		executing = state;
	}

	bool HasCommandToRun() const {
		return commandCurrent > 0;
	}

	long SetCancelFlag(long value) {
		Lock lock(mutex);
		long cancelFlagPrevious = cancelFlag;
		cancelFlag = value;
		return cancelFlagPrevious;
	}

	long Cancelled() {
		Lock lock(mutex);
		return cancelFlag;
	}

	void ClearJobs();
	void AddCommand(const SString &command, const FilePath &directory, JobSubsystem jobType, const SString &input, int flags);
};

#endif
