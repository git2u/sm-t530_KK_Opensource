// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_core_impl.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/browser_process.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "cc/layers/layer.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/android/sync_input_event_filter.h"
#include "content/browser/android/touch_point.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/java/java_bound_object.h"
#include "content/browser/renderer_host/java/java_bridge_dispatcher_host_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/ssl/ssl_host_state.h"
#include "content/browser/web_contents/interstitial_page_impl.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_transition_types.h"
#include "jni/ContentViewCore_jni.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/android/WebInputEventFactory.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/size_f.h"
#include "webkit/glue/webmenuitem.h"
#include "webkit/user_agent/user_agent_util.h"
#if defined(SBROWSER_ENABLE_ADBLOCKER)
#include "content/public/browser/browser_thread.h"
#include "content/browser/loader/sbr/sbr_web_adblocker.h"
#endif

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebInputEventFactory;

// Describes the type and enabled state of a select popup item.
// Keep in sync with the value defined in SelectPopupDialog.java
enum PopupItemType {
  POPUP_ITEM_TYPE_GROUP = 0,
  POPUP_ITEM_TYPE_DISABLED,
  POPUP_ITEM_TYPE_ENABLED
};

namespace content {

namespace {

const void* kContentViewUserDataKey = &kContentViewUserDataKey;

int GetRenderProcessIdFromRenderViewHost(RenderViewHost* host) {
  DCHECK(host);
  RenderProcessHost* render_process = host->GetProcess();
  DCHECK(render_process);
  if (render_process->HasConnection())
    return render_process->GetHandle();
  else
    return 0;
}

ScopedJavaLocalRef<jobject> CreateJavaRect(
    JNIEnv* env,
    const gfx::Rect& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_ContentViewCore_createRect(env,
                                      static_cast<int>(rect.x()),
                                      static_cast<int>(rect.y()),
                                      static_cast<int>(rect.right()),
                                      static_cast<int>(rect.bottom())));
};
}  // namespace

// Enables a callback when the underlying WebContents is destroyed, to enable
// nulling the back-pointer.
class ContentViewCoreImpl::ContentViewUserData
    : public base::SupportsUserData::Data {
 public:
  explicit ContentViewUserData(ContentViewCoreImpl* content_view_core)
      : content_view_core_(content_view_core) {
  }

  virtual ~ContentViewUserData() {
    // TODO(joth): When chrome has finished removing the TabContents class (see
    // crbug.com/107201) consider inverting relationship, so ContentViewCore
    // would own WebContents. That effectively implies making the WebContents
    // destructor private on Android.
    delete content_view_core_;
  }

  ContentViewCoreImpl* get() const { return content_view_core_; }

 private:
  // Not using scoped_ptr as ContentViewCoreImpl destructor is private.
  ContentViewCoreImpl* content_view_core_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentViewUserData);
};

// static
ContentViewCoreImpl* ContentViewCoreImpl::FromWebContents(
    content::WebContents* web_contents) {
  ContentViewCoreImpl::ContentViewUserData* data =
      reinterpret_cast<ContentViewCoreImpl::ContentViewUserData*>(
          web_contents->GetUserData(kContentViewUserDataKey));
  return data ? data->get() : NULL;
}

// static
ContentViewCore* ContentViewCore::FromWebContents(
    content::WebContents* web_contents) {
  return ContentViewCoreImpl::FromWebContents(web_contents);
}

// static
ContentViewCore* ContentViewCore::GetNativeContentViewCore(JNIEnv* env,
                                                           jobject obj) {
  return reinterpret_cast<ContentViewCore*>(
      Java_ContentViewCore_getNativeContentViewCore(env, obj));
}

ContentViewCoreImpl::ContentViewCoreImpl(JNIEnv* env, jobject obj,
                                         bool hardware_accelerated,
                                         WebContents* web_contents,
                                         ui::ViewAndroid* view_android,
                                         ui::WindowAndroid* window_android)
    : java_ref_(env, obj),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      root_layer_(cc::Layer::Create()),
      tab_crashed_(false),
      view_android_(view_android),
      window_android_(window_android) {
  CHECK(web_contents) <<
      "A ContentViewCoreImpl should be created with a valid WebContents.";

  // When a tab is restored (from a saved state), it does not have a renderer
  // process.  We treat it like the tab is crashed. If the content is loaded
  // when the tab is shown, tab_crashed_ will be reset.  Since
  // RenderWidgetHostView is associated with the lifetime of the renderer
  // process, we use it to test whether there is a renderer process.

  // Touch lockup issue fix - tab_crashed_ needs to true only in OnTabCrashed()
  //tab_crashed_ = !(web_contents->GetRenderWidgetHostView());

  LOG(INFO)  << "ContentViewCoreImpl::ContentViewCoreImpl: tab_crashed_: " << tab_crashed_; 	

  // TODO(leandrogracia): make use of the hardware_accelerated argument.

  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  dpi_scale_ = display.device_scale_factor();

  // Currently, the only use case we have for overriding a user agent involves
  // spoofing a desktop Linux user agent for "Request desktop site".
  // Automatically set it for all WebContents so that it is available when a
  // NavigationEntry requires the user agent to be overridden.
  const char kLinuxInfoStr[] = "X11; Linux x86_64";
  std::string product = content::GetContentClient()->GetProduct();
  std::string spoofed_ua =
      webkit_glue::BuildUserAgentFromOSAndProduct(kLinuxInfoStr, product);
  web_contents->SetUserAgentOverride(spoofed_ua);

  InitWebContents();
}

#if defined (SBROWSER_WML)
bool ContentViewCoreImpl::IsWMLPage(){
    return web_contents_->IsWMLPage(); 
}
#endif
ContentViewCoreImpl::~ContentViewCoreImpl() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  java_ref_.reset();
  if (!j_obj.is_null()) {
    Java_ContentViewCore_onNativeContentViewCoreDestroyed(
        env, j_obj.obj(), reinterpret_cast<jint>(this));
  }
  if(NULL != web_contents_)
  	web_contents_ = NULL;
  // Make sure nobody calls back into this object while we are tearing things
  // down.
  notification_registrar_.RemoveAll();
}

void ContentViewCoreImpl::OnJavaContentViewCoreDestroyed(JNIEnv* env,
                                                         jobject obj) {
   if(NULL != web_contents_)
  	web_contents_ = NULL;                                                       
  DCHECK(env->IsSameObject(java_ref_.get(env).obj(), obj));
  java_ref_.reset();
}

