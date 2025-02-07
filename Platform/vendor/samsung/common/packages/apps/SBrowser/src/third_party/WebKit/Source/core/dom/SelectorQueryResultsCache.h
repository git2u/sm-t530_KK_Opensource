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

#ifndef SelectorQueryResultsCache_h
#define SelectorQueryResultsCache_h

#include "Node.h"

#if defined(SBROWSER_QCOM_DOM_CACHED_SELECTOR)

#include <wtf/text/AtomicString.h>

namespace WebCore {

#define SELECTOR_QUERY_RESULTS_CACHE_SIZE 64 //keep a power of 2, or you MUST modify getHashIdx() to use modulo

class NodeList;

class SelectorQueryResultsCache {
    public:
        static SelectorQueryResultsCache* getInstance();

        PassRefPtr<NodeList> addQuery(const Node *node, const AtomicString& selectors, PassRefPtr<NodeList> nodeList);
        PassRefPtr<NodeList> getQuery(const Node *node, const AtomicString& selectors);
        void invalidateAll();

    private:
        struct Item {
            const Node *node;
            String selectors;
            RefPtr<NodeList> nodeList;
        };
        struct Item *items[SELECTOR_QUERY_RESULTS_CACHE_SIZE];

        SelectorQueryResultsCache();
        ~SelectorQueryResultsCache();

        static unsigned int getHashIdx(const Node *node, const AtomicString& selectors);
};

} //namespace

#endif // #if defined(SBROWSER_QCOM_DOM_CACHED_SELECTOR)

#endif /* SelectorQueryResultsCache_h */