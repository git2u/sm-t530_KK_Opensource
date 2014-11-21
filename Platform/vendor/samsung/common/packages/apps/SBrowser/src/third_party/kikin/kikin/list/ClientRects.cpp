/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#include "config.h"
#include "kikin/list/ClientRects.h"

#include "core/dom/ExceptionCode.h"
#include "core/dom/ClientRect.h"

using namespace WebCore;

namespace kikin { namespace list {

ClientRects::ClientRects()
{
}

ClientRects::~ClientRects()
{
}
    
void ClientRects::append(PassRefPtr<ClientRect> rect)
{
    if (rect)
        m_list.append(rect);
}
    
void ClientRects::append(PassRefPtr<ClientRects> clientRects)
{
    if (clientRects) {
        for (unsigned i = 0; i < clientRects->length(); i++) {
            m_list.append(clientRects->item(i));
        }
    }
}
    
void ClientRects::insert(PassRefPtr<ClientRect> rect, unsigned index)
{
    if (rect) {
        if (index < m_list.size())
            m_list.insert(index, rect);
        else 
            m_list.append(rect);
    }
        
}
    
void ClientRects::replace(WTF::PassRefPtr<WebCore::ClientRect> rect, unsigned index)
{
    if (rect && index > 0 && index < m_list.size()) {
        m_list[index] = rect;
    }
}
    
void ClientRects::remove(unsigned index)
{
    if (index < m_list.size())
    {
        m_list.remove(index);
    }
}
    
unsigned ClientRects::length() const
{
    return m_list.size();
}

ClientRect* ClientRects::item(unsigned index)
{
    if (index >= m_list.size()) {
        // FIXME: this should throw an exception.
        // ec = INDEX_SIZE_ERR;
        return 0;
    }

    return m_list[index].get();
}
    
} // namespace list
    
} // namespace kikin