void ContentViewCoreImpl::InitWebContents() {
  DCHECK(web_contents_);
  notification_registrar_.Add(
      this, NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
      Source<NavigationController>(&web_contents_->GetController()));
  notification_registrar_.Add(
      this, NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::NotificationService::AllBrowserContextsAndSources());
  notification_registrar_.Add(
      this, NOTIFICATION_WEB_CONTENTS_CONNECTED,
      Source<WebContents>(web_contents_));
  notification_registrar_.Add(
      this, NOTIFICATION_WEB_CONTENTS_SWAPPED,
      Source<WebContents>(web_contents_));

  static_cast<WebContentsViewAndroid*>(web_contents_->GetView())->
      SetContentViewCore(this);
  DCHECK(!web_contents_->GetUserData(kContentViewUserDataKey));
  web_contents_->SetUserData(kContentViewUserDataKey,
                             new ContentViewUserData(this));
#if defined(SBROWSER_ENABLE_ADBLOCKER)
	CommandLine* cmd = CommandLine::ForCurrentProcess();
	bool isAdBlockerEnabled = cmd->HasSwitch(switches::webAdBlockerEnabled);
	if(isAdBlockerEnabled)
	{
		LOG(INFO)<<" Vishal: Calling WebAdblocker::Init.";
		scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
		BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
			   message_loop_proxy->PostTask(FROM_HERE, base::Bind(&WebAdBlocker::Init));
			   MessageLoop::current()->Run();							 
	}
#endif
}

void ContentViewCoreImpl::Observe(int type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDER_VIEW_HOST_CHANGED: {
      std::pair<RenderViewHost*, RenderViewHost*>* switched_details =
          Details<std::pair<RenderViewHost*, RenderViewHost*> >(details).ptr();
      int old_pid = 0;
      if (switched_details->first) {
        old_pid = GetRenderProcessIdFromRenderViewHost(
            switched_details->first);
      }
      int new_pid = GetRenderProcessIdFromRenderViewHost(
          web_contents_->GetRenderViewHost());
      if (new_pid != old_pid) {
        // Notify the Java side of the change of the current renderer process.
        JNIEnv* env = AttachCurrentThread();
        ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
        if (!obj.is_null()) {
          Java_ContentViewCore_onRenderProcessSwap(
              env, obj.obj(), old_pid, new_pid);
        }
      }
      break;
    }
    case NOTIFICATION_RENDERER_PROCESS_CREATED: {
      // Notify the Java side of the current renderer process.
      RenderProcessHost* source_process_host =
          Source<RenderProcessHost>(source).ptr();
      RenderProcessHost* current_process_host =
          web_contents_->GetRenderViewHost()->GetProcess();

      if (source_process_host == current_process_host) {
        int pid = GetRenderProcessIdFromRenderViewHost(
            web_contents_->GetRenderViewHost());
        JNIEnv* env = AttachCurrentThread();
        ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
        if (!obj.is_null()) {
          Java_ContentViewCore_onRenderProcessSwap(env, obj.obj(), 0, pid);
        }
      }
      break;
    }
    case NOTIFICATION_WEB_CONTENTS_CONNECTED: {
      JNIEnv* env = AttachCurrentThread();
      ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
      if (!obj.is_null()) {
        Java_ContentViewCore_onWebContentsConnected(env, obj.obj());
      }
      break;
    }
    case NOTIFICATION_WEB_CONTENTS_SWAPPED: {
      JNIEnv* env = AttachCurrentThread();
      ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
      if (!obj.is_null()) {
        Java_ContentViewCore_onWebContentsSwapped(env, obj.obj());
      }
    }
  }
}

RenderWidgetHostViewAndroid*
    ContentViewCoreImpl::GetRenderWidgetHostViewAndroid() {
  RenderWidgetHostView* rwhv = NULL;
  if (web_contents_) {
    rwhv = web_contents_->GetRenderWidgetHostView();
    if (web_contents_->ShowingInterstitialPage()) {
     if(web_contents_->GetInterstitialPage()!=NULL)
      {
       rwhv = static_cast<InterstitialPageImpl*>(
           web_contents_->GetInterstitialPage())->
               GetRenderViewHost()->GetView();
      }
    }
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

ScopedJavaLocalRef<jobject> ContentViewCoreImpl::GetJavaObject() {
  JNIEnv* env = AttachCurrentThread();
  return java_ref_.get(env);
}

jint ContentViewCoreImpl::GetBackgroundColor(JNIEnv* env, jobject obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return SK_ColorWHITE;
  return rwhva->GetCachedBackgroundColor();
}

void ContentViewCoreImpl::SetBackgroundColor(JNIEnv* env,
                                             jobject obj,
                                             jint color) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return;
  rwhva->OnDidChangeBodyBackgroundColor(color);
}

void ContentViewCoreImpl::OnHide(JNIEnv* env, jobject obj) {
  Hide();
}

void ContentViewCoreImpl::OnShow(JNIEnv* env, jobject obj) {
  Show();
}

void ContentViewCoreImpl::Show() {
  GetWebContents()->WasShown();
}

void ContentViewCoreImpl::Hide() {
  GetWebContents()->WasHidden();
}

void ContentViewCoreImpl::OnTabCrashed() {
LOG(INFO)  << "TouchLockup ContentViewCoreImpl::OnTabCrashed"; 	
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_resetVSyncNotification(env, obj.obj());

  // if tab_crashed_ is already true, just return. e.g. if two tabs share the
  // render process, this will be called for each tab when the render process
  // crashed. If user reload one tab, a new render process is created. It can be
  // shared by the other tab. But if user closes the tab before reload the other
  // tab, the new render process will be shut down. This will trigger the other
  // tab's OnTabCrashed() called again as two tabs share the same
  // BrowserRenderProcessHost.
  LOG(INFO)  << "ContentViewCoreImpl::OnTabCrashed: tab_crashed_: " << tab_crashed_; 	
  if (tab_crashed_)
    return;
  tab_crashed_ = true;
  LOG(INFO)  << "TouchLockup ContentViewCoreImpl::OnTabCrashed sending notification to app";
  Java_ContentViewCore_onTabCrash(env, obj.obj());
}

// All positions and sizes are in CSS pixels.
// Note that viewport_width/height is a best effort based.
// ContentViewCore has the actual information about the physical viewport size.
void ContentViewCoreImpl::UpdateFrameInfo(
    const gfx::Vector2dF& scroll_offset,
    float page_scale_factor,
    const gfx::Vector2dF& page_scale_factor_limits,
    const gfx::SizeF& content_size,
    const gfx::SizeF& viewport_size,
    const gfx::Vector2dF& controls_offset,
    const gfx::Vector2dF& content_offset,
    float overdraw_bottom_height){
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

Java_ContentViewCore_updateFrameInfo(
      env, obj.obj(),
      scroll_offset.x(),
      scroll_offset.y(),
      page_scale_factor,
      page_scale_factor_limits.x(),
      page_scale_factor_limits.y(),
      content_size.width(),
      content_size.height(),
      viewport_size.width(),
      viewport_size.height(),
      controls_offset.y(),
      content_offset.y(),
      overdraw_bottom_height
      );

  for (size_t i = 0; i < update_frame_info_callbacks_.size(); ++i) {
    update_frame_info_callbacks_[i].Run(
        content_size, scroll_offset, page_scale_factor);
  }
}

void ContentViewCoreImpl::SetTitle(const string16& title) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtitle =
      ConvertUTF8ToJavaString(env, UTF16ToUTF8(title));
  Java_ContentViewCore_setTitle(env, obj.obj(), jtitle.obj());
}

