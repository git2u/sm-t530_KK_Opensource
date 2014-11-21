/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */
#define LOG_TAG "kikin_html_tokenizer_character"

#include <config.h>
#include "kikin/html/character/HtmlTokenizer.h"

#include "core/dom/Node.h"
#include "core/dom/Range.h"

// #include <wtf/text/CString.h>

#include "kikin/html/HtmlToken.h"
#include "kikin/html/HtmlRange.h"

#include "kikin/list/Ranges.h"
#include "kikin/list/Tokens.h"

using namespace WebCore;

namespace kikin { namespace html { namespace character {
    
// static
WTF::PassRefPtr<kikin::list::Tokens> HtmlTokenizer::tokenize(WTF::PassRefPtr<kikin::list::Ranges> ranges,
		WebCore::Node* startContainer, const int offset) {

	RefPtr<list::Tokens> tokens = list::Tokens::create();

	// for every range
    for (unsigned j = 0; j < ranges->length(); j++) {
        HtmlRange* range = ranges->item(j);
        Node* node = range->getRange()->startContainer();
        int startOffset = range->getRange()->startOffset();
        int endOffset = range->getRange()->endOffset();
        WTF::String text = node->textContent();

    	// create a token for every character
    	RefPtr<HtmlToken> previousToken = NULL;
    	for (int i = startOffset; i < endOffset; i++) {
    		bool isSelected = node->isSameNode(startContainer) && offset == i;
    		WTF::String character = text.substring(i, 1);

    		// create a token
    	    RefPtr<HtmlToken> token = HtmlToken::create(node, character, i, (i + 1));
    		token->setSelected(isSelected);
    		token->setPreviousToken(previousToken.get());
    		if (previousToken) {
    			previousToken->setNextToken(token.get());
    		}
    		tokens->append(token.get());

    		previousToken = token;
    	}
    }

	// return tokens
	return tokens.release();
}

} /* namespace character */

} /* namespace html */

} /* namespace kikin */
