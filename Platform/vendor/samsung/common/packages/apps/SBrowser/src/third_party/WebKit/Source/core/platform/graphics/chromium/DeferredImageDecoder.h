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

#ifndef DeferredImageDecoder_h
#define DeferredImageDecoder_h

#include "SkBitmap.h"
#include "core/platform/graphics/ImageSource.h"
#include "core/platform/graphics/IntSize.h"
#include "core/platform/image-decoders/ImageDecoder.h"
#include <wtf/Forward.h>

namespace WebCore {

class ImageFrameGenerator;
class SharedBuffer;

class DeferredImageDecoder {
public:
    ~DeferredImageDecoder();
    static PassOwnPtr<DeferredImageDecoder> create(const SharedBuffer& data, ImageSource::AlphaOption, ImageSource::GammaAndColorProfileOption);

    static PassOwnPtr<DeferredImageDecoder> createForTesting(PassOwnPtr<ImageDecoder>);

    static bool isLazyDecoded(const SkBitmap&);

    static SkBitmap createResizedLazyDecodingBitmap(const SkBitmap&, const SkISize& scaledSize, const SkIRect& scaledSubset);

    static void setEnabled(bool);

#if defined(SBROWSER_POWER_SAVER_MODE_IMPL)
    static void setPowerSaverModeEnabled(bool);
    static void setDisableProgressiveImageDecoding(bool);
    static void setDisableGIFAnimation(bool);
#endif
    String filenameExtension() const;

    ImageFrame* frameBufferAtIndex(size_t index);

    void setData(SharedBuffer* data, bool allDataReceived);

    bool isSizeAvailable();
    IntSize size() const;
    IntSize frameSizeAtIndex(size_t index) const;
    size_t frameCount();
    int repetitionCount() const;
    void clearFrameBufferCache(size_t);
    bool frameHasAlphaAtIndex(size_t index) const;
    bool frameIsCompleteAtIndex(size_t) const;
    float frameDurationAtIndex(size_t) const;
    unsigned frameBytesAtIndex(size_t index) const;
    ImageOrientation orientation() const;
    bool hotSpot(IntPoint&) const;
#if defined(SBROWSER_POWER_SAVER_MODE_IMPL)
    static bool s_DisableProgressiveImageDecoding;
    static bool s_DisableGIFAnimation;
#endif
private:
    explicit DeferredImageDecoder(PassOwnPtr<ImageDecoder> actualDecoder);
    SkBitmap createLazyDecodingBitmap();
    void setData(PassRefPtr<SharedBuffer>, bool allDataReceived);

    RefPtr<SharedBuffer> m_data;
    bool m_allDataReceived;
    OwnPtr<ImageDecoder> m_actualDecoder;

    String m_filenameExtension;
    IntSize m_size;
    ImageOrientation m_orientation;

    ImageFrame m_lazyDecodedFrame;
    RefPtr<ImageFrameGenerator> m_frameGenerator;

    static bool s_enabled;
};

} // namespace WebCore

#endif
