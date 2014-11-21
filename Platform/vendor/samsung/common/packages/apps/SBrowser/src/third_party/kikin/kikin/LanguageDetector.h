/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Ludovic Cabre
 */

#ifndef LanguageDetector_h
#define LanguageDetector_h

#include "common/String.h"
#include "kikin/Language.h"

#define EXPORT __attribute__((visibility("default")))

namespace kikin {

class EXPORT LanguageDetector {
public:

	static Language detect(const kString& text, const int offset);
	static Language getDeviceLanguage();
	static Language languageFromString(const kString& lang);

private:

	static size_t findSeperator(const kString& text, const int offset, const bool forward);
	static bool IsCJKLanguage(const Language lang);

private:

	static const char16 s_separators[];

};

} // namespace kikin

#endif // LanguageDetector_h
