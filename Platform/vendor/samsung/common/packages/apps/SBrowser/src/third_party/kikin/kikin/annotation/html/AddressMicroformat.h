/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinAddressMicroformatAnnotation_h
#define KikinAddressMicroformatAnnotation_h

#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
    
    /* Forward decalaration of CSSSelectorList */
    class CSSSelectorList;
    
    /* Forward decalaration of Element */
    class Element;
    
    /* Forward decalaration of Node */
    class Node;
    
    /* Forward decalaration of NodeList */
    class NodeList;
    
    /* Forward decalaration of StaticNodeList */
    class StaticNodeList;
    
    /* Forward decalaration of Exception */
    typedef int ExceptionCode;
}

namespace kikin {
    
    /* Forward decalaration of Entity */
    class HtmlEntity;
    
    /* Forward decalaration of Token */
    class HtmlToken;
    
namespace annotation {
    
/* Tries to find the address micformat for the passed token. */
class AddressMicroformat {
    
public:
    
    /* 
     * Analyzes the token and if address microformat is found, 
     * returns the address entity.
     */
    static WTF::PassRefPtr<HtmlEntity> analyzeToken(HtmlToken* token);
    
private:
    
    /* Don't allow constructors for the class. */
    AddressMicroformat();
    ~AddressMicroformat();
    
    /* Creates the list of address microformat types. */
    static void createTypesList();
    
    /* Checks if the element has given class name. */
    static bool hasClass(WebCore::Element* element, WTF::String& className);
    
    /* Checks if the element has given property. */
    static bool hasItemProp(WebCore::Element* element, WTF::String& itemPropName);
    
    /* Checks if the node is a given type of address microformat. */
    static bool hasType(WebCore::Node* node, WTF::String& type);
    
    /* Checks if the node has any type of address microformat. */
    static bool hasAddressMicroformat(WebCore::Node* node);
    
    /* 
     * Returns all the parent node which matches the passed CSSSelectorList. 
     * If shouldCheckImmediateParentOnly is true, then it checkes for immediate parent only. 
     */
    static WTF::PassRefPtr<WebCore::StaticNodeList> createParentSelectorNodeList(WebCore::Node* node, const WebCore::CSSSelectorList& querySelectorList, bool shouldCheckImmediateParentOnly);
    
    /* 
     * Returns all the parent node which matches the passed selectors. 
     * If shouldCheckImmediateParentOnly is true, then it checkes for immediate parent only. 
     */
    static WTF::PassRefPtr<WebCore::NodeList> parentQuerySelectorAll(WebCore::Node* node, const WTF::String& selectors, WebCore::ExceptionCode& ec, bool shouldCheckImmediateParentOnly);
    
    /* Returns all the child nodes with microformat address type. */
    static WTF::PassRefPtr<WebCore::NodeList> getAllMicroformatsElements(WebCore::Node* node);
    
    /* Returns all the parent nodes with microformat address type. */
    static WTF::PassRefPtr<WebCore::NodeList> getTopMicroformatElements(WebCore::Node* node);
    
private:
    
    /* Types of microformat addresses. */
    static WTF::Vector<WTF::String> s_types;
    
};

}   // namespace annotation
    
}   // namespace kikin

#endif // KikinAddressMicroformatAnnotation_h
