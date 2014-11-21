

#ifndef CHROME_BROWSER_ANDROID_SBR_TAB_SBR_TAB_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_SBR_TAB_SBR_TAB_BRIDGE_H_

#include <jni.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/sessions/in_memory_tab_restore_service.h"


#if defined(SBROWSER_FLUSH_PENDING_MESSAGES)
#include "content/public/browser/render_view_host.h"
#endif

namespace browser_sync {
class SyncedTabDelegate;
}

namespace chrome {
namespace android {
class SbrWebContentsDelegateAndroid;
}
}

namespace content {
class WebContents;
}

namespace ui {
class WindowAndroid;
}

#if defined(SBROWSER_FLUSH_PENDING_MESSAGES)
class SbrTabBridge: public TabAndroid, public content::RenderViewHost::MessageFlushObserver {
#else
class SbrTabBridge: public TabAndroid {
#endif

 public:
  SbrTabBridge(JNIEnv* env, jobject obj );
                     
  void Destroy(JNIEnv* env, jobject obj);

  // --------------------------------------------------------------------------
  // TabAndroid Methods
  // --------------------------------------------------------------------------
  virtual content::WebContents* GetWebContents() OVERRIDE;

  virtual browser_sync::SyncedTabDelegate* GetSyncedTabDelegate() OVERRIDE;

  virtual void OnReceivedHttpAuthRequest(jobject auth_handler,
                                         const string16& host,
                                         const string16& realm) OVERRIDE;
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;

  virtual void ShowCustomContextMenu(
      const content::ContextMenuParams& params,
      const base::Callback<void(int)>& callback) OVERRIDE;

  virtual void AddShortcutToBookmark(const GURL& url,
                                     const string16& title,
                                     const SkBitmap& skbitmap,
                                     int r_value,
                                     int g_value,
                                     int b_value) OVERRIDE;
  virtual void EditBookmark(int64 node_id, bool is_folder) OVERRIDE;

  virtual void RunExternalProtocolDialog(const GURL& url) OVERRIDE;

  // Register the Tab's native methods through JNI.
  static bool RegisterSbrTabBridge(JNIEnv* env);

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------
 #if defined(SBROWSER_THUMBNAIL)
 void UpdateThumbnail(JNIEnv* env, jobject obj,  jobject bitmap);
  #endif // 	SBROWSER_THUMBNAIL 


  void InitWebContents(JNIEnv* env, jobject obj, jobject paramContentViewCore, 
                               jobject sbr_webcontent_delegate, int tabId);

  void TabSetWindowId ( JNIEnv* env, jobject obj, jint windowId);
  

  void DestroyWebContents(JNIEnv* env, jobject obj, bool flag);
//SBROWSER_MULTIINSTANCE_TAB_DRAG_N_DROP >>
  int ReleaseWebContents(JNIEnv* env, jobject obj);
  int GetWebContents(JNIEnv* env, jobject obj);
//SBROWSER_MULTIINSTANCE_TAB_DRAG_N_DROP << 
  base::android::ScopedJavaLocalRef<jstring> FixupUrl(JNIEnv* env,
                                                      jobject obj,
                                                      jstring url);

  void SetInterceptNavigationDelegate ( JNIEnv* env,
                               jobject obj,
                               jobject intercept_navigation_delegate) ;


  void CreateHistoricalTab(JNIEnv* env, jobject obj, jint tab_index);
  void SetWindowId(JNIEnv* env, jobject obj, jint window_id);
  int GetRenderProcessPid ( JNIEnv* env, jobject obj) ;
  int GetRenderProcessPrivateSizeKBytes ( JNIEnv* env, jobject obj) ;
  void PurgeRenderProcessNativeMemory ( JNIEnv* env, jobject obj );
  base::android::ScopedJavaLocalRef<jstring> GetWebContentDisplayHost  ( JNIEnv* env, jobject obj );
  void SaveTabIdForSessionSync ( JNIEnv* env, jobject obj );
  void LoadUrl(JNIEnv* env, jobject obj, jstring url, jint Pagetransition,jint ua_override_option);
  void LaunchBlockedPopups (JNIEnv* env, jobject obj);
   int  GetPopupBlockedCount (JNIEnv* env, jobject obj);
   bool  IsInitialNavigation(JNIEnv* env, jobject obj);
   base::android::ScopedJavaLocalRef<jbyteArray> GetStateAsByteArray (JNIEnv* env, jobject obj);
    base::android::ScopedJavaLocalRef<jobject> GetFaviconBitmap(JNIEnv* env,
												jobject obj);
#if defined(SBROWSER_FLUSH_PENDING_MESSAGES)
  void FlushPendingMessages(JNIEnv* env, jobject obj);
  void DidFlushPendingMessages() OVERRIDE;
#endif

 protected:
  virtual ~SbrTabBridge();

 private:
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<chrome::android::SbrWebContentsDelegateAndroid>
          web_contents_delegate_;
    JavaObjectWeakGlobalRef weak_java_client_;

  DISALLOW_COPY_AND_ASSIGN(SbrTabBridge);
};

#endif  // CHROME_BROWSER_ANDROID_SBR_TAB_SBR_TAB_BRIDGE_H_
