// http://www.cl.cam.ac.uk/~mgk25/unicode.html

#include "stdio.h"
#include "assert.h"

#ifdef _MSC_VER
#pragma warning(disable: 4514) // nreferenced inline function has been removed
#endif

class Utf8_16 {
public:
	typedef unsigned short utf16; // 16 bits
	typedef unsigned char utf8; // 8 bits
	typedef unsigned char ubyte;
	enum encodingType {
	    eUnknown,
	    eUtf16BigEndian,
	    eUtf16LittleEndian,  // Default on Windows
	    eUtf8,
	    eLast
	};
	static const utf8 k_Boms[eLast][3];
};

// Reads UTF-16 and outputs UTF-8
class Utf16_Iter : public Utf8_16 {
public:
	Utf16_Iter();
	void reset();
	void set(const ubyte* pBuf, size_t nLen, encodingType eEncoding);
	utf8 get() const {
		return m_nCur;
	}
	void operator++();
	operator bool() { return m_pRead <= m_pEnd; }

protected:
	void toStart(); // Put to start state, swap bytes if necessary
	enum eState {
	    eStart,
	    e2Bytes2,
	    e3Bytes2,
	    e3Bytes3
	};
protected:
	encodingType m_eEncoding;
	eState m_eState;
	utf8 m_nCur;
	utf16 m_nCur16;
	const ubyte* m_pBuf;
	const ubyte* m_pRead;
	const ubyte* m_pEnd;
};

// Reads UTF-8 and outputs UTF-16
class Utf8_Iter : public Utf8_16 {
public:
	Utf8_Iter();
	void reset();
	void set(const ubyte* pBuf, size_t nLen, encodingType eEncoding);
	utf16 get() const {
#ifdef _DEBUG
		assert(m_eState == eStart);
#endif
		return m_nCur;
	}
	bool canGet() const { return m_eState == eStart; }
	void operator++();
	operator bool() { return m_pRead <= m_pEnd; }

protected:
	void swap();
	void toStart(); // Put to start state, swap bytes if necessary
	enum eState {
	    eStart,
	    e2Bytes_Byte2,
	    e3Bytes_Byte2,
	    e3Bytes_Byte3
	};
protected:
	encodingType m_eEncoding;
	eState m_eState;
	utf16 m_nCur;
	const ubyte* m_pBuf;
	const ubyte* m_pRead;
	const ubyte* m_pEnd;
};

// Reads UTF16 and outputs UTF8
class Utf8_16_Read : public Utf8_16 {
public:
	Utf8_16_Read();
	~Utf8_16_Read();

	int convert(char* buf, int len);
	char* getNewBuf() { return reinterpret_cast<char*>(m_pNewBuf); }

	encodingType getEncoding() const { return m_eEncoding; }
protected:
	int determineEncoding();
private:
	encodingType m_eEncoding;
	ubyte* m_pBuf;
	ubyte* m_pNewBuf;
	size_t m_nBufSize;
	bool m_bFirstRead;
	int m_nLen;
	Utf16_Iter m_Iter16;
};

// Read in a UTF-8 buffer and write out to UTF-16 or UTF-8
class Utf8_16_Write : public Utf8_16 {
public:
	Utf8_16_Write();
	~Utf8_16_Write();

	void setEncoding(encodingType eType);

	FILE * fopen(const char *_name, const char *_type);
	size_t fwrite(const void* p, size_t _size);
	void fclose();
protected:
	encodingType m_eEncoding;
	FILE* m_pFile;
	utf16* m_pBuf;
	size_t m_nBufSize;
	bool m_bFirstWrite;
};
