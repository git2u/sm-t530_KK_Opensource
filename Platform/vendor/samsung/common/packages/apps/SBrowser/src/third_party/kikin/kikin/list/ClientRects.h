/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinClientRectList_h
#define KikinClientRectList_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/RefPtr.h>

namespace WebCore {
    
    /* Forward declarartion of ClentRect */
    class ClientRect;
}

namespace kikin { namespace list {

/* List of client rects. */
class ClientRects : public RefCounted<ClientRects> {
public:
    
    /* Creates the instance of the class and adds the reference. */
    static PassRefPtr<ClientRects> create() { return adoptRef(new ClientRects); }
    
    /* Destructor */
    ~ClientRects();

    /** Returns number of ClientRects in the list. */
    unsigned length() const;
    
    /** Returns the ClientRect at index. */
    WebCore::ClientRect* item(unsigned index);
    
    /** Appends the list of rects to the existing list. */
    void append(WTF::PassRefPtr<WebCore::ClientRect> rect);
    
    /** Appends the list of rects to the existing list. */
    void append(WTF::PassRefPtr<ClientRects> ranges);
    
    /** Remove the ClientRect at index. */
    void remove(unsigned index);
    
    /** Inserts the ClientRect at specified index. */
    void insert(WTF::PassRefPtr<WebCore::ClientRect> rect, unsigned index);
    
    /** Replace the ClientRect at specified index. */
    void replace(WTF::PassRefPtr<WebCore::ClientRect> rect, unsigned index);

private:
    
    /* Constructor */
    ClientRects();
    
private:
    
    /* List of rects */
    Vector<RefPtr<WebCore::ClientRect> > m_list;
}; 

} // namespace list
    
} // namespace kikin

#endif // KikinClientRectList_h
