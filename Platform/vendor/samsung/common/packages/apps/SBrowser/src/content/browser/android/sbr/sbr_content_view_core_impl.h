
#ifndef CONTENT_BROWSER_ANDROID_SBR_CONTENT_VIEW_H_
#define CONTENT_BROWSER_ANDROID_SBR_CONTENT_VIEW_H_

#include <map>

#include "base/android/jni_android.h"
#include "base/memory/scoped_ptr.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/android/content_view_core_impl.h"
#include <vector>
#include "base/android/jni_helper.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/public/browser/android/content_view_core.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"
#include "content/public/common/context_menu_params.h"
#include "webkit/glue/webmenuitem.h"


namespace content {
#if defined(SBROWSER_HIDE_URLBAR)
class HideUrlBarCmd;
#endif

class SbrContentViewCore {
 public:

  SbrContentViewCore(JNIEnv* env,
                      jobject obj);
  void Destroy(JNIEnv* env, jobject obj);
  virtual ~SbrContentViewCore();
  
  void SetNativeContentView(JNIEnv* env, jobject obj,jint nativeContentView);
#if defined (SBROWSER_SAVEPAGE_IMPL)
  void GetScrapBitmap(JNIEnv* env, jobject obj, jint bitmapWidth, jint bitmapHeight);
  void OnBitmapForScrap(const SkBitmap& skbitmap, bool response);
  void SavePageAs(JNIEnv* env, jobject obj); 
  void ScrapPageSavedFileName ( const base::FilePath::StringType& pure_file_name);
  void GetBitmapFromCachedResource(JNIEnv* env, jobject obj, jstring imgUrl );
  void OnReceiveBitmapFromCache(const SkBitmap& bitmap);
#endif
  #if defined(SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT)
  void SetBingSearchEngineAsDefault(bool enable);
  #endif

#if defined(SBROWSER_TEXT_SELECTION)
  void GetSelectionBitmap(JNIEnv* env , jobject obj);
   void SelectedBitmap(const SkBitmap& Bitmap);
   void PointOnRegion(bool isOnRegion);
   void clearTextSelection(JNIEnv* env , jobject obj);
   void selectClosestWord(JNIEnv* env , jobject obj,int x , int y);
   void CheckBelongToSelection(JNIEnv* env, jobject obj, jint TouchX, jint TouchY);

