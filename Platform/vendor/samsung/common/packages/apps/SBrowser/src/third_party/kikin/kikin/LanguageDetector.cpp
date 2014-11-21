/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#include "kikin/LanguageDetector.h"

#ifdef ANDROID
#   include <sys/system_properties.h>
#endif

#include "common/Log.h"

// #define CLD_WINDOWS
#include "encodings/compact_lang_det/compact_lang_det.h"
#include "encodings/compact_lang_det/compact_lang_det_impl.h"
#include "encodings/compact_lang_det/win/cld_basictypes.h"

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

namespace kikin {

// static
const char16 LanguageDetector::s_separators[] = {
	0x0009, /* <control-0009> to <control-000D> */
	0x000A, /* LINE FEED (LF) */
	0x000B, /* LINE TABULATION */
	0x000C, /* FORM FEED (FF) */
	0x0020, /* SPACE */
	0x0028, // LEFT PARENTHESIS
	0x0029, // RIGHT PARENTHESIS
	0x0085, /* NEXT LINE (NEL) */
	0x00A0, /* NO-BREAK SPACE */
	0x1680, /* OGHAM SPACE MARK */
	0x180E, /* MONGOLIAN VOWEL SEPARATOR */
	0x2000, /* EN QUAD */
	0x2001, /* EM QUAD */
	0x2002, /* EN SPACE */
	0x2003, /* EM SPACE */
	0x2004, /* THREE-PER-EM SPACE */
	0x2005, /* FOUR-PER-EM SPACE */
	0x2006, /* SIX-PER-EM SPACE */
	0x2007, /* FIGURE SPACE */
	0x2008, /* PUNCTUATION SPACE */
	0x2009, /* THIN SPACE */
	0x200A, /* HAIR SPACE */
	0x200C, /* Zero Width Non-Joiner */
	0x2028, /* Line Separator */
	0x2029, /* Paragraph Separator */
	0x202F, /* Narrow No-Break Space */
	0x205F, /* Medium Mathematical Space */
	0x3000, /* Ideographic Space */
	0xFF08, // FULLWIDTH LEFT PARENTHESIS '（'
	0xFF09, // FULLWIDTH RIGHT PARENTHESIS '）'
	0
};

// static
Language LanguageDetector::detect(const kString& text, const int offset) {
    bool is_reliable = false;
//    LOGD("text:%s \t length:%d \t offset:%d",text.toUTF8().data(), text.length(), offset);

    // get one word around the selection offset
    size_t prevSpace = LanguageDetector::findSeperator(text, offset, false);
    size_t nextSpace = LanguageDetector::findSeperator(text, (offset + 1), true);

//    LOGD("previous space:%d \t next space:%d", prevSpace, nextSpace);

    int start = ((kString::npos != prevSpace) ? (prevSpace + 1) : 0);
    int end = ((kString::npos != nextSpace) ? nextSpace : text.length());

//    LOGD("start:%d \t end:%d", start, end);

    start = (((offset - start) > 50) ? (offset - 50) : start);
    end = (((end - offset) > 50) ? (offset + 50) : end);

//    LOGD("start:%d \t end:%d", start, end);

    kString wordAtOffset = text.substring(start, (end - start));
    std::string wordAtOffset_utf8 = wordAtOffset.toUTF8();

//    LOGD("word at offset:%s", wordAtOffset_utf8.data());

    // analyze that word only
    Language wordLang = UNKNOWN_LANGUAGE;
    wordLang = (Language)CompactLangDet::DetectLanguage(0, wordAtOffset_utf8.data(), wordAtOffset_utf8.length(), true, &is_reliable);

    std::string text_utf8 = text.toUTF8();

    // analyze the whole text
    Language textLang = UNKNOWN_LANGUAGE;
	textLang = (Language)CompactLangDet::DetectLanguage(0, text_utf8.data(), text_utf8.length(), true, &is_reliable);

	// if the text/word language do not match at all
    if (LanguageDetector::IsCJKLanguage(wordLang) != LanguageDetector::IsCJKLanguage(textLang)) {
    	// return word language
    	// DISABLED FOR NOW
    	//return wordLang;
    }

    // can't figure out what it is?
    if (!is_reliable && !LanguageDetector::IsCJKLanguage(textLang)) {
    	// default to English
    	return ENGLISH;
    }

    return textLang;
}

// static
bool LanguageDetector::IsCJKLanguage(const Language lang) {
	return lang == CHINESE_T || lang == JAPANESE || lang == KOREAN || lang == CHINESE;
}

// static
size_t LanguageDetector::findSeperator(const kString& text, const int offset, const bool forward) {

	if (forward) {
		return text.findFirstOf(s_separators, offset);
	} else {
		return text.findLastOf(s_separators, offset);
	}

	return kString::npos;
}

// static
Language LanguageDetector::getDeviceLanguage() {
#ifdef ANDROID
	char value[PROP_VALUE_MAX] = "";
    int len = __system_property_get("persist.sys.language", value);
    if (len >= 0) {
	    Language ret = LanguageDetector::languageFromString(kString(value, len));
	    return ret;
    } else {
    	LOGE("Failed to retrieve system language");
    }
#endif
    return UNKNOWN_LANGUAGE;
}

Language LanguageDetector::languageFromString(const kString& text) {
	kString language = text;

	// is it a complex language-country code?
	size_t separatorIdx = text.indexOf('-');
	if (kString::npos != separatorIdx) {
		// remove the country
		language = text.substring(0, separatorIdx);
	}

	// convert to lower case
//	std::transform(language.begin(), language.end(), language.begin(), ::tolower);
	language.toLower();

	if (language == "fr") {
		return FRENCH;
	} else if (language == "de") {
		return GERMAN;
	} else if (language == "es") {
		return SPANISH;
	} else if (language == "en") {
		return ENGLISH;
	} else if (language == "nl") {
		return DUTCH;
	} else if (language == "cn") {
		return CHINESE;
	}

	return UNKNOWN_LANGUAGE;
}

} // namespace kikin
