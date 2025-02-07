// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     org/chromium/chrome/browser/ContentViewUtil

#ifndef org_chromium_chrome_browser_ContentViewUtil_JNI
#define org_chromium_chrome_browser_ContentViewUtil_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kContentViewUtilClassPath[] =
    "org/chromium/chrome/browser/ContentViewUtil";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_ContentViewUtil_clazz = NULL;
}  // namespace

static jint CreateNativeWebContents(JNIEnv* env, jclass clazz,
    jboolean incognito);

static void DestroyNativeWebContents(JNIEnv* env, jclass clazz,
    jint webContentsPtr);

// Step 2: method stubs.

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_ContentViewUtil_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kContentViewUtilClassPath).obj()));
  static const JNINativeMethod kMethodsContentViewUtil[] = {
    { "nativeCreateNativeWebContents",
"("
"Z"
")"
"I", reinterpret_cast<void*>(CreateNativeWebContents) },
    { "nativeDestroyNativeWebContents",
"("
"I"
")"
"V", reinterpret_cast<void*>(DestroyNativeWebContents) },
  };
  const int kMethodsContentViewUtilSize = arraysize(kMethodsContentViewUtil);

  if (env->RegisterNatives(g_ContentViewUtil_clazz,
                           kMethodsContentViewUtil,
                           kMethodsContentViewUtilSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_chrome_browser_ContentViewUtil_JNI
