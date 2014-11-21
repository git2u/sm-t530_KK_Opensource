/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */
#define LOG_TAG "kikin_html_tokenizer_cn"

#include <config.h>
#include "kikin/html/cn/HtmlTokenizer.h"

#include "core/dom/Node.h"
#include "core/dom/Range.h"

//#include <wtf/text/CString.h>
//#include <cwd.h>

#include "kikin/Tokenizer.h"
#include "kikin/html/HtmlToken.h"
#include "kikin/html/HtmlRange.h"

#include "kikin/list/Ranges.h"
#include "kikin/list/Tokens.h"

using namespace WebCore;

namespace kikin { namespace html { namespace cn {
    
// static
WTF::PassRefPtr<kikin::list::Tokens> HtmlTokenizer::tokenize(WTF::PassRefPtr<kikin::list::Ranges> ranges,
		WebCore::Node* startContainer, const int offset)
{
	RefPtr<list::Tokens> tokens = list::Tokens::create();
	RefPtr<HtmlToken> previousToken = NULL;

	// for every range
    for (unsigned j = 0; j < ranges->length(); j++) {
        HtmlRange* range = ranges->item(j);
        Node* node = range->getRange()->startContainer();
        int startOffset = range->getRange()->startOffset();
        int endOffset = range->getRange()->endOffset();

        const String& str = node->textContent();
        int currentOffset = startOffset;
        RefPtr<HtmlToken> token = NULL;
        UChar32 c = 0;

        // for each character
        int i = startOffset;
        for (i = startOffset; i < endOffset; i++) {
            c = str.characterStartingAt(i);

            bool isDelim = Tokenizer::isDelimiter(c);
            bool isSeparator = Tokenizer::isSeparator(c);
            bool isChinese = HtmlTokenizer::isChineseCharacter(c);
            if (isDelim || isSeparator || isChinese) {
                if (currentOffset != i) {
                	// create a token for the character
                    bool isSelected = node->isSameNode(startContainer) && (offset < i) && (offset >= currentOffset);
                    token = HtmlTokenizer::createToken(node, str, currentOffset, i, isSelected, previousToken);
                    previousToken = token;
                    tokens->append(token.release());
                }

                // create a token for the separator
                if (isSeparator || isChinese) {
                	bool isSelected = node->isSameNode(startContainer) && (offset < i+1) && (offset >= i);
                    token = HtmlTokenizer::createToken(node, str, i, i+1, isSelected, previousToken);
                    previousToken = token;
                    tokens->append(token.release());
                }

                currentOffset = i + 1;
            }
        }

        if (str.length() > 0 && c != ' ' && c != '\r' && c != '\n' && currentOffset < static_cast<int>(str.length())) {
            token = HtmlTokenizer::createToken(node, str, currentOffset, i, false, previousToken);
            previousToken = token;
            tokens->append(token.release());
        }


/*    	// get word boundaries
     	WTF::CString utf8String = text.utf8();
		const char* utf8Data = utf8String.data();
    	std::vector< std::pair<int, int> > boundaries = cwd::getWordBoundaries(utf8Data, utf8String.length());

    	// for every boundaries
    	RefPtr<HtmlToken> token = NULL;
    	RefPtr<HtmlToken> previousToken = NULL;
    	for (int i=0; i<boundaries.size(); i++) {
    		std::pair<int, int> boundary = boundaries[i];
    		int wordIndex = boundary.first;
    		int wordSize = boundary.second;
    		bool isSelected = node->isSameNode(startContainer) && offset >= startOffset+wordIndex && offset < startOffset+wordIndex+wordSize;

    		// create a token
    		WTF::String word = text.substring(wordIndex, wordSize);
    	    token = HtmlToken::create(node, word, startOffset+wordIndex, startOffset+wordIndex+wordSize);
    		token->setSelected(isSelected);
    		if (previousToken) {
        		token->setPreviousToken(previousToken);
    			previousToken->setNextToken(token);
    		}

    		// save previous token
    		previousToken = token;

    		// add token to the list
    		tokens->append(token.release());
    	}*/
    }

	return tokens;
}

// static
bool HtmlTokenizer::isChineseCharacter(const UChar32 character) {
	return (character >= 0x4E00 && character <= 0x9FFF)  	// CJK Unified Ideographs
			|| (character >= 0x1100 && character <= 0x11FF)  	// Hangul Jamo
			|| (character >= 0x3040 && character <= 0x309F)  	// Hirangana
			|| (character >= 0x30A0 && character <= 0x30FF)  	// Katakana
			|| (character >= 0x3100 && character <= 0x318F)  	// Bopomofo && Jangul Compatability Jamo
			|| (character >= 0x31F0 && character <= 0x32FF)  	// Katakana Phonetic Extensions, Enclosed CJK Letters and Months
			|| (character >= 0x3400 && character <= 0x4DFF)  	// CJK Unified Ideographs Extension A
			|| (character >= 0x20000 && character <= 0x2A6DF)  	// CJK Unified Ideographs Extension B
			|| (character >= 0xF900 && character <= 0xFAFF)  	// CJK Compatibility Ideographs
			|| (character >= 0x2F800 && character <= 0x2FA1F);
}

// static
PassRefPtr<HtmlToken> HtmlTokenizer::createToken(Node* node, const String& str, const int startOffset,
		const int endOffset, const bool isSelected, PassRefPtr<HtmlToken> previousTokenRef)
{
    RefPtr<HtmlToken> token = HtmlToken::create(node, str.substring(startOffset, (endOffset - startOffset)),
    		startOffset, endOffset);
    token->setSelected(isSelected);

    RefPtr<HtmlToken> previousToken = previousTokenRef;
    token->setPreviousToken(previousToken.get());
    if (previousToken) {
        previousToken->setNextToken(token.get());
    }

    previousToken.clear();
    return token.release();
}

} /* namespace cn */

} /* namespace html */

} /* namespace kikin */
