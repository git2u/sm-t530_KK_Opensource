/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */
#define LOG_TAG "kikin_tokenizer_cn"

#include <config.h>
#include "kikin/html/HtmlTokenizer.h"

#include "core/dom/Node.h"
#include "core/dom/Range.h"

#include "kikin/Tokenizer.h"

#include "kikin/html/HtmlRange.h"
#include "kikin/html/HtmlToken.h"
#include "kikin/html/HtmlTokenizer.h"
#include "kikin/html/cn/HtmlTokenizer.h"
#include "kikin/html/character/HtmlTokenizer.h"

#include "kikin/list/Ranges.h"
#include "kikin/list/Tokens.h"

#include "kikin/util/KikinUtils.h"

using namespace WebCore;

namespace kikin {
    
// static
WTF::PassRefPtr<list::Tokens> HtmlTokenizer::tokenize(const Language language,
		WTF::PassRefPtr<list::Ranges> ranges, Node* startContainer, const int offset)
{
    if (language == CHINESE || language == CHINESE_T) {
    	// special Chinese tokekizer
    	return html::cn::HtmlTokenizer::tokenize(ranges, startContainer, offset);

    } else if (language == JAPANESE) {
    	// default tokenizer for unsupported character based languages
    	return html::character::HtmlTokenizer::tokenize(ranges, startContainer, offset);

    } else {
    	// use default tokenizer
    	return HtmlTokenizer::tokenize(ranges, startContainer, offset);
    }
}

// static
PassRefPtr<list::Tokens> HtmlTokenizer::tokenize(PassRefPtr<list::Ranges> ranges, Node* startContainer, const int offset)
{
    RefPtr<list::Tokens> tokens = list::Tokens::create();
    RefPtr<HtmlToken> previousToken = NULL;

    for (unsigned j = 0; j < ranges->length(); j++) {
        bool hasUppercase = false;
        bool hasLowercase = false;
        bool hasDigit = false;
        bool hasOther = false;
        bool isSelected = false;

        HtmlRange* range = ranges->item(j);
        Node* node = range->getRange()->startContainer();

//        LOGE_KIKIN("Kikin range text:%ls, %d", range->getText().utf8().data(), __LINE__);

        const String& str = node->textContent();
        int startOffset = range->getRange()->startOffset();
        int currentOffset = startOffset;
        int endOffset = range->getRange()->endOffset();

        RefPtr<HtmlToken> token = NULL;
//        UChar c = str[startOffset];
        UChar32 c = str.characterStartingAt(startOffset);

        int i = startOffset;
        for (i = startOffset; i < endOffset; i++) {
//            c = str[i];
            c = str.characterStartingAt(i);

            bool isDelimiter = Tokenizer::isDelimiter(c);
            bool isSeparator = Tokenizer::isSeparator(c);

//            LOGD("Character:'%lc(0x%04x)' \tdelimeter:%s \tseparator:%s, %d", c, c, (isDelimiter ? "true" : "false"),
//                                    (isSeparator ? "true" : "false"), __LINE__);

            if (isDelimiter || (isSeparator &&
            		!Tokenizer::isPartOfWord((const_cast<WTF::String&>(str)).charactersWithNullTermination(), c, i, currentOffset))) {
                if (currentOffset != i) {
                    isSelected = (node->isSameNode(startContainer) && (offset < i) && (offset >= currentOffset));

//                    LOGD("Creating token, %d", __LINE__);
                    token = HtmlTokenizer::createToken(node, str, currentOffset, i, hasDigit, hasLowercase, hasUppercase,
                                                        hasOther, isSelected, previousToken);
//                    LOGD("Token created, %d", __LINE__);
                    previousToken = token;
                    tokens->append(token.release());

                    hasDigit = false;
                    hasLowercase = false;
                    hasUppercase = false;
                    hasOther = false;
                }

                // create a token for the separator
                if (isSeparator) {
//                    LOGD("Creating separator token, %d", __LINE__);
                	isSelected = ((offset < i+1) && (offset >= i));
                    token = HtmlTokenizer::createToken(node, str, i, i+1, false, false, false, true, isSelected,
                                                        previousToken);
//                    LOGD("separator token created, %d", __LINE__);
                    previousToken = token;
                    tokens->append(token.release());
                }

                if (!U16_IS_SINGLE(c)) {
//                    LOGD("We are on 32-bit char:%lc(0x%04x), %d", c, c, __LINE__);
                    i++;
                }
                currentOffset = i + 1;

            } else {
                if (Tokenizer::isNumber(c)) {
                    hasDigit = true;
                } else if (Tokenizer::isLowerCase(c)) {
                    hasLowercase = true;
                } else if (Tokenizer::isUpperCase(c)) {
                    hasUppercase = true;
                } else {
                    hasOther = true;
                }
            }
        }

        if (str.length() > 0 && c != ' ' && c != '\r' && c != '\n' && currentOffset < static_cast<int>(str.length())) {
            token = HtmlTokenizer::createToken(node, str, currentOffset, i, hasDigit, hasLowercase, hasUppercase,
                                                hasOther, false, previousToken);
            previousToken = token;
            tokens->append(token.release());
        }

//        if (token) {
//            token.clear();
//        }
    }

//    if (previousToken) {
//        previousToken.clear();
//    }

    return tokens.release();
}

// static
PassRefPtr<HtmlToken> HtmlTokenizer::createToken(Node* node, const String& str, const int startOffset,
		const int endOffset, const bool hasDigit, const bool hasLowercase, const bool hasUppercase,
		const bool hasOther, const bool isSelected, PassRefPtr<HtmlToken> previousTokenRef)
{
//    LOGD("***************************************************************");
    RefPtr<HtmlToken> token = HtmlToken::create(node, str.substring(startOffset, (endOffset - startOffset)), startOffset, endOffset);
    token->setDigit(hasDigit);
    token->setLowercase(hasLowercase);
    token->setUppercase(hasUppercase);
    token->setOther(hasOther);
    token->setSelected(isSelected);

    token->setEndOfSentence(Tokenizer::isEndOfSenetence(token->getText()));
//    LOGD("Token text: %ls  \tIs end of sentence:%s, %d", token->getText().utf8().data(),
//                (token->isEndOfSentence() ? "true" : "false"), __LINE__);

    RefPtr<HtmlToken> previousToken = previousTokenRef;
    token->setPreviousToken(previousToken.get());

    if (previousToken) {
        previousToken->setNextToken(token.get());
        Node* previousTokenNode = previousToken->getNode();
        Node* currentTokenNode = token->getNode();

        Element* previousTokenElement = previousTokenNode->parentElement();
        Element* currentTokenElement = currentTokenNode->parentElement();
        if ((!previousTokenNode->isSameNode(currentTokenNode) &&
                !(util::KikinUtils::isChildNode(previousTokenElement, currentTokenElement) ||
                  util::KikinUtils::isChildNode(currentTokenElement, previousTokenElement))) ||
            token->startsWith("("))
        {
//            LOGD("Marking token as end of sentence. %d", __LINE__);
            previousToken->setEndOfSentence(true);
        }

//        previousToken.clear();
    }

//    LOGD("***************************************************************");

    previousToken.clear();
    return token.release();
}

} /* namespace kikin */
