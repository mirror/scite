// @file Utf8_16.cxx
// Copyright (C) 2002 Scott Kirkwood
//
// Permission to use, copy, modify, distribute and sell this code
// and its documentation for any purpose is hereby granted without fee,
// provided that the above copyright notice appear in all copies or
// any derived copies.  Scott Kirkwood makes no representations
// about the suitability of this software for any purpose.
// It is provided "as is" without express or implied warranty.
////////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <cstring>
#include <cstdio>

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <memory>

#include "Cookie.h"
#include "Utf8_16.h"

namespace {

using utf16 = unsigned short; // 16 bits
using utf8 = unsigned char; // 8 bits
using ubyte = unsigned char;

enum encodingType {
	eUnknown,
	eUtf16BigEndian,
	eUtf16LittleEndian,  // Default on Windows
	eUtf8,
	eCookie
};

const utf8 k_Boms[][3] = {
	{0x00, 0x00, 0x00},  // Unknown
	{0xFE, 0xFF, 0x00},  // Big endian
	{0xFF, 0xFE, 0x00},  // Little endian
	{0xEF, 0xBB, 0xBF},  // UTF8
	{0x00, 0x00, 0x00},  // Cookie
};

enum { SURROGATE_LEAD_FIRST = 0xD800 };
enum { SURROGATE_LEAD_LAST = 0xDBFF };
enum { SURROGATE_TRAIL_FIRST = 0xDC00 };
enum { SURROGATE_TRAIL_LAST = 0xDFFF };
enum { SURROGATE_FIRST_VALUE = 0x10000 };

// Reads UTF-8 and outputs UTF-16

class Utf8_Iter {
public:
	Utf8_Iter(std::string_view buf, encodingType eEncoding) noexcept;
	int get() const noexcept {
		assert(m_eState == eStart);
		return m_nCur;
	}
	bool canGet() const noexcept { return m_eState == eStart; }
	void operator++() noexcept;
	operator bool() const noexcept { return m_pRead <= m_pEnd; }

protected:
	void toStart() noexcept; // Put to start state
	enum eState {
		eStart,
		eSecondOf4Bytes,
		ePenultimate,
		eFinal
	};
protected:
	encodingType m_eEncoding;
	eState m_eState;
	int m_nCur;
	// These 2 pointers are for externally allocated memory passed to constructor
	const ubyte *m_pRead;	// Ends at m_pEnd+1
	const ubyte *m_pEnd;
};

Utf8_Iter::Utf8_Iter(std::string_view buf, encodingType eEncoding) noexcept {
	const ubyte *pBuf = reinterpret_cast<const ubyte *>(buf.data());
	m_pRead = pBuf;
	m_pEnd = pBuf + buf.size();
	m_eState = eStart;
	m_nCur = 0;
	m_eEncoding = eEncoding;
	operator++();
}

// Go to the next byte.
void Utf8_Iter::operator++() noexcept {
	switch (m_eState) {
	case eStart:
		if ((0xF0 & *m_pRead) == 0xF0) {
			m_nCur = (0x7 & *m_pRead) << 18;
			m_eState = eSecondOf4Bytes;
		} else if ((0xE0 & *m_pRead) == 0xE0) {
			m_nCur = (~0xE0 & *m_pRead) << 12;
			m_eState = ePenultimate;
		} else if ((0xC0 & *m_pRead) == 0xC0) {
			m_nCur = (~0xC0 & *m_pRead) << 6;
			m_eState = eFinal;
		} else {
			m_nCur = *m_pRead;
			toStart();
		}
		break;
	case eSecondOf4Bytes:
		m_nCur |= (0x3F & *m_pRead) << 12;
		m_eState = ePenultimate;
		break;
	case ePenultimate:
		m_nCur |= (0x3F & *m_pRead) << 6;
		m_eState = eFinal;
		break;
	case eFinal:
		m_nCur |= 0x3F & *m_pRead;
		toStart();
		break;
	}
	++m_pRead;
}

void Utf8_Iter::toStart() noexcept {
	m_eState = eStart;
}

// ==================================================================

// Reads UTF-16 and outputs UTF-8

class Utf16_Iter {
public:
	Utf16_Iter() noexcept = default;
	void set(std::string_view buf, UniMode eEncoding) noexcept;
	utf8 get() const noexcept {
		return m_nCur;
	}
	void operator++() noexcept;
	operator bool() const noexcept { return m_pRead <= m_pEnd; }
	utf16 read(const ubyte *pRead) const noexcept;
	bool retained() const noexcept;
protected:
	enum eState {
		eStart,
		eSecondOf4Bytes,
		ePenultimate,
		eFinal
	};
protected:
	using UWord = std::array<ubyte, 2>;
	UniMode m_eEncoding = UniMode::uni8Bit;
	eState m_eState = eStart;
	utf8 m_nCur = 0;
	int m_nCur16 = 0;
	UWord prefix{};
	UWord m_leadSurrogate{};
	// These 2 pointers are for externally allocated memory passed to set
	const ubyte *m_pRead = nullptr;
	const ubyte *m_pEnd = nullptr;
};

void Utf16_Iter::set(std::string_view buf, UniMode eEncoding) noexcept {
	const ubyte *pBuf = reinterpret_cast<const ubyte *>(buf.data());
	m_pRead = pBuf;
	m_pEnd = pBuf + buf.length();
	m_eEncoding = eEncoding;
	prefix = m_leadSurrogate;
	m_leadSurrogate.fill(0);
	if (buf.length() >= 2) {
		const utf16 lastElement = read(m_pEnd - 2);
		if (lastElement >= SURROGATE_LEAD_FIRST && lastElement <= SURROGATE_LEAD_LAST) {
			// Buffer ends with lead surrogate so cut off buffer and store
			m_leadSurrogate[0] = m_pEnd[-2];
			m_leadSurrogate[1] = m_pEnd[-1];
			m_pEnd -= 2;
		}
	}
	operator++();
	// Note: m_eState, m_nCur, m_nCur16 not reinitialized.
}

// Goes to the next byte.
// Not the next symbol which you might expect.
// This way we can continue from a partial buffer that doesn't align
void Utf16_Iter::operator++() noexcept {
	switch (m_eState) {
	case eStart:
		if (m_pRead >= m_pEnd) {
			++m_pRead;
			break;
		}
		if (prefix[0] || prefix[1]) {
			m_nCur16 = read(prefix.data());
			prefix.fill(0);
		} else {
			m_nCur16 = read(m_pRead);
			m_pRead += 2;
		}
		if (m_nCur16 >= SURROGATE_LEAD_FIRST && m_nCur16 <= SURROGATE_LEAD_LAST) {
			if (m_pRead >= m_pEnd) {
				// Have a lead surrogate at end of document with no access to trail surrogate.
				// May be end of document.
				--m_pRead;	// With next increment, leave pointer just past buffer
			} else {
				const int trail = read(m_pRead);
				++m_pRead;
				m_nCur16 = (((m_nCur16 & 0x3ff) << 10) | (trail & 0x3ff)) + SURROGATE_FIRST_VALUE;
			}
			++m_pRead;
		}

		if (m_nCur16 < 0x80) {
			m_nCur = m_nCur16 & 0xFF;
			m_eState = eStart;
		} else if (m_nCur16 < 0x800) {
			m_nCur = 0xC0 | ((m_nCur16 >> 6) & 0xFF);
			m_eState = eFinal;
		} else if (m_nCur16 < SURROGATE_FIRST_VALUE) {
			m_nCur = 0xE0 | ((m_nCur16 >> 12) & 0xFF);
			m_eState = ePenultimate;
		} else {
			m_nCur = 0xF0 | ((m_nCur16 >> 18) & 0xFF);
			m_eState = eSecondOf4Bytes;
		}
		break;
	case eSecondOf4Bytes:
		m_nCur = 0x80 | ((m_nCur16 >> 12) & 0x3F);
		m_eState = ePenultimate;
		break;
	case ePenultimate:
		m_nCur = 0x80 | ((m_nCur16 >> 6) & 0x3F);
		m_eState = eFinal;
		break;
	case eFinal:
		m_nCur = 0x80 | (m_nCur16 & 0x3F);
		m_eState = eStart;
		break;
	}
}

utf16 Utf16_Iter::read(const ubyte *pRead) const noexcept {
	if (m_eEncoding == UniMode::uni16LE) {
		return pRead[0] | static_cast<utf16>(pRead[1] << 8);
	} else {
		return pRead[1] | static_cast<utf16>(pRead[0] << 8);
	}
}

bool Utf16_Iter::retained() const noexcept {
	return m_leadSurrogate[0] || m_leadSurrogate[1];
}

// ==================================================================

bool mem_equal(const void *ptr1, const void *ptr2, size_t num) noexcept {
	return 0 == memcmp(ptr1, ptr2, num);
}

UniMode DetermineEncoding(std::string_view text) noexcept {
	if (text.length() >= 2) {
		if (mem_equal(text.data(), k_Boms[eUtf16BigEndian], 2)) {
			return UniMode::uni16BE;
		}
		if (mem_equal(text.data(), k_Boms[eUtf16LittleEndian], 2)) {
			return UniMode::uni16LE;
		} 
		if (text.length() >= 3 && mem_equal(text.data(), k_Boms[eUtf8], 3)) {
			return UniMode::utf8;
		}
	}

	return UniMode::uni8Bit;
}

size_t LengthBOM(UniMode encoding) noexcept {
	switch (encoding) {
	case UniMode::uni8Bit:
	case UniMode::cookie:
		return 0;
	case UniMode::uni16BE:
	case UniMode::uni16LE:
		return 2;
	case UniMode::utf8:
		return 3;
	}
	return 0;
}

// Reads UTF16 and outputs UTF8
class Utf8_16_Reader : public Utf8_16::Reader {
public:
	Utf8_16_Reader() noexcept;

