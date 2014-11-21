
#ifndef SBR_FIND_ON_PAGE_DELEGATE_H_
#define SBR_FIND_ON_PAGE_DELEGATE_H_

#include <jni.h>

#include "components/web_contents_delegate_android/web_contents_delegate_android.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class FindNotificationDetails;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
class RectF;
}

namespace content {

class SbrFindOnPageDelegate
    : public components::WebContentsDelegateAndroid,
      public content::NotificationObserver {
 public:
  SbrFindOnPageDelegate(JNIEnv* env, jobject obj, content::WebContents* web_contents);
  virtual ~SbrFindOnPageDelegate();

  virtual void FindReply(content::WebContents* web_contents,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;
  virtual void CloseContents(content::WebContents* web_contents) OVERRIDE;
 
 private:
  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnFindResultAvailable(content::WebContents* web_contents,
                             const FindNotificationDetails* find_result);

  content::NotificationRegistrar notification_registrar_;

  //base::TimeTicks navigation_start_time_;
};

// Register the native methods through JNI.
bool RegisterSbrFindOnPageDelegate(JNIEnv* env);

}  

#endif  // SBR_FIND_ON_PAGE_DELEGATE_H_

