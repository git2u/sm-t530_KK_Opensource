/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinHtmlTokenizer_character_h
#define KikinHtmlTokenizer_character_h

#include <wtf/PassRefPtr.h>

#define EXPORT __attribute__((visibility("default")))

namespace WebCore {

    /* Forward declaration of Node. */
    class Node;

} // namespace WebCore

namespace kikin {

namespace list {

	/* Forward declaration of Ranges. */
    class Ranges;

	/* Forward declaration of Tokens. */
    class Tokens;

} // namespace list
    
namespace html {

namespace character {

/* Tokenizes the text in different token. */
class EXPORT HtmlTokenizer {
public:
    
    static WTF::PassRefPtr<kikin::list::Tokens> tokenize(WTF::PassRefPtr<kikin::list::Ranges> ranges,
    		WebCore::Node* startContainer, const int offset);
    
};

}   // namespace character

}   // namespace html
    
}   // namespace kikin

#endif // KikinTokenizer_h