	// Deleted so Utf8_16_Read objects can not be copied.
	Utf8_16_Reader(const Utf8_16_Reader &) = delete;
	Utf8_16_Reader(Utf8_16_Reader &&) = delete;
	Utf8_16_Reader &operator=(const Utf8_16_Reader &) = delete;
	Utf8_16_Reader &operator=(Utf8_16_Reader &&) = delete;

	~Utf8_16_Reader() noexcept override;

	std::string_view convert(std::string_view buf) override;

	UniMode getEncoding() const noexcept override {
		return static_cast<UniMode>(static_cast<int>(m_eEncoding));
	}

private:
	UniMode m_eEncoding = UniMode::uni8Bit;
	// m_pNewBuf may be allocated by Utf8_16_Read::convert
	std::vector<ubyte> m_pNewBuf;
	bool m_bFirstRead = true;
	Utf16_Iter m_Iter16;
};

// ==================================================================

Utf8_16_Reader::Utf8_16_Reader() noexcept = default;

Utf8_16_Reader::~Utf8_16_Reader() noexcept = default;

std::string_view Utf8_16_Reader::convert(std::string_view buf) {
	if (m_bFirstRead) {
		m_eEncoding = DetermineEncoding(buf);
		buf.remove_prefix(LengthBOM(m_eEncoding));
		if (m_eEncoding == UniMode::uni8Bit) {
			m_eEncoding = CodingCookieValue(buf);
		}
		m_bFirstRead = false;
	}

	if ((m_eEncoding == UniMode::uni8Bit) || (m_eEncoding == UniMode::utf8) || (m_eEncoding == UniMode::cookie)) {
		// Do nothing, pass through omitting BOM when present
		return buf;
	}

	// Else...
	m_pNewBuf.clear();

	if (!buf.length() && !m_Iter16.retained())
		return {};
	m_Iter16.set(buf, m_eEncoding);

	for (; m_Iter16; ++m_Iter16) {
		m_pNewBuf.push_back(m_Iter16.get());
	}

	return std::string_view(reinterpret_cast<const char *>(m_pNewBuf.data()), m_pNewBuf.size());
}

}

