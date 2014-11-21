// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

#include <map>
#include <queue>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "cc/layers/texture_layer_client.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d_f.h"
#if defined(SBROWSER_HIDE_URLBAR)
#include "cc/sbr/urlbar_utils.h"
#endif

struct ViewHostMsg_TextInputState_Params;

struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
struct GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params;

namespace cc {
class Layer;
class TextureLayer;
}

namespace WebKit {
class WebExternalTextureLayer;
class WebTouchEvent;
class WebMouseEvent;
}

namespace content {
class ContentViewCoreImpl;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class SurfaceTextureTransportClient;
struct NativeWebKeyboardEvent;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class RenderWidgetHostViewAndroid : public RenderWidgetHostViewBase,
                                    public cc::TextureLayerClient {
 public:
  RenderWidgetHostViewAndroid(RenderWidgetHostImpl* widget,
                              ContentViewCoreImpl* content_view_core);
  virtual ~RenderWidgetHostViewAndroid();

  // RenderWidgetHostView implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE;
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual void MovePluginWindows(
      const gfx::Vector2d& scroll_offset,
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual gfx::Size GetPhysicalBackingSize() const OVERRIDE;
  virtual float GetOverdrawBottomHeight() const OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void ImeCompositionRangeChanged(
      const ui::Range& range,
      const std::vector<gfx::Rect>& character_bounds) OVERRIDE;
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect,
      const gfx::Vector2d& scroll_delta,
      const std::vector<gfx::Rect>& copy_rects) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const string16& tooltip_text) OVERRIDE;
  virtual void SelectionChanged(const string16& text,
                                size_t offset,
                                const ui::Range& range) OVERRIDE;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) OVERRIDE;
  virtual void ScrollOffsetChanged() OVERRIDE;
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceSuspend() OVERRIDE;
  virtual void AcceleratedSurfaceRelease() OVERRIDE;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) OVERRIDE;
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) OVERRIDE;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool CanCopyToVideoFrame() const OVERRIDE;
  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) OVERRIDE;
  virtual gfx::Rect GetBoundsInRootWindow() OVERRIDE;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;
  virtual void ProcessAckedTouchEvent(const WebKit::WebTouchEvent& touch,
                                      InputEventAckState ack_result) OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual void UnhandledWheelEvent(
      const WebKit::WebMouseWheelEvent& event) OVERRIDE;
  virtual InputEventAckState FilterInputEvent(
      const WebKit::WebInputEvent& input_event) OVERRIDE;
  virtual void OnAccessibilityNotifications(
      const std::vector<AccessibilityHostMsg_NotificationParams>&
          params) OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;
  virtual void HasTouchEventHandlers(bool need_touch_events) OVERRIDE;
  virtual void OnSwapCompositorFrame(
      scoped_ptr<cc::CompositorFrame> frame) OVERRIDE;
  virtual void ShowDisambiguationPopup(const gfx::Rect& target_rect,
                                       const SkBitmap& zoomed_bitmap) OVERRIDE;
#if defined(SBROWSER_HIDE_URLBAR)
  virtual void OnHideUrlBarEventNotify(int event, int event_data1, int event_data2) OVERRIDE;
#endif
#if defined(SBROWSER_FLING_END_NOTIFICATION_CANE)
  virtual void OnFlingAnimationCompleted(bool page_end) OVERRIDE;
#endif

  // cc::TextureLayerClient implementation.
  virtual unsigned PrepareTexture(cc::ResourceUpdateQueue* queue) OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE;
  virtual bool PrepareTextureMailbox(cc::TextureMailbox* mailbox) OVERRIDE;

  // for webselectdialog to go down
#if defined(SBROWSER_FORM_NAVIGATION)
  virtual void selectPopupClose() OVERRIDE;
  virtual void selectPopupCloseZero() OVERRIDE;
#endif
#if defined (SBROWSER_MULTIINSTANCE_TAB_DRAG_AND_DROP)
  virtual bool GetTabDragAndDropIsInProgress() const OVERRIDE; 
#endif
  // Non-virtual methods
  void SetContentViewCore(ContentViewCoreImpl* content_view_core);
  SkColor GetCachedBackgroundColor() const;
  void SendKeyEvent(const NativeWebKeyboardEvent& event);
  void SendTouchEvent(const WebKit::WebTouchEvent& event);
  void SendMouseEvent(const WebKit::WebMouseEvent& event);
  void SendMouseWheelEvent(const WebKit::WebMouseWheelEvent& event);
  void SendGestureEvent(const WebKit::WebGestureEvent& event);
  void SendVSync(base::TimeTicks frame_time);
#if defined(SBROWSER_BLOCK_SENDING_FRAME_MESSAGE)
  void NeedToUpdateFrameInfo(bool enabled);
