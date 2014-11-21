/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinStringIterator_h
#define KikinStringIterator_h

#include "common/String.h"

#define EXPORT __attribute__((visibility("default")))

namespace kikin {

/** Forward declaration of kString. */
class kString;
    
/* String iterator of kString. */
class EXPORT kStringIterator {
public:

    /* Constructor */
	kStringIterator(const kString& text, const size_t moveToIndex = 0) : m_text(text), m_index(moveToIndex), m_isForwarded(false) {}
	kStringIterator(const kStringIterator& other) : m_text(other.m_text), m_index(other.m_index), m_isForwarded(other.m_isForwarded) {}
    
    /* Destructor */
    ~kStringIterator() { }
    
    // Operators
//    void operator ++ ();
//    void operator ++ (int);
//	void operator -- ();
//	void operator -- (int);
//	void operator += (int count);
//	void operator -= (int count);
//	char16 operator *() const;

	// one code point UTF-16 character
	char16 next();
	char16 previous();

	// two code point UTF-16 character (UTF-32 character)
	uint32 nextChar32();
	uint32 previousChar32();

	// iterator helper functions
	bool hasNext() const;
	bool hasPrevious() const;

	// returns the index of last character read
	// if nothing is read, then returns the character
	// from where iterator is started
	size_t index() const;
	size_t nextIndex() const;
	size_t previousIndex() const;

private:
    
    /* text */
    const kString m_text;

    /* index */
    size_t m_index;

    bool m_isForwarded;
};

}   // namespace kikin

#endif // KikinString_h
