#ifndef  CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_PASSWORD_PREFERENCES_H
#define CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_PASSWORD_PREFERENCES_H
#pragma once

#include "base/android/jni_android.h"


jobject SbrGetSavedNamePassword(JNIEnv* env, jclass clazz, int index);

jobject SbrGetSavedPasswordException(JNIEnv* env, jclass clazz, int index);

void SbrRemoveSavedNamePassword(JNIEnv* env, jclass clazz, int index);

void SbrRemoveSavedPasswordException(JNIEnv* env, jclass clazz, int index); 

void SbrStartPasswordListRequest(JNIEnv* env, jclass clazz, jobject owner); 

void SbrStopPasswordListRequest(JNIEnv* env, jclass clazz);


#endif //#ifndef CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_PASSWORD_PREFERENCES_H 