   void SetSelectionVisibilty(bool isVisible) ;
   void RequestSelectionVisibilityStatus(JNIEnv* env, jobject obj);
   void GetSelectionRect(JNIEnv* env, jobject obj);
   void UpdateCurrentSelectionRect(const gfx::Rect& selectionRect);
   void UnmarkSelection(JNIEnv* env, jobject obj);
#endif
#if defined(SBROWSER_FP_SUPPORT)
   void NotifyAutofillBegin();
#endif
#if defined (SBROWSER_SCROLL_INPUTTEXT)
void  ScrollEditWithCursor(JNIEnv* env , jobject obj,int scrollX);
void OnTextFieldBoundsChanged(const gfx::Rect&  input_edit_rect,bool isScrollable);
#endif

#if defined(SBROWSER_HOVER_HIGHLIGHT)
void ShowHoverFocus(JNIEnv* env, jobject obj,jfloat x, jfloat y,jlong time_ms, jboolean high_light);
#endif
#if defined(SBROWSER_IMAGEDRAG_IMPL)
void GetBitmapForCopyImage(JNIEnv* env, jobject obj,jfloat x, jfloat y);
#endif
#if defined (SBROWSER_SUPPORT)
  base::android::ScopedJavaLocalRef<jstring> Accept_Headers(JNIEnv* env , jobject obj);
#endif

#if defined(SBROWSER_SBR_NATIVE_CONTENTVIEW)  && defined(SBROWSER_ENTERKEY_LONGPRESS)
void  performLongClickOnFocussedNode(JNIEnv* env, jobject obj, jlong time_ms);
#endif

#if defined (SBROWSER_MULTIINSTANCE_DRAG_DROP)
void HandleSelectionDrop(JNIEnv* env , jobject obj, int x, int y, jstring text);
#endif

#if defined (SBROWSER_HANDLE_MOUSECLICK_CTRL)
void HandleMouseClickWithCtrlkey(JNIEnv* env , jobject obj, int x, int y);
void OnOpenUrlInNewTab(const string16& mouseClickUrl);
#endif

#if defined(SBROWSER_TEXT_SELECTION) && defined(SBROWSER_HTML_DRAG_N_DROP)
   void PerformLongPress(JNIEnv* env, jobject obj, jlong time_ms,
                 jfloat x, jfloat y,
                 jboolean disambiguation_popup_tap);
   void SelectedMarkup(const string16& markup);
   void GetSelectionMarkup(JNIEnv* env , jobject obj);
#endif
#if defined (SBROWSER_ENABLE_ECHO_PWD)
void SetPasswordEcho(JNIEnv* env, jobject obj,jboolean pwdEchoEnabled);
#endif
#if defined(SBROWSER_IMAGEDRAG_IMPL)
   void SetBitmapForDragging(const SkBitmap& bitmap,int width, int height);
#endif

#if defined(SBROWSER_SBR_NATIVE_CONTENTVIEW) && defined(SBROWSER_CONTEXT_MENU_FLAG)
   void ShowContextMenu(const ContextMenuParams& params);
#endif
#if defined(SBROWSER_FORM_NAVIGATION)
void ShowSelectPopupMenu(const std::vector<WebMenuItem>& items, int selected_item, bool multiple, int privateimeoptions);
#endif

#if defined(SBROWSER_FORM_NAVIGATION)
void selectPopupClose();
void selectPopupCloseZero();
#endif

#if defined(SBROWSER_FORM_NAVIGATION) 
void UpdateImeAdapter(int native_ime_adapter,
                                           int text_input_type,
                                           const std::string& text,
                                           int selection_start,
                                           int selection_end,
                                           int composition_start,
                                           int composition_end,
                                           bool show_ime_if_needed,
                                           int privateIMEOptions
#if defined (SBROWSER_ENABLE_JPN_SUPPORT_SUGGESTION_OPTION)
                                           ,bool spellCheckingEnabled
#endif
                                           );
#endif

//#if defined(SBROWSER_FORM_NAVIGATION)
void moveFocusToNext(JNIEnv* env , jobject obj);
void moveFocusToPrevious(JNIEnv* env , jobject obj);
//#endif 
#if defined (SBROWSER_MULTIINSTANCE_TAB_DRAG_AND_DROP)
  bool GetTabDragAndDropIsInProgress();
#endif
// SAMSUNG : SmartClip >>

void GetSmartClipData(JNIEnv* env , jobject obj,jint x, jint y, jint w, jint h);
#if defined(SBROWSER_SMART_CLIP)
void OnSmartClipDataExtracted(const string16& result);
#endif
// SAMSUNG : SmartClip <<
#if defined (SBROWSER_SOFTBITMAP_IMPL)
void UpdateSoftBitmap(const SkBitmap& bitmap);
void ReCaptureSoftBitmap(JNIEnv* env, jobject obj,  
                         jint x, jint y, jint height, 
                         jfloat pageScaleFactor, jboolean fromPageLoadFinish);
void UpdateReCaptureSoftBitmap(const SkBitmap& bitmap, bool fromPageLoadFinish);
#endif

#if defined (SBROWSER_PRINT_IMPL)
  void PrintBegin(JNIEnv* env, jobject obj);
  void PrintPage(JNIEnv* env, jobject obj, jint pageNum);
  void PrintEnd(JNIEnv* env, jobject obj);
  void OnPrintBegin(int totalPageCount);
  void OnBitmapForPrint(const SkBitmap& skbitmap, int responseCode);
#endif	

#if defined(SBROWSER_FPAUTH_IMPL)
  void FPAuthStatus(JNIEnv* env, jobject obj, jboolean enabled, jstring usrName);
  bool OnCheckFPAuth();
  void OnLaunchFingerPrintActivity();
#endif
  // --------------------------- kikin Modification starts ---------------------------
  // This must not be under the kikin flag as these are JNI functions.
  void GetSelectionContext(JNIEnv* env, jobject obj, bool should_update_selection_context);
  void UpdateSelection(JNIEnv* env, jobject obj, jstring old_query, jstring new_query);
  void OnSelectionContextExtracted(const std::map<std::string, std::string>& selectionContext);
  void RestoreSelection(JNIEnv* env, jobject obj);
  void ClearSelection(JNIEnv* env, jobject obj);
  // --------------------------- kikin Modification ends -----------------------------
 #if defined (SBROWSER_WML)
 jboolean IsWMLPage(JNIEnv* env, jobject obj);
 #endif
#if defined (SBROWSER_SBR_NATIVE_CONTENTVIEW)
   void LoadDataWithBaseUrl(JNIEnv* env, jobject obj, jstring data
            ,jstring baseurl, jstring mimetype, jstring encoding, jstring historyurl);
#endif
#if defined(SBROWSER_GRAPHICS_GETBITMAP)
  jboolean GetBitmapFromCompositor(JNIEnv* env,
                                   jobject obj,  jint x, jint y, jint width, jint height,
                                   jobject jbitmap,jint imageFormat);

