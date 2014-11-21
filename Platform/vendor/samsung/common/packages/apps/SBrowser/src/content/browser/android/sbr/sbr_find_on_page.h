//#ifndef CHROME_BROWSER_ANDROID_FIND_ON_PAGE_H_
//#define CHROME_BROWSER_ANDROID_FIND_ON_PAGE_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "content/browser/android/sbr/sbr_find_on_page_delegate.h"
namespace conetnt {

class SbrFindOnPageDelegate;

}

namespace content {
class WebContents;


class FindOnPage{
 public:
  FindOnPage(JNIEnv* env,
                     jobject obj,
                     content::WebContents* web_contents);
  void Destroy(JNIEnv* env, jobject obj);
  
void StartFinding(JNIEnv* env, jobject obj, jstring search_string,
                    jboolean forward_direction,
                    jboolean case_sensitive);

  // Stops the current Find operation.
  void StopFinding(JNIEnv* env, jobject obj,jint selection_action);

  void ActivateNearestFindResult(JNIEnv* env, jobject obj,
                                 jfloat x, jfloat y);
void SetSbrFindOnPageDelegate(JNIEnv* env, jobject obj,
                                           jobject sbr_webcontent_delegate);
   base::android::ScopedJavaLocalRef<jstring> GetPreviousFindText(JNIEnv* env, jobject obj) const;

 protected:
  virtual ~FindOnPage();

 private:
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<content::SbrFindOnPageDelegate>
          web_contents_delegate_;
  enum { keepSelectionOnPage = 0, clearSelectionOnPage = 1, activateSelectionOnPage = 2  };

  DISALLOW_COPY_AND_ASSIGN(FindOnPage);
};

bool RegisterFindOnPage(JNIEnv * env);
}

