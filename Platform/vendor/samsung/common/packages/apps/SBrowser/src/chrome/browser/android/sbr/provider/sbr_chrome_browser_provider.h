
#ifndef CHROME_BROWSER_ANDROID_PROVIDER_SBR_SBR_CHROME_BROWSER_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_PROVIDER_SBR_SBR_CHROME_BROWSER_PROVIDER_H_

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/provider/chrome_browser_provider.h"

class SbrChromeBrowserProvider : public ChromeBrowserProvider {
public:
  SbrChromeBrowserProvider(JNIEnv* env, jobject obj);
  void Destroy(JNIEnv*, jobject);
  // JNI registration.
  static bool RegisterSbrChromeBrowserProvider(JNIEnv* env);

  void SetMyDevice(JNIEnv* env, jobject, int64 id);
  #if defined(SBROWSER_OPERATOR_BOOKMARKS)
  jint RemoveAllOldOperatorBookmarks(JNIEnv* env, jobject);
  jboolean IsOperatorBookmark(JNIEnv* env,
                              jobject,
                              jlong id);
  void ConvertOperatorBookmarkAsUserBookmark(JNIEnv* env,
                                         jobject,
                                         jlong id);
 jboolean IsOperatorBookmarkEditable(JNIEnv* env,
                              jobject,
                              jlong id);
  #endif //#if defined(SBROWSER_OPERATOR_BOOKMARKS)
  

  base::android::ScopedJavaLocalRef<jobject> GetBookmarkedNodeForURL(JNIEnv* env,
      jobject,
      jstring jurl);
  base::android::ScopedJavaLocalRef<jobject> GetBookmarknodeForId(JNIEnv* env, jobject, jlong id);
  jboolean IsValidURL(JNIEnv* env, jobject, jstring jurl);

  jint GetBookmarkURLNodeCount(JNIEnv* env, jobject);

  jlong MoveBookmark(JNIEnv* env,
                    jobject,
                    jlong id,
                    jlong parent_id,
                    jlong index);

  void UpdateMyDeviceTitle(JNIEnv* env,
                                  jobject obj,
                                  jlong id,
                                  jstring title);

  jlong AddSCloudSyncBookmark(JNIEnv* env, jobject,
                    jstring jurl,
                    jstring jtitle,
                    jboolean is_folder,
                    jlong parent_id,
                    jlong isEditable);
#if defined(SBROWSER_OPERATOR_BOOKMARKS)
  jlong AddOperatorBookmark(JNIEnv* env, jobject,
                    jstring jurl,
                    jstring jtitle,
                    jboolean is_folder,
                    jlong parent_id,
                    jlong editable,
                    jlong operatorBookmark);
#endif //SBROWSER_OPERATOR_BOOKMARKS

#ifdef SBROWSER_QUICK_LAUNCH
  base::android::ScopedJavaLocalRef<jbyteArray> GetURLThumbnail(JNIEnv* env,
                                                                jobject,
                                                                jstring jurl);
#endif

private:
  virtual ~SbrChromeBrowserProvider();
  #if defined(SBROWSER_HISTORY)
  void HistoryDataChanged();
   // Override NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
    // Used to register/unregister notification observer.
  content::NotificationRegistrar notification_registrar_;
  JavaObjectWeakGlobalRef weak_java_provider_;
  #endif //#if defined(SBROWSER_HISTORY)


  BookmarkModel* bookmark_model_;
};

#endif  // CHROME_BROWSER_ANDROID_PROVIDER_SBR_SBR_CHROME_BROWSER_PROVIDER_H_
