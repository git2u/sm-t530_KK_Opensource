// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     org/chromium/chrome/browser/search_engines/TemplateUrlService

#ifndef org_chromium_chrome_browser_search_engines_TemplateUrlService_JNI
#define org_chromium_chrome_browser_search_engines_TemplateUrlService_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kTemplateUrlServiceClassPath[] =
    "org/chromium/chrome/browser/search_engines/TemplateUrlService";
const char kTemplateUrlClassPath[] =
    "org/chromium/chrome/browser/search_engines/TemplateUrlService$TemplateUrl";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TemplateUrlService_clazz = NULL;
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TemplateUrl_clazz = NULL;
}  // namespace

static jint Init(JNIEnv* env, jobject obj);

// Step 2: method stubs.
static void Load(JNIEnv* env, jobject obj,
    jint nativeTemplateUrlServiceAndroid) {
  DCHECK(nativeTemplateUrlServiceAndroid) << "Load";
  TemplateUrlServiceAndroid* native =
      reinterpret_cast<TemplateUrlServiceAndroid*>(nativeTemplateUrlServiceAndroid);
  return native->Load(env, obj);
}

static jboolean IsLoaded(JNIEnv* env, jobject obj,
    jint nativeTemplateUrlServiceAndroid) {
  DCHECK(nativeTemplateUrlServiceAndroid) << "IsLoaded";
  TemplateUrlServiceAndroid* native =
      reinterpret_cast<TemplateUrlServiceAndroid*>(nativeTemplateUrlServiceAndroid);
  return native->IsLoaded(env, obj);
}

static jint GetTemplateUrlCount(JNIEnv* env, jobject obj,
    jint nativeTemplateUrlServiceAndroid) {
  DCHECK(nativeTemplateUrlServiceAndroid) << "GetTemplateUrlCount";
  TemplateUrlServiceAndroid* native =
      reinterpret_cast<TemplateUrlServiceAndroid*>(nativeTemplateUrlServiceAndroid);
  return native->GetTemplateUrlCount(env, obj);
}

static jobject GetPrepopulatedTemplateUrlAt(JNIEnv* env, jobject obj,
    jint nativeTemplateUrlServiceAndroid,
    jint i) {
  DCHECK(nativeTemplateUrlServiceAndroid) << "GetPrepopulatedTemplateUrlAt";
  TemplateUrlServiceAndroid* native =
      reinterpret_cast<TemplateUrlServiceAndroid*>(nativeTemplateUrlServiceAndroid);
  return native->GetPrepopulatedTemplateUrlAt(env, obj, i).Release();
}

static void SetDefaultSearchProvider(JNIEnv* env, jobject obj,
    jint nativeTemplateUrlServiceAndroid,
    jint selectedIndex) {
  DCHECK(nativeTemplateUrlServiceAndroid) << "SetDefaultSearchProvider";
  TemplateUrlServiceAndroid* native =
      reinterpret_cast<TemplateUrlServiceAndroid*>(nativeTemplateUrlServiceAndroid);
  return native->SetDefaultSearchProvider(env, obj, selectedIndex);
}

static jint GetDefaultSearchProvider(JNIEnv* env, jobject obj,
    jint nativeTemplateUrlServiceAndroid) {
  DCHECK(nativeTemplateUrlServiceAndroid) << "GetDefaultSearchProvider";
  TemplateUrlServiceAndroid* native =
      reinterpret_cast<TemplateUrlServiceAndroid*>(nativeTemplateUrlServiceAndroid);
  return native->GetDefaultSearchProvider(env, obj);
}

static base::subtle::AtomicWord g_TemplateUrl_create = 0;
static ScopedJavaLocalRef<jobject> Java_TemplateUrl_create(JNIEnv* env, jint id,
    jstring shortName,
    jstring keyword) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TemplateUrl_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_TemplateUrl_clazz,
      "create",

"("
"I"
"Ljava/lang/String;"
"Ljava/lang/String;"
")"
"Lorg/chromium/chrome/browser/search_engines/TemplateUrlService$TemplateUrl;",
      &g_TemplateUrl_create);

  jobject ret =
    env->CallStaticObjectMethod(g_TemplateUrl_clazz,
      method_id, id, shortName, keyword);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord g_TemplateUrlService_templateUrlServiceLoaded =
    0;
static void Java_TemplateUrlService_templateUrlServiceLoaded(JNIEnv* env,
    jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TemplateUrlService_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TemplateUrlService_clazz,
      "templateUrlServiceLoaded",

"("
")"
"V",
      &g_TemplateUrlService_templateUrlServiceLoaded);

  env->CallVoidMethod(obj,
      method_id);
  base::android::CheckException(env);

}

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_TemplateUrlService_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kTemplateUrlServiceClassPath).obj()));
  g_TemplateUrl_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kTemplateUrlClassPath).obj()));
  static const JNINativeMethod kMethodsTemplateUrlService[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
    { "nativeLoad",
"("
"I"
")"
"V", reinterpret_cast<void*>(Load) },
    { "nativeIsLoaded",
"("
"I"
")"
"Z", reinterpret_cast<void*>(IsLoaded) },
    { "nativeGetTemplateUrlCount",
"("
"I"
")"
"I", reinterpret_cast<void*>(GetTemplateUrlCount) },
    { "nativeGetPrepopulatedTemplateUrlAt",
"("
"I"
"I"
")"
"Lorg/chromium/chrome/browser/search_engines/TemplateUrlService$TemplateUrl;",
    reinterpret_cast<void*>(GetPrepopulatedTemplateUrlAt) },
    { "nativeSetDefaultSearchProvider",
"("
"I"
"I"
")"
"V", reinterpret_cast<void*>(SetDefaultSearchProvider) },
    { "nativeGetDefaultSearchProvider",
"("
"I"
")"
"I", reinterpret_cast<void*>(GetDefaultSearchProvider) },
  };
  const int kMethodsTemplateUrlServiceSize =
      arraysize(kMethodsTemplateUrlService);

  if (env->RegisterNatives(g_TemplateUrlService_clazz,
                           kMethodsTemplateUrlService,
                           kMethodsTemplateUrlServiceSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_chrome_browser_search_engines_TemplateUrlService_JNI
