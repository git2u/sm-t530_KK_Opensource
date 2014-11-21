/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#define LOG_TAG "kikin_address_microformat_annotation"

#include "config.h"
#include "kikin/annotation/html/AddressMicroformat.h"

#include "core/css/CSSParser.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Node.h"
#include "core/dom/StaticNodeList.h"
#include "core/platform/text/RegularExpression.h"

#include "kikin/html/HtmlEntity.h"
#include "kikin/html/HtmlToken.h"
#include "kikin/html/HtmlRange.h"
#include "kikin/list/Ranges.h"

// #include "HTMLNames.h"



//#include <utils/Log.h>
//#include <wtf/text/CString.h>
//#include <wtf/text/AtomicString.h>

//#undef LOGD
//#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

using namespace WebCore;

namespace kikin { namespace annotation {
    
// static 
WTF::Vector<WTF::String> AddressMicroformat::s_types;
    
AddressMicroformat::AddressMicroformat() {}
    
AddressMicroformat::~AddressMicroformat() {}
    
// static 
WTF::PassRefPtr<HtmlEntity> AddressMicroformat::analyzeToken(HtmlToken* token)
{
    Node* node = token->getNode();
    Node* element = (node->isTextNode() ? node->parentNode() : node);

    // check if element has address microformat
    if (AddressMicroformat::hasAddressMicroformat(element)) {

        // get the parent element with address microformat
        RefPtr<NodeList> rootElements = AddressMicroformat::getTopMicroformatElements(element);
        if (rootElements && rootElements->length() > 0) {

            // Get all the child address micformat element for the root address microformat element.
            RefPtr<NodeList> elements = AddressMicroformat::getAllMicroformatsElements(rootElements->item(0));
            if (elements && elements->length() > 0) {
                // create the list of ranges for all the address microformat elements found
                // and return the entity from the ranges.
                RefPtr<list::Ranges> ranges = list::Ranges::create();
                unsigned i = 0; 
                
                while (i < elements->length()) {
                    Node* node = elements->item(i);
                    RefPtr<HtmlRange> range = HtmlRange::createFromNode(node, "address");
                    ranges->append(range.release());
                    i++;
                }

                return HtmlEntity::create(ranges.release(), "address", "");
            }
        }
    }
    
    // If no microformat address element is founnd, return null
    return NULL;
}
    
// static 
void AddressMicroformat::createTypesList() 
{
//    types: ['stress-address', 'street-address', 'locality', 'country-name', 'region', 'postal-code', 'extended-address', 'org']
    
    s_types.append("stress-address");
    s_types.append("street-address");
    s_types.append("locality");
    s_types.append("country-name");
    s_types.append("region");
    s_types.append("postal-code");
    s_types.append("extended-address");
    s_types.append("org");
}
    
// static
bool AddressMicroformat::hasClass(WebCore::Element* element, WTF::String& className)
{
    const String classAttributeName = "class";
    
    // check if element has an attribute of name class
    if (element->hasAttribute(classAttributeName)) { 
        // get class names
        const String& classes = element->getAttribute(classAttributeName).string();
//        LOGD("Class attribute string:%ls, %d", classes.utf8().data(), __LINE__);
        
        // check if class names contains the type of class we are looking for
        const String regexString = className + " |$";    // space or end of sentence
        RegularExpression regex(regexString, WTF::TextCaseInsensitive);
        int matchLength = 0;
        int matchIndex = regex.match(classes, 0, &matchLength);
//        LOGD("Regex:%ls \tMatch Index:%d \tMatch Length:%d, %d", regexString.utf8().data(), matchIndex, matchLength, __LINE__);
        
        // If we found return true
        if (matchIndex != -1) {
            return true;
        }
    }
    
    return false;
}
    
// static
bool AddressMicroformat::hasItemProp(WebCore::Element* element, WTF::String& itemPropName)
{
    const String itemPropAttributeName = "itemprop";
    
    // check if element has an attribute of name itemprop
    if (element->hasAttribute(itemPropAttributeName)) { 
        // get item prop
        const String& itemProp = element->getAttribute(itemPropAttributeName).string();
//        LOGD("item prop attribute string:%ls, %d", itemProp.utf8().data(), __LINE__);
        
        // check if item prop is equal to itemPropName
        if (equalIgnoringCase(itemProp, itemPropName)) {
            return true;
        }
    }
    
    return false;
}
    
// static 
PassRefPtr<StaticNodeList> AddressMicroformat::createParentSelectorNodeList(Node* node, const CSSSelectorList& querySelectorList, bool shouldCheckImmediateParentOnly)
{
    Vector<RefPtr<Node> > nodes;
    Document* document = node->document();
    const CSSSelector* onlySelector = querySelectorList.hasOneSelector() ? querySelectorList.first() : 0;
    bool strictParsing = !document->inQuirksMode();
    
    // If there is only one element with the id in the document, then append that element
    if (strictParsing && node->inDocument() && onlySelector && onlySelector->m_match == CSSSelector::Id &&
    		!document->containsMultipleElementsWithId(onlySelector->value()))
    {
        Element* element = document->getElementById(onlySelector->value());

        if (element && !node->isDocumentNode() && node->isDescendantOf(element))
        {
        	SelectorChecker selectorChecker(document, SelectorChecker::QueryingRules);
        	SelectorChecker::SelectorCheckingContext selectorCheckingContext(onlySelector, element, SelectorChecker::VisitedMatchDisabled);
        	selectorCheckingContext.behaviorAtBoundary = SelectorChecker::StaysWithinTreeScope;
        	selectorCheckingContext.scope = !node->isDocumentNode() && node->isContainerNode() ? toContainerNode(node) : 0;
        	PseudoId ignoreDynamicPseudo = NOPSEUDO;

        	if ((selectorChecker.match(selectorCheckingContext, ignoreDynamicPseudo, DOMSiblingTraversalStrategy()) == SelectorChecker::SelectorMatches) &&
        			(!shouldCheckImmediateParentOnly || node->parentElement()->isSameNode(element)))
        	{
        		nodes.append(element);
        	}
        }
    } else if (shouldCheckImmediateParentOnly) {
        // If we have to check immediate parent only, then don't look for
        // other parent elements.
        Element* parentElement = node->parentElement();
        if (parentElement) {
            for (const CSSSelector* selector = querySelectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
            	SelectorChecker selectorChecker(document, SelectorChecker::QueryingRules);
				SelectorChecker::SelectorCheckingContext selectorCheckingContext(selector, parentElement, SelectorChecker::VisitedMatchDisabled);
				selectorCheckingContext.behaviorAtBoundary = SelectorChecker::StaysWithinTreeScope;
				selectorCheckingContext.scope = !node->isDocumentNode() && node->isContainerNode() ? toContainerNode(node) : 0;
				PseudoId ignoreDynamicPseudo = NOPSEUDO;

                if (selectorChecker.match(selectorCheckingContext, ignoreDynamicPseudo, DOMSiblingTraversalStrategy()) == SelectorChecker::SelectorMatches) {
                    nodes.append(parentElement);
                    break;
                }
            }
        }
    } else {
        // look for all the parent elements till the document node.
        for (Element* parentElement = node->parentElement(); parentElement && !parentElement->isSameNode(document); parentElement = parentElement->parentElement()) {
            for (const CSSSelector* selector = querySelectorList.first(); selector; selector = CSSSelectorList::next(selector)) {

            	SelectorChecker selectorChecker(document, SelectorChecker::QueryingRules);
				SelectorChecker::SelectorCheckingContext selectorCheckingContext(selector, parentElement, SelectorChecker::VisitedMatchDisabled);
				selectorCheckingContext.behaviorAtBoundary = SelectorChecker::StaysWithinTreeScope;
				selectorCheckingContext.scope = !node->isDocumentNode() && node->isContainerNode() ? toContainerNode(node) : 0;
				PseudoId ignoreDynamicPseudo = NOPSEUDO;

                if (selectorChecker.match(selectorCheckingContext, ignoreDynamicPseudo, DOMSiblingTraversalStrategy()) == SelectorChecker::SelectorMatches) {
                    nodes.append(parentElement);
                    break;
                }
            }
        }
    }
    
    // return the list of element matching CSS selectors.
    return StaticNodeList::adopt(nodes);
}
    
// static 
PassRefPtr<NodeList> AddressMicroformat::parentQuerySelectorAll(Node* node, const String& selectors, ExceptionCode& ec, bool shouldCheckImmediateParentOnly)
{
    // if selector list is empty, then don't check for anything and set the error code.
    if (selectors.isEmpty()) {
        ec = SYNTAX_ERR;
        return 0;
    }
    
    // creates the CSS selectors list from the string
    CSSParser parser(node->document());
	CSSSelectorList querySelectorList;
	parser.parseSelector(selectors, querySelectorList);
    
    // If there is a syntax error in the selectors list
    // return and set the exception
    if (!querySelectorList.first() || querySelectorList.hasInvalidSelector()) {
        ec = SYNTAX_ERR;
        return 0;
    }
    
    // Throw a NAMESPACE_ERR if the selector includes any namespace prefixes.
    if (querySelectorList.selectorsNeedNamespaceResolution()) {
        ec = NAMESPACE_ERR;
        return 0;
    }
    
    // return the list of parent element for the selectors.
    return createParentSelectorNodeList(node, querySelectorList, shouldCheckImmediateParentOnly);
}
    
// static 
bool AddressMicroformat::hasType(WebCore::Node* node, WTF::String& type)
{

    if (!node || !node->isElementNode()) {
//        LOGD("Has a valid node:%s. \tIs valid HTML element:%s, %d", (node ? "true" : "false"), ((node && node->isElementNode()) ? "true" : "false"), __LINE__);
        return false;
    }
        
    
    Element* ele = static_cast<Element*> (node);
    if (!ele) {
//        LOGD("Cannot cast node to element. %d", __LINE__);
        return false;
    }
    
//    LOGD("Type we are checking:%ls, %d", type.utf8().data(), __LINE__);
    
    // check if the element has a class of given type
    if (AddressMicroformat::hasClass(ele, type)) {
        return true;
    }
    
    // check if the element has a item property of given type
    if (AddressMicroformat::hasClass(ele, type)) {
        return true;
    }
    
    String selectors = "." + type + ",*[itemprop=\"" + type + "\"]";
//    LOGD("Selectors string:%ls. %d", selectors.utf8().data(), __LINE__);
    
    ExceptionCode ec = 0;
    RefPtr<NodeList> nodes = AddressMicroformat::parentQuerySelectorAll(node, selectors, ec, true);
    ASSERT(!ec);
    
    bool result = (nodes && (nodes->length() > 0));
//    if (nodes) {
////        LOGD("Number of nodes found:%ld. %d", nodes->length(), __LINE__);
//        nodes.release();
//    } else {
////        LOGD("We hadn't received a valid list of nodes. %d", __LINE__);
//    }
    
    return result;
}

// static 
bool AddressMicroformat::hasAddressMicroformat(WebCore::Node* node)
{
    if (!node) {
//        LOGD("Node received is not valid. %d", __LINE__);
        return false;
    }
        
    
    if (s_types.isEmpty()) {
        AddressMicroformat::createTypesList();
    }
    
    for (unsigned i = 0; i < s_types.size(); i++) {
        bool ret = AddressMicroformat::hasType(node, s_types[i]);
        if (ret) {
            return true;
        }
    }
    
    return false;
}

// static 
WTF::PassRefPtr<WebCore::NodeList> AddressMicroformat::getAllMicroformatsElements(WebCore::Node* node)
{
    if (!node) {
//        LOGD("Node received is not valid. %d", __LINE__);
        return NULL;
    }
    
    ExceptionCode ec = 0;
    RefPtr<NodeList> nodes = node->querySelectorAll(WTF::String(".street-address,.locality,.country-name,.region,.postal-code,.extended-address,.org"), ec);
    ASSERT(!ec);
    if (nodes && (nodes->length() > 0)) {
        return nodes.release();
    }
    
    nodes = node->querySelectorAll(WTF::String("span[itemprop=\"stress-address\"],span[itemprop=\"street-address\"],span[itemprop=\"locality\"],span[itemprop=\"country-name\"],span[itemprop=\"region\"],span[itemprop=\"postal-code\"],span[itemprop=\"extended-address\"]"), ec);
    ASSERT(!ec);
    if (nodes && (nodes->length() > 0)) {
        return nodes.release();
    }
    
    return NULL;
}

// static 
WTF::PassRefPtr<WebCore::NodeList> AddressMicroformat::getTopMicroformatElements(WebCore::Node* node)
{
    if (!node || !node->isElementNode()) {
//        LOGD("Has a valid node:%s. \tIs valid HTML element:%s, %d", (node ? "true" : "false"), ((node && node->isElementNode()) ? "true" : "false"), __LINE__);
        return NULL;
    }
    
    ExceptionCode ec = 0;
    RefPtr<NodeList> nodes = AddressMicroformat::parentQuerySelectorAll(node, "address,.location,.adr,*[itemprop=\"address\"]", ec, false);
    ASSERT(!ec);
    
    return nodes.release();
}
    
} /* namespace annotation */
    
} /* namespace kikin */
