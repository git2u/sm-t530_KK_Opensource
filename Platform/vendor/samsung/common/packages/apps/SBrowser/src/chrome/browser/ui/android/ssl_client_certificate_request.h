// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_SSL_CLIENT_CERTIFICATE_REQUEST_H_
#define CHROME_BROWSER_UI_ANDROID_SSL_CLIENT_CERTIFICATE_REQUEST_H_

#include <jni.h>

namespace chrome {
namespace android {
#if defined(SBROWSER_CAC)
void OnSystemRequestCompletionExt(JNIEnv* env, jclass clazz, jint request_id,
		jobjectArray encoded_chain_ref, jobject private_key_ref);
#else
// Register JNI methods.
bool RegisterSSLClientCertificateRequestAndroid(JNIEnv* env);
#endif
}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ANDROID_SSL_CLIENT_CERTIFICATE_REQUEST_H_
