/*
* Copyright (c) 2013, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#if defined(SBROWSER_QCOM_DOM_CACHED_SELECTOR)
#include "SelectorQueryResultsCache.h"

#include "Node.h"
#include "NodeList.h"

#include <wtf/text/AtomicString.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

SelectorQueryResultsCache* SelectorQueryResultsCache::getInstance() {
    static SelectorQueryResultsCache *instance = 0;
    if (!instance)
        instance = new SelectorQueryResultsCache();
    return instance;
}

SelectorQueryResultsCache::SelectorQueryResultsCache() {
    for (int i = 0; i < SELECTOR_QUERY_RESULTS_CACHE_SIZE; i++){
        items[i] = NULL;
    }
}

SelectorQueryResultsCache::~SelectorQueryResultsCache() {
    invalidateAll();
    for (int i = 0; i < SELECTOR_QUERY_RESULTS_CACHE_SIZE; i++){
        if (items[i]){
            delete items[i];
        }
    }
}

PassRefPtr<NodeList> SelectorQueryResultsCache::addQuery(const Node *node, const AtomicString& selectors, PassRefPtr<NodeList> nodeList) {
    unsigned int hashIdx = SelectorQueryResultsCache::getHashIdx(node, selectors);
    SelectorQueryResultsCache::Item *item = items[hashIdx];

    if (!item)
        item = items[hashIdx] = new Item;
    item->node = node;
    item->selectors = selectors.string();
    item->nodeList = RefPtr<NodeList>(nodeList);

    RefPtr<NodeList> tmp = item->nodeList;
    return tmp.release();
}

PassRefPtr<NodeList> SelectorQueryResultsCache::getQuery(const Node *node, const AtomicString& selectors) {
    //Caller must guarantee node is non-null
    unsigned int hashIdx = SelectorQueryResultsCache::getHashIdx(node, selectors);
    SelectorQueryResultsCache::Item *item = items[hashIdx];

    if (!item || !node->isSameNode(item->node) || (item->selectors != selectors.string()))
        return 0;
    RefPtr<NodeList> tmp = item->nodeList;
    return tmp.release();
}

void SelectorQueryResultsCache::invalidateAll() {
    for (int i = 0; i < SELECTOR_QUERY_RESULTS_CACHE_SIZE; i++) {
        SelectorQueryResultsCache::Item *item = items[i];
        if (item) {
            item->node = 0;
            item->nodeList = 0;
        }
    }
}

unsigned int SelectorQueryResultsCache::getHashIdx(const Node *node, const AtomicString& selectors) {
    unsigned int hashKey = 0;
    const String s = selectors.string();
    for (unsigned int i = 0; i < s.length(); i++)
        hashKey = hashKey * 31 + s[i];
    return hashKey & (SELECTOR_QUERY_RESULTS_CACHE_SIZE - 1); //faster than modulo
    //TODO hash hashKey with node address >> something based on the size of node to get the significant bits into [0:8]
}

} //namespace
#endif // #if defined(SBROWSER_QCOM_DOM_CACHED_SELECTOR)