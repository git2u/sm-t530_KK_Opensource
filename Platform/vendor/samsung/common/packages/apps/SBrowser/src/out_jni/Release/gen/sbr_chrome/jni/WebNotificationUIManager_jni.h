// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     com/sec/android/app/sbrowser/common/WebNotificationUIManager

#ifndef com_sec_android_app_sbrowser_common_WebNotificationUIManager_JNI
#define com_sec_android_app_sbrowser_common_WebNotificationUIManager_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kWebNotificationUIManagerClassPath[] =
    "com/sec/android/app/sbrowser/common/WebNotificationUIManager";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_WebNotificationUIManager_clazz = NULL;
}  // namespace

// Step 2: method stubs.

static base::subtle::AtomicWord g_WebNotificationUIManager_CreateNotification =
    0;
static void Java_WebNotificationUIManager_CreateNotification(JNIEnv* env,
    jstring paramString1,
    jstring title,
    jstring body) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_WebNotificationUIManager_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_WebNotificationUIManager_clazz,
      "CreateNotification",

"("
"Ljava/lang/String;"
"Ljava/lang/String;"
"Ljava/lang/String;"
")"
"V",
      &g_WebNotificationUIManager_CreateNotification);

  env->CallStaticVoidMethod(g_WebNotificationUIManager_clazz,
      method_id, paramString1, title, body);
  base::android::CheckException(env);

}

static base::subtle::AtomicWord
    g_WebNotificationUIManager_CreateHTMLNotification = 0;
static void Java_WebNotificationUIManager_CreateHTMLNotification(JNIEnv* env,
    jstring contentUrl) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_WebNotificationUIManager_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_WebNotificationUIManager_clazz,
      "CreateHTMLNotification",

"("
"Ljava/lang/String;"
")"
"V",
      &g_WebNotificationUIManager_CreateHTMLNotification);

  env->CallStaticVoidMethod(g_WebNotificationUIManager_clazz,
      method_id, contentUrl);
  base::android::CheckException(env);

}

static base::subtle::AtomicWord g_WebNotificationUIManager_ShowNotification = 0;
static void Java_WebNotificationUIManager_ShowNotification(JNIEnv* env) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_WebNotificationUIManager_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_WebNotificationUIManager_clazz,
      "ShowNotification",

"("
")"
"V",
      &g_WebNotificationUIManager_ShowNotification);

  env->CallStaticVoidMethod(g_WebNotificationUIManager_clazz,
      method_id);
  base::android::CheckException(env);

}

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_WebNotificationUIManager_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kWebNotificationUIManagerClassPath).obj()));
  return true;
}

#endif  // com_sec_android_app_sbrowser_common_WebNotificationUIManager_JNI
