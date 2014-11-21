#ifndef  CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_COOKIES_PREFERENCES_H
#define CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_COOKIES_PREFERENCES_H
#pragma once

#include "base/android/jni_android.h"

void SbrGetCurrentCookieCount(JNIEnv* env, jobject obj);
void SbrGetCookiesForUrl(JNIEnv* env, jobject obj,jstring url);
void sbrGetCurrentCacheSize(JNIEnv* env, jobject obj);

#endif //#ifndef  CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_COOKIES_PREFERENCES_H

