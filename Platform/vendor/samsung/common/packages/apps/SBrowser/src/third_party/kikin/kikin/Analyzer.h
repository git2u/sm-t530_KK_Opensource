/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinAnalyzer_h
#define KikinAnalyzer_h

#include "common/String.h"
#include "kikin/Language.h"

#define EXPORT __attribute__((visibility("default")))

namespace kikin {

/* Forward decalaration of Entity. */
class Entity;
    
/* Forward decalaration of Token. */
class Token;
    
/* 
 * Analyzes the text for the passed range and returns the entity 
 * if there is any around the range. If no entity is found around
 * the range, returns the text of the range as an entity.
 */
class EXPORT Analyzer {

public:
    
    /* Analyzes the text around the Range. */
    static Entity* analyze(Token* token, const Language lang, const kString& countryCode);
    
private:
    
    Analyzer() {}
    ~Analyzer() {}
};
    
}   // namespace kikin

#endif // KikinAnalyzer_h
