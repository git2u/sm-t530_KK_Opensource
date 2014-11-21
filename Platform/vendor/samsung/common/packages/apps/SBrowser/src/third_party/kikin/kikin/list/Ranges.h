/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinRangeList_h
#define KikinRangeList_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/RefPtr.h>

namespace kikin {

/* Forward declaration of Range. */
class HtmlRange;
    
namespace list {
    
/* Stores the list of Ranges. */
class Ranges : public RefCounted<Ranges> {
public:
    
    static WTF::PassRefPtr<Ranges> create() { return adoptRef(new Ranges); }
    ~Ranges();

    unsigned length() const;
    HtmlRange* item(unsigned index);
    void append(WTF::PassRefPtr<HtmlRange> range);
    void append(WTF::PassRefPtr<Ranges> ranges);
    void insert(WTF::PassRefPtr<HtmlRange> range, unsigned index);
    void remove(unsigned index);
    int indexOf(HtmlRange* range);
    //void reverse();

private:
    
    Ranges();
    //WTF::PassRefPtr<HtmlRange> item(unsigned index);
    
private:
    Vector<WTF::RefPtr<HtmlRange> > m_list;
}; 

} // namespace list
    
} // namespace kikin

#endif // KikinRangeList_h
