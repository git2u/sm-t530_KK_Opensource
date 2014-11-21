/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinHtmlAnalyzer_h
#define KikinHtmlAnalyzer_h

#include <wtf/text/WTFString.h>

#include "common/String.h"
#include "kikin/Language.h"

namespace WebCore {
    
    /* Forward declaration of Range. */
    class Range;
    
    /* Forward declaration of Node. */
    class Node;
    
    /* Forward declaration of Element. */
    class Element;

    /* Forward declaration of Document. */
	class Document;
}

namespace kikin {

/* Forward declaration of Entity. */
class HtmlEntity;

/* Forward declaration of Token. */
class HtmlToken;
    
namespace list {
    
    /* Forward declaration of Tokens. */
    class Tokens;
    
    /* Forward declaration of Ranges. */
    class Ranges;
    
} /* namespace list */
    
/* 
 * Analyzes the text for the passed range and returns the entity 
 * if there is any around the range. If no entity is found around
 * the range, returns the text of the range as an entity.
 */
class HtmlAnalyzer {

public:
    
    /* Analyzes the text around the Range. */
    static WTF::PassRefPtr<HtmlEntity> analyze(WTF::PassRefPtr<WebCore::Range> range);
    
private:
    
    /* Checks if the element is of given type. */
    static bool isElementOfType(WebCore::Element** element, const char* tagName);
    
    /* Detect the selection language */
    static Language detectLanguage(WebCore::Range* range, list::Ranges* ranges);

    /** Returns page language. */
    static Language getPageLanguage(WebCore::Document* document);

    /** Returns country code from page URL. */
	static kString getCountryCodeFromURL(WebCore::Document* document);

private:
    
    /** Do not allow construction of this class */
    HtmlAnalyzer() {}
    ~HtmlAnalyzer() {}
};
    
}   // namespace kikin

#endif // KikinHtmlAnalyzer_h
