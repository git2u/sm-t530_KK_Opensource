#ifndef  CHROME_BROWSER_ANDROID_SBR_NET_SBR_ANDROID_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_ANDROID_SBR_NET_SBR_ANDROID_NETWORK_LIBRARY_H_
#pragma once

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

jint SbrAndroidNetworkLibrary_verifyServerCertificates(JNIEnv* env, jobjectArray certChain, jstring authType);
bool SbrAndroidNetworkLibrary_willCheckRevocation();

bool RegisterSbrAndroidNetowrkLibrary(JNIEnv* env);
#endif // CHROME_BROWSER_ANDROID_SBR_NET_SBR_ANDROID_NETWORK_LIBRARY_H_
