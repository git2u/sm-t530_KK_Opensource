/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef Kikin_Html_Entity_h
#define Kikin_Html_Entity_h

#include "kikin/Entity.h"

#include <wtf/RefCounted.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

/* Forward declaration of Document. */
class Document;

} // namespace WebCore

namespace kikin {
    
namespace list {

/* Forward declaration of Ranges. */    
class Ranges;

/* Forward declaration of ClientRects. */
class ClientRects;
    
} /* namespace list */

class HtmlRange;

/* Stroes  the information about the entity. */
class HtmlEntity : public Entity, public RefCounted<HtmlEntity> {
public:

    /* Creates the instace fo class and adds the reference to it. */
    static HtmlEntity* create(WTF::PassRefPtr<list::Ranges> ranges, const WTF::String& type,
                                              const WTF::String& language);
    
    /* Destructor */
    virtual ~HtmlEntity();
    
    /* Returns the list of client rects. */
    WTF::PassRefPtr<list::ClientRects> getClientRects(WebCore::Document* ownerDocument);
    
    /* Extends the selection to text. */
    WTF::PassRefPtr<list::ClientRects> modifySelectionByText(const WTF::String& oldQuery, const WTF::String& newQuery,
                                                             WebCore::Document* ownerDocument);
    
    /* Returns the text for the entity. */
    const WTF::String getText() const;
    
    /* Returns the text before the entity (max of 50 characters). */
    const WTF::String getPreviousText() const;
    
    /* Returns the text after the entity (max of 50 characters). */
    const WTF::String getPostText() const;
    
    /* Returns the element tag name for the entity. */
    const WTF::String getTag() const;
    
    /* Returns the al for the entity. */
    const WTF::String getAlt() const;
    
    /* Returns the element's href for the entity. */
    const WTF::String getHref() const;
    
    /* Resets the text saved before and after the entity. */
    void resetWordsAround() const;
    
    /* Finds and saves the words before and after the entity. */
    void saveWordsAround() const;

    /** Returns the first range for in the entity. */
    HtmlRange* getFirstRange();

    /** Returns the last range in the entity. */
    HtmlRange* getLastRange();

    /** Returns the previous range from the given range in the entity, */
    HtmlRange* getPreviousRange(HtmlRange* range);

    /** Returns the next range from the given range in the entity. */
    HtmlRange* getNextRange(HtmlRange* range);

    /** Removes the given range from the entity. */
    void removeRange(HtmlRange* range);
    
    /* Returns the value of the entity. */
	const WTF::String getType_WTF() const;

	/* Returns the value of the entity. */
	const WTF::String getValue_WTF() const;

	/* Returns the language of the entity. */
	const WTF::String getLanguage_WTF() const;

	/* Returns the language of the entity. */
	const WTF::String getTouchedWord_WTF() const;

private:
    
    /* Constructor */
    HtmlEntity(WTF::PassRefPtr<list::Ranges> ranges, const WTF::String& type, const WTF::String& language);
    
private:
    
    /* Ranges for different tokens in the entity. */
    WTF::RefPtr<list::Ranges> m_ranges;

};

}   // namespace kikin

#endif // Kikin_Html_Entity_h
