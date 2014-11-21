/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#define LOG_TAG "kikin_html_token"

#include "config.h"
#include "kikin/html/HtmlToken.h"

#include "core/dom/Range.h"
#include "core/dom/Node.h"
#include "core/dom/Element.h"
#include "core/dom/Document.h"

#include "kikin/html/HtmlEntity.h"
#include "kikin/html/HtmlRange.h"
#include "kikin/list/Ranges.h"

//#undef LOGD
#define LOGD_KIKIN(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

using namespace WebCore;

namespace kikin {
    
// static
WTF::PassRefPtr<HtmlToken> HtmlToken::create(WebCore::Node* node, const WTF::String& text, int startOffset, int endOffset)
{
	return adoptRef(new HtmlToken(node, text, startOffset, endOffset));
}
    
    
HtmlToken::HtmlToken(Node* node, const String& text, int startOffset, int endOffset) :
		Token((const_cast<WTF::String&>(text)).charactersWithNullTermination(), startOffset, endOffset),
		m_node(node)
{
}
    
HtmlToken::~HtmlToken()
{
}

Node* HtmlToken::getNode() {
    return m_node;
}
    
bool HtmlToken::checkPreviousTokenIsTwitterTag()
{
    // On pages like twitter.com, twitter tags are in different element node
    // and we consider them as a diferent token. So to make sure, if previous
    // token is a valid twitter tag, we will check if the previous token is a
    // twitter tag and is a child element of existing element.
    HtmlToken* previousToken = static_cast<HtmlToken*>(this->getPreviousToken());
    if (!previousToken) {
    	return false;
    }

    const kString & previousTokenText_kstring = previousToken->getText();
    String previousTokenText = WTF::String(previousTokenText_kstring.data(), previousTokenText_kstring.length());
    Element* previousTokenElement;
    if (previousToken->getNode()->isElementNode()) {
        previousTokenElement = static_cast<Element*> (previousToken->getNode());
    } else {
        previousTokenElement = previousToken->getNode()->parentElement();
    }
    
    ASSERT(previousTokenElement);
    if (((previousTokenText == "@") || (previousTokenText == "#")) &&
    		previousTokenElement->parentElement()->isSameNode(this->getNode()->parentElement()))
    {
        return true;
    }
    
    return false;
}

bool HtmlToken::isOnSameNode(Token* token) const
{
    return m_node->isSameNode(static_cast<HtmlToken*>(token)->getNode());
}

bool HtmlToken::isOnSameElement(Token* token) const
{
    Element* tokenElement = ((token && static_cast<HtmlToken*>(token)->getNode()) ?
                                    static_cast<HtmlToken*>(token)->getNode()->parentElement() : NULL);
    Element* currentTokenElement = m_node ? m_node->parentElement() : NULL;
    return tokenElement && currentTokenElement && currentTokenElement->isSameNode(tokenElement);
}

const kString HtmlToken::toText(int nextCount) {
	HtmlToken* endToken = this;
    while (--nextCount >= 0 && endToken) {
        endToken = (HtmlToken*)endToken->getNextToken();
    }

    // if we do not have valid end token, then return empty string
    // This is a fail safe check and should not happen ever.
    // We should return the valid next count
    if (endToken) {
        return kString();
    }

    ASSERT(endToken);
    ASSERT(m_node);

    Document* document = m_node->document();
    ASSERT(document);

    // create a new range
    ExceptionCode ec = 0;
    RefPtr<WebCore::Range> range = document->createRange();

    // set start
    range->setStart(this->getNode(), this->getStartOffset(), ec);
    ASSERT(!ec);

    // set end
    range->setEnd(endToken->getNode(), endToken->getEndOffset(), ec);
    ASSERT(!ec);

    // get text
	return range->text().charactersWithNullTermination();
}
    
Entity* HtmlToken::toEntity(const kString& type, const kString& language, int nextCount)
{
	WTF::String wtfType = WTF::String(type.data(), type.length());
	RefPtr<list::Ranges> ranges = list::Ranges::create();
    
    ASSERT(m_node);
    
    Document* document = m_node->document();
    ASSERT(document);
    
    RefPtr<WebCore::Range> range = document->createRange();
    ASSERT(range);
    
    Node* currentNode = m_node;
    HtmlToken* endToken = this;

    ExceptionCode ec = 0;
    range->setStart(currentNode, this->getStartOffset(), ec);
    ASSERT(!ec);
    
    while (nextCount-- > 0) {
        endToken = static_cast<HtmlToken*>(endToken->getNextToken());
        if (endToken && !endToken->getNode()->isSameNode(currentNode)) {
            ASSERT(endToken->getPreviousToken());
            
            range->setEnd(currentNode, endToken->getPreviousToken()->getEndOffset(), ec);
            ASSERT(!ec);
            
            Node* startContainer = range->startContainer();
            ranges->append(HtmlRange::create(range.release(), startContainer, wtfType));
            range = document->createRange();
            
            currentNode = endToken->getNode();
            range->setStart(currentNode, endToken->getStartOffset(),  ec);
            ASSERT(!ec);
        }
    }

    // if we do not have valid end token at the end, then
    // do not create the last range and append it. This is a
    // fail safe check and should not happen ever.
    if (endToken) {
		range->setEnd(endToken->getNode(), endToken->getEndOffset(), ec);
		ASSERT(!ec);

		Node* startContainer = range->startContainer();
		ranges->append(HtmlRange::create(range.release(), startContainer, wtfType));
    } else {
        // release the memory
        range.release();
    }

    return HtmlEntity::create(ranges.release(), wtfType, WTF::String(language.data(), language.length()));
}

} /* namespace kikin */