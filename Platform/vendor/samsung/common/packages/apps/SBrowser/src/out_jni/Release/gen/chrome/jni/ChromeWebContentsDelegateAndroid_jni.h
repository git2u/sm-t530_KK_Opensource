// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     org/chromium/chrome/browser/ChromeWebContentsDelegateAndroid

#ifndef org_chromium_chrome_browser_ChromeWebContentsDelegateAndroid_JNI
#define org_chromium_chrome_browser_ChromeWebContentsDelegateAndroid_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kChromeWebContentsDelegateAndroidClassPath[] =
    "org/chromium/chrome/browser/ChromeWebContentsDelegateAndroid";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_ChromeWebContentsDelegateAndroid_clazz = NULL;
}  // namespace

// Step 2: method stubs.

static base::subtle::AtomicWord
    g_ChromeWebContentsDelegateAndroid_onFindResultAvailable = 0;
static void Java_ChromeWebContentsDelegateAndroid_onFindResultAvailable(JNIEnv*
    env, jobject obj, jobject result) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_ChromeWebContentsDelegateAndroid_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_ChromeWebContentsDelegateAndroid_clazz,
      "onFindResultAvailable",

"("
"Lorg/chromium/chrome/browser/FindNotificationDetails;"
")"
"V",
      &g_ChromeWebContentsDelegateAndroid_onFindResultAvailable);

  env->CallVoidMethod(obj,
      method_id, result);
  base::android::CheckException(env);

}

static base::subtle::AtomicWord
    g_ChromeWebContentsDelegateAndroid_onFindMatchRectsAvailable = 0;
static void
    Java_ChromeWebContentsDelegateAndroid_onFindMatchRectsAvailable(JNIEnv* env,
    jobject obj, jobject result) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_ChromeWebContentsDelegateAndroid_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_ChromeWebContentsDelegateAndroid_clazz,
      "onFindMatchRectsAvailable",

"("
"Lorg/chromium/chrome/browser/FindMatchRectsDetails;"
")"
"V",
      &g_ChromeWebContentsDelegateAndroid_onFindMatchRectsAvailable);

  env->CallVoidMethod(obj,
      method_id, result);
  base::android::CheckException(env);

}

static base::subtle::AtomicWord g_ChromeWebContentsDelegateAndroid_createRect =
    0;
static ScopedJavaLocalRef<jobject>
    Java_ChromeWebContentsDelegateAndroid_createRect(JNIEnv* env, jint x,
    jint y,
    jint right,
    jint bottom) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_ChromeWebContentsDelegateAndroid_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_ChromeWebContentsDelegateAndroid_clazz,
      "createRect",

"("
"I"
"I"
"I"
"I"
")"
"Landroid/graphics/Rect;",
      &g_ChromeWebContentsDelegateAndroid_createRect);

  jobject ret =
    env->CallStaticObjectMethod(g_ChromeWebContentsDelegateAndroid_clazz,
      method_id, x, y, right, bottom);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord g_ChromeWebContentsDelegateAndroid_createRectF =
    0;
static ScopedJavaLocalRef<jobject>
    Java_ChromeWebContentsDelegateAndroid_createRectF(JNIEnv* env, jfloat x,
    jfloat y,
    jfloat right,
    jfloat bottom) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_ChromeWebContentsDelegateAndroid_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_ChromeWebContentsDelegateAndroid_clazz,
      "createRectF",

"("
"F"
"F"
"F"
"F"
")"
"Landroid/graphics/RectF;",
      &g_ChromeWebContentsDelegateAndroid_createRectF);

  jobject ret =
    env->CallStaticObjectMethod(g_ChromeWebContentsDelegateAndroid_clazz,
      method_id, x, y, right, bottom);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord
    g_ChromeWebContentsDelegateAndroid_createFindNotificationDetails = 0;
static ScopedJavaLocalRef<jobject>
    Java_ChromeWebContentsDelegateAndroid_createFindNotificationDetails(JNIEnv*
    env, jint numberOfMatches,
    jobject rendererSelectionRect,
    jint activeMatchOrdinal,
    jboolean finalUpdate) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_ChromeWebContentsDelegateAndroid_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_ChromeWebContentsDelegateAndroid_clazz,
      "createFindNotificationDetails",

"("
"I"
"Landroid/graphics/Rect;"
"I"
"Z"
")"
"Lorg/chromium/chrome/browser/FindNotificationDetails;",
      &g_ChromeWebContentsDelegateAndroid_createFindNotificationDetails);

  jobject ret =
    env->CallStaticObjectMethod(g_ChromeWebContentsDelegateAndroid_clazz,
      method_id, numberOfMatches, rendererSelectionRect, activeMatchOrdinal,
          finalUpdate);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord
    g_ChromeWebContentsDelegateAndroid_createFindMatchRectsDetails = 0;
static ScopedJavaLocalRef<jobject>
    Java_ChromeWebContentsDelegateAndroid_createFindMatchRectsDetails(JNIEnv*
    env, jint version,
    jint numRects,
    jobject activeRect) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_ChromeWebContentsDelegateAndroid_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_ChromeWebContentsDelegateAndroid_clazz,
      "createFindMatchRectsDetails",

"("
"I"
"I"
"Landroid/graphics/RectF;"
")"
"Lorg/chromium/chrome/browser/FindMatchRectsDetails;",
      &g_ChromeWebContentsDelegateAndroid_createFindMatchRectsDetails);

  jobject ret =
    env->CallStaticObjectMethod(g_ChromeWebContentsDelegateAndroid_clazz,
      method_id, version, numRects, activeRect);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord
    g_ChromeWebContentsDelegateAndroid_setMatchRectByIndex = 0;
static void Java_ChromeWebContentsDelegateAndroid_setMatchRectByIndex(JNIEnv*
    env, jobject findMatchRectsDetails,
    jint index,
    jobject rect) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_ChromeWebContentsDelegateAndroid_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_ChromeWebContentsDelegateAndroid_clazz,
      "setMatchRectByIndex",

"("
"Lorg/chromium/chrome/browser/FindMatchRectsDetails;"
"I"
"Landroid/graphics/RectF;"
")"
"V",
      &g_ChromeWebContentsDelegateAndroid_setMatchRectByIndex);

  env->CallStaticVoidMethod(g_ChromeWebContentsDelegateAndroid_clazz,
      method_id, findMatchRectsDetails, index, rect);
  base::android::CheckException(env);

}

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_ChromeWebContentsDelegateAndroid_clazz =
      reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env,
          kChromeWebContentsDelegateAndroidClassPath).obj()));
  return true;
}

#endif  // org_chromium_chrome_browser_ChromeWebContentsDelegateAndroid_JNI