void ContentViewCoreImpl::OnBackgroundColorChanged(SkColor color) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_onBackgroundColorChanged(env, obj.obj(), color);
}

void ContentViewCoreImpl::ShowSelectPopupMenu(
    const std::vector<WebMenuItem>& items, int selected_item, bool multiple) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;

  // For multi-select list popups we find the list of previous selections by
  // iterating through the items. But for single selection popups we take the
  // given |selected_item| as is.
  ScopedJavaLocalRef<jintArray> selected_array;
  if (multiple) {
    scoped_ptr<jint[]> native_selected_array(new jint[items.size()]);
    size_t selected_count = 0;
    for (size_t i = 0; i < items.size(); ++i) {
      if (items[i].checked)
        native_selected_array[selected_count++] = i;
    }

    selected_array.Reset(env, env->NewIntArray(selected_count));
    env->SetIntArrayRegion(selected_array.obj(), 0, selected_count,
                           native_selected_array.get());
  } else {
    selected_array.Reset(env, env->NewIntArray(1));
    jint value = selected_item;
    env->SetIntArrayRegion(selected_array.obj(), 0, 1, &value);
  }

  ScopedJavaLocalRef<jintArray> enabled_array(env,
                                              env->NewIntArray(items.size()));
  std::vector<string16> labels;
  labels.reserve(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    labels.push_back(items[i].label);
    jint enabled =
        (items[i].type == WebMenuItem::GROUP ? POPUP_ITEM_TYPE_GROUP :
            (items[i].enabled ? POPUP_ITEM_TYPE_ENABLED :
                POPUP_ITEM_TYPE_DISABLED));
    env->SetIntArrayRegion(enabled_array.obj(), i, 1, &enabled);
  }
  ScopedJavaLocalRef<jobjectArray> items_array(
      base::android::ToJavaArrayOfStrings(env, labels));
  Java_ContentViewCore_showSelectPopup(env, j_obj.obj(),
                                       items_array.obj(), enabled_array.obj(),
                                       multiple, selected_array.obj());
}

void ContentViewCoreImpl::ConfirmTouchEvent(InputEventAckState ack_result) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_ContentViewCore_confirmTouchEvent(env, j_obj.obj(),
                                         static_cast<jint>(ack_result));
}

#if defined(SBROWSER_FLING_END_NOTIFICATION_CANE)
void ContentViewCoreImpl::OnFlingAnimationCompleted(bool page_end){
 JNIEnv* env = AttachCurrentThread();
   ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_ContentViewCore_onFlingAnimationCompleted(env, j_obj.obj(), page_end);
}
#endif

void ContentViewCoreImpl::HasTouchEventHandlers(bool need_touch_events) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_ContentViewCore_hasTouchEventHandlers(env,
                                             j_obj.obj(),
                                             need_touch_events);
}

bool ContentViewCoreImpl::HasFocus() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_ContentViewCore_hasFocus(env, obj.obj());
}

void ContentViewCoreImpl::OnSelectionChanged(const std::string& text) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtext = ConvertUTF8ToJavaString(env, text);
  Java_ContentViewCore_onSelectionChanged(env, obj.obj(), jtext.obj());
}

static void DestroyIncognitoProfile(JNIEnv* , jclass) {
   g_browser_process->profile_manager()->GetDefaultProfile()->
       DestroyOffTheRecordProfile();
}

void ContentViewCoreImpl::OnSelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jobject> anchor_rect_dip(
      CreateJavaRect(env, params.anchor_rect));
  ScopedJavaLocalRef<jobject> focus_rect_dip(
      CreateJavaRect(env, params.focus_rect));
  Java_ContentViewCore_onSelectionBoundsChanged(env, obj.obj(),
                                                anchor_rect_dip.obj(),
                                                params.anchor_dir,
                                                focus_rect_dip.obj(),
                                                params.focus_dir,
                                                params.is_anchor_first);
}

void ContentViewCoreImpl::ShowPastePopup(int x_dip, int y_dip) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_showPastePopup(env, obj.obj(),
                                      static_cast<jint>(x_dip),
                                      static_cast<jint>(y_dip));
}

unsigned int ContentViewCoreImpl::GetScaledContentTexture(
    float scale,
    gfx::Size* out_size) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return 0;

  return view->GetScaledContentTexture(scale, out_size);
}

void ContentViewCoreImpl::AddFrameInfoCallback(
    const UpdateFrameInfoCallback& callback) {
  update_frame_info_callbacks_.push_back(callback);
}

void ContentViewCoreImpl::RemoveFrameInfoCallback(
    const UpdateFrameInfoCallback& callback) {
  for (size_t i = 0; i < update_frame_info_callbacks_.size(); ++i) {
    if (update_frame_info_callbacks_[i].Equals(callback)) {
      update_frame_info_callbacks_.erase(
          update_frame_info_callbacks_.begin() + i);
      return;
    }
  }
}

void ContentViewCoreImpl::StartContentIntent(const GURL& content_url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jcontent_url =
      ConvertUTF8ToJavaString(env, content_url.spec());
  Java_ContentViewCore_startContentIntent(env,
                                          j_obj.obj(),
                                          jcontent_url.obj());
}

void ContentViewCoreImpl::ShowDisambiguationPopup(
    const gfx::Rect& target_rect,
    const SkBitmap& zoomed_bitmap) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jobject> rect_object(CreateJavaRect(env, target_rect));

  ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(&zoomed_bitmap);
  DCHECK(!java_bitmap.is_null());

  Java_ContentViewCore_showDisambiguationPopup(env,
                                               obj.obj(),
                                               rect_object.obj(),
                                               java_bitmap.obj());
}

