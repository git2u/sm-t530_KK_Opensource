
#ifndef CHROME_BROWSER_ANDROID_SBR_LOCATIONBAR_SBR_LOCATION_BAR_H_
#define CHROME_BROWSER_ANDROID_SBR_LOCATIONBAR_SBR_LOCATION_BAR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/android/jni_helper.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

class Profile;

// The native part of the Java SbrLocationBar class.
// Note that there should only be one instance of this class whose lifecycle
// is managed from the Java side.
class SbrLocationBar : public content::NotificationObserver {
 public:
 enum SecurityLevel {
    NONE = 0,          // HTTP/no URL/user is editing
    EV_SECURE,         // HTTPS with valid EV cert
    SECURE,            // HTTPS (non-EV)
    SECURITY_WARNING,  // HTTPS, but unable to check certificate revocation
                       // status or with insecure content on the page
    SECURITY_ERROR,    // Attempted HTTPS and failed, page not authenticated
    NUM_SECURITY_LEVELS,
  };
  // TODO(jcivelli): this constructor should not take a profile, it should get
  //                 it from the current tab (and update the profile on the
  //                 autocomplete_controller_ when the user switches tab).
  SbrLocationBar(JNIEnv* env, jobject obj, Profile* profile);
  void Destroy(JNIEnv* env, jobject obj);

  // Called by the Java code when the user clicked on the security button.
  void OnSecurityButtonClicked(JNIEnv* env, jobject, jobject context, jobject contentview);

  // Called to get the SSL security level of current tab.
  int GetSecurityLevel(JNIEnv* env, jobject, jobject contentview);

 private:
  virtual ~SbrLocationBar();

  // NotificationObserver method:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 // MAYUR :: This is not required now as per the explanation in SbrLocationbar.cc
 // void OnSSLVisibleStateChanged(const content::NotificationSource& source,
 //                               const content::NotificationDetails& details);

  void OnUrlsStarred(const content::NotificationSource& source,
                     const content::NotificationDetails& details);

  JavaObjectWeakGlobalRef weak_java_location_bar_;

  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SbrLocationBar);
};

// Registers the SbrLocationBar native method.
bool RegisterSbrLocationBar(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_LOCATION_BAR_H_