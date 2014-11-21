/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinTokenizer_h
#define KikinTokenizer_h

#include "base/hash_tables.h"

#include "common/String.h"

#define EXPORT __attribute__((visibility("default")))

namespace kikin {
    
/* Tokenizes the text in different token. */
class EXPORT Tokenizer {
public:
    
    /* Checks if the text is the end of sentence. */
    static bool isEndOfSenetence(const kString& text);
    static bool isDelimiter(const uint32 character);
    static bool isSeparator(const uint32 character);
    static bool isNewLine(const uint32 character);
    static bool isNumber(const uint32 character);
    static bool isUpperCase(const uint32 character);
    static bool isLowerCase(const uint32 character);
    static bool isGroup(const uint32 character);
    static bool isPartOfWord(const kString& text, const uint32 character, const int index, const int previousWhiteSpaceIndex);

private:
    
    /* Constructor */
    Tokenizer();
    
    /* Destructor */
    ~Tokenizer();
    
    /* Creates the list of not end of sentence words. */
    static void createNotEndOfSentenceList();
    
    /** Returns whether the word is an initials. */
    static bool isInitials(const kString& text, const int index);

    /** Returns whether the word is an abbreviation. */
	static bool isAbbreviation(const kString& text, const int index, const int previousWhiteSpaceIndex);

private:
    
    /* List of not end of sentence word. */
    static base::hash_set<kString> s_notEndOfSentence;
    
    static const char16 s_delimitersList[];
    static const char16 s_lineTerminatorsList[];
    static const char16 s_groupCharactersList[];
    static const char16 s_punctuationsList[];

};
    
}   // namespace kikin

#endif // KikinTokenizer_h
