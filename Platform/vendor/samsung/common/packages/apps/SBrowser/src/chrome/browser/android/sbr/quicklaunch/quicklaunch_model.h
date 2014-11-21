
#ifndef CHROME_BROWSER_ANDROID_SBR_QUICKLAUNCH_QUICKLAUNCH_MODLE_H
#define CHROME_BROWSER_ANDROID_SBR_QUICKLAUNCH_QUICKLAUNCH_MODLE_H


#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/time_format.h"
#include "base/android/jni_helper.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/web_ui.h"
#include <string>

#if defined(SBROWSER_QUICKLAUNCH_OBSERVER)
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "chrome/browser/history/top_sites.h"
#endif

class QuickLaunchModel
#if defined(SBROWSER_QUICKLAUNCH_OBSERVER)
    : public content::NotificationObserver
#endif
{

public:
  QuickLaunchModel(JNIEnv* env, jobject obj);

  ~QuickLaunchModel();

  void Destroy(JNIEnv* env, jobject obj);
  // JNI registration.
  static bool RegisterQuickLaunchModel(JNIEnv* env);

  void GetMostVisited(JNIEnv* env, jobject obj, jint result_count, int days_back);

  jboolean IsMostVisited(JNIEnv* env,
                        jobject obj,
                        jstring url);

  jboolean IsBlacklistedURL(JNIEnv* env,
                        jobject obj,
                        jstring url);
  void BlacklistURLFromMostVisited(JNIEnv* env,
                                  jobject obj,
                                  jstring jurl);

private:

#if defined(SBROWSER_QUICKLAUNCH_OBSERVER)
  // Override NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  void QueryTopSites();
  void NotifyMostVisitedThumbnailChanged();
#endif

  void StartQueryForMostVisited();
  void OnMostVisitedUrlsAvailable(const history::MostVisitedURLList& data);
  void SetPagesValueFromTopSites(const history::MostVisitedURLList& data);
  void NotifyMostVisitedCompleted();
  void ClearModelMostVisitedList();
  bool BlacklistUrl(const GURL& url);

  // no of most_visited_url required
  int result_count_;

  // most_visited_urls from no of days history
  int days_back_;

  history::MostVisitedURLList most_visited_list;

  JavaObjectWeakGlobalRef weak_java_quicklaunch_model;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<QuickLaunchModel> weak_ptr_factory_;

  // We pre-fetch the first set of result pages.  This variable is false until
  // we get the first getMostVisited() call.
  bool is_most_visited_request_;

  bool first_request_;

#if defined(SBROWSER_QUICKLAUNCH_OBSERVER)
  // Used to register/unregister notification observer.
  content::NotificationRegistrar notification_registrar_;
#endif
};
#endif //define CHROME_BROWSER_ANDROID_SBR_QUICKLAUNCH_QUICKLAUNCH_MODLE_H
