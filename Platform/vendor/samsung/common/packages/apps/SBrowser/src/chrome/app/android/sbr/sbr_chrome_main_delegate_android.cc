
#include "chrome/app/android/sbr/sbr_chrome_main_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chrome/browser/android/sbr/tab/sbr_tab_bridge.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"

static const char kDefaultCountryCode[] = "US";

static base::android::RegistrationMethod kRegistrationMethods[] = {
    { "SbrTabBridge", SbrTabBridge::RegisterSbrTabBridge },
};

ChromeMainDelegateAndroid* ChromeMainDelegateAndroid::Create() {
  return new SbrChromeMainDelegateAndroid();
}

SbrChromeMainDelegateAndroid::SbrChromeMainDelegateAndroid() {
}

SbrChromeMainDelegateAndroid::~SbrChromeMainDelegateAndroid() {
}

bool SbrChromeMainDelegateAndroid::BasicStartupComplete(int* exit_code) {
  TemplateURLPrepopulateData::InitCountryCode(kDefaultCountryCode);
  return ChromeMainDelegateAndroid::BasicStartupComplete(exit_code);
}

bool SbrChromeMainDelegateAndroid::RegisterApplicationNativeMethods(
    JNIEnv* env) {
  return ChromeMainDelegateAndroid::RegisterApplicationNativeMethods(env) &&
      base::android::RegisterNativeMethods(env,
                                           kRegistrationMethods,
                                           arraysize(kRegistrationMethods));
}
