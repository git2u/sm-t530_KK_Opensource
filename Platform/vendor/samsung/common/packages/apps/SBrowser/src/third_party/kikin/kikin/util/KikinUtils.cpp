/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#define LOG_TAG "kikin_utils"

#include "config.h"
#include "KikinUtils.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeList.h"

#define LOGE_KIKIN(...) android_printLog(ANDROID_LOG_DEBUG, "Kikin_Utils", __VA_ARGS__)

using namespace WebCore;

namespace kikin { namespace util {
    
    
// static 
Node* KikinUtils::getNextTextNode(Node* node, bool acrossLinks)
{
//    if (node)
//        LOGD("Node name:%ls. %d", node->nodeName().utf8().data(), __LINE__);
    
    if (!node || !node->parentNode() || (node->nodeName().lower() == "a" && !acrossLinks))
        return NULL;
    
    RefPtr<NodeList> childNodes = node->parentNode()->childNodes();
    int idx = -1;
    
    for (unsigned i = 0; i < childNodes->length(); i++) {
        if (node->isSameNode(childNodes->item(i))) {
            idx = i;
            break;
        }
    }
    
    Node* pnode = NULL;
    if (idx < static_cast<int>(childNodes->length() - 1)) {
        pnode = childNodes->item(idx + 1);
        while (pnode->hasChildNodes()) {
            pnode = pnode->firstChild();
        }
    } else {
        pnode = KikinUtils::getNextTextNode(static_cast<Node*>(node->parentNode()), acrossLinks);
    }
    
//    childNodes.clear();
    return pnode;
}
    
// static 
Node* KikinUtils::getPreviousTextNode(Node* node, bool acrossLinks)
{
//    if (node)
//        LOGD("Node name:%ls. %d", node->nodeName().utf8().data(), __LINE__);
    
    if (!node || !node->parentNode() || (node->nodeName().lower() == "a" && !acrossLinks))
        return NULL;
    
    RefPtr<NodeList> childNodes = node->parentNode()->childNodes();
    int idx = -1;
    
    for (unsigned i = 0; i < childNodes->length(); i++) {
        if (node->isSameNode(childNodes->item(i))) {
            idx = i;
            break;
        }
    }
    
    Node* pnode = NULL;
    if (idx > 0) {
        pnode = childNodes->item(idx - 1);
        while (pnode->hasChildNodes()) {
            pnode = pnode->lastChild();
        }
        
    } else {
        pnode = KikinUtils::getPreviousTextNode(static_cast<Node*>(node->parentNode()), acrossLinks);
    }
    
//    childNodes.clear();
    return pnode;
}
    
// static 
Vector<Node*> KikinUtils::getTextNodesBetween(Node* parentNode, Node* startNode, Node* endNode)
{
    Vector<Node*> ret;
    
    if (!startNode || !endNode) {
//        LOGD("Valid start node: %s, %d", (startNode ? "true" : "false"), __LINE__);
//        LOGD("Valid end node: %s, %d", (endNode ? "true" : "false"), __LINE__);
//        LOGD("Returning emoty node list. %d", __LINE__);
        return ret;
    }
    
    Node* nextNode = startNode;
    while (nextNode) {
        ret.append(nextNode);
        
        if (nextNode->isSameNode(endNode)) {
//            LOGD("We have reached the end node, break the loop. %d", __LINE__);
            break;
        }
        
        nextNode = KikinUtils::getNextTextNode(nextNode, true);
        
//        if (!nextNode) {
//            LOGD("Valid text node is not found breaking the loop. %d", __LINE__);
//        }
    }
    
    return ret;
}
    
// static 
WebCore::Node* KikinUtils::findLeaf(WebCore::Node* node, bool firstOrLast)
{
    while (node && node->hasChildNodes()) {
        node = (firstOrLast ? node->firstChild() : node->lastChild());
    }
    
    return node;
}

// static
bool KikinUtils::isChildNode(WebCore::Element* rootNode, WebCore::Element* node)
{
//    if (node) {
//        LOGE_KIKIN("Node name:%ls. %d", node->tagName().utf8().data(), __LINE__);
//    }
//
//    if (rootNode) {
//        LOGE_KIKIN("Root Node name:%ls. %d", rootNode->tagName().utf8().data(), __LINE__);
//    }

    bool isParentNode = node->parentElement()->isSameNode(rootNode);
//    LOGE_KIKIN("Is parent node:%s. %d", (isParentNode ? "true" : "false"), __LINE__);
    return isParentNode;
}

} /* namespace util */
    
} /* namespace kikin */
