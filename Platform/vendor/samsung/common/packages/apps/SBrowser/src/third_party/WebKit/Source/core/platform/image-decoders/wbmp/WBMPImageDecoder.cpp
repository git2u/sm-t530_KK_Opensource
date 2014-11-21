/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
 
#include "config.h"
#if defined(SBROWSER_WBMP_SUPPORT)
#include "WBMPImageDecoder.h"

#include <algorithm>
#include <wtf/PassOwnPtr.h>

typedef unsigned char  uint8_t;
#define wbmpAlign8(x)     (((x) + 7) & ~7)

namespace WebCore {

WBMPImageDecoder::WBMPImageDecoder(ImageSource::AlphaOption alphaOption,
                                 ImageSource::GammaAndColorProfileOption gammaAndColorProfileOption)
    : ImageDecoder(alphaOption, gammaAndColorProfileOption)
{
}

WBMPImageDecoder::~WBMPImageDecoder()
{
}
    
static void expand_bits_to_bytes( uint8_t  dst[],  const uint8_t  src[],  int bits)
{
    int bytes = bits >> 3;
    
    for (int i = 0; i < bytes; i++) {
        unsigned mask = *src++;
        dst[0] = (mask >> 7) & 1;
        dst[1] = (mask >> 6) & 1;
        dst[2] = (mask >> 5) & 1;
        dst[3] = (mask >> 4) & 1;
        dst[4] = (mask >> 3) & 1;
        dst[5] = (mask >> 2) & 1;
        dst[6] = (mask >> 1) & 1;
        dst[7] = (mask >> 0) & 1;
        dst += 8;
    }
    
    bits &= 7;
    if (bits > 0) {
        unsigned mask = *src;
        do {
            *dst++ = (mask >> 7) & 1;;
            mask <<= 1;
        } while (--bits != 0);    
    }
}


void WBMPImageDecoder::setData(SharedBuffer* data, bool allDataReceived)
{
#if defined(SBROWSER_WBMP_CORRUPTION_FIX)
	m_allDataReceived = allDataReceived;
	if(allDataReceived)
    	ImageDecoder::setData(data, allDataReceived);
#else
	ImageDecoder::setData(data, allDataReceived);
#endif
}

bool WBMPImageDecoder::decode( bool onlysize)
{
   if(onlysize 
#if defined(SBROWSER_WBMP_CORRUPTION_FIX)
	|| !m_allDataReceived
#endif	
   )
	return false;
	const SharedBuffer& data = *m_data;
	unsigned l_size = data.size();
	if(l_size == 0)
		return setFailed();
	const char* l_data = data.data();
    //const char* src = l_data+4;
    size_t srcRB = wbmpAlign8(m_width) >> 3;
  	ImageFrame& buffer = m_frameBufferCache[0];
    buffer.setHasAlpha(false);
    ASSERT(buffer.status() != ImageFrame::FrameComplete);
    if (buffer.status() == ImageFrame::FrameEmpty) {
        if (!buffer.setSizeForWBMP(m_width,m_height))
            return setFailed();
        buffer.setStatus(ImageFrame::FramePartial);
        buffer.setHasAlpha(false); 
    }
	const char* src = l_data + l_size - (srcRB * m_height);
    unsigned char* dest = (unsigned char*)buffer.getAddr8(0,0);
    for (unsigned int y = 0; y < m_height; y++)
    {
        expand_bits_to_bytes(dest, (unsigned char*)src, m_width);
        dest += buffer.rowBytes();
        src += srcRB;
    }

    if (!m_frameBufferCache.isEmpty())
        m_frameBufferCache[0].setStatus(ImageFrame::FrameComplete);

    return true;
	
}

ImageFrame* WBMPImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index)
        return 0;

    if (m_frameBufferCache.isEmpty()) {
        m_frameBufferCache.resize(1);
        m_frameBufferCache[0].setPremultiplyAlpha(m_premultiplyAlpha);
    }

    ImageFrame& frame = m_frameBufferCache[0];
    if (frame.status() != ImageFrame::FrameComplete)
        decode(false);
    return &frame;
	
}
bool WBMPImageDecoder::isSizeAvailable()
{    
	if (!ImageDecoder::isSizeAvailable())
         decode(true);

    return ImageDecoder::isSizeAvailable();
}
bool WBMPImageDecoder::setSize(unsigned width, unsigned height)
{
    if (!ImageDecoder::setSize(width, height))
        return false;
	m_width = width;
	m_height =  height;
    prepareScaleDataIfNecessary();
    return true;
}

bool WBMPImageDecoder::setFailed()
{
   return ImageDecoder::setFailed();
}

} //namespace
#endif //#if defined(SBROWSER_WBMP_SUPPORT)