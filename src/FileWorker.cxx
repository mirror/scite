// SciTE - Scintilla based Text Editor
/** @file FileWorker.cxx
 ** Implementation of classes to perform background file tasks as threads.
 **/
// Copyright 2011 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdio>

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>

#include "ILoader.h"

#include "GUI.h"

#include "FilePath.h"
#include "Cookie.h"
#include "Worker.h"
#include "Utf8_16.h"
#include "FileWorker.h"

constexpr double timeBetweenProgress = 0.4;

FileWorker::FileWorker(WorkerListener *pListener_, const FilePath &path_, size_t size_, FILE *fp_) :
	pListener(pListener_), path(path_), size(size_), err(0), fp(fp_), sleepTime(0), nextProgress(timeBetweenProgress) {
}

FileWorker::~FileWorker() noexcept {
}

double FileWorker::Duration() noexcept {
	return et.Duration();
}

FileLoader::FileLoader(WorkerListener *pListener_, Scintilla::ILoader *pLoader_, const FilePath &path_, size_t size_, FILE *fp_) :
	FileWorker(pListener_, path_, size_, fp_), pLoader(pLoader_), readSoFar(0), unicodeMode(UniMode::uni8Bit) {
	SetSizeJob(size);
}

void FileLoader::Execute() noexcept {
	try {
		if (fp) {
			std::unique_ptr<Utf8_16::Reader> convert = Utf8_16::Reader::Allocate();
			std::vector<char> data(blockSize);
			size_t lenFile = fread(data.data(), 1, data.size(), fp);
			while ((lenFile > 0) && (err == 0) && (!Cancelling())) {
				GUI::SleepMilliseconds(sleepTime);
				const std::string_view converted = convert->convert(std::string_view(data.data(), lenFile));
				err = pLoader->AddData(converted.data(), converted.size());
				IncrementProgress(lenFile);
				if (et.Duration() > nextProgress) {
					nextProgress = et.Duration() + timeBetweenProgress;
					pListener->PostOnMainThread(WORK_FILEPROGRESS, this);
				}
				lenFile = fread(data.data(), 1, data.size(), fp);
			}
			fclose(fp);
			fp = nullptr;
			if (err == 0) {
				// Handle case where convert is holding a lead surrogate but no more data
				const std::string_view convertedTrail = convert->convert("");
				err = pLoader->AddData(convertedTrail.data(), convertedTrail.size());
			}
			unicodeMode = convert->getEncoding();
		}
	} catch (...) {
		err = 1;
	}
	SetCompleted();
	try {
		pListener->PostOnMainThread(WORK_FILEREAD, this);
	} catch (...) {
		err = 1;
	}
}

void FileLoader::Cancel() noexcept {
	FileWorker::Cancel();
	try {
		pLoader->Release();
	} catch (...) {
		// Release will never throw
	}
	pLoader = nullptr;
}

FileStorer::FileStorer(WorkerListener *pListener_, std::string_view bytes_, const FilePath &path_,
		       FILE *fp_, UniMode unicodeMode_, bool visibleProgress_) :
	FileWorker(pListener_, path_, bytes_.size(), fp_), documentBytes(bytes_.data()), writtenSoFar(0),
	unicodeMode(unicodeMode_), visibleProgress(visibleProgress_) {
	SetSizeJob(size);
	convert = Utf8_16::Writer::Allocate(unicodeMode, blockSize);
}

static constexpr bool IsUTF8TrailByte(int ch) noexcept {
	return (ch >= 0x80) && (ch < (0x80 + 0x40));
}

void FileStorer::Execute() noexcept {
	try {
		if (fp) {
			const std::string_view documentView(documentBytes, size);
			const size_t lengthDoc = size;
			for (size_t startBlock = 0; startBlock < lengthDoc && (!Cancelling());) {
				GUI::SleepMilliseconds(sleepTime);
				size_t grabSize = std::min(lengthDoc - startBlock, blockSize);
				if ((unicodeMode != UniMode::uni8Bit) && (startBlock + grabSize < lengthDoc)) {
					// Round down so only whole characters retrieved.
					size_t startLast = grabSize;
					while ((startLast > 0) && ((grabSize - startLast) < 6) &&
						IsUTF8TrailByte(static_cast<unsigned char>(documentBytes[startBlock + startLast])))
						startLast--;
					if ((grabSize - startLast) < 5)
						grabSize = startLast;
				}
				const size_t written = convert->fwrite(documentView.substr(startBlock, grabSize), fp);
				IncrementProgress(grabSize);
				if (et.Duration() > nextProgress) {
					nextProgress = et.Duration() + timeBetweenProgress;
					pListener->PostOnMainThread(WORK_FILEPROGRESS, this);
				}
				if (written == 0) {
					err = 1;
					break;
				}
				startBlock += grabSize;
			}
			if (fclose(fp) != 0) {
				err = 1;
			}
			fp = nullptr;
		}
	} catch (...) {
		err = 1;
	}
	SetCompleted();
	try {
		pListener->PostOnMainThread(WORK_FILEWRITTEN, this);
	} catch (...) {
		err = 1;
	}
}

void FileStorer::Cancel() noexcept {
	FileWorker::Cancel();
}
