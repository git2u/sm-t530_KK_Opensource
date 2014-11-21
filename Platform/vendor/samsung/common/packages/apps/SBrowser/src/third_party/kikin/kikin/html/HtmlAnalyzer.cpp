/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "kikin_html_analyzer"

#include "config.h"
#include "kikin/html/HtmlAnalyzer.h"

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLCollection.h"
// #include "ClientRect.h"

#include "core/platform/text/RegularExpression.h"
// #include <wtf/text/CString.h>
#include <wtf/RefPtr.h>

#include "common/Log.h"
#include "common/String.h"

#include "kikin/Analyzer.h"
#include "kikin/LanguageDetector.h"
#include "kikin/Tokenizer.h"

#include "kikin/annotation/html/AddressMicroformat.h"

#include "kikin/html/HtmlEntity.h"
#include "kikin/html/HtmlRange.h"
#include "kikin/html/HtmlToken.h"
#include "kikin/html/HtmlTokenizer.h"

#include "kikin/list/Ranges.h"
#include "kikin/list/Tokens.h"

#include "kikin/util/KikinUtils.h"
#include "kikin/util/DeviceUtils.h"

using namespace WebCore;

namespace kikin {
    
// static
PassRefPtr<HtmlEntity> HtmlAnalyzer::analyze(WTF::PassRefPtr<WebCore::Range> range)
{
    if (!range || !range->startContainer() || !range->startContainer()->isTextNode()) {
        return NULL;
    }
    
    // get the parent element and check if the range is on special type of elements
    Element* element = range->startContainer()->parentElement();
    bool shouldSelectNodeCompletely = false;
    WTF::String elementType;

    // treat h1 and link elements as special elements
    if (HtmlAnalyzer::isElementOfType(&element, "h1")) {
    	shouldSelectNodeCompletely = true;
    	elementType = "tag[h1]";
    } else if (HtmlAnalyzer::isElementOfType(&element, "a") || HtmlAnalyzer::isElementOfType(&element, "link")) {
    	shouldSelectNodeCompletely = true;
    	elementType = "tag[a]";
    }

    // select the element completely if they are of special kind
    if (shouldSelectNodeCompletely) {
        RefPtr<HtmlRange> kikinRange = HtmlRange::createFromNode(element, elementType);

    	if (kikinRange) {
    		RefPtr<list::Ranges> ranges = list::Ranges::create();
    		ranges->append(kikinRange.release());
    		return HtmlEntity::create(ranges.release(), elementType, "");
    	} else {
    		return NULL;
    	}
    }

    RefPtr<list::Ranges> ranges = HtmlRange::createRangesFromStartingPoint(range->startContainer(), range->startOffset(), 100);

    // detect the language
    Language lang = HtmlAnalyzer::detectLanguage(range.get(), ranges.get());

	// tokenize ranges
	RefPtr<list::Tokens> tokens = HtmlTokenizer::tokenize(lang, ranges.release(), range->startContainer(), range->startOffset());

    int cursorOffset = range->startOffset();
    HtmlToken* token = NULL;
    HtmlToken* closestToken = NULL; // in case the user is over a blank, snap to the left most token
    int closestOffset = 1000;
    
    for (unsigned i = 0; i < tokens->length(); i++) {
        
        token = static_cast<HtmlToken*>(tokens->item(i));
        if (token->getNode()->isSameNode(range->startContainer())) {
            if (static_cast<int>(token->getStartOffset()) <= cursorOffset && static_cast<int>(token->getEndOffset()) > cursorOffset) {
                break;
            }

            int offset = cursorOffset - token->getEndOffset();
            if (offset < 0)
                offset = -offset;
            
            if (offset < closestOffset) {
                closestToken = token;
                closestOffset = offset;
            }
        } 
    }
            
    if (!token) {
        if (closestToken) {
            token = closestToken;
        } else {
            return NULL;
        }
    }
    
    kString countryCode = HtmlAnalyzer::getCountryCodeFromURL(range->ownerDocument());
    if (countryCode.isEmpty()) {
    	countryCode = util::DeviceUtils::getDeviceCountryCode();
    }

    RefPtr<HtmlEntity> entity = adoptRef(static_cast<HtmlEntity*>(Analyzer::analyze(token, lang, countryCode)));
    ASSERT(entity);
    
	const kString entityType = entity->getType();
    if (entityType == "text" || entityType == "state" || entityType == "a_beginning" || entityType == "a_bridge" || entityType == "nouns") {
        RefPtr<HtmlEntity> addressMicroformatEntity = static_pointer_cast<HtmlEntity>(annotation::AddressMicroformat::analyzeToken(token));
        if (addressMicroformatEntity) {
            addressMicroformatEntity->setTouchedWord(token->getText());
            return addressMicroformatEntity.release();
        }
    }
    
    return entity.release();
}
    
// static
bool HtmlAnalyzer::isElementOfType(Element** element, const char* tagName)
{
	if (*element) {
		String elementTagName = (*element)->tagName().lower();
		if (elementTagName == tagName) {
		    return true;
		} else {
			Element* parentElement = (*element)->parentElement();
			if (parentElement) {
				elementTagName = parentElement->tagName().lower();
				if (elementTagName == tagName) {
				    *element = parentElement;
					return true;
				}
			}
		}
	}

	return false;
}
    
// static
Language HtmlAnalyzer::detectLanguage(WebCore::Range* range, list::Ranges* ranges)
{
	// try to get the language from the page
	Language lang = HtmlAnalyzer::getPageLanguage(range->ownerDocument());
	if (lang == UNKNOWN_LANGUAGE) {
		// get text from ranges
		WTF::String text;
		int offset = 0;
		for (unsigned i = 0; i < ranges->length(); i++) {

			// is this range the one where the finger is?
			if (ranges->item(i)->getRange()->startContainer()->isSameNode(range->startContainer())) {
				// save the selection offset in the concatenated text
				offset = text.length() + (range->startOffset() - ranges->item(i)->getRange()->startOffset());
			}

			// concat text
			text.append(ranges->item(i)->getText());
			text.append(" ");
		}

	    // detect language of the text
	    lang = LanguageDetector::detect(text.charactersWithNullTermination(), offset);

	    // try to get default device language
	    if (lang == UNKNOWN_LANGUAGE) {
	    	lang = LanguageDetector::getDeviceLanguage();
	    }

	    // default language if nothing found
	    if (lang == UNKNOWN_LANGUAGE) {
	    	lang = ENGLISH;
	    }
	}

	return lang;
}

// static
Language HtmlAnalyzer::getPageLanguage(WebCore::Document* document) {
	WTF::String lang;

	// get the language from the document element
	WebCore::Element* documentElement = document->documentElement();
	if (documentElement != NULL) {
		// try to get form <html lang="">
		if (lang.length() == 0) {
			lang = documentElement->getAttribute("lang");
		}

		// try to get form <html xml:lang="">
		if (lang.length() == 0) {
			lang = documentElement->getAttribute("xml:lang");
		}
	}

	// did not find anything yet?
	if (lang.length() == 0) {
		// get the language from <head>
		WebCore::HTMLHeadElement* headElement = document->head();
		if (headElement != NULL) {
			// get all children <meta>
			RefPtr<WebCore::HTMLCollection> children = headElement->children();
			for (unsigned i = 0; i < children->length(); i++) {
				WebCore::Node* node = children->item(i);
				if (node->isHTMLElement()) {
					// get <meta> element
					WebCore::HTMLElement* metaElement = static_cast<WebCore::HTMLElement*>(node);

					// get name="lang"
					WTF::String metaName = metaElement->getAttribute("name");
					if (WTF::equalIgnoringCase(metaName, "lang")) {
						// get content attribute
						lang = metaElement->getAttribute("content");
					}

					// get http-equiv="Content-Language"
					WTF::String equivName = metaElement->getAttribute("http-equiv");
					if (WTF::equalIgnoringCase(equivName, "Content-Language")) {
						// get content attribute
						lang = metaElement->getAttribute("content");
					}

					if (lang.length() != 0) {
						// finish, we found a language
						break;
					}
				}
			}
		}
	}

	// did we find a language?
	if (lang.length() != 0) {
		// try converting text to Language
		Language ret = LanguageDetector::languageFromString(lang.charactersWithNullTermination());
		if (ret != UNKNOWN_LANGUAGE) return ret;
	}
	kString countryCodeFromURL = HtmlAnalyzer::getCountryCodeFromURL(document);

	// try to extract host from url
	if (!countryCodeFromURL.isEmpty()) {
		if (countryCodeFromURL == "FR") {
			return FRENCH;
		} else if (countryCodeFromURL == "DE") {
			return GERMAN;
		} else if ((countryCodeFromURL == "UK") || (countryCodeFromURL == "IE")) {
			return ENGLISH;
		} else if (countryCodeFromURL == "NL") {
			return DUTCH;
		} else if (countryCodeFromURL == "ES") {
			return SPANISH;
		} else if (countryCodeFromURL == "CN") {
			return CHINESE;
		}
	}

	return UNKNOWN_LANGUAGE;
}

// static
kString HtmlAnalyzer::getCountryCodeFromURL(WebCore::Document* document)
{
	WTF::String url = document->documentURI();
	size_t protocolIdx = url.find("://");
	if (protocolIdx != notFound) {
		size_t hostIdx = url.find("/", protocolIdx+3);
		WTF::String host = url.substring(protocolIdx+3, protocolIdx != notFound ? hostIdx - protocolIdx - 3 : url.length() - protocolIdx - 3);
		if (host.length() > 0) {
			if (host.endsWith(".fr")) {
				return kString("FR");
			} else if (host.endsWith(".de")) {
				return kString("DE");
			} else if (host.endsWith(".co.uk")) {
				return kString("UK");
			} else if (host.endsWith(".nl")) {
				return kString("NL");
			} else if (host.endsWith(".ie")) {
				return kString("IE");
			} else if (host.endsWith(".es")) {
				return kString("ES");
			} else if (host.endsWith(".cn")) {
				return kString("CN");
			}
		}
	}

	return kString();
}
    
} /* namespace kikin */