#endif
  void OnProcessImeBatchStateAck(bool is_begin);
  void OnDidChangeBodyBackgroundColor(SkColor color);
  void OnStartContentIntent(const GURL& content_url);
  void OnSetVSyncNotificationEnabled(bool enabled);

  int GetNativeImeAdapter();

  void WasResized();

  WebKit::WebGLId GetScaledContentTexture(float scale, gfx::Size* out_size);
  bool PopulateBitmapWithContents(jobject jbitmap);

  bool HasValidFrame() const;

  // Select all text between the given coordinates.
  void SelectRange(const gfx::Point& start, const gfx::Point& end);


#if defined (SBROWSER_ENABLE_ECHO_PWD)
	void SetPasswordEcho(bool pwdEchoEnabled);
#endif


#if defined (SBROWSER_SAVEPAGE_IMPL)
  void GetScrapBitmap(int bitmapWidth, int bitmapHeight);
  void GetBitmapFromCachedResource(std::string imgUrl);
#endif
#if defined (SBROWSER_PRINT_IMPL)
  void PrintBegin();
  void PrintPage(int pageNum);
  void PrintEnd();
#endif

#if defined(SBROWSER_FPAUTH_IMPL)
 void FPAuthStatus(bool enabled, std::string usrName);
#endif

#if defined(SBROWSER_TEXT_SELECTION)
void GetSelectionBitmap();
void selectClosestWord(int x , int y);
void clearTextSelection();
void CheckBelongToSelection(int TouchX, int TouchY);
void RequestSelectionVisibilityStatus();
void GetSelectionRect();
void UnmarkSelection();
#endif
#if defined(SBROWSER_SMART_CLIP)
void RequestSmartClipData(int x, int y, int w, int h);
#endif
  void MoveCaret(const gfx::Point& point);
// SAMSUNG: Reader >>
#if defined(SBROWSER_READER_NATIVE)
  void RecognizeArticle(int mode);
  void OnRecognizeArticleResult(bool isArticle);
#endif
// SAMSUNG: Reader <<

  void RequestContentClipping(const gfx::Rect& clipping,
                              const gfx::Size& content_size);
// --------------------------- kikin Modification starts ---------------------------
  #if defined (SBROWSER_KIKIN)
  void GetSelectionContext(const bool shouldUpdateSelectionContext);
  void OnSelectionContextExtracted(const std::map<std::string, std::string>& selectionContext);
  void UpdateSelection(const std::string& oldSelection, const std::string& newSelection);
  void RestoreSelection();
  void ClearSelection();
  #endif
// ---------------------------  kikin Modification ends  ---------------------------
#if defined(SBROWSER_HIDE_URLBAR)
  void HideUrlBarCmdReq(int cmd, bool urlbar_active, bool override_allowed, SkBitmap* urlbar_bitmap);
#endif

#if defined(SBROWSER_KEYLAG)
  void InformJSDeferrState(bool isDeferred);
#endif

 private:
  void BuffersSwapped(const gpu::Mailbox& mailbox,
                      const gfx::Size texture_size,
                      const gfx::SizeF content_size,
                      const base::Closure& ack_callback);

  void RunAckCallbacks();

  void ResetClipping();
  void ClipContents(const gfx::Rect& clipping, const gfx::Size& content_size);

  // The model object.
  RenderWidgetHostImpl* host_;

  // Whether or not this widget is potentially attached to the view hierarchy.
  // This view may not actually be attached if this is true, but it should be
  // treated as such, because as soon as a ContentViewCore is set the layer
  // will be attached automatically.
  bool is_layer_attached_;

  // ContentViewCoreImpl is our interface to the view system.
  ContentViewCoreImpl* content_view_core_;

  ImeAdapterAndroid ime_adapter_android_;

  // Body background color of the underlying document.
  SkColor cached_background_color_;

  // The texture layer for this view when using browser-side compositing.
  scoped_refptr<cc::TextureLayer> texture_layer_;

  // The layer used for rendering the contents of this view.
  // It is either owned by texture_layer_ or surface_texture_transport_
  // depending on the mode.
  scoped_refptr<cc::Layer> layer_;

  // The most recent texture id that was pushed to the texture layer.
  unsigned int texture_id_in_layer_;

  // The most recent texture size that was pushed to the texture layer.
  gfx::Size texture_size_in_layer_;

  // The most recent content size that was pushed to the texture layer.
  gfx::Size content_size_in_layer_;

  // Used for image transport when needing to share resources across threads.
  scoped_ptr<SurfaceTextureTransportClient> surface_texture_transport_;

  // The mailbox of the previously received frame.
  gpu::Mailbox current_mailbox_;

  // The mailbox of the frame we last returned.
  gpu::Mailbox last_mailbox_;

  std::queue<base::Closure> ack_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAndroid);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
