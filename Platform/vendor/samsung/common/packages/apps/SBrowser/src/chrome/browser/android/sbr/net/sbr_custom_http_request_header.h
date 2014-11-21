
#ifndef  CHROME_BROWSER_ANDROID_SBR_NET_SBR_CUSTOM_HTTP_REQUEST_HEADER_H_
#define CHROME_BROWSER_ANDROID_SBR_NET_SBR_CUSTOM_HTTP_REQUEST_HEADER_H_
#pragma once

#include "base/android/jni_helper.h"
#include "net/http/http_request_headers.h"

namespace net {

bool hasCustomHttpRequestHeader();
HttpRequestHeaders getCustomHttpRequestHeader();
bool hasCustomHttpAcceptLanguageHeader();
HttpRequestHeaders getCustomHttpAcceptLanguageHeader();

bool RegisterCustomHttpRequestHeader(JNIEnv* env);

} // namespace net

#endif // CHROME_BROWSER_ANDROID_SBR_NET_SBR_CUSTOM_HTTP_REQUEST_HEADER_H_