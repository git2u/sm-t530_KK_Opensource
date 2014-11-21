/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef DeviceUtils_h
#define DeviceUtils_h

#include "common/String.h"

#define EXPORT __attribute__((visibility("default")))

namespace kikin { namespace util {
    
/* Helper class for finding the device specific things. */
class EXPORT DeviceUtils {

public:
    
    /* Returns the next text node. */
    static kString getDeviceCountryCode();
    
private:
    
    /** Returns the country code from string. */
    static kString getCountryCodeFromString(const kString& text);

private:

    /* Don't allow constructor */
    DeviceUtils() {}
    ~DeviceUtils() {}
    
};

}   // namespace util
    
}   // namespace kikin

#endif // DeviceUtils_h
