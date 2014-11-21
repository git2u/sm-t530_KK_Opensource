/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#include "config.h"
#include "kikin/list/Ranges.h"

#include "core/dom/ExceptionCode.h"
#include "kikin/html/HtmlRange.h"

namespace kikin { namespace list {

Ranges::Ranges()
{
}

Ranges::~Ranges()
{
}
    
void Ranges::append(PassRefPtr<HtmlRange> range)
{
    if (range)
        m_list.append(range);
}
    
void Ranges::append(PassRefPtr<Ranges> ranges)
{
    if (ranges) {
        for (unsigned i = 0; i < ranges->length(); i++) {
            m_list.append(ranges->item(i));
        }
    }
}
    
void Ranges::insert(PassRefPtr<HtmlRange> range, unsigned index)
{
    if (range) {
        if (index < m_list.size())
            m_list.insert(index, range);
        else 
            m_list.append(range);
    }
    
}
    

unsigned Ranges::length() const
{
    return m_list.size();
}

HtmlRange* Ranges::item(unsigned index)
{
    if (index >= m_list.size()) {
        // FIXME: this should throw an exception.
        // ec = INDEX_SIZE_ERR;
        return 0;
    }

    return m_list[index].get();
}

void Ranges::remove(unsigned index)
{
    if (index >= m_list.size()) {
        // FIXME: this should throw an exception.
        // ec = INDEX_SIZE_ERR;
        return;
    }

    return m_list.remove(index);
}

int Ranges::indexOf(HtmlRange* range)
{
    for (unsigned i = 0; i < m_list.size(); i++) {
        if (range == m_list[i]) {
            return static_cast<int>(i);
        }
    }

    return -1;
}
    
//WTF::PassRefPtr<HtmlRange> Ranges::item(unsigned index)
//{
//    if (index >= m_list.size()) {
//        // FIXME: this should throw an exception.
//        // ec = INDEX_SIZE_ERR;
//        return 0;
//    }
//    
//    return m_list[index];
//}
    
//void Ranges::reverse()
//{
//    Vector<RefPtr<HtmlRange> > list;
//    list.reserveInitialCapacity(m_list.size());
//    for (size_t i = m_list.size() - 1; i >= 0 ; --i)
//        list.append(m_list[i]);
//    
//    m_list.clear();
//    
//    m_list.swap(list);
//}

} // namespace list
    
} // namespace kikin
