/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
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

#ifndef StringBuffer_h
#define StringBuffer_h

#include <wtf/Assertions.h>
#include <wtf/unicode/Unicode.h>
#include <limits>

namespace WTF {

template <typename CharType>
class StringBuffer {
    WTF_MAKE_NONCOPYABLE(StringBuffer);
public:
    explicit StringBuffer(unsigned length)
        : m_length(length)
    {
        RELEASE_ASSERT(m_length <= std::numeric_limits<unsigned>::max() / sizeof(CharType));
        m_data = static_cast<CharType*>(fastMalloc(m_length * sizeof(CharType)));
    }

    ~StringBuffer()
    {
        fastFree(m_data);
    }

    void shrink(unsigned newLength)
    {
        ASSERT(newLength <= m_length);
        m_length = newLength;
    }

    void resize(unsigned newLength)
    {
        if (newLength > m_length) {
            RELEASE_ASSERT(newLength <= std::numeric_limits<unsigned>::max() / sizeof(UChar));
            m_data = static_cast<UChar*>(fastRealloc(m_data, newLength * sizeof(UChar)));
        }
        m_length = newLength;
    }

    unsigned length() const { return m_length; }
    CharType* characters() { return m_data; }

    CharType& operator[](unsigned i) { ASSERT_WITH_SECURITY_IMPLICATION(i < m_length); return m_data[i]; }

    CharType* release() { CharType* data = m_data; m_data = 0; return data; }

private:
    CharType* m_data;  // Pointers first: crbug.com/232031
    unsigned m_length;
};
#if ENABLE(SBROWSER_TEXT_CODEC_MEMORY_REDUCTION)
template <typename CharType>
class StringBufferConst
{
public:
    StringBufferConst(const CharType* data, unsigned length)
        : m_length(length),
        m_constData(data)
    {
        RELEASE_ASSERT(m_length <= std::numeric_limits<unsigned>::max() / sizeof(CharType));
    }
    unsigned length() const { return m_length; }
    const CharType* characters() { return m_constData; }
    const CharType* release() { const CharType* data = m_constData; m_constData = 0; return data; }
private:
    const  CharType* m_constData;    
    unsigned m_length;
};
#endif
} // namespace WTF

using WTF::StringBuffer;
#if ENABLE(SBROWSER_TEXT_CODEC_MEMORY_REDUCTION)
using WTF::StringBufferConst;
#endif

#endif // StringBuffer_h
