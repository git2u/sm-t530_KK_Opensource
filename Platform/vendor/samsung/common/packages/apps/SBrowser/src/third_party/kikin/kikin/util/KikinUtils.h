/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinUtils_h
#define KikinUtils_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {
    
    /* Forward declaration of Node. */
    class Node;

    /* Forward declaration of Element. */
    class Element;
}

namespace kikin { namespace util {
    
/* Helper class for finding the text nodes. */
class KikinUtils {

public:
    
    /* Returns the next text node. */
    static WebCore::Node* getNextTextNode(WebCore::Node* node, bool acrossLinks);
    
    /* Returns the previous text node. */
    static WebCore::Node* getPreviousTextNode(WebCore::Node* node, bool acrossLinks);
    
    /* Returns the leaf node of the element. */
    static WebCore::Node* findLeaf(WebCore::Node* element, bool firstOrLast);
    
    /* Returns the text nodes between the nodes. */
    static WTF::Vector<WebCore::Node*> getTextNodesBetween(WebCore::Node* parentNode, WebCore::Node* startNode, WebCore::Node* endNode);

    /** Checks whether given node is a direct or indirect child node of the root node. */
    static bool isChildNode(WebCore::Element* rootNode, WebCore::Element* node);
    
private:
    
    /* Don't allow constructor */
    KikinUtils() {}
    ~KikinUtils() {}
    
};

}   // namespace util
    
}   // namespace kikin

#endif // KikinUtils_h
