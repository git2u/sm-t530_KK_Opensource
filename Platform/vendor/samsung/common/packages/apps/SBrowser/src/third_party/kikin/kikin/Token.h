/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinToken_h
#define KikinToken_h

#include "base/memory/ref_counted.h"

#include "common/String.h"

#define EXPORT __attribute__((visibility("default")))

namespace kikin {
    
/* Forward declaration of Entity. */
class Entity;
    
/* 
 * Token is an individual words within the given text. Tokenizer class creates the list
 * of tokens from the given piece of text. 
 */
class EXPORT Token : public base::RefCountedThreadSafe<Token> {
public:

    /* Constructor */
    Token(const kString& text, const size_t startOffset, const size_t endOffset);
    
    /* Destructor */
    virtual ~Token() = 0;
    
    /** Sets the boolean saying if the token has digit. */
    void setDigit(const bool hasDigit);
    
    /** Sets the boolean saying if the token has lower case character. */
    void setLowercase(const bool hasLowercase);
    
    /** Sets the boolean saying if the token has upper case character. */
    void setUppercase(const bool hasUppercase);
    
    /** Sets the boolean saying if the token has some other characters other than numbers and alphabets. */
    void setOther(const bool hasOther);
    
    /** Sets the boolean saying if the token is the last token in the sentence. */
    void setEndOfSentence(const bool isEndOfSenetence);
    
    /** Sets the boolean saying if the token is the selected token. */
    void setSelected(const bool isSelected);
    
    /** Sets the previous token. */
    void setPreviousToken(Token* const token);
    
    /** Sets the next token. */
    void setNextToken(Token* const token);
    
    /** Returns the next token. */
    Token* getNextToken() const;
    
    /** Returns the previous token. */
    Token* getPreviousToken() const;
    
    /** Returns whether the token text starts with the given text. */
    bool startsWith(const kString& text) const;

    /** Returns whether the token text ends with the given text. */
    bool endsWith(const kString& text) const;

    /** Returns whether the token text starts with the uppercase letter. */
    bool startsWithUppercase() const;
    
    /** Returns the text of the token. */
    const kString& getText() const;
    
    /** Returns whether the token is the first word in the sentence. */
    virtual bool isBeginningOfSentence() const;
    
    /** Returns whether the token is the first word in the sentence. */
    virtual bool isBeginningOfLine() const;

    /** Returns whether the token is the last word in the sentence. */
    bool isEndOfSentence() const;
    
    /** Returns whether the token is selected. */
    bool isSelected() const;
    
    /** Returns whether the token has uppercase character. */
    bool hasUppercase() const;
    
    /** Returns whether all the characters in the token are uppercase. */
    bool hasAllUppercase() const;
    
    /** Returns whether the token has lowercase character. */
    bool hasLowercase() const;
    
    /** Returns whether the token has digit character. */
    bool hasDigit() const;
    
    /** Returns the offset from where the token starts in the text. */
    size_t getStartOffset() const;
    
    /** Returns the offset where the token ends in the text. */
    size_t getEndOffset() const;
    
    /* Creates the entity object from the token */
    Entity* toEntity(const kString& type, const kString& language, const Token* lastToken);

    /* Checks whether the previous token is a twitter tag. */
    virtual bool checkPreviousTokenIsTwitterTag() = 0;
    
    /* Checks whether the token is on same node as the passed token. */
    virtual bool isOnSameNode(Token* token) const = 0;

    /* Checks whether the token is on same element as the passed token. */
    virtual bool isOnSameElement(Token* token) const = 0;
    
    /* 
     * Creates the entity object from the token. 
     *
     * While creating the entity object, this token is going to be the first token in the entity. 
     * Next count tells how many more tokens following this token should be included in the entity.
     * Type tells what type of entity is created.
     *
     */
    virtual Entity* toEntity(const kString& type, const kString& language, int nextCount) = 0;
    
    /*
     * Get the text from the current token to "nextCount" after.
     */
    virtual const kString toText(int nextCount) = 0;

private:

//    const kString stripWhiteSpace(const kString& text) const;

private:
    
    /** Text for the token. */
    kString m_text;
    
    /** Index position in text from where token starts. */
    size_t m_startOffset;
    
    /** Index position in text where token starts. */
    size_t m_endOffset;
    
    /** Next Token. */
    Token* m_nextToken;
    
    /** Previous token. */
    Token* m_previousToken;
    
    /** Whether the token has digit character. */
    bool m_hasDigit;
    
    /** Whether the token has lowercase character. */
    bool m_hasLowercase;
    
    /** Whether the token has uppercase character. */
    bool m_hasUppercase;
    
    /** Whether the token has any character other than digits and alphabets. */
    bool m_hasOther;

    /** Whether the token is the last token in the sentence. */
    bool m_isEndOfSentence;
    
    /** Whether the token is selected. */
    bool m_isSelected;
    
};

}   // namespace kikin

#endif // KikinToken_h
