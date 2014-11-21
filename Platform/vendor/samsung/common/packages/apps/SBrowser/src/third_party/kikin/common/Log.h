/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinLog_h
#define KikinLog_h

#ifdef ANDROID

#define STR2(X) #X
#define STR(X) STR2(X)

#define CAT2(X,Y) X##Y
#define CAT(X,Y) CAT2(X,Y)

#define KIKIN_LOG_TAG __FILE__ " (" STR(__LINE__) ") "

#include <android/log.h>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, KIKIN_LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, KIKIN_LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, KIKIN_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, KIKIN_LOG_TAG, __VA_ARGS__)

#else

#define LOGD(format, ...) printf("DEBUG: " format "\n", ##__VA_ARGS__)
#define LOGI(format, ...) printf("INFO : " format "\n", ##__VA_ARGS__)
#define LOGW(format, ...) printf("WARN : " format "\n", ##__VA_ARGS__)
#define LOGE(format, ...) printf("ERROR: " format "\n", ##__VA_ARGS__)

#endif

#endif // KikinLog_h