void ContentViewCoreImpl::RequestExternalVideoSurface(int player_id) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_ContentViewCore_requestExternalVideoSurface(
      env, obj.obj(), static_cast<jint>(player_id));
}

void ContentViewCoreImpl::NotifyGeometryChange(int player_id,
                                               const gfx::RectF& rect) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_ContentViewCore_notifyGeometryChange(env,
                                            obj.obj(),
                                            static_cast<jint>(player_id),
                                            static_cast<jfloat>(rect.x()),
                                            static_cast<jfloat>(rect.y()),
                                            static_cast<jfloat>(rect.width()),
                                            static_cast<jfloat>(rect.height()));
}

gfx::Size ContentViewCoreImpl::GetPhysicalBackingSize() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return gfx::Size();
  return gfx::Size(
      Java_ContentViewCore_getPhysicalBackingWidthPix(env, j_obj.obj()),
      Java_ContentViewCore_getPhysicalBackingHeightPix(env, j_obj.obj()));
}

gfx::Size ContentViewCoreImpl::GetViewportSizePix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return gfx::Size();
  return gfx::Size(
      Java_ContentViewCore_getViewportWidthPix(env, j_obj.obj()),
      Java_ContentViewCore_getViewportHeightPix(env, j_obj.obj()));
}

gfx::Size ContentViewCoreImpl::GetViewportSizeOffsetPix() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return gfx::Size();
  return gfx::Size(
      Java_ContentViewCore_getViewportSizeOffsetWidthPix(env, j_obj.obj()),
      Java_ContentViewCore_getViewportSizeOffsetHeightPix(env, j_obj.obj()));
}

gfx::Size ContentViewCoreImpl::GetViewportSizeDip() const {
  return gfx::ToCeiledSize(
      gfx::ScaleSize(GetViewportSizePix(), 1.0f / GetDpiScale()));
}

gfx::Size ContentViewCoreImpl::GetViewportSizeOffsetDip() const {
  return gfx::ToCeiledSize(
      gfx::ScaleSize(GetViewportSizeOffsetPix(), 1.0f / GetDpiScale()));
}

float ContentViewCoreImpl::GetOverdrawBottomHeightDip() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return 0.f;
  return Java_ContentViewCore_getOverdrawBottomHeightPix(env, j_obj.obj())
      / GetDpiScale();
}

InputEventAckState ContentViewCoreImpl::FilterInputEvent(
    const WebKit::WebInputEvent& input_event) {
  if (!input_event_filter_)
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  return input_event_filter_->HandleInputEvent(input_event);
}

void ContentViewCoreImpl::AttachLayer(scoped_refptr<cc::Layer> layer) {
  root_layer_->AddChild(layer);
}

void ContentViewCoreImpl::RemoveLayer(scoped_refptr<cc::Layer> layer) {
  layer->RemoveFromParent();
}

void ContentViewCoreImpl::OnTabRestore(JNIEnv* env, jobject obj) {
    LOG(INFO)  << "ContentViewCoreImpl::OnTabRestore";
    tab_crashed_ = false;
}

void ContentViewCoreImpl::LoadUrl(	 	
    NavigationController::LoadURLParams& params) {
    LOG(INFO)  << "ContentViewCoreImpl::LoadUrl";
  GetWebContents()->GetController().LoadURLWithParams(params);
  tab_crashed_ = false;
}

void ContentViewCoreImpl::SetVSyncNotificationEnabled(bool enabled) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_setVSyncNotificationEnabled(
      env, obj.obj(), static_cast<jboolean>(enabled));
}

ui::ViewAndroid* ContentViewCoreImpl::GetViewAndroid() const {
  // view_android_ should never be null for Chrome.
  DCHECK(view_android_);
  return view_android_;
}

ui::WindowAndroid* ContentViewCoreImpl::GetWindowAndroid() const {
  // This should never be NULL for Chrome, but will be NULL for WebView.
  DCHECK(window_android_);
  return window_android_;
}

scoped_refptr<cc::Layer> ContentViewCoreImpl::GetLayer() const {
  return root_layer_.get();
}

// ----------------------------------------------------------------------------
// Methods called from Java via JNI
// ----------------------------------------------------------------------------

void ContentViewCoreImpl::SelectPopupMenuItems(JNIEnv* env, jobject obj,
                                               jintArray indices) {
  RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(
      web_contents_->GetRenderViewHost());
  DCHECK(rvhi);
  if (indices == NULL) {
    rvhi->DidCancelPopupMenu();
    return;
  }

  int selected_count = env->GetArrayLength(indices);
  std::vector<int> selected_indices;
  jint* indices_ptr = env->GetIntArrayElements(indices, NULL);
  for (int i = 0; i < selected_count; ++i)
    selected_indices.push_back(indices_ptr[i]);
  env->ReleaseIntArrayElements(indices, indices_ptr, JNI_ABORT);
  rvhi->DidSelectPopupMenuItems(selected_indices);
}

void ContentViewCoreImpl::LoadUrl(
    JNIEnv* env, jobject obj,
    jstring url,
    jint load_url_type,
    jint transition_type,
    jint ua_override_option,
    jstring extra_headers,
    jbyteArray post_data,
    jstring base_url_for_data_url,
    jstring virtual_url_for_data_url,
    jboolean can_load_local_resources) {
  DCHECK(url);
  NavigationController::LoadURLParams params(
      GURL(ConvertJavaStringToUTF8(env, url)));

  params.load_type = static_cast<NavigationController::LoadURLType>(
      load_url_type);
  params.transition_type = PageTransitionFromInt(transition_type);
  params.override_user_agent =
      static_cast<NavigationController::UserAgentOverrideOption>(
          ua_override_option);

  if (extra_headers)
    params.extra_headers = ConvertJavaStringToUTF8(env, extra_headers);

  if (post_data) {
    std::vector<uint8> http_body_vector;
    base::android::JavaByteArrayToByteVector(env, post_data, &http_body_vector);
    params.browser_initiated_post_data =
        base::RefCountedBytes::TakeVector(&http_body_vector);
  }

  if (base_url_for_data_url) {
    params.base_url_for_data_url =
        GURL(ConvertJavaStringToUTF8(env, base_url_for_data_url));
  }

  if (virtual_url_for_data_url) {
    params.virtual_url_for_data_url =
        GURL(ConvertJavaStringToUTF8(env, virtual_url_for_data_url));
  }

  params.can_load_local_resources = can_load_local_resources;

  LoadUrl(params);
}

