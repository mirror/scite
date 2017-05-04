// SciTE - Scintilla based Text Editor
/** @file Worker.h
 ** Definition of classes to perform background tasks as threads.
 **/
// Copyright 2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

struct Worker {
private:
	std::unique_ptr<Mutex> mutex;
	volatile bool completed;
	volatile bool cancelling;
	volatile size_t jobSize;
	volatile size_t jobProgress;
public:
	Worker() : mutex(Mutex::Create()), completed(false), cancelling(false), jobSize(1), jobProgress(0) {
	}
	// Deleted so Worker objects can not be copied.
	Worker(const Worker &) = delete;
	void operator=(const Worker &) = delete;
	virtual ~Worker() {
	}
	virtual void Execute() {}
	bool FinishedJob() const {
		Lock lock(mutex.get());
		return completed;
	}
	void SetCompleted() {
		Lock lock(mutex.get());
		completed = true;
	}
	bool Cancelling() const {
		Lock lock(mutex.get());
		return cancelling;
	}
	size_t SizeJob() const {
		Lock lock(mutex.get());
		return jobSize;
	}
	void SetSizeJob(size_t size) {
		Lock lock(mutex.get());
		jobSize = size;
	}
	size_t ProgressMade() const {
		Lock lock(mutex.get());
		return jobProgress;
	}
	void IncrementProgress(size_t increment) {
		Lock lock(mutex.get());
		jobProgress += increment;
	}
	virtual void Cancel() {
		{
			Lock lock(mutex.get());
			cancelling = true;
		}
		// Wait for writing thread to finish
		for (;;) {
			Lock lock(mutex.get());
			if (completed)
				return;
		}
	}
};

struct WorkerListener {
	virtual void PostOnMainThread(int cmd, Worker *pWorker) = 0;
};
