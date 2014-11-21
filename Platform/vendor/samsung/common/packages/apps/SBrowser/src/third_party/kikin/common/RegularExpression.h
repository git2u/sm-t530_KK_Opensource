/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef RegularExpression_h
#define RegularExpression_h

#include <unicode/uregex.h>

#include "base/memory/scoped_ptr.h"
#include "common/String.h"

#define EXPORT __attribute__((visibility("default")))

namespace kikin {

/* kikin's custom Regular Expression class. */
class EXPORT kRegex {
public:

	static kRegex* create(const char* pattern, bool isCaseSensitive = true);
	static kRegex* create(const char16* pattern, bool isCaseSensitive = true);
	static kRegex* create(const uint32* pattern, bool isCaseSensitive = true);
	static kRegex* create(const kString& pattern, bool isCaseSensitive = true);
	static kRegex* create(const std::string& pattern, bool isCaseSensitive = true);


	/* Destructor */
	~kRegex();

	// compile methods
	// Method returns false, if regex is invalid
	bool compile(const char* pattern, bool isCaseSensitive = true);
	bool compile(const char16* pattern, bool isCaseSensitive = true);
	bool compile(const uint32* pattern, bool isCaseSensitive = true);
	bool compile(const kString& pattern, bool isCaseSensitive = true);
	bool compile(const std::string& pattern, bool isCaseSensitive = true);

	// match methods

	// Attempts to match the entire input region against the pattern.
	// If start index is specified, function attempts to match the
	// input beginning at the specified start index and extending to
	// the end of the input
	bool matches(const char* input, const size_t startIndex = 0) const;
	bool matches(const char16* input, const size_t startIndex = 0) const;
	bool matches(const uint32* input, const size_t inputLength, const size_t startIndex = 0) const;
	bool matches(const kString& input, const size_t startIndex = 0) const;
	bool matches(const std::string& input, const size_t startIndex = 0) const;

	// Find the pattern match in the input string.
	// Returns the index at which match starts and length of the match
	// User can also specify the offset from where to start finding the match.
	bool find(const char* input, int* matchIndex, size_t* matchLength, const size_t startIndex = 0) const;
	bool find(const char16* input, int* matchIndex, size_t* matchLength, const size_t startIndex = 0) const;
	bool find(const uint32* input, const size_t inputLength, int* matchIndex, size_t* matchLength, const size_t startIndex = 0) const;
	bool find(const kString& input, int* matchIndex, size_t* matchLength, const size_t startIndex = 0) const;
	bool find(const std::string& input, int* matchIndex, size_t* matchLength, const size_t startIndex = 0) const;

private:

	// Avoid default constructors
	kRegex(URegularExpression* regex);
	kRegex(const kRegex&);
	void operator=(const kRegex&);

private:

	/* compiled regular expression. */
	URegularExpression* m_regex;

};

}   // namespace kikin

#endif // RegularExpression_h