jint ContentViewCoreImpl::GetCurrentRenderProcessId(JNIEnv* env, jobject obj) {
  return GetRenderProcessIdFromRenderViewHost(
      web_contents_->GetRenderViewHost());
}

ScopedJavaLocalRef<jstring> ContentViewCoreImpl::GetURL(
    JNIEnv* env, jobject) const {
  return ConvertUTF8ToJavaString(env, GetWebContents()->GetURL().spec());
}

ScopedJavaLocalRef<jstring> ContentViewCoreImpl::GetTitle(
    JNIEnv* env, jobject obj) const {
  return ConvertUTF16ToJavaString(env, GetWebContents()->GetTitle());
}

jboolean ContentViewCoreImpl::IsIncognito(JNIEnv* env, jobject obj) {
  return GetWebContents()->GetBrowserContext()->IsOffTheRecord();
}

WebContents* ContentViewCoreImpl::GetWebContents() const {
  return web_contents_;
}

void ContentViewCoreImpl::SetFocus(JNIEnv* env, jobject obj, jboolean focused) {
  if (!GetRenderWidgetHostViewAndroid())
    return;

  if (focused)
    GetRenderWidgetHostViewAndroid()->Focus();
  else
    GetRenderWidgetHostViewAndroid()->Blur();
}

void ContentViewCoreImpl::SendOrientationChangeEvent(JNIEnv* env,
                                                     jobject obj,
                                                     jint orientation) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->UpdateScreenInfo(rwhv->GetNativeView());

  RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(
      web_contents_->GetRenderViewHost());
  rvhi->SendOrientationChangeEvent(orientation);
}

jboolean ContentViewCoreImpl::SendTouchEvent(JNIEnv* env,
                                             jobject obj,
                                             jlong time_ms,
                                             jint type,
                                             jobjectArray pts) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv) {
    using WebKit::WebTouchEvent;
    WebKit::WebTouchEvent event;
    TouchPoint::BuildWebTouchEvent(env, type, time_ms, GetDpiScale(), pts,
        event);
    rwhv->SendTouchEvent(event);
    return true;
  }
  return false;
}

float ContentViewCoreImpl::GetTouchPaddingDip() {
  return 48.0f / GetDpiScale();
}

float ContentViewCoreImpl::GetDpiScale() const {
  return dpi_scale_;
}

void ContentViewCoreImpl::SetInputHandler(
    WebKit::WebCompositorInputHandler* input_handler) {
  if (!input_event_filter_)
    input_event_filter_.reset(new SyncInputEventFilter);

  input_event_filter_->SetInputHandler(input_handler);
}

void ContentViewCoreImpl::RequestContentClipping(
    const gfx::Rect& clipping,
    const gfx::Size& content_size) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->RequestContentClipping(clipping, content_size);
}

jboolean ContentViewCoreImpl::SendMouseMoveEvent(JNIEnv* env,
                                                 jobject obj,
                                                 jlong time_ms,
                                                 jfloat x,
                                                 jfloat y) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (!rwhv)
    return false;

  WebKit::WebMouseEvent event = WebInputEventFactory::mouseEvent(
      WebInputEventFactory::MouseEventTypeMove,
      WebKit::WebMouseEvent::ButtonNone,
      time_ms / 1000.0, x / GetDpiScale(), y / GetDpiScale(), 0, 1);

  rwhv->SendMouseEvent(event);
  return true;
}

jboolean ContentViewCoreImpl::SendMouseWheelEvent(JNIEnv* env,
                                                  jobject obj,
                                                  jlong time_ms,
                                                  jfloat x,
                                                  jfloat y,
                                                  jfloat vertical_axis) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (!rwhv)
    return false;

  WebKit::WebInputEventFactory::MouseWheelDirectionType type;
  if (vertical_axis > 0) {
    type = WebInputEventFactory::MouseWheelDirectionTypeUp;
  } else if (vertical_axis < 0) {
    type = WebInputEventFactory::MouseWheelDirectionTypeDown;
  } else {
    return false;
  }
  WebKit::WebMouseWheelEvent event = WebInputEventFactory::mouseWheelEvent(
      type, time_ms / 1000.0, x / GetDpiScale(), y / GetDpiScale());

  rwhv->SendMouseWheelEvent(event);
  return true;
}

WebGestureEvent ContentViewCoreImpl::MakeGestureEvent(
    WebInputEvent::Type type, long time_ms, float x, float y,
    InputEventVSyncStatus vsync_status) const {
  WebGestureEvent event;
  event.type = type;
  event.x = x / GetDpiScale();
  event.y = y / GetDpiScale();
  event.timeStampSeconds = time_ms / 1000.0;
  event.sourceDevice = WebGestureEvent::Touchscreen;
  if (vsync_status == LAST_INPUT_EVENT_FOR_VSYNC)
    event.modifiers |= WebInputEvent::IsLastInputEventForCurrentVSync;
  return event;
}

void ContentViewCoreImpl::SendGestureEvent(
    const WebKit::WebGestureEvent& event) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollBegin(JNIEnv* env, jobject obj, jlong time_ms,
                                      jfloat x, jfloat y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureScrollBegin, time_ms, x, y,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollEnd(JNIEnv* env, jobject obj, jlong time_ms) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureScrollEnd, time_ms, 0, 0,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::ScrollBy(JNIEnv* env, jobject obj, jlong time_ms,
                                   jfloat x, jfloat y, jfloat dx, jfloat dy,
                                   jboolean last_input_event_for_vsync) {
  InputEventVSyncStatus vsync_status =
      last_input_event_for_vsync ? LAST_INPUT_EVENT_FOR_VSYNC
                                 : NOT_LAST_INPUT_EVENT_FOR_VSYNC;
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureScrollUpdate, time_ms, x, y, vsync_status);
  event.data.scrollUpdate.deltaX = -dx / GetDpiScale();
  event.data.scrollUpdate.deltaY = -dy / GetDpiScale();

  SendGestureEvent(event);
}

