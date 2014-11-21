
#if defined(SBROWSER_CAC)
#ifndef CHROME_BROWSER_ANDROID_SBR_UI_ANDROID_SBR_SSL_CLIENT_CERTIFICATE_REQUEST_H_
#define CHROME_BROWSER_ANDROID_SBR_UI_ANDROID_SBR_SSL_CLIENT_CERTIFICATE_REQUEST_H_
#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

namespace chrome {
namespace android {

jboolean 
  SbrSSLClientCertificateRequest_selectClientCertificate(JNIEnv* env, jint nativePtr,
  jobjectArray keyTypes, jobjectArray encodedPrincipals, jstring hostName, jint port);
// Register JNI methods.
bool RegisterSbrSSLClientCertificateRequestAndroid(JNIEnv* env);

}  // namespace android
}  // namespace chrome
#endif  // CHROME_BROWSER_ANDROID_SBR_UI_ANDROID_SBR_SSL_CLIENT_CERTIFICATE_REQUEST_H_
#endif // SBROWSER_CAC