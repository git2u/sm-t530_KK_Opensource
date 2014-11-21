/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinHtmlTokenizer_cn_h
#define KikinHtmlTokenizer_cn_h

#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

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
    
namespace html {

namespace cn {

/* Tokenizes the text in different token. */
class EXPORT HtmlTokenizer {
public:
    
    static WTF::PassRefPtr<kikin::list::Tokens> tokenize(WTF::PassRefPtr<kikin::list::Ranges> ranges,
    		WebCore::Node* startContainer, const int offset);

    static PassRefPtr<HtmlToken> createToken(WebCore::Node* node, const WTF::String& str,
    		const int startOffset, const int endOffset, const bool isSelected, PassRefPtr<HtmlToken> previousTokenRef);

    static bool isChineseCharacter(const UChar32 character);
    
};

}   // namespace cn

}   // namespace html
    
}   // namespace kikin

#endif // KikinTokenizer_h
