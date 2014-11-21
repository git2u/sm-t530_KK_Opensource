/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinHtmlTokenizer_h
#define KikinHtmlTokenizer_h

#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

#include "kikin/Language.h"

#define EXPORT __attribute__((visibility("default")))

namespace WebCore {

    /* Forward declaration of Node. */
    class Node;

} // namespace WebCore

namespace kikin {

/* Forward decalaration of Token. */
class HtmlToken;

namespace list {

	/* Forward declaration of Ranges. */
    class Ranges;

	/* Forward declaration of Tokens. */
    class Tokens;

} // namespace list

/* Tokenizes the text in different token. */
class EXPORT HtmlTokenizer {
public:
    
    static WTF::PassRefPtr<list::Tokens> tokenize(const Language lang,
    		WTF::PassRefPtr<list::Ranges> ranges, WebCore::Node* startContainer, const int offset);

    static WTF::PassRefPtr<list::Tokens> tokenize(WTF::PassRefPtr<list::Ranges> ranges,
    		WebCore::Node* startContainer, const int offset);
    
    static PassRefPtr<HtmlToken> createToken(WebCore::Node* node, const String& str, const int startOffset,
    		const int endOffset, const bool hasDigit, const bool hasLowercase, const bool hasUppercase,
    		const bool hasOther, const bool isSelected, PassRefPtr<HtmlToken> previousTokenRef);
};

}   // namespace kikin

#endif // KikinTokenizer_h
