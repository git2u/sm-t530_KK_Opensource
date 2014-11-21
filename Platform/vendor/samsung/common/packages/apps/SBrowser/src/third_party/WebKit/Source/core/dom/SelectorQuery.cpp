/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/SelectorQuery.h"

#include "core/css/CSSParser.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorCheckerFastPath.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/dom/Document.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/StyledElement.h"

namespace WebCore {

void SelectorDataList::initialize(const CSSSelectorList& selectorList)
{
    ASSERT(m_selectors.isEmpty());

    unsigned selectorCount = 0;
    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
        selectorCount++;

    m_selectors.reserveInitialCapacity(selectorCount);
    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
        m_selectors.uncheckedAppend(SelectorData(selector, SelectorCheckerFastPath::canUse(selector)));
}

inline bool SelectorDataList::selectorMatches(const SelectorData& selectorData, Element* element, const Node* rootNode) const
{
    if (selectorData.isFastCheckable && !element->isSVGElement()) {
        SelectorCheckerFastPath selectorCheckerFastPath(selectorData.selector, element);
        if (!selectorCheckerFastPath.matchesRightmostSelector(SelectorChecker::VisitedMatchDisabled))
            return false;
        return selectorCheckerFastPath.matches();
    }

#if defined(SBROWSER_QCOM_DOM_PREFETCH)
    const CSSSelector *selector = selectorData.selector;
    if (selector->m_match == CSSSelector::Tag && !SelectorChecker::tagMatches(element, selector->tagQName()))
        return false;

    if (selector->m_match == CSSSelector::Class &&
            !(element->hasClass() && static_cast<StyledElement*>(element)->classNames().contains(selector->value())))
        return false;

    if (selector->m_match == CSSSelector::Id &&
            !(element->hasID() && element->idForStyleResolution() == selector->value()))
        return false;
#endif

    SelectorChecker selectorChecker(element->document(), SelectorChecker::QueryingRules);
    SelectorChecker::SelectorCheckingContext selectorCheckingContext(selectorData.selector, element, SelectorChecker::VisitedMatchDisabled);
    selectorCheckingContext.behaviorAtBoundary = SelectorChecker::StaysWithinTreeScope;
    selectorCheckingContext.scope = !rootNode->isDocumentNode() && rootNode->isContainerNode() ? toContainerNode(rootNode) : 0;
    PseudoId ignoreDynamicPseudo = NOPSEUDO;
    return selectorChecker.match(selectorCheckingContext, ignoreDynamicPseudo, DOMSiblingTraversalStrategy()) == SelectorChecker::SelectorMatches;
}

#if defined(SBROWSER_QCOM_DOM_PREFETCH)
inline bool SelectorDataList::selectorMatches(SelectorChecker &selectorChecker, const SelectorData& selectorData, Element* element, const Node* rootNode) const
{
    if (selectorData.isFastCheckable && !element->isSVGElement()) {
        SelectorCheckerFastPath selectorCheckerFastPath(selectorData.selector, element);
        if (!selectorCheckerFastPath.matchesRightmostSelector(SelectorChecker::VisitedMatchDisabled))
            return false;
        return selectorCheckerFastPath.matches();
    }

    const CSSSelector *selector = selectorData.selector;
    if (selector->m_match == CSSSelector::Tag && !SelectorChecker::tagMatches(element, selector->tagQName()))
        return false;

    if (selector->m_match == CSSSelector::Class &&
            !(element->hasClass() && static_cast<StyledElement*>(element)->classNames().contains(selector->value())))
        return false;

    if (selector->m_match == CSSSelector::Id &&
            !(element->hasID() && element->idForStyleResolution() == selector->value()))
        return false;

    SelectorChecker::SelectorCheckingContext selectorCheckingContext(selectorData.selector, element, SelectorChecker::VisitedMatchDisabled);
    selectorCheckingContext.behaviorAtBoundary = SelectorChecker::StaysWithinTreeScope;
    selectorCheckingContext.scope = !rootNode->isDocumentNode() && rootNode->isContainerNode() ? toContainerNode(rootNode) : 0;
    PseudoId ignoreDynamicPseudo = NOPSEUDO;
    return selectorChecker.match(selectorCheckingContext, ignoreDynamicPseudo, DOMSiblingTraversalStrategy()) == SelectorChecker::SelectorMatches;
}
#endif

bool SelectorDataList::matches(Element* targetElement) const
{
    ASSERT(targetElement);
#if defined(SBROWSER_QCOM_DOM_PREFETCH)
    SelectorChecker selectorChecker(targetElement->document(), SelectorChecker::QueryingRules);
#endif
    unsigned selectorCount = m_selectors.size();
    for (unsigned i = 0; i < selectorCount; ++i) {
#if defined(SBROWSER_QCOM_DOM_PREFETCH)
        if (selectorMatches(selectorChecker, m_selectors[i], targetElement, targetElement))
#else
        if (selectorMatches(m_selectors[i], targetElement, targetElement))
#endif
            return true;
    }

    return false;
}

PassRefPtr<NodeList> SelectorDataList::queryAll(Node* rootNode) const
{
    Vector<RefPtr<Node> > result;
#if defined(SBROWSER_QCOM_DOM_PREFETCH)
    // Only use or create cache if we are traversing the whole tree
    if (rootNode->isDocumentNode() && Node::IsArchitectureCompatible()) {
        if (rootNode->nextElement())
            executeCached<false>(rootNode, result);
        else
            executeUncached<false, true>(rootNode, result);
    }
    else {
        executeUncached<false, false>(rootNode, result);
    }
#else
    execute<false>(rootNode, result);
#endif
        
    return StaticNodeList::adopt(result);
}

PassRefPtr<Element> SelectorDataList::queryFirst(Node* rootNode) const
{
    Vector<RefPtr<Node> > result;
#if defined(SBROWSER_QCOM_DOM_PREFETCH)
    if (rootNode->isDocumentNode() && rootNode->nextElement())
        executeCached<true>(rootNode, result);
    else
        executeUncached<true, false>(rootNode, result);
#else
    execute<true>(rootNode, result);
#endif
    if (result.isEmpty())
        return 0;
    ASSERT(result.size() == 1);
    ASSERT(result.first()->isElementNode());
    return toElement(result.first().get());
}

#if defined(SBROWSER_DOM_PERF_IMPROVEMENT)
const CSSSelector* SelectorDataList::selectorForIdLookup(Node* rootNode) const
{
    // We need to return the matches in document order. To use id lookup while there is possiblity of multiple matches
    // we would need to sort the results. For now, just traverse the document in that case.
    if (m_selectors.size() != 1)
        return 0;
    if (!rootNode->inDocument())
        return 0;
    if (rootNode->document()->inQuirksMode())
        return 0;
    for (const CSSSelector* selector = m_selectors[0].selector; selector; selector = selector->tagHistory()) {
        if (selector->m_match == CSSSelector::Id)
            return selector;
        if (selector->relation() != CSSSelector::SubSelector)
            break;
        }
    return 0;
}
#else
bool SelectorDataList::canUseIdLookup(Node* rootNode) const
{
    // We need to return the matches in document order. To use id lookup while there is possiblity of multiple matches
    // we would need to sort the results. For now, just traverse the document in that case.
    if (m_selectors.size() != 1)
        return false;
    if (m_selectors[0].selector->m_match != CSSSelector::Id)
        return false;
    if (!rootNode->inDocument())
        return false;
    if (rootNode->document()->inQuirksMode())
        return false;
    if (rootNode->document()->containsMultipleElementsWithId(m_selectors[0].selector->value()))
        return false;
    return true;
    }
#endif

static inline bool isTreeScopeRoot(Node* node)
{
    ASSERT(node);
    return node->isDocumentNode() || node->isShadowRoot();
}

#if defined(SBROWSER_QCOM_DOM_PREFETCH)
template <bool firstMatchOnly, bool cacheElements>
ALWAYS_INLINE
void SelectorDataList::executeUncached(Node* rootNode, Vector<RefPtr<Node> >& matchedElements) const
#else
template <bool firstMatchOnly>
void SelectorDataList::execute(Node* rootNode, Vector<RefPtr<Node> >& matchedElements) const
#endif   
{
#if defined(SBROWSER_QCOM_DOM_PREFETCH)
    SelectorChecker selectorChecker(rootNode->document(), SelectorChecker::QueryingRules);
#endif

#if defined(SBROWSER_DOM_PERF_IMPROVEMENT)
    if (const CSSSelector* idSelector = selectorForIdLookup(rootNode)) { 
#else
    if (canUseIdLookup(rootNode)) {
#endif        
        ASSERT(m_selectors.size() == 1);
#if defined(SBROWSER_DOM_PERF_IMPROVEMENT)
        const AtomicString& idToMatch = idSelector->value(); 
        if (UNLIKELY(rootNode->treeScope()->containsMultipleElementsWithId(idToMatch))) { 
            const Vector<Element*>* elements = rootNode->treeScope()->getAllElementsById(idToMatch); 
            ASSERT(elements); 
            size_t count = elements->size(); 
            for (size_t i = 0; i < count; ++i) { 
                Element* element = elements->at(i); 
#if defined(SBROWSER_QCOM_DOM_PREFETCH)
                if (selectorMatches(selectorChecker, m_selectors[0], element, rootNode)) { 
#else              
                if (selectorMatches(m_selectors[0], element, rootNode)) { 
#endif
                    matchedElements.append(element); 
                    if (firstMatchOnly) 
                        return; 
                } 
            } 
            return; 
        } 
        Element* element = rootNode->treeScope()->getElementById(idToMatch); 
#else        
        const CSSSelector* selector = m_selectors[0].selector;
        Element* element = rootNode->treeScope()->getElementById(selector->value());
#endif        
        if (!element || !(isTreeScopeRoot(rootNode) || element->isDescendantOf(rootNode)))
            return;
#if defined(SBROWSER_QCOM_DOM_PREFETCH)
        if (selectorMatches(selectorChecker, m_selectors[0], element, rootNode))
#else
        if (selectorMatches(m_selectors[0], element, rootNode))
#endif
            matchedElements.append(element);
        return;
    }

#if defined(SBROWSER_QCOM_DOM_PREFETCH)   
    Node* previous = 0;
    if (cacheElements)
        previous = rootNode;
#endif
    unsigned selectorCount = m_selectors.size();

    Node* n = rootNode->firstChild();
    while (n) {
#if defined(SBROWSER_QCOM_DOM_PREFETCH)        
        n->prefetchTarget();   
#endif    
        if (n->isElementNode()) {
            Element* element = toElement(n);
#if defined(SBROWSER_QCOM_DOM_PREFETCH)           
            if (cacheElements) {
                previous->setNextElement(n);
                previous = n;
                n->setNextElement(0);
            }
#endif           
            for (unsigned i = 0; i < selectorCount; ++i) {
#if defined(SBROWSER_QCOM_DOM_PREFETCH)
                if (selectorMatches(selectorChecker, m_selectors[i], element, rootNode)) {
#else
                if (selectorMatches(m_selectors[i], element, rootNode)) {
#endif
                    matchedElements.append(element);
                    if (firstMatchOnly)
                        return;
                    break;
                }
            }
            if (element->firstChild()) {
                n = element->firstChild();
                continue;
            }
        }
        while (!n->nextSibling()) {
#if defined(SBROWSER_QCOM_DOM_PREFETCH)          
            n->prefetchTarget();            
#endif          
            n = n->parentNode();
            if (n == rootNode)
                return;
        }
        n = n->nextSibling();
    }
}

#if defined(SBROWSER_QCOM_DOM_PREFETCH)
template <bool firstMatchOnly>
ALWAYS_INLINE
void SelectorDataList::executeCached(Node* rootNode, Vector<RefPtr<Node> >& matchedElements) const
{
    SelectorChecker selectorChecker(rootNode->document(), SelectorChecker::QueryingRules);

#if defined(SBROWSER_DOM_PERF_IMPROVEMENT)
    if (const CSSSelector* idSelector = selectorForIdLookup(rootNode)) { 
#else
    if (canUseIdLookup(rootNode)) {
#endif 
        ASSERT(m_selectors.size() == 1);
#if defined(SBROWSER_DOM_PERF_IMPROVEMENT)
        const AtomicString& idToMatch = idSelector->value(); 
        if (UNLIKELY(rootNode->treeScope()->containsMultipleElementsWithId(idToMatch))) { 
            const Vector<Element*>* elements = rootNode->treeScope()->getAllElementsById(idToMatch); 
            ASSERT(elements); 
            size_t count = elements->size(); 
            for (size_t i = 0; i < count; ++i) { 
                Element* element = elements->at(i); 
                if (selectorMatches(m_selectors[0], element, rootNode)) { 
                    matchedElements.append(element); 
                    if (firstMatchOnly) 
                        return; 
                } 
            } 
            return; 
        } 
        Element* element = rootNode->treeScope()->getElementById(idToMatch); 
#else        
        const CSSSelector* selector = m_selectors[0].selector;
        Element* element = rootNode->treeScope()->getElementById(selector->value());
#endif        
        if (!element || !(isTreeScopeRoot(rootNode) || element->isDescendantOf(rootNode)))
            return;
        if (selectorMatches(selectorChecker, m_selectors[0], element, rootNode))
            matchedElements.append(element);
        return;
    }

    unsigned selectorCount = m_selectors.size();
    if (1 == selectorCount) {
        Node* n = rootNode->nextElement();
        while (n) {
            Node* nextElement = n->nextElement();
#if COMPILER_SUPPORTS(BUILTIN_PREFETCH)
            if (LIKELY(nextElement != 0)) {
                __builtin_prefetch((char *)nextElement);
                __builtin_prefetch(((char *)nextElement) + 64);
            }
#endif
            ASSERT(n->isElementNode());
            Element* element = static_cast<Element*>(n);
            if (selectorMatches(selectorChecker, m_selectors[0], element, rootNode)) {
                matchedElements.append(element);
                if (firstMatchOnly)
                    return;
            }
            n = nextElement;
        }
    }
    else {
        Node* n = rootNode->nextElement();
        while (n) {
            Node* nextElement = n->nextElement();
#if COMPILER_SUPPORTS(BUILTIN_PREFETCH)
            if (LIKELY(nextElement != 0)) {
                __builtin_prefetch((char *)nextElement);
                __builtin_prefetch(((char *)nextElement) + 64);
            }
#endif
            ASSERT(n->isElementNode());
            Element* element = static_cast<Element*>(n);
            for (unsigned i = 0; i < selectorCount; ++i) {
                if (selectorMatches(selectorChecker, m_selectors[i], element, rootNode)) {
                    matchedElements.append(element);
                    if (firstMatchOnly)
                        return;
                    break;
                }
            }
            n = nextElement;
        }
    }
}
#endif

SelectorQuery::SelectorQuery(const CSSSelectorList& selectorList)
    : m_selectorList(selectorList)
{
    m_selectors.initialize(m_selectorList);
}

bool SelectorQuery::matches(Element* element) const
{
    return m_selectors.matches(element);
}

PassRefPtr<NodeList> SelectorQuery::queryAll(Node* rootNode) const
{
    return m_selectors.queryAll(rootNode);
}

PassRefPtr<Element> SelectorQuery::queryFirst(Node* rootNode) const
{
    return m_selectors.queryFirst(rootNode);
}

SelectorQuery* SelectorQueryCache::add(const AtomicString& selectors, Document* document, ExceptionCode& ec)
{
    HashMap<AtomicString, OwnPtr<SelectorQuery> >::iterator it = m_entries.find(selectors);
    if (it != m_entries.end())
        return it->value.get();

    CSSParser parser(document);
    CSSSelectorList selectorList;
    parser.parseSelector(selectors, selectorList);

    if (!selectorList.first() || selectorList.hasInvalidSelector()) {
        ec = SYNTAX_ERR;
        return 0;
    }

    // throw a NAMESPACE_ERR if the selector includes any namespace prefixes.
    if (selectorList.selectorsNeedNamespaceResolution()) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    const int maximumSelectorQueryCacheSize = 256;
    if (m_entries.size() == maximumSelectorQueryCacheSize)
        m_entries.remove(m_entries.begin());
    
    OwnPtr<SelectorQuery> selectorQuery = adoptPtr(new SelectorQuery(selectorList));
    SelectorQuery* rawSelectorQuery = selectorQuery.get();
    m_entries.add(selectors, selectorQuery.release());
    return rawSelectorQuery;
}

void SelectorQueryCache::invalidate()
{
    m_entries.clear();
}

}
