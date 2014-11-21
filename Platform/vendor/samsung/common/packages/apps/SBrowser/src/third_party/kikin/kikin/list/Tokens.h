/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinTokenList_h
#define KikinTokenList_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/RefPtr.h>

#define EXPORT __attribute__((visibility("default")))

namespace kikin {

/* Forward declaration of HtmlToken. */
class HtmlToken;
    
namespace list {
    
/* Stores the list of tokens. */
class EXPORT Tokens : public RefCounted<Tokens> {
public:
    static PassRefPtr<Tokens> create() { return adoptRef(new Tokens); }
    ~Tokens();

    unsigned length() const;
    HtmlToken* item(unsigned index);
    void append(PassRefPtr<HtmlToken> token);
    HtmlToken* findSelected();

private:
    Tokens();
    
private:
    WTF::Vector<WTF::RefPtr<HtmlToken> > m_list;
}; 

} // namespace list
    
} // namespace kikin

#endif // KikinTokenList_h
