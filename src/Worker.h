// SciTE - Scintilla based Text Editor
/** @file Worker.h
 ** Definition of classes to perform background tasks as threads.
 **/
// Copyright 2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

struct Worker {
	volatile bool completed;
	volatile bool cancelling;
	volatile int jobSize;
	volatile int jobProgress;

	Worker() : completed(false), cancelling(false), jobSize(1), jobProgress(0) {
	}
	virtual ~Worker() {}
	virtual void Execute() {}
	bool FinishedJob() const {
		return completed;
	}
	virtual void Cancel() {
		cancelling = true;
		// Wait for writing thread to finish
		while (!completed)
			;
	}
};

struct WorkerListener {
	virtual void PostOnMainThread(int cmd, Worker *pWorker) = 0;
};
