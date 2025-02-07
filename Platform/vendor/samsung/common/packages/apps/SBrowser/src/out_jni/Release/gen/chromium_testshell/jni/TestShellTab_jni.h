// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     org/chromium/chrome/testshell/TestShellTab

#ifndef org_chromium_chrome_testshell_TestShellTab_JNI
#define org_chromium_chrome_testshell_TestShellTab_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kTestShellTabClassPath[] =
    "org/chromium/chrome/testshell/TestShellTab";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestShellTab_clazz = NULL;
}  // namespace

static jint Init(JNIEnv* env, jobject obj,
    jint webContentsPtr,
    jint windowAndroidPtr);

// Step 2: method stubs.
static void Destroy(JNIEnv* env, jobject obj,
    jint nativeTestShellTab) {
  DCHECK(nativeTestShellTab) << "Destroy";
  TestShellTab* native = reinterpret_cast<TestShellTab*>(nativeTestShellTab);
  return native->Destroy(env, obj);
}

static void InitWebContentsDelegate(JNIEnv* env, jobject obj,
    jint nativeTestShellTab,
    jobject chromeWebContentsDelegateAndroid) {
  DCHECK(nativeTestShellTab) << "InitWebContentsDelegate";
  TestShellTab* native = reinterpret_cast<TestShellTab*>(nativeTestShellTab);
  return native->InitWebContentsDelegate(env, obj,
      chromeWebContentsDelegateAndroid);
}

static jstring FixupUrl(JNIEnv* env, jobject obj,
    jint nativeTestShellTab,
    jstring url) {
  DCHECK(nativeTestShellTab) << "FixupUrl";
  TestShellTab* native = reinterpret_cast<TestShellTab*>(nativeTestShellTab);
  return native->FixupUrl(env, obj, url).Release();
}

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_TestShellTab_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kTestShellTabClassPath).obj()));
  static const JNINativeMethod kMethodsTestShellTab[] = {
    { "nativeInit",
"("
"I"
"I"
")"
"I", reinterpret_cast<void*>(Init) },
    { "nativeDestroy",
"("
"I"
")"
"V", reinterpret_cast<void*>(Destroy) },
    { "nativeInitWebContentsDelegate",
"("
"I"
"Lorg/chromium/chrome/browser/ChromeWebContentsDelegateAndroid;"
")"
"V", reinterpret_cast<void*>(InitWebContentsDelegate) },
    { "nativeFixupUrl",
"("
"I"
"Ljava/lang/String;"
")"
"Ljava/lang/String;", reinterpret_cast<void*>(FixupUrl) },
  };
  const int kMethodsTestShellTabSize = arraysize(kMethodsTestShellTab);

  if (env->RegisterNatives(g_TestShellTab_clazz,
                           kMethodsTestShellTab,
                           kMethodsTestShellTabSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_chrome_testshell_TestShellTab_JNI