namespace Utf8_16 {

std::unique_ptr<Reader> Reader::Allocate() {
	return std::make_unique<Utf8_16_Reader>();
}

}

namespace {

// ==================================================================


// Read in a UTF-8 buffer and write out to UTF-16 or UTF-8
class Utf8_16_Write : public Utf8_16::Writer {
public:
	explicit Utf8_16_Write(UniMode unicodeMode, size_t bufferSize);

	// Deleted so Utf8_16_Write objects can not be copied.
	Utf8_16_Write(const Utf8_16_Write &) = delete;
	Utf8_16_Write(Utf8_16_Write &&) = delete;
	Utf8_16_Write &operator=(const Utf8_16_Write &) = delete;
	Utf8_16_Write &operator=(Utf8_16_Write &&) = delete;

	~Utf8_16_Write() noexcept override;

	void appendCodeUnit(int codeUnit);
	size_t fwrite(std::string_view buf, FILE *pFile) override;

protected:
	encodingType m_eEncoding = eUnknown;
	std::vector<utf16> m_buf16;
	bool m_bFirstWrite = true;
};

Utf8_16_Write::Utf8_16_Write(UniMode unicodeMode, size_t bufferSize) {
	if (unicodeMode != UniMode::cookie) {	// Save file with cookie without BOM.
		m_eEncoding = static_cast<encodingType>(static_cast<int>(unicodeMode));
	}
	if (m_eEncoding == eUtf16BigEndian || m_eEncoding == eUtf16LittleEndian) {
		// Pre-allocate m_buf16 so should not allocate in storing thread where harder to report failure
		m_buf16.reserve(bufferSize / 2 + 1);
	}
};

Utf8_16_Write::~Utf8_16_Write() noexcept = default;

void Utf8_16_Write::appendCodeUnit(int codeUnit) {
	if (m_eEncoding == eUtf16LittleEndian) {
		m_buf16.push_back(codeUnit & 0xFFFF);
	} else {
		const utf16 swapped = static_cast<utf16>((codeUnit & 0xFF) << 8) | ((codeUnit & 0xFF00) >> 8);
		m_buf16.push_back(swapped);
	}
}

size_t Utf8_16_Write::fwrite(std::string_view buf, FILE *pFile) {
	if (!pFile) {
		return 0; // fail
	}

	if (m_eEncoding == eUnknown) {
		// Normal write
		return ::fwrite(buf.data(), 1, buf.size(), pFile);
	}

	if (m_eEncoding == eUtf8) {
		if (m_bFirstWrite)
			::fwrite(k_Boms[m_eEncoding], 3, 1, pFile);
		m_bFirstWrite = false;
		return ::fwrite(buf.data(), 1, buf.size(), pFile);
	}

	m_buf16.clear();

	if (m_bFirstWrite) {
		if (m_eEncoding == eUtf16BigEndian || m_eEncoding == eUtf16LittleEndian) {
			// Write the BOM
			::fwrite(k_Boms[m_eEncoding], 2, 1, pFile);
		}

		m_bFirstWrite = false;
	}

	Utf8_Iter iter8(buf, m_eEncoding);

	for (; iter8; ++iter8) {
		if (iter8.canGet()) {
			int codePoint = iter8.get();
			if (codePoint >= SURROGATE_FIRST_VALUE) {
				codePoint -= SURROGATE_FIRST_VALUE;
				const int lead = (codePoint >> 10) + SURROGATE_LEAD_FIRST;
				appendCodeUnit(lead);
				const int trail = (codePoint & 0x3ff) + SURROGATE_TRAIL_FIRST;
				appendCodeUnit(trail);
			} else {
				appendCodeUnit(codePoint);
			}
		}
	}

	const size_t ret = ::fwrite(m_buf16.data(),
		sizeof(utf16), m_buf16.size(), pFile);

	return ret;
}

}

namespace Utf8_16 {

std::unique_ptr<Writer> Writer::Allocate(UniMode unicodeMode, size_t bufferSize) {
	return std::make_unique<Utf8_16_Write>(unicodeMode, bufferSize);
}

}
