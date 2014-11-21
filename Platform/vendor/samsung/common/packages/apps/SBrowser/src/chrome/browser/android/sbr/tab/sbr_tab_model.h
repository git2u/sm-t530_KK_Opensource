
#ifndef CHROME_BROWSER_ANDROID_SBR_TAB_SBR_TAB_MODEL_H_
#define CHROME_BROWSER_ANDROID_SBR_TAB_SBR_TAB_MODEL_H_

#include <jni.h>
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "content/public/browser/web_contents.h"
#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"


class SbrTabModel :public TabModel
{
public :

   SbrTabModel(JNIEnv* env, jobject obj,Profile* profile );   
   virtual  ~SbrTabModel();
  virtual int GetTabCount() const OVERRIDE;
  virtual int GetActiveIndex() const OVERRIDE;
  virtual content::WebContents* GetWebContentsAt(int index) const OVERRIDE;
  virtual SessionID::id_type GetTabIdAt(int index) const OVERRIDE;

  // Used for restoring tabs from synced foreign sessions.
  virtual void CreateTab(content::WebContents* web_contents)OVERRIDE;

  // Return true if we are currently restoring sessions asynchronously.
  virtual bool IsSessionRestoreInProgress() const OVERRIDE;

  virtual void OpenClearBrowsingData() const OVERRIDE;
  void Destroy(JNIEnv* env, jobject obj);
    // Register the Tab's native methods through JNI.
  static bool RegisterSbrTabModel(JNIEnv* env);

private :
	   JavaObjectWeakGlobalRef weak_java_delegate_;
	    base::android::ScopedJavaLocalRef<jobject> mJavaObj;
		
};

#endif  // CHROME_BROWSER_ANDROID_SBR_TAB_SBR_TAB_BRIDGE_H_
