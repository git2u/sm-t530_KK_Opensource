/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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

#ifndef WBMPImageDecoder_h
#define WBMPImageDecoder_h
#if defined(SBROWSER_WBMP_SUPPORT)

#include "core/platform/image-decoders/ImageDecoder.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

    // This class decodes the WBMP image format.
    class WBMPImageDecoder : public ImageDecoder {
    public:
        WBMPImageDecoder(ImageSource::AlphaOption, ImageSource::GammaAndColorProfileOption);
        virtual ~WBMPImageDecoder();
		virtual void setData(SharedBuffer*, bool allDataReceived);
        // ImageDecoder
        virtual String filenameExtension() const { return "wbmp"; }
        virtual bool isSizeAvailable();
        virtual bool setSize(unsigned width, unsigned height);
        virtual ImageFrame* frameBufferAtIndex(size_t index);
		virtual bool setFailed();

    private:
	    size_t m_width;
		size_t m_height;
#if defined(SBROWSER_WBMP_CORRUPTION_FIX)
		bool m_allDataReceived;
#endif
        // Decodes the image.  If |onlySize| is true, stops decoding after
        // calculating the image size.  If decoding fails but there is no more
        // data coming, sets the "decode failure" flag.
        bool decode(bool onlySize);
    };

} // namespace WebCore
#endif //#if defined(SBROWSER_WBMP_SUPPORT)

#endif
