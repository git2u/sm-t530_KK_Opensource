
#ifndef CHROME_BROWSER_ANDROID_SBR_DELEGATE_SBR_CHROME_WEB_CONTENTS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SBR_DELEGATE_SBR_CHROME_WEB_CONTENTS_DELEGATE_ANDROID_H_

#include <jni.h>

#include "base/files/file_path.h"
#include "base/time.h"
#include "components/web_contents_delegate_android/web_contents_delegate_android.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class FindNotificationDetails;

namespace content {
struct FileChooserParams;
class WebContents;
}

namespace gfx {
class Rect;
class RectF;
}

namespace chrome {
namespace android {

// Chromium Android specific WebContentsDelegate.
// Should contain any WebContentsDelegate implementations required by
// the Chromium Android port but not to be shared with WebView.
class SbrWebContentsDelegateAndroid
    : public components::WebContentsDelegateAndroid,
      public content::NotificationObserver {
 public:
  SbrWebContentsDelegateAndroid(JNIEnv* env, jobject obj, content::WebContents* web_contents);
  virtual ~SbrWebContentsDelegateAndroid();

  virtual void RunFileChooser(content::WebContents* web_contents,
                              const content::FileChooserParams& params)
                              OVERRIDE;
  virtual void CloseContents(content::WebContents* web_contents) OVERRIDE;
  virtual void FindReply(content::WebContents* web_contents,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;
  virtual void FindMatchRectsReply(content::WebContents* web_contents,
                                   int version,
                                   const std::vector<gfx::RectF>& rects,
                                   const gfx::RectF& active_rect) OVERRIDE;
  virtual content::JavaScriptDialogManager*
  GetJavaScriptDialogManager() OVERRIDE;
  virtual void DidNavigateToPendingEntry(content::WebContents* source) OVERRIDE;
  virtual void DidNavigateMainFramePostCommit(
      content::WebContents* source) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual bool RequestPpapiBrokerPermission(
      content::WebContents* web_contents,
      const GURL& url,
      const base::FilePath& plugin_path,
      const base::Callback<void(bool)>& callback) OVERRIDE;

 private:
  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnFindResultAvailable(content::WebContents* web_contents,
                             const FindNotificationDetails* find_result);

  void notifyFaviconUpdateForCurrentTab( content::WebContents* web_contents,
   										 const bool* find_result);

  void notifyPopupBlockedStateChangedForTab( content::WebContents* web_contents);
   										

  content::NotificationRegistrar notification_registrar_;

  base::TimeTicks navigation_start_time_;
};

// Register the native methods through JNI.
bool RegisterSbrWebContentsDelegateAndroid(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_SBR_DELEGATE_SBR_CHROME_WEB_CONTENTS_DELEGATE_ANDROID_H_

