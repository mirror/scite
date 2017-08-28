// SciTE - Scintilla based Text Editor
/** @file FileWorker.h
 ** Definition of classes to perform background file tasks as threads.
 **/
// Copyright 2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

/// Base size of file I/O operations.
const size_t blockSize = 131072;

struct FileWorker : public Worker {
	WorkerListener *pListener;
	FilePath path;
	size_t size;
	int err;
	FILE *fp;
	GUI::ElapsedTime et;
	int sleepTime;
	double nextProgress;

	FileWorker(WorkerListener *pListener_, const FilePath &path_, size_t size_, FILE *fp_);
	virtual ~FileWorker();
	virtual double Duration();
	virtual void Cancel() {
		Worker::Cancel();
	}
	virtual bool IsLoading() const = 0;
};

class FileLoader : public FileWorker {
public:
	ILoader *pLoader;
	size_t readSoFar;
	UniMode unicodeMode;

	FileLoader(WorkerListener *pListener_, ILoader *pLoader_, const FilePath &path_, size_t size_, FILE *fp_);
	virtual ~FileLoader();
	virtual void Execute();
	virtual void Cancel();
	virtual bool IsLoading() const {
		return true;
	}
};

class FileStorer : public FileWorker {
public:
	const char *documentBytes;
	size_t writtenSoFar;
	UniMode unicodeMode;
	bool visibleProgress;

	FileStorer(WorkerListener *pListener_, const char *documentBytes_, const FilePath &path_,
		size_t size_, FILE *fp_, UniMode unicodeMode_, bool visibleProgress_);
	virtual ~FileStorer();
	virtual void Execute();
	virtual void Cancel();
	virtual bool IsLoading() const {
		return false;
	}
};

enum {
	WORK_FILEREAD = 1,
	WORK_FILEWRITTEN = 2,
	WORK_FILEPROGRESS = 3,
	WORK_PLATFORM = 100
};