void ContentViewCoreImpl::FlingStart(JNIEnv* env, jobject obj, jlong time_ms,
                                     jfloat x, jfloat y, jfloat vx, jfloat vy) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureFlingStart, time_ms, x, y,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);

  // Velocity should not be scaled by DIP since that interacts poorly with the
  // deceleration constants.  The DIP scaling is done on the renderer.
  event.data.flingStart.velocityX = vx;
  event.data.flingStart.velocityY = vy;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::FlingCancel(JNIEnv* env, jobject obj, jlong time_ms) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureFlingCancel, time_ms, 0, 0,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::SingleTap(JNIEnv* env, jobject obj, jlong time_ms,
                                    jfloat x, jfloat y,
                                    jboolean disambiguation_popup_tap) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureTap, time_ms, x, y,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);

  event.data.tap.tapCount = 1;
  if (disambiguation_popup_tap) {
    const float touch_padding_dip = GetTouchPaddingDip();
    event.data.tap.width = touch_padding_dip;
    event.data.tap.height = touch_padding_dip;
  }

  SendGestureEvent(event);
}

void ContentViewCoreImpl::ShowPressState(JNIEnv* env, jobject obj,
                                         jlong time_ms,
                                         jfloat x, jfloat y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureTapDown, time_ms, x, y,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::ShowPressCancel(JNIEnv* env,
                                          jobject obj,
                                          jlong time_ms,
                                          jfloat x,
                                          jfloat y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureTapCancel, time_ms, x, y,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::DoubleTap(JNIEnv* env, jobject obj, jlong time_ms,
                                    jfloat x, jfloat y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureDoubleTap, time_ms, x, y,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::LongPress(JNIEnv* env, jobject obj, jlong time_ms,
                                    jfloat x, jfloat y,
                                    jboolean disambiguation_popup_tap) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureLongPress, time_ms, x, y,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);

  if (!disambiguation_popup_tap) {
    const float touch_padding_dip = GetTouchPaddingDip();
    event.data.longPress.width = touch_padding_dip;
    event.data.longPress.height = touch_padding_dip;
  }

  SendGestureEvent(event);
}

void ContentViewCoreImpl::LongTap(JNIEnv* env, jobject obj, jlong time_ms,
                                  jfloat x, jfloat y,
                                  jboolean disambiguation_popup_tap) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GestureLongTap, time_ms, x, y,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);

  if (!disambiguation_popup_tap) {
    const float touch_padding_dip = GetTouchPaddingDip();
    event.data.longPress.width = touch_padding_dip;
    event.data.longPress.height = touch_padding_dip;
  }

  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchBegin(JNIEnv* env, jobject obj, jlong time_ms,
                                     jfloat x, jfloat y) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GesturePinchBegin, time_ms, x, y,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchEnd(JNIEnv* env, jobject obj, jlong time_ms) {
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GesturePinchEnd, time_ms, 0, 0,
      NOT_LAST_INPUT_EVENT_FOR_VSYNC);
  SendGestureEvent(event);
}

void ContentViewCoreImpl::PinchBy(JNIEnv* env, jobject obj, jlong time_ms,
                                  jfloat anchor_x, jfloat anchor_y,
                                  jfloat delta,
                                  jboolean last_input_event_for_vsync) {
  InputEventVSyncStatus vsync_status =
      last_input_event_for_vsync ? LAST_INPUT_EVENT_FOR_VSYNC
                                 : NOT_LAST_INPUT_EVENT_FOR_VSYNC;
  WebGestureEvent event = MakeGestureEvent(
      WebInputEvent::GesturePinchUpdate, time_ms, anchor_x, anchor_y,
      vsync_status);
  event.data.pinchUpdate.scale = delta;

  SendGestureEvent(event);
}

void ContentViewCoreImpl::SelectBetweenCoordinates(JNIEnv* env, jobject obj,
                                                   jfloat x1, jfloat y1,
                                                   jfloat x2, jfloat y2) {
  if (GetRenderWidgetHostViewAndroid()) {
    GetRenderWidgetHostViewAndroid()->SelectRange(
        gfx::Point(x1 / GetDpiScale(), y1 / GetDpiScale()),
        gfx::Point(x2 / GetDpiScale(), y2 / GetDpiScale()));
  }
}

void ContentViewCoreImpl::MoveCaret(JNIEnv* env, jobject obj,
                                    jfloat x, jfloat y) {
  if (GetRenderWidgetHostViewAndroid()) {
    GetRenderWidgetHostViewAndroid()->MoveCaret(
        gfx::Point(x / GetDpiScale(), y / GetDpiScale()));
  }
}

jboolean ContentViewCoreImpl::CanGoBack(JNIEnv* env, jobject obj) {
  return web_contents_->GetController().CanGoBack();
}

jboolean ContentViewCoreImpl::CanGoForward(JNIEnv* env, jobject obj) {
  return web_contents_->GetController().CanGoForward();
}

jboolean ContentViewCoreImpl::CanGoToOffset(JNIEnv* env, jobject obj,
                                            jint offset) {
  return web_contents_->GetController().CanGoToOffset(offset);
}

void ContentViewCoreImpl::GoBack(JNIEnv* env, jobject obj) {
	LOG(INFO)  << "ContentViewCoreImpl::GoBack";
  web_contents_->GetController().GoBack();
  tab_crashed_ = false;
}

void ContentViewCoreImpl::GoForward(JNIEnv* env, jobject obj) {
	LOG(INFO)  << "ContentViewCoreImpl::GoForward";
  web_contents_->GetController().GoForward();
  tab_crashed_ = false;
}

void ContentViewCoreImpl::GoToOffset(JNIEnv* env, jobject obj, jint offset) {
  web_contents_->GetController().GoToOffset(offset);
}

void ContentViewCoreImpl::GoToNavigationIndex(JNIEnv* env,
                                              jobject obj,
                                              jint index) {
  web_contents_->GetController().GoToIndex(index);
}

void ContentViewCoreImpl::StopLoading(JNIEnv* env, jobject obj) {
	LOG(INFO)  << "ContentViewCoreImpl::StopLoading";
  web_contents_->Stop();
}

void ContentViewCoreImpl::Reload(JNIEnv* env, jobject obj) {
	LOG(INFO)  << "ContentViewCoreImpl::Reload";
  // Set check_for_repost parameter to false as we have no repost confirmation
  // dialog ("confirm form resubmission" screen will still appear, however).
  if (web_contents_->GetController().NeedsReload())
    web_contents_->GetController().LoadIfNecessary();
  else
    web_contents_->GetController().Reload(true);
  tab_crashed_ = false;
}

void ContentViewCoreImpl::CancelPendingReload(JNIEnv* env, jobject obj) {
  web_contents_->GetController().CancelPendingReload();
}

void ContentViewCoreImpl::ContinuePendingReload(JNIEnv* env, jobject obj) {
  web_contents_->GetController().ContinuePendingReload();
}

void ContentViewCoreImpl::ClearHistory(JNIEnv* env, jobject obj) {
  web_contents_->GetController().PruneAllButActive();
}

