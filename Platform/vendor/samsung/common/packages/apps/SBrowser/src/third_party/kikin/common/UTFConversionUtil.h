/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef UTFConversionUtil_h
#define UTFConversionUtil_h

#include "base/string16.h"

#define EXPORT __attribute__((visibility("default")))

namespace kikin {

/* kikin's UTF conversion util helper. */
class EXPORT UTFConversionUtil {
public:

	static size_t convertOffsetFromUTF32toUTF16(const uint32* text, const size_t offset);
	static size_t convertOffsetFromUTF16toUTF8(const char16* text, const size_t offset);
	static size_t convertOffsetFromUTF8toUTF16(const char* text, const size_t offset);
	static size_t convertOffsetFromUTF16toUTF32(const char16* text, const size_t offset);

private:

	// Avoid default constructors
	UTFConversionUtil();
	UTFConversionUtil(const UTFConversionUtil&);
	void operator=(const UTFConversionUtil&);

};

}   // namespace kikin

#endif // UTFConversionUtil_h
