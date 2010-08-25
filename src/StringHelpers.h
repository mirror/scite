// SciTE - Scintilla based Text Editor
/** @file StringHelpers.h
 ** Definition of widely useful string functions.
 **/
// Copyright 2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

bool StartsWith(GUI::gui_string const &s, GUI::gui_string const &end);
bool EndsWith(GUI::gui_string const &s, GUI::gui_string const &end);
int Substitute(GUI::gui_string &s, const GUI::gui_string &sFind, const GUI::gui_string &sReplace);

char *Slash(const char *s, bool quoteQuotes);
unsigned int UnSlash(char *s);
unsigned int UnSlashLowOctal(char *s);

class ILocalize {
public:
	virtual GUI::gui_string Text(const char *s, bool retainIfNotFound=true) = 0;
};

/**
 * This is a fixed length list of strings suitable for display in combo boxes
 * as a memory of user entries.
 */
template < int sz >
class EntryMemory {
	std::string entries[sz];
public:
	void Insert(const std::string &s) {
		for (int i = 0; i < sz; i++) {
			if (entries[i] == s) {
				for (int j = i; j > 0; j--) {
					entries[j] = entries[j - 1];
				}
				entries[0] = s;
				return;
			}
		}
		for (int k = sz - 1; k > 0; k--) {
			entries[k] = entries[k - 1];
		}
		entries[0] = s;
	}
	void AppendIfNotPresent(const std::string &s) {
		for (int i = 0; i < sz; i++) {
			if (entries[i] == s) {
				return;
			}
			if (0 == entries[i].length()) {
				entries[i] = s;
				return;
			}
		}
	}
	void AppendList(const std::string &s, char sep = '|') {
		int start = 0;
		int end = 0;
		while (s[end] != '\0') {
			end = start;
			while ((s[end] != sep) && (s[end] != '\0'))
				++end;
			AppendIfNotPresent(s.substr(start, end-start));
			start = end + 1;
		}
	}
	int Length() const {
		int len = 0;
		for (int i = 0; i < sz; i++)
			if (entries[i].length())
				len++;
		return len;
	}
	std::string At(int n) const {
		return entries[n];
	}
	std::vector<std::string>AsVector() {
		std::vector<std::string> ret;
		for (int i = 0; i < sz; i++) {
			if (entries[i].length())
				ret.push_back(entries[i].c_str());
		}
		return ret;
	}
};

typedef EntryMemory < 10 > ComboMemory;
