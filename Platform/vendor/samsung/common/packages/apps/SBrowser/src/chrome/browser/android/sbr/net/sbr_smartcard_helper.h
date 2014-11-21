	
#ifndef  CHROME_BROWSER_ANDROID_SBR_NET_SBR_SMARTCARD_HELPER_H_
#define CHROME_BROWSER_ANDROID_SBR_NET_SBR_SMARTCARD_HELPER_H_
#pragma once
	
#include "base/android/jni_helper.h"
	
namespace net {

bool RegisterSmartcardHelper(JNIEnv* env);

} // namespace net
#endif // CHROME_BROWSER_ANDROID_SBR_NET_SBR_SMARTCARD_HELPER_H_

