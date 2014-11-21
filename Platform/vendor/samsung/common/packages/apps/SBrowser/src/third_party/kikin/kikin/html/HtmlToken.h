/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinHtmlToken_h
#define KikinHtmlToken_h

#include "wtf/RefCounted.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

#include "common/String.h"

#include "kikin/Token.h"

namespace WebCore {
    
    /* Forward declaration of Node. */
    class Node;
}

namespace kikin {
    
/* Implementation of Token for Html text. */
class HtmlToken : public Token, public WTF::RefCounted<HtmlToken> {
    
public:

    /* Creates the instance of Token and adds the reference to it. */
    static WTF::PassRefPtr<HtmlToken> create(WebCore::Node* node, const WTF::String& text, int startOffset, int endOffset);
    
    /* Destructor */
    virtual ~HtmlToken();
    
    WebCore::Node* getNode();
    
    /* Checks whether the previous token is a twitter tag. */
    virtual bool checkPreviousTokenIsTwitterTag();
    
    /* Checks whether the token is on same node as the passed token. */
    virtual bool isOnSameNode(Token* token) const;

    /* Checks whether the token is on same element as the passed token. */
    virtual bool isOnSameElement(Token* token) const;

    /* Creates the entity object for the token by including passed number of next tokens. */
    virtual Entity* toEntity(const kString& type, const kString& language, int nextCount);
    
    /* Get the text from the current token to "nextCount" after. */
    virtual const kString toText(int nextCount);

private:

    /* Constructor */
    HtmlToken(WebCore::Node* node, const WTF::String& text, int startOffset, int endOffset);
    
private:
    
    WebCore::Node* m_node;
    
};

}   // namespace kikin

#endif // KikinHtmlToken_h
