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
    jobCLI = 0, jobGUI = 1, jobShell = 2, jobExtension = 3, jobHelp = 4, jobOtherHelp = 5, jobGrep = 6, jobImmediate = 7};

JobSubsystem SubsystemFromChar(char c);

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

struct JobMode {
	JobSubsystem jobType;
	int saveBefore;
	bool isFilter;
	int flags;
	std::string input;
	JobMode(PropSetFile &props, int item, const char *fileNameExt);
};

class Job {
public:
	std::string command;
	FilePath directory;
	JobSubsystem jobType;
	std::string input;
	int flags;

	Job();
	Job(const std::string &command_, const FilePath &directory_, JobSubsystem jobType_, const std::string &input_, int flags_);
	void Clear();
};

class JobQueue {
public:
	std::unique_ptr<Mutex> mutex;
	bool clearBeforeExecute;
	bool isBuilding;
	bool isBuilt;
	bool executing;
	enum { commandMax = 2 };
	int commandCurrent;
	std::vector<Job> jobQueue;
	bool jobUsesOutputPane;
	long cancelFlag;
	bool timeCommands;

	JobQueue();
	~JobQueue();
	bool TimeCommands() const;
	bool ClearBeforeExecute() const;
	bool ShowOutputPane() const;
	bool IsExecuting() const;
	void SetExecuting(bool state);
	bool HasCommandToRun() const;
	long SetCancelFlag(long value);
	long Cancelled();

	void ClearJobs();
	void AddCommand(const std::string &command, const FilePath &directory, JobSubsystem jobType, const std::string &input, int flags);
};

#endif