void ContentViewCoreImpl::AddJavascriptInterface(
    JNIEnv* env,
    jobject /* obj */,
    jobject object,
    jstring name,
    jclass safe_annotation_clazz,
    jobject retained_object_set) {
  ScopedJavaLocalRef<jobject> scoped_object(env, object);
  ScopedJavaLocalRef<jclass> scoped_clazz(env, safe_annotation_clazz);
  JavaObjectWeakGlobalRef weak_retained_object_set(env, retained_object_set);

  // JavaBoundObject creates the NPObject with a ref count of 1, and
  // JavaBridgeDispatcherHostManager takes its own ref.
  JavaBridgeDispatcherHostManager* java_bridge =
      web_contents_->java_bridge_dispatcher_host_manager();
  java_bridge->SetRetainedObjectSet(weak_retained_object_set);
  NPObject* bound_object = JavaBoundObject::Create(scoped_object, scoped_clazz,
                                                   java_bridge->AsWeakPtr());
  java_bridge->AddNamedObject(ConvertJavaStringToUTF16(env, name),
                              bound_object);
  WebKit::WebBindings::releaseObject(bound_object);
}

void ContentViewCoreImpl::RemoveJavascriptInterface(JNIEnv* env,
                                                    jobject /* obj */,
                                                    jstring name) {
  web_contents_->java_bridge_dispatcher_host_manager()->RemoveNamedObject(
      ConvertJavaStringToUTF16(env, name));
}

void ContentViewCoreImpl::UpdateVSyncParameters(JNIEnv* env, jobject /* obj */,
                                                jlong timebase_micros,
                                                jlong interval_micros) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return;

  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      view->GetRenderWidgetHost());

  host->UpdateVSyncParameters(
      base::TimeTicks::FromInternalValue(timebase_micros),
      base::TimeDelta::FromMicroseconds(interval_micros));
}

void ContentViewCoreImpl::OnVSync(JNIEnv* env, jobject /* obj */,
                                  jlong frame_time_micros) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return;

  view->SendVSync(base::TimeTicks::FromInternalValue(frame_time_micros));
}

void ContentViewCoreImpl::NeedToUpdateFrameInfo(JNIEnv* env, jobject /* obj */,
                                  jboolean enable) {
#if defined(SBROWSER_BLOCK_SENDING_FRAME_MESSAGE)
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return;

  view->NeedToUpdateFrameInfo(enable);
#endif
}

jboolean ContentViewCoreImpl::PopulateBitmapFromCompositor(JNIEnv* env,
                                                           jobject obj,
                                                           jobject jbitmap) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view)
    return false;

  return view->PopulateBitmapWithContents(jbitmap);
}

void ContentViewCoreImpl::WasResized(JNIEnv* env, jobject obj) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (view)
    view->WasResized();
}

void ContentViewCoreImpl::ShowInterstitialPage(
    JNIEnv* env, jobject obj, jstring jurl, jint delegate_ptr) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  InterstitialPageDelegateAndroid* delegate =
      reinterpret_cast<InterstitialPageDelegateAndroid*>(delegate_ptr);
  InterstitialPage* interstitial = InterstitialPage::Create(
      web_contents_, false, url, delegate);
  delegate->set_interstitial_page(interstitial);
  interstitial->Show();
}

jboolean ContentViewCoreImpl::IsShowingInterstitialPage(JNIEnv* env,
                                                        jobject obj) {
  return web_contents_->ShowingInterstitialPage();
}

void ContentViewCoreImpl::AttachExternalVideoSurface(JNIEnv* env,
                                                     jobject obj,
                                                     jint player_id,
                                                     jobject jsurface) {
#if defined(GOOGLE_TV)
  RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(
      web_contents_->GetRenderViewHost());
  if (rvhi && rvhi->media_player_manager()) {
    rvhi->media_player_manager()->AttachExternalVideoSurface(
        static_cast<int>(player_id), jsurface);
  }
#endif
}

void ContentViewCoreImpl::DetachExternalVideoSurface(JNIEnv* env,
                                                     jobject obj,
                                                     jint player_id) {
#if defined(GOOGLE_TV)
  RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(
      web_contents_->GetRenderViewHost());
  if (rvhi && rvhi->media_player_manager()) {
    rvhi->media_player_manager()->DetachExternalVideoSurface(
        static_cast<int>(player_id));
  }
#endif
}

jboolean ContentViewCoreImpl::IsRenderWidgetHostViewReady(JNIEnv* env,
                                                          jobject obj) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  return view && view->HasValidFrame();
}

void ContentViewCoreImpl::ExitFullscreen(JNIEnv* env, jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  host->ExitFullscreen();
}

void ContentViewCoreImpl::UpdateTopControlsState(JNIEnv* env,
                                                 jobject obj,
                                                 bool enable_hiding,
                                                 bool enable_showing,
                                                 bool animate) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  host->Send(new ViewMsg_UpdateTopControlsState(host->GetRoutingID(),
                                                enable_hiding,
                                                enable_showing,
                                                animate));
}

void ContentViewCoreImpl::ShowImeIfNeeded(JNIEnv* env, jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  host->Send(new ViewMsg_ShowImeIfNeeded(host->GetRoutingID()));
}

void ContentViewCoreImpl::ScrollFocusedEditableNodeIntoView(JNIEnv* env,
                                                            jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  host->Send(new InputMsg_ScrollFocusedEditableNodeIntoRect(
      host->GetRoutingID(), gfx::Rect()));
}

namespace {

static void AddNavigationEntryToHistory(JNIEnv* env, jobject obj,
                                        jobject history,
                                        NavigationEntry* entry,
                                        int index) {
  // Get the details of the current entry
  ScopedJavaLocalRef<jstring> j_url(
      ConvertUTF8ToJavaString(env, entry->GetURL().spec()));
  ScopedJavaLocalRef<jstring> j_virtual_url(
      ConvertUTF8ToJavaString(env, entry->GetVirtualURL().spec()));
  ScopedJavaLocalRef<jstring> j_original_url(
      ConvertUTF8ToJavaString(env, entry->GetOriginalRequestURL().spec()));
  ScopedJavaLocalRef<jstring> j_title(
      ConvertUTF16ToJavaString(env, entry->GetTitle()));
  //ScopedJavaLocalRef<jobject> j_bitmap;
  //const FaviconStatus& status = entry->GetFavicon();
   //if (status.valid && status.image.ToSkBitmap()->getSize() > 0)
   //  j_bitmap = gfx::ConvertToJavaBitmap(status.image.ToSkBitmap());
   // Add the item to the list
   Java_ContentViewCore_addToNavigationHistory(
       env, obj, history, index, j_url.obj(), j_virtual_url.obj(),
       j_original_url.obj(), j_title.obj()/*, j_bitmap.obj()*/);
}

}  // namespace

