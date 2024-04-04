// @file Utf8_16.h
// Copyright (C) 2002 Scott Kirkwood
//
// Permission to use, copy, modify, distribute and sell this code
// and its documentation for any purpose is hereby granted without fee,
// provided that the above copyright notice appear in all copies or
// any derived copies.  Scott Kirkwood makes no representations
// about the suitability of this software for any purpose.
// It is provided "as is" without express or implied warranty.
//
// Notes: Used the UTF information I found at:
//   http://www.cl.cam.ac.uk/~mgk25/unicode.html
////////////////////////////////////////////////////////////////////////////////

#ifndef UTF8_16_H
#define UTF8_16_H

namespace Utf8_16 {

class Reader {
public:
	Reader() noexcept = default;

	// Deleted so Reader objects can not be copied.
	Reader(const Reader &) = delete;
	Reader(Reader &&) = delete;
	Reader &operator=(const Reader &) = delete;
	Reader &operator=(Reader &&) = delete;

	virtual ~Reader() noexcept {};

	virtual std::string_view convert(std::string_view buf) = 0;
	virtual UniMode getEncoding() const noexcept = 0;

	static std::unique_ptr<Reader> Allocate();
};

class Writer {
public:
	Writer() noexcept = default;

	// Deleted so Writer objects can not be copied.
	Writer(const Writer &) = delete;
	Writer(Writer &&) = delete;
	Writer &operator=(const Writer &) = delete;
	Writer &operator=(Writer &&) = delete;

	virtual ~Writer() noexcept {};

	virtual size_t fwrite(std::string_view buf, FILE *pFile) = 0;

	static std::unique_ptr<Writer> Allocate(UniMode unicodeMode, size_t bufferSize);
};

}

#endif
