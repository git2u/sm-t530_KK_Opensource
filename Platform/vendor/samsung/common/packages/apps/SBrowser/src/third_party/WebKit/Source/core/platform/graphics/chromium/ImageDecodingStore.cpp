/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/graphics/chromium/ImageDecodingStore.h"

#include "core/platform/SharedBuffer.h"
#include "core/platform/chromium/TraceEvent.h"
#include "core/platform/graphics/chromium/ImageFrameGenerator.h"
#include "core/platform/graphics/chromium/ScaledImageFragment.h"

namespace WebCore {

namespace {

// 32MB memory limit for cache.
static const size_t defaultCacheLimitInBytes = 32768 * 1024;
static ImageDecodingStore* s_instance = 0;

static void setInstance(ImageDecodingStore* imageDecodingStore)
{
    delete s_instance;
    s_instance = imageDecodingStore;
}

} // namespace

ImageDecodingStore::ImageDecodingStore()
    : m_cacheLimitInBytes(defaultCacheLimitInBytes)
    , m_memoryUsageInBytes(0)
{
}

ImageDecodingStore::~ImageDecodingStore()
{
#ifndef NDEBUG
    setCacheLimitInBytes(0);
    ASSERT(!m_cacheMap.size());
    ASSERT(!m_orderedCacheList.size());
    ASSERT(!m_cachedSizeMap.size());
#endif
}

ImageDecodingStore* ImageDecodingStore::instance()
{
    return s_instance;
}

void ImageDecodingStore::initializeOnce()
{
    setInstance(ImageDecodingStore::create().leakPtr());
}

void ImageDecodingStore::shutdown()
{
    setInstance(0);
}

bool ImageDecodingStore::lockCache(const ImageFrameGenerator* generator, const SkISize& scaledSize, CacheCondition condition, const ScaledImageFragment** cachedImage, ImageDecoder** decoder)
{
    ASSERT(cachedImage);

    CacheEntry* cacheEntry = 0;
    Vector<OwnPtr<CacheEntry> > cacheEntriesToDelete;
    {
        MutexLocker lock(m_mutex);
        CacheMap::iterator iter = m_cacheMap.find(std::make_pair(generator, scaledSize));
        if (iter == m_cacheMap.end())
            return false;
        cacheEntry = iter->value.get();
        ScaledImageFragment* image = cacheEntry->cachedImage();
        if (condition == CacheMustBeComplete && !image->isComplete())
            return false;

        // Incomplete cache entry cannot be used more than once.
        ASSERT(image->isComplete() || !cacheEntry->useCount());

        image->bitmap().lockPixels();
        if (image->bitmap().getPixels()) {
            // Increment use count such that it doesn't get evicted.
            cacheEntry->incrementUseCount();

            // Complete cache entry doesn't have a decoder.
            ASSERT(!image->isComplete() || !cacheEntry->cachedDecoder());

            if (decoder)
                *decoder = cacheEntry->cachedDecoder();
            *cachedImage = image;
        } else {
            image->bitmap().unlockPixels();
            removeFromCacheInternal(cacheEntry, &cacheEntriesToDelete);
            removeFromCacheListInternal(cacheEntriesToDelete);
            return false;
        }
    }

    return true;
}

void ImageDecodingStore::unlockCache(const ImageFrameGenerator* generator, const ScaledImageFragment* cachedImage)
{
    MutexLocker lock(m_mutex);
    cachedImage->bitmap().unlockPixels();
    CacheMap::iterator iter = m_cacheMap.find(std::make_pair(generator, cachedImage->scaledSize()));
    ASSERT(iter != m_cacheMap.end());

    CacheEntry* cacheEntry = iter->value.get();
    cacheEntry->decrementUseCount();

    // Put the entry to the end of list.
    m_orderedCacheList.remove(cacheEntry);
    m_orderedCacheList.append(cacheEntry);
}

const ScaledImageFragment* ImageDecodingStore::insertAndLockCache(const ImageFrameGenerator* generator, PassOwnPtr<ScaledImageFragment> image, PassOwnPtr<ImageDecoder> decoder)
{
    // Prune old cache entries to give space for the new one.
    prune();

    ScaledImageFragment* cachedImage = image.get();
    OwnPtr<CacheEntry> newCacheEntry;

    // ImageDecoder is not used any more if cache is complete.
    if (image->isComplete())
        newCacheEntry = CacheEntry::createAndUse(generator, image);
    else
        newCacheEntry = CacheEntry::createAndUse(generator, image, decoder);

    MutexLocker lock(m_mutex);
    // Lock the underlying SkBitmap to prevent it from being purged.
    cachedImage->bitmap().lockPixels();
    ASSERT(!m_cacheMap.contains(newCacheEntry->cacheKey()));
    insertCacheInternal(newCacheEntry.release());
    return cachedImage;
}

// SAMSUNG CHANGE : Apply opensoure 16432002 >>
bool ImageDecodingStore::isCached(const ImageFrameGenerator* generator, const SkISize& scaledSize)
{
    MutexLocker lock(m_mutex);
    CacheMap::iterator iter = m_cacheMap.find(std::make_pair(generator, scaledSize));
    if (iter == m_cacheMap.end())
        return false;
    return true;
}
// SAMSUNG CHANGE : Apply opensoure 16432002 <<

const ScaledImageFragment* ImageDecodingStore::overwriteAndLockCache(const ImageFrameGenerator* generator, const ScaledImageFragment* cachedImage, PassOwnPtr<ScaledImageFragment> newImage)
{
    OwnPtr<ImageDecoder> trash;
    const ScaledImageFragment* newCachedImage = 0;
    {
        MutexLocker lock(m_mutex);
        cachedImage->bitmap().unlockPixels();
        CacheMap::iterator iter = m_cacheMap.find(std::make_pair(generator, cachedImage->scaledSize()));
        ASSERT(iter != m_cacheMap.end());

        CacheEntry* cacheEntry = iter->value.get();
        ASSERT(cacheEntry->useCount() == 1);
        ASSERT(!cacheEntry->cachedImage()->isComplete());

        bool isNewImageDiscardable = DiscardablePixelRef::isDiscardable(newImage->bitmap().pixelRef());
        if (cacheEntry->isDiscardable() && !isNewImageDiscardable)
            incrementMemoryUsage(cacheEntry->memoryUsageInBytes());
        else if (!cacheEntry->isDiscardable() && isNewImageDiscardable)
            decrementMemoryUsage(cacheEntry->memoryUsageInBytes());
        trash = cacheEntry->overwriteCachedImage(newImage);
        newCachedImage = cacheEntry->cachedImage();
        // Lock the underlying SkBitmap to prevent it from being purged.
        newCachedImage->bitmap().lockPixels();
    }

    return newCachedImage;
}

void ImageDecodingStore::removeCacheIndexedByGenerator(const ImageFrameGenerator* generator)
{
    Vector<OwnPtr<CacheEntry> > cacheEntriesToDelete;
    {
        MutexLocker lock(m_mutex);
        CachedSizeMap::iterator iter = m_cachedSizeMap.find(generator);
        if (iter == m_cachedSizeMap.end())
            return;

        // Get all cached sizes indexed by generator.
        Vector<SkISize> cachedSizeList;
        copyToVector(iter->value, cachedSizeList);

        // For each cached size find the corresponding CacheEntry and remove it from
        // m_cacheMap.
        for (size_t i = 0; i < cachedSizeList.size(); ++i) {
            CacheIdentifier key = std::make_pair(generator, cachedSizeList[i]);
            ASSERT(m_cacheMap.contains(key));

            const CacheEntry* cacheEntry = m_cacheMap.get(key);
            ASSERT(!cacheEntry->useCount());
            removeFromCacheInternal(cacheEntry, &cacheEntriesToDelete);
        }

        // Remove from cache list as well.
        removeFromCacheListInternal(cacheEntriesToDelete);
    }
}

void ImageDecodingStore::setCacheLimitInBytes(size_t cacheLimit)
{
    {
        MutexLocker lock(m_mutex);
        m_cacheLimitInBytes = cacheLimit;
    }
    prune();
}

size_t ImageDecodingStore::memoryUsageInBytes()
{
    MutexLocker lock(m_mutex);
    return m_memoryUsageInBytes;
}

unsigned ImageDecodingStore::cacheEntries()
{
    MutexLocker lock(m_mutex);
    return m_cacheMap.size();
}

void ImageDecodingStore::prune()
{
    TRACE_EVENT0("webkit", "ImageDecodingStore::prune");

    Vector<OwnPtr<CacheEntry> > cacheEntriesToDelete;
    {
        MutexLocker lock(m_mutex);

        // Head of the list is the least recently used entry.
        const CacheEntry* cacheEntry = m_orderedCacheList.head();

        // Walk the list of cache entries starting from the least recently used
        // and then keep them for deletion later.
        while (cacheEntry && (m_memoryUsageInBytes > m_cacheLimitInBytes || !m_cacheLimitInBytes)) {
            // Cache is not used; Remove it.
            if (!cacheEntry->useCount())
                removeFromCacheInternal(cacheEntry, &cacheEntriesToDelete);
            cacheEntry = cacheEntry->next();
        }

        // Remove from cache list as well.
        removeFromCacheListInternal(cacheEntriesToDelete);
    }
}

void ImageDecodingStore::insertCacheInternal(PassOwnPtr<CacheEntry> cacheEntry)
{
    if (!cacheEntry->isDiscardable())
        incrementMemoryUsage(cacheEntry->memoryUsageInBytes());
    TRACE_COUNTER1("webkit", "ImageDecodingStoreMemoryUsageBytes", m_memoryUsageInBytes);

    // m_orderedCacheList is used to support LRU operations to reorder cache
    // entries quickly.
    m_orderedCacheList.append(cacheEntry.get());

    CacheIdentifier key = cacheEntry->cacheKey();
    // m_cacheMap is used for indexing and quick lookup of a cached image. It owns
    // all cache entries.
    m_cacheMap.add(key, cacheEntry);
    TRACE_COUNTER1("webkit", "ImageDecodingStoreNumOfEntries", m_cacheMap.size());

    // m_cachedSizeMap keeps all scaled sizes associated with an ImageFrameGenerator.
    CachedSizeMap::AddResult result = m_cachedSizeMap.add(key.first, SizeSet());
    result.iterator->value.add(key.second);
}

void ImageDecodingStore::removeFromCacheInternal(const CacheEntry* cacheEntry, Vector<OwnPtr<CacheEntry> >* deletionList)
{
    if (!cacheEntry->isDiscardable())
        decrementMemoryUsage(cacheEntry->memoryUsageInBytes());
    TRACE_COUNTER1("webkit", "ImageDecodingStoreMemoryUsageBytes", m_memoryUsageInBytes);

    // Remove from m_cacheMap.
    CacheIdentifier key = cacheEntry->cacheKey();
    deletionList->append(m_cacheMap.take(key));
    TRACE_COUNTER1("webkit", "ImageDecodingStoreNumOfEntries", m_cacheMap.size());

    // Remove from m_cachedSizeMap.
    CachedSizeMap::iterator iter = m_cachedSizeMap.find(key.first);
    ASSERT(iter != m_cachedSizeMap.end());
    iter->value.remove(key.second);
    if (!iter->value.size())
        m_cachedSizeMap.remove(iter);
}

void ImageDecodingStore::removeFromCacheListInternal(const Vector<OwnPtr<CacheEntry> >& deletionList)
{
    for (size_t i = 0; i < deletionList.size(); ++i)
        m_orderedCacheList.remove(deletionList[i].get());
}

} // namespace WebCore
