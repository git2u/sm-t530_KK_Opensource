/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinString_h
#define KikinString_h

#include <vector>
#include <functional>

#include "base/string16.h"

#define EXPORT __attribute__((visibility("default")))

namespace kikin {

/** Forward declaration of kStringIteartor. */
class kStringIterator;
    
/* kikin's custom UTF-16 string class. */
class EXPORT kString {
public:

    /* Constructor */
	kString() : m_impl() {}
	kString(const char* data, size_t length = string16::npos);
	kString(const char16* data, size_t length = string16::npos);
	kString(const uint32* data, size_t length = string16::npos);
	kString(const string16& other) : m_impl(other) {}
	kString(const kString& other) : m_impl(other.m_impl) {}
	kString& operator=(const kString& other);
    
    /* Destructor */
    ~kString();
    
    // operators
	char16 operator [] (size_t offset) const;
	bool operator == (const kString& string) const;
	bool operator == (const char16* string) const;
	bool operator == (const uint32& character) const;
	bool operator != (const kString& string) const;
	bool operator != (const char16* string) const;
	bool operator != (const uint32& character) const;
	void operator += (const kString& string);
	void operator += (const char16* string);
	kString operator + (const kString& string) const;
	kString operator + (const char16* string) const;

	// methods

	// This method returns the char32 character at the offset.
	// And if calculated char32 character is not valid, it returns false.
	// Also, it adjusts the position of offset to the right position to get next
	// char32 character.
	bool char32At(size_t* offset, uint32* character) const;
	size_t size() const { return m_impl.length(); }
	size_t length() const { return m_impl.length(); }
	size_t hashCode() const;
	size_t indexOf(const kString& text, size_t pos = 0) const;
	size_t indexOf(const char16* text, size_t pos = 0) const;
	size_t indexOf(const uint32& character, size_t pos = 0) const;
	size_t lastIndexOf(const kString& text, size_t pos = string16::npos) const;
	size_t lastIndexOf(const char16* text, size_t pos = string16::npos) const;
	size_t lastIndexOf(const uint32& character, size_t pos = string16::npos) const;
	size_t findFirstOf(const kString& text, size_t pos = 0) const;
	size_t findFirstOf(const char16* text, size_t pos = 0) const;
	size_t findFirstNotOf(const kString& text, size_t pos = 0) const;
	size_t findFirstNotOf(const char16* text, size_t pos = 0) const;
	size_t findLastOf(const kString& text, size_t pos = string16::npos) const;
	size_t findLastOf(const char16* text, size_t pos = string16::npos) const;
	size_t findLastNotOf(const kString& text, size_t pos = string16::npos) const;
	size_t findLastNotOf(const char16* text, size_t pos = string16::npos) const;

	bool equals(const kString& string, bool caseSensitive = true) const;
	bool equals(const char16* string, bool caseSensitive = true) const;
	bool contains(const uint32& character) const;
	bool endsWith(const uint32& character, bool caseSensitive = true) const;
	bool endsWith(const char16* string, bool caseSensitive = true) const;
	bool endsWith(const kString& string, bool caseSensitive = true) const;
	bool startsWith(const uint32& character, bool caseSensitive = true) const;
	bool startsWith(const char16* string, bool caseSensitive = true) const;
	bool startsWith(const kString& string, bool caseSensitive = true) const;

	size_t count(const uint32& character) const;
	size_t count(const char16* string) const;
	size_t count(const kString& string) const;

	void append(const kString& string);
	void append(const char16* string);

	void clear() { m_impl.clear(); }
	bool isEmpty() const;
	void toLower();
	void toUpper();
	void trimWhiteSpace();
	void remove(size_t pos, size_t len = string16::npos);
	void removeFromLeft(size_t nbCharacters) { remove(0, nbCharacters); }
	void removeFromRight(size_t nbCharacters) { remove(length() - nbCharacters); }

	// substring
	std::string toUTF8() const;
	kString substring(size_t start, size_t length = string16::npos) const;
	kString right(size_t nbCharacters) const;
	kString left(size_t nbCharacters) const;

	std::vector<kString> split(const kString& separator, bool allowEmptyEntries = false) const;
	std::vector<kString> split(const char16* separator, bool allowEmptyEntries = false) const;
	std::vector<kString> split(const uint32& separator, bool allowEmptyEntries = false) const;

	kString toLowerCase() const;
	kString toUpperCase() const;
	const char16* data() const { return m_impl.data(); }

	static kString fromInt(int value);
	static kString fromDouble(double value);
	static kString fromFloat(float value);
	static kString fromLong(long value);

	int toInt() const;
	float toFloat() const;
	double toDouble() const;
	long toLong() const;

	kStringIterator iterator() const;

	// regex
	bool matchesCompletely(const kString& pattern, bool isCaseSensitive = true) const;
	bool matchesCompletely(const char16* pattern, bool isCaseSensitive = true) const;
	bool matchesCompletely(const std::string& pattern, bool isCaseSensitive = true) const;
	bool matchesCompletely(const char* pattern, bool isCaseSensitive = true) const;
	bool matchesCompletely(const uint32* pattern, bool isCaseSensitive = true) const;

	bool matches(const kString& pattern, int* matchIndex, int* matchLength, bool isCaseSensitive = true, int startIndex = 0) const;
	bool matches(const char16* pattern, int* matchIndex, int* matchLength, bool isCaseSensitive = true, int startIndex = 0) const;
	bool matches(const std::string& pattern, int* matchIndex, int* matchLength, bool isCaseSensitive = true, int startIndex = 0) const;
	bool matches(const char* pattern, int* matchIndex, int* matchLength, bool isCaseSensitive = true, int startIndex = 0) const;
	bool matches(const uint32* pattern, int* matchIndex, int* matchLength, bool isCaseSensitive = true, int startIndex = 0) const;

public:

	static const size_t npos = string16::npos;

private:

	bool has2CodePointUTF16Character(const char16* text) const;
	bool matchesCompletely(const char* input, const size_t inputLength, const char* pattern, bool isCaseSensitive) const;
	bool matches(const char* input, const size_t inputLength, const std::string& pattern, int* matchIndex, int* matchLength,
			bool isCaseSensitive = true, int startIndex = 0) const;

private:
    
    /* char16 string. */
    string16 m_impl;
};

}   // namespace kikin

namespace std {

template <> struct hash<kikin::kString> {
	size_t operator()(const kikin::kString& text) const {
		return text.hashCode();
	}
};

template <> struct equal_to<kikin::kString> {
	size_t operator()(const kikin::kString& a, const kikin::kString& b) const {
		return (a == b);
	}
};

}

#endif // KikinString_h
