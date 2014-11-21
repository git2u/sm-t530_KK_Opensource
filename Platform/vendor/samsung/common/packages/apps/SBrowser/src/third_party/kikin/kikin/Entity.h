/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinEntity_h
#define KikinEntity_h

#include <string>

#include "common/String.h"
#include "base/memory/ref_counted.h"

#define EXPORT __attribute__((visibility("default")))

namespace kikin {
    
/* Stores  the information about the entity. */
class EXPORT Entity : public base::RefCountedThreadSafe<Entity> {
public:

    /* Constructor */
    Entity(const kString& type, const kString& language);
    
    /* Destructor */
    virtual ~Entity() = 0;
    
    /* Returns the value of the entity. */
    const kString getType() const;
    
    /* Returns the value of the entity. */
    const kString getValue() const;
    
    /* Returns the language of the entity. */
    const kString getLanguage() const;

    /* Returns the language of the entity. */
    const kString getTouchedWord() const;

    /* Set the value of the entity. */
    void setValue(const kString& value);

    /* Set the value of the entity. */
    void setLanguage(const kString& language);

    /* Sets the original word at the point of touch. */
    void setTouchedWord(const kString& touchedWord);
    
private:
    
    /* Type of the entity. */
    kString m_type;
    
    /* Value for the entity. */
    kString m_value;

    /* Lnaguage for the entity. */
    kString m_language;

    /* original word at the point of touch. */
    kString m_touchedWord;
};

}   // namespace kikin

#endif // KikinEntity_h