  jboolean GetBitmapFromCompositor(JNIEnv* env,
                                   jobject obj,
                                   jobject jbitmap);
#endif //SBROWSER_GRAPHICS_GETBITMAP
#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
	void SetWindow(gfx::AcceleratedWidget window) { _window = window;}
	gfx::AcceleratedWidget GetWindow() { return _window;}
	void OnCompositingSurfaceUpdated();
#endif
  void SetInterceptNavigationDelegate ( JNIEnv* env,
                               jobject obj,
                               jobject intercept_navigation_delegate);
#if defined(SBROWSER_CONTEXT_MENU_FLAG)
  void ShowCustomContextMenu(
      const content::ContextMenuParams& params,
      const base::Callback<void(int)>& callback);

  void OnCustomMenuAction(JNIEnv* env, jobject obj, jint action);
#endif
#if defined(SBROWSER_HIDE_URLBAR)
  void HideUrlBarCmdReq(JNIEnv* env, jobject obj,
                                jint cmd,
                                jobject urlbar_bitmap,
                                jboolean override_allowed,
                                jboolean urlbar_active);

  void HideUrlBarCmdReq(HideUrlBarCmd& cmdReq);

  void OnHideUrlBarEventNotify(int event, int event_data1, int event_data2);

#else
  void HideUrlBarCmdReq(JNIEnv* env, jobject obj,
                                jint cmd,
                                jobject urlbar_bitmap,
                                jboolean override_allowed,
                                jboolean urlbar_active);
#endif

#if defined(SBROWSER_READER_NATIVE)
	void RecognizeArticle(JNIEnv* env, jobject obj, int mode);
	void OnRecognizeArticleResult(bool isArticle);
#endif

  // SAMSUNG CHANGE : Add SbrScrollBy for Air Jump, Spen hover scrolling, Smart Scroll >>
#if defined(SBROWSER_SBR_NATIVE_CONTENTVIEW)
    void SbrScrollBy(JNIEnv* env, jobject obj, jlong time_ms,
                                       jint x, jint y, jfloat dx, jfloat dy,
                                       jboolean last_input_event_for_vsync);
    void SbrScrollBegin(JNIEnv* env, jobject obj, jlong time_ms,
                                      jfloat x, jfloat y);
    void SbrScrollEnd(JNIEnv* env, jobject obj, jlong time_ms);
#endif
  // SAMSUNG CHANGE : Add SbrScrollBy for Air Jump, Spen hover scrolling, Smart Scroll <<
  private:
// SAMSUNG CHANGE : Add SbrScrollBy for Air Jump, Spen hover scrolling, Smart Scroll >>
#if defined(SBROWSER_SBR_NATIVE_CONTENTVIEW)
  	  enum InputEventVSyncStatus {
          NOT_LAST_INPUT_EVENT_FOR_VSYNC,
          LAST_INPUT_EVENT_FOR_VSYNC
      };
#endif
// SAMSUNG CHANGE : Add SbrScrollBy for Air Jump, Spen hover scrolling, Smart Scroll <<
  	  JavaObjectWeakGlobalRef java_ref_;
	  ContentViewCoreImpl* contentview_core_;
#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
	  gfx::AcceleratedWidget _window;
#endif
	  

#if defined(SBROWSER_CONTEXT_MENU_FLAG)
    base::Callback<void(int)> context_menu_selected_callback_;
#endif

};

// This is called for each ContentView.
//jint Init(JNIEnv* env, jobject obj, jint native_content_view) ;

bool RegisterSbrContentViewCore(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SBR_CONTENT_VIEW_H_
