
#ifndef  CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_SEARCH_ENGINE_PREFERENCES_H
#define CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_SEARCH_ENGINE_PREFERENCES_H
#pragma once

#include "base/android/jni_android.h"

void SbrSetSearchEngine(JNIEnv* env, jobject obj, jint selected_index); 
void SbrUpdateSearchEngineInJava(JNIEnv* env, jobject obj);
void SbrSetSearchEngineEx(JNIEnv* env, jobject obj, jobject engine);
jobjectArray SbrGetLocalizedSearchEngines(JNIEnv* env, jobject obj);
jstring SbrGetURLForSearchEngine(JNIEnv* env, jobject obj, jint selected_index);
jstring SbrGetFaviconURLForSearchEngine(JNIEnv* env, jobject obj, jint selected_index);
jstring SbrGetURLForCurrentSearchEngine(JNIEnv* env, jobject obj); 
jstring SbrGetNameForCurrentSearchEngine(JNIEnv* env, jobject obj); 
jstring SbrGetFaviconURLForCurrentSearchEngine(JNIEnv* env, jobject obj);


#endif //#ifndef  CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_SEARCH_ENGINE_PREFERENCES_H