int ContentViewCoreImpl::GetNavigationHistory(JNIEnv* env,
                                              jobject obj,
                                              jobject history) {
  // Iterate through navigation entries to populate the list
  const NavigationController& controller = web_contents_->GetController();
  int count = controller.GetEntryCount();
  for (int i = 0; i < count; ++i) {
    AddNavigationEntryToHistory(
        env, obj, history, controller.GetEntryAtIndex(i), i);
  }

  return controller.GetCurrentEntryIndex();
}

void ContentViewCoreImpl::GetDirectedNavigationHistory(JNIEnv* env,
                                                       jobject obj,
                                                       jobject history,
                                                       jboolean is_forward,
                                                       jint max_entries) {
  // Iterate through navigation entries to populate the list
  const NavigationController& controller = web_contents_->GetController();
  int count = controller.GetEntryCount();
  int num_added = 0;
  int increment_value = is_forward ? 1 : -1;
  for (int i = controller.GetCurrentEntryIndex() + increment_value;
       i >= 0 && i < count;
       i += increment_value) {
    if (num_added >= max_entries)
      break;
    AddNavigationEntryToHistory(
        env, obj, history, controller.GetEntryAtIndex(i), i);
    num_added++;
  }
}

ScopedJavaLocalRef<jstring>
ContentViewCoreImpl::GetOriginalUrlForActiveNavigationEntry(JNIEnv* env,
                                                            jobject obj) {
  NavigationEntry* entry = web_contents_->GetController().GetActiveEntry();
  if (entry == NULL)
    return ScopedJavaLocalRef<jstring>(env, NULL);
  return ConvertUTF8ToJavaString(env, entry->GetOriginalRequestURL().spec());
}

int ContentViewCoreImpl::GetNativeImeAdapter(JNIEnv* env, jobject obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return 0;
  return rwhva->GetNativeImeAdapter();
}

jboolean ContentViewCoreImpl::NeedsReload(JNIEnv* env, jobject obj) {
  return web_contents_->GetController().NeedsReload();
}

void ContentViewCoreImpl::UndoScrollFocusedEditableNodeIntoView(
    JNIEnv* env,
    jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  host->Send(
      new ViewMsg_UndoScrollFocusedEditableNodeIntoView(host->GetRoutingID()));
}

namespace {
void JavaScriptResultCallback(const ScopedJavaGlobalRef<jobject>& callback,
                              const base::Value* result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(result, &json);
  ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
  Java_ContentViewCore_onEvaluateJavaScriptResult(env,
                                                  j_json.obj(),
                                                  callback.obj());
}
}  // namespace

void ContentViewCoreImpl::EvaluateJavaScript(JNIEnv* env,
                                             jobject obj,
                                             jstring script,
                                             jobject callback) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  DCHECK(host);

  if (!callback) {
    // No callback requested.
    host->ExecuteJavascriptInWebFrame(string16(),  // frame_xpath
                                      ConvertJavaStringToUTF16(env, script));
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  content::RenderViewHost::JavascriptResultCallback c_callback =
      base::Bind(&JavaScriptResultCallback, j_callback);

  host->ExecuteJavascriptInWebFrameCallbackResult(
      string16(),  // frame_xpath
      ConvertJavaStringToUTF16(env, script),
      c_callback);
}

bool ContentViewCoreImpl::GetUseDesktopUserAgent(
    JNIEnv* env, jobject obj) {
  NavigationEntry* entry = web_contents_->GetController().GetActiveEntry();
  return entry && entry->GetIsOverridingUserAgent();
}

void ContentViewCoreImpl::UpdateImeAdapter(int native_ime_adapter,
                                           int text_input_type,
                                           const std::string& text,
                                           int selection_start,
                                           int selection_end,
                                           int composition_start,
                                           int composition_end,
                                           bool show_ime_if_needed) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jstring_text = ConvertUTF8ToJavaString(env, text);
  Java_ContentViewCore_updateImeAdapter(env, obj.obj(),
                                        native_ime_adapter, text_input_type,
                                        jstring_text.obj(),
                                        selection_start, selection_end,
                                        composition_start, composition_end,
                                        show_ime_if_needed);
}

void ContentViewCoreImpl::ProcessImeBatchStateAck(bool is_begin) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCore_processImeBatchStateAck(env, obj.obj(), is_begin);
}

void ContentViewCoreImpl::ClearSslPreferences(JNIEnv* env, jobject obj) {
  SSLHostState* state = SSLHostState::GetFor(
      web_contents_->GetController().GetBrowserContext());
  state->Clear();
}

void ContentViewCoreImpl::SetUseDesktopUserAgent(
    JNIEnv* env,
    jobject obj,
    jboolean enabled,
    jboolean reload_on_state_change) {
  if (GetUseDesktopUserAgent(env, obj) == enabled)
    return;

  // Make sure the navigation entry actually exists.
  NavigationEntry* entry = web_contents_->GetController().GetActiveEntry();
  if (!entry)
    return;

  // Set the flag in the NavigationEntry.
  entry->SetIsOverridingUserAgent(enabled);

  // Send the override to the renderer.
  if (reload_on_state_change) {
    // Reloading the page will send the override down as part of the
    // navigation IPC message.
    NavigationControllerImpl& controller =
        static_cast<NavigationControllerImpl&>(web_contents_->GetController());
	if(enabled)
	LOG(INFO) << "URLUpdate ContentViewCoreImpl::SetUseDesktopUserAgent desktop UA";
	else
	LOG(INFO) << "URLUpdate ContentViewCoreImpl::SetUseDesktopUserAgent mobile UA";
    controller.ReloadOriginalRequestURL(false);
  }
}

// This is called for each ContentView.
jint Init(JNIEnv* env, jobject obj,
          jboolean hardware_accelerated,
          jint native_web_contents,
          jint view_android,
          jint window_android) {
  ContentViewCoreImpl* view = new ContentViewCoreImpl(
      env, obj, hardware_accelerated,
      reinterpret_cast<WebContents*>(native_web_contents),
      reinterpret_cast<ui::ViewAndroid*>(view_android),
      reinterpret_cast<ui::WindowAndroid*>(window_android));
  return reinterpret_cast<jint>(view);
}

bool RegisterContentViewCore(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
