// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#if defined(SBROWSER_KEYLAG)
#include "base/time.h"
#include "chrome/common/chrome_switches.h"
#endif
#include "cc/base/switches.h"
#include "cc/base/thread.h"
#include "cc/base/thread_impl.h"
#include "cc/output/output_surface.h"
#include "cc/trees/layer_tree_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/input_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/gpu/compositor_output_surface.h"
#include "content/renderer/gpu/compositor_software_output_device.h"
#include "content/renderer/gpu/input_handler_manager.h"
#include "content/renderer/gpu/mailbox_output_surface.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "ipc/ipc_sync_message.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHelperPlugin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPagePopup.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenuInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#if defined(SBROWSER_KEYLAG)
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#endif
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_switches.h"
#include "ui/surface/transport_dib.h"
#include "webkit/compositor_bindings/web_rendering_stats_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

#if defined(OS_ANDROID)
#include "content/renderer/android/synchronous_compositor_output_surface.h"
#endif

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#endif  // defined(OS_POSIX)

#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
#include "base/sbr/msc.h"
#endif
#if defined(SBROWSER_HIDE_URLBAR)
#include "cc/sbr/urlbar_utils.h"
#endif

using WebKit::WebCompositionUnderline;
using WebKit::WebCursorInfo;
using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebNavigationPolicy;
using WebKit::WebPagePopup;
using WebKit::WebPoint;
using WebKit::WebPopupMenu;
using WebKit::WebPopupMenuInfo;
using WebKit::WebPopupType;
using WebKit::WebRange;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebSize;
using WebKit::WebTextDirection;
using WebKit::WebTouchEvent;
using WebKit::WebVector;
using WebKit::WebWidget;

namespace {
const char* GetEventName(WebInputEvent::Type type) {
#define CASE_TYPE(t) case WebInputEvent::t:  return #t
  switch(type) {
    CASE_TYPE(Undefined);
    CASE_TYPE(MouseDown);
    CASE_TYPE(MouseUp);
    CASE_TYPE(MouseMove);
    CASE_TYPE(MouseEnter);
    CASE_TYPE(MouseLeave);
    CASE_TYPE(ContextMenu);
    CASE_TYPE(MouseWheel);
    CASE_TYPE(RawKeyDown);
    CASE_TYPE(KeyDown);
    CASE_TYPE(KeyUp);
    CASE_TYPE(Char);
    CASE_TYPE(GestureScrollBegin);
    CASE_TYPE(GestureScrollEnd);
    CASE_TYPE(GestureScrollUpdate);
    CASE_TYPE(GestureFlingStart);
    CASE_TYPE(GestureFlingCancel);
    CASE_TYPE(GestureTap);
    CASE_TYPE(GestureTapDown);
    CASE_TYPE(GestureTapCancel);
    CASE_TYPE(GestureDoubleTap);
    CASE_TYPE(GestureTwoFingerTap);
    CASE_TYPE(GestureLongPress);
    CASE_TYPE(GestureLongTap);
    CASE_TYPE(GesturePinchBegin);
    CASE_TYPE(GesturePinchEnd);
    CASE_TYPE(GesturePinchUpdate);
    CASE_TYPE(TouchStart);
    CASE_TYPE(TouchMove);
    CASE_TYPE(TouchEnd);
    CASE_TYPE(TouchCancel);
    default:
      // Must include default to let WebKit::WebInputEvent add new event types
      // before they're added here.
      DLOG(WARNING) << "Unhandled WebInputEvent type in GetEventName.\n";
      break;
  }
#undef CASE_TYPE
  return "";
}
}
namespace content {

RenderWidget::RenderWidget(WebKit::WebPopupType popup_type,
                           const WebKit::WebScreenInfo& screen_info,
                           bool swapped_out)
    : routing_id_(MSG_ROUTING_NONE),
      surface_id_(0),
      webwidget_(NULL),
      opener_id_(MSG_ROUTING_NONE),
      init_complete_(false),
      current_paint_buf_(NULL),
      overdraw_bottom_height_(0.f),
      next_paint_flags_(0),
      filtered_time_per_frame_(0.0f),
      update_reply_pending_(false),
      auto_resize_mode_(false),
      need_update_rect_for_auto_resize_(false),
      using_asynchronous_swapbuffers_(false),
      num_swapbuffers_complete_pending_(0),
      did_show_(false),
      is_hidden_(false),
      is_fullscreen_(false),
      needs_repainting_on_restore_(false),
      has_focus_(false),
      handling_input_event_(false),
      handling_ime_event_(false),
      closing_(false),
      is_swapped_out_(swapped_out),
      input_method_is_active_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      can_compose_inline_(true),
#if defined(SBROWSER_KEYLAG)
      change_listener_registered_(false),
#endif
      popup_type_(popup_type),
      pending_window_rect_count_(0),
      suppress_next_char_events_(false),
      is_accelerated_compositing_active_(false),
      animation_update_pending_(false),
      invalidation_task_posted_(false),
      screen_info_(screen_info),
      device_scale_factor_(screen_info_.deviceScaleFactor),
      throttle_input_events_(true),
#if defined(SBROWSER_DEFER_COMMITS_ON_GESTURE)
      defer_commits_(false),
#endif
      is_threaded_compositing_enabled_(false),
#if defined(SBROWSER_BLOCK_UPDATING_INPUT_AT_TOUCHEND)
      block_updating_input_state_(false),
#endif
      weak_ptr_factory_(this) {
  if (!swapped_out)
    RenderProcess::current()->AddRefProcess();
  DCHECK(RenderThread::Get());
  has_disable_gpu_vsync_switch_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuVsync);
  is_threaded_compositing_enabled_ =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableThreadedCompositing);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";
  STLDeleteElements(&updates_pending_swap_);
  if (current_paint_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_paint_buf_);
    current_paint_buf_ = NULL;
  }
  // If we are swapped out, we have released already.
  if (!is_swapped_out_)
    RenderProcess::current()->ReleaseProcess();
}

// static
RenderWidget* RenderWidget::Create(int32 opener_id,
                                   WebKit::WebPopupType popup_type,
                                   const WebKit::WebScreenInfo& screen_info) {
  DCHECK(opener_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderWidget> widget(
      new RenderWidget(popup_type, screen_info, false));
  if (widget->Init(opener_id)) {  // adds reference on success.
    return widget;
  }
  return NULL;
}

// static
WebWidget* RenderWidget::CreateWebWidget(RenderWidget* render_widget) {
  switch (render_widget->popup_type_) {
    case WebKit::WebPopupTypeNone:  // Nothing to create.
      break;
    case WebKit::WebPopupTypeSelect:
    case WebKit::WebPopupTypeSuggestion:
      return WebPopupMenu::create(render_widget);
    case WebKit::WebPopupTypePage:
      return WebPagePopup::create(render_widget);
    case WebKit::WebPopupTypeHelperPlugin:
      return WebKit::WebHelperPlugin::create(render_widget);
    default:
      NOTREACHED();
  }
  return NULL;
}

#if defined(SBROWSER_FORM_NAVIGATION) 
void RenderWidget::SendTouchEndAck() {
 LOG(INFO) <<__FUNCTION__<<"  sendtouch ack called ";
  UpdateTextInputState(SHOW_IME_IF_NEEDED);
 /*
  WebKit::WebTextInputInfo text_input_info;
  if (webwidget_)
    text_input_info = webwidget_->textInputInfo();

  ViewHostMsg_TextInputState_Params p;
  p.type = WebKitToUiTextInputType(text_input_info.type);
  p.value = text_input_info.value.utf8();
  p.selection_start = text_input_info.selectionStart;
  p.selection_end = text_input_info.selectionEnd;
  p.composition_start = text_input_info.compositionStart;
  p.composition_end = text_input_info.compositionEnd;
  p.request_time = base::Time();
  p.privateIMEOptions = text_input_info.privateIMEOptions;
  //SLOG(INFO) <<__FUNCTION__<<" render_widget prevEnabled "<<p.privateIMEOptions;
  GetSelectionBounds(&p.caret_rect, &p.caret_rect);

  IPC::Message* touch_ack =
      new ViewHostMsg_TextInputStateChanged(routing_id_, p);
  Send(touch_ack);

  text_input_info_ = text_input_info;
  text_input_type_ = p.type;
  */
}
void RenderWidget::SendClosePopup()
{
Send(new ViewHostMsg_OnCloseSelectPopupZero(routing_id_));
}
#endif

bool RenderWidget::Init(int32 opener_id) {
  return DoInit(opener_id,
                RenderWidget::CreateWebWidget(this),
                new ViewHostMsg_CreateWidget(opener_id, popup_type_,
                                             &routing_id_, &surface_id_));
}

bool RenderWidget::DoInit(int32 opener_id,
                          WebWidget* web_widget,
                          IPC::SyncMessage* create_widget_message) {
  DCHECK(!webwidget_);

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  webwidget_ = web_widget;

  bool result = RenderThread::Get()->Send(create_widget_message);
  if (result) {
    RenderThread::Get()->AddRoute(routing_id_, this);
    // Take a reference on behalf of the RenderThread.  This will be balanced
    // when we receive ViewMsg_Close.
    AddRef();
    return true;
  } else {
    // The above Send can fail when the tab is closing.
    return false;
  }
}

// This is used to complete pending inits and non-pending inits.
void RenderWidget::CompleteInit() {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  init_complete_ = true;

  if (webwidget_ && is_threaded_compositing_enabled_) {
    webwidget_->enterForceCompositingMode(true);
  }
  if (compositor_) {
    compositor_->setSurfaceReady();
  }
  DoDeferredUpdate();

  Send(new ViewHostMsg_RenderViewReady(routing_id_));
}

void RenderWidget::SetSwappedOut(bool is_swapped_out) {
  // We should only toggle between states.
  DCHECK(is_swapped_out_ != is_swapped_out);
  is_swapped_out_ = is_swapped_out;

  // If we are swapping out, we will call ReleaseProcess, allowing the process
  // to exit if all of its RenderViews are swapped out.  We wait until the
  // WasSwappedOut call to do this, to avoid showing the sad tab.
  // If we are swapping in, we call AddRefProcess to prevent the process from
  // exiting.
  if (!is_swapped_out)
    RenderProcess::current()->AddRefProcess();
}

bool RenderWidget::AllowPartialSwap() const {
  return true;
}

bool RenderWidget::SynchronouslyDisableVSync() const {
#if defined(OS_ANDROID)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSynchronousRendererCompositor)) {
    return true;
  }
#endif
  return false;
}

bool RenderWidget::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidget, message)
    IPC_MESSAGE_HANDLER(InputMsg_HandleInputEvent, OnHandleInputEvent)    
    IPC_MESSAGE_HANDLER(InputMsg_CursorVisibilityChange,
                        OnCursorVisibilityChange)
    IPC_MESSAGE_HANDLER(InputMsg_MouseCaptureLost, OnMouseCaptureLost)
    IPC_MESSAGE_HANDLER(InputMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewMsg_CreatingNew_ACK, OnCreatingNewAck)
    IPC_MESSAGE_HANDLER(ViewMsg_Resize, OnResize)
    IPC_MESSAGE_HANDLER(ViewMsg_ChangeResizeRect, OnChangeResizeRect)
    IPC_MESSAGE_HANDLER(ViewMsg_WasHidden, OnWasHidden)
    IPC_MESSAGE_HANDLER(ViewMsg_WasShown, OnWasShown)
    IPC_MESSAGE_HANDLER(ViewMsg_WasSwappedOut, OnWasSwappedOut)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateRect_ACK, OnUpdateRectAck)
    IPC_MESSAGE_HANDLER(ViewMsg_SwapBuffers_ACK,
                        OnViewContextSwapBuffersComplete)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInputMethodActive, OnSetInputMethodActive)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeSetComposition, OnImeSetComposition)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeConfirmComposition, OnImeConfirmComposition)
    IPC_MESSAGE_HANDLER(ViewMsg_PaintAtSize, OnPaintAtSize)
    IPC_MESSAGE_HANDLER(ViewMsg_Repaint, OnRepaint)
    IPC_MESSAGE_HANDLER(ViewMsg_SmoothScrollCompleted, OnSmoothScrollCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_SetTextDirection, OnSetTextDirection)
    IPC_MESSAGE_HANDLER(ViewMsg_Move_ACK, OnRequestMoveAck)
    IPC_MESSAGE_HANDLER(ViewMsg_ScreenInfoChanged, OnScreenInfoChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateScreenRects, OnUpdateScreenRects)
#if defined(SBROWSER_IMAGEDRAG_IMPL)   
    IPC_MESSAGE_HANDLER(ViewMsg_GetBitmapForCopyImage,OnGetBitmapForCopyImage)
#endif
#if defined(SBROWSER_TEXT_SELECTION)	 
    IPC_MESSAGE_HANDLER(ViewMsg_UnmarkSelection, onUnmarkSelection)
#endif

#if defined(SBROWSER_SBR_NATIVE_CONTENTVIEW)  && defined(SBROWSER_ENTERKEY_LONGPRESS)
	IPC_MESSAGE_HANDLER(ViewMsg_LongPressOnFocussed, OnLongPressOnFocussed)
#endif

#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeBatchStateChanged, OnImeBatchStateChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_ShowImeIfNeeded, OnShowImeIfNeeded)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_Snapshot, OnSnapshot)
#if defined(SBROWSER_HIDE_URLBAR)
    IPC_MESSAGE_HANDLER(ViewMsg_HideUrlBarCmd,OnHideUrlBarCmd)
#endif

#if defined(SBROWSER_FLUSH_PENDING_MESSAGES)
    IPC_MESSAGE_HANDLER(ViewMsg_FlushPendingMessages, OnFlushPendingMessages);
#endif

#if defined(SBROWSER_KEYLAG)
    IPC_MESSAGE_HANDLER(ViewMsg_DeferrJavaScript, OnDeferredInputJS)
#endif

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderWidget::Send(IPC::Message* message) {
  // Don't send any messages after the browser has told us to close, and filter
  // most outgoing messages while swapped out.
  if ((is_swapped_out_ &&
       !SwappedOutMessages::CanSendWhileSwappedOut(message)) ||
      closing_) {
    delete message;
    return false;
  }

  // If given a messsage without a routing ID, then assign our routing ID.
  if (message->routing_id() == MSG_ROUTING_NONE)
    message->set_routing_id(routing_id_);

  return RenderThread::Get()->Send(message);
}

void RenderWidget::Resize(const gfx::Size& new_size,
                          const gfx::Size& physical_backing_size,
                          float overdraw_bottom_height,
                          const gfx::Rect& resizer_rect,
                          bool is_fullscreen,
                          ResizeAck resize_ack) {
  // A resize ack shouldn't be requested if we have not ACK'd the previous one.
  DCHECK(resize_ack != SEND_RESIZE_ACK || !next_paint_is_resize_ack());
  DCHECK(resize_ack == SEND_RESIZE_ACK || resize_ack == NO_RESIZE_ACK);

  // Ignore this during shutdown.
  if (!webwidget_)
    return;

  if (compositor_) {
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
  MSC_MESSAGE_2("RENDERER","RENDERER",__FUNCTION__,new_size.ToString(),physical_backing_size.ToString());
#endif
    compositor_->setViewportSize(new_size, physical_backing_size);
    compositor_->SetOverdrawBottomHeight(overdraw_bottom_height);
  }

  physical_backing_size_ = physical_backing_size;
  overdraw_bottom_height_ = overdraw_bottom_height;
  resizer_rect_ = resizer_rect;

  // NOTE: We may have entered fullscreen mode without changing our size.
  bool fullscreen_change = is_fullscreen_ != is_fullscreen;
  if (fullscreen_change)
    WillToggleFullscreen();
  is_fullscreen_ = is_fullscreen;

  if (size_ != new_size) {
    // TODO(darin): We should not need to reset this here.
    needs_repainting_on_restore_ = false;

    size_ = new_size;

    paint_aggregator_.ClearPendingUpdate();

    // When resizing, we want to wait to paint before ACK'ing the resize.  This
    // ensures that we only resize as fast as we can paint.  We only need to
    // send an ACK if we are resized to a non-empty rect.
    webwidget_->resize(new_size);
    if (!new_size.IsEmpty()) {
      if (!is_accelerated_compositing_active_) {
        // Resize should have caused an invalidation of the entire view.
        DCHECK(paint_aggregator_.HasPendingUpdate());
      }

      // Send the Resize_ACK flag once we paint again if requested.
      if (resize_ack == SEND_RESIZE_ACK)
        set_next_paint_is_resize_ack();
    }
  } else {
    resize_ack = NO_RESIZE_ACK;
  }

  if (fullscreen_change)
    DidToggleFullscreen();

  // If a resize ack is requested and it isn't set-up, then no more resizes will
  // come in and in general things will go wrong.
  DCHECK(resize_ack != SEND_RESIZE_ACK || new_size.IsEmpty() ||
         next_paint_is_resize_ack());
}

void RenderWidget::OnClose() {
  if (closing_)
    return;
  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE) {
    RenderThread::Get()->RemoveRoute(routing_id_);
    SetHidden(false);
  }

  // If there is a Send call on the stack, then it could be dangerous to close
  // now.  Post a task that only gets invoked when there are no nested message
  // loops.
  base::MessageLoop::current()->PostNonNestableTask(
      FROM_HERE, base::Bind(&RenderWidget::Close, this));

  // Balances the AddRef taken when we called AddRoute.
  Release();
}

// Got a response from the browser after the renderer decided to create a new
// view.
void RenderWidget::OnCreatingNewAck() {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  CompleteInit();
}

void RenderWidget::OnResize(const gfx::Size& new_size,
                            const gfx::Size& physical_backing_size,
                            float overdraw_bottom_height,
                            const gfx::Rect& resizer_rect,
                            bool is_fullscreen) {
  LOG(INFO)<<"RenderWidget::OnResize";
  Resize(new_size, physical_backing_size, overdraw_bottom_height, resizer_rect,
         is_fullscreen, SEND_RESIZE_ACK);
}

void RenderWidget::OnChangeResizeRect(const gfx::Rect& resizer_rect) {
  if (resizer_rect_ != resizer_rect) {
    gfx::Rect view_rect(size_);

    gfx::Rect old_damage_rect = gfx::IntersectRects(view_rect, resizer_rect_);
    if (!old_damage_rect.IsEmpty())
      paint_aggregator_.InvalidateRect(old_damage_rect);

    gfx::Rect new_damage_rect = gfx::IntersectRects(view_rect, resizer_rect);
    if (!new_damage_rect.IsEmpty())
      paint_aggregator_.InvalidateRect(new_damage_rect);

    resizer_rect_ = resizer_rect;

    if (webwidget_)
      webwidget_->didChangeWindowResizerRect();
  }
}

void RenderWidget::OnWasHidden() {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasHidden");
  LOG(INFO)<<"RenderWidget::OnWasHidden()";
  // Go into a mode where we stop generating paint and scrolling events.
  SetHidden(true);
}

void RenderWidget::OnWasShown(bool needs_repainting) {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasShown");
  LOG(INFO)<<"RenderWidget::OnWasShown()";
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // See OnWasHidden
  SetHidden(false);

  if (!needs_repainting && !needs_repainting_on_restore_)
    return;
  needs_repainting_on_restore_ = false;

  // Tag the next paint as a restore ack, which is picked up by
  // DoDeferredUpdate when it sends out the next PaintRect message.
  set_next_paint_is_restore_ack();

  // Generate a full repaint.
  if (!is_accelerated_compositing_active_) {
    didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
  } else {
    scheduleComposite();
  }
}

void RenderWidget::OnWasSwappedOut() {
  // If we have been swapped out and no one else is using this process,
  // it's safe to exit now.  If we get swapped back in, we will call
  // AddRefProcess in SetSwappedOut.
  if (is_swapped_out_)
    RenderProcess::current()->ReleaseProcess();
}

void RenderWidget::OnRequestMoveAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
}

void RenderWidget::OnUpdateRectAck() {
  TRACE_EVENT0("renderer", "RenderWidget::OnUpdateRectAck");
  DCHECK(update_reply_pending_);
  update_reply_pending_ = false;

  // If we sent an UpdateRect message with a zero-sized bitmap, then we should
  // have no current paint buffer.
  if (current_paint_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_paint_buf_);
    current_paint_buf_ = NULL;
  }

  // If swapbuffers is still pending, then defer the update until the
  // swapbuffers occurs.
  if (num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending) {
    TRACE_EVENT0("renderer", "EarlyOut_SwapStillPending");
    return;
  }

  // Notify subclasses that software rendering was flushed to the screen.
  if (!is_accelerated_compositing_active_) {
    DidFlushPaint();
  }

  // Continue painting if necessary...
  DoDeferredUpdateAndSendInputAck();
}

bool RenderWidget::SupportsAsynchronousSwapBuffers() {
  // Contexts using the command buffer support asynchronous swapbuffers.
  // See RenderWidget::CreateOutputSurface().
  if (RenderThreadImpl::current()->compositor_message_loop_proxy())
    return false;

  return true;
}

GURL RenderWidget::GetURLForGraphicsContext3D() {
  return GURL();
}

bool RenderWidget::ForceCompositingModeEnabled() {
  return false;
}

scoped_ptr<cc::OutputSurface> RenderWidget::CreateOutputSurface() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableSoftwareCompositingGLAdapter)) {
      return scoped_ptr<cc::OutputSurface>(
          new CompositorOutputSurface(routing_id(), NULL,
              new CompositorSoftwareOutputDevice()));
  }

  // Explicitly disable antialiasing for the compositor. As of the time of
  // this writing, the only platform that supported antialiasing for the
  // compositor was Mac OS X, because the on-screen OpenGL context creation
  // code paths on Windows and Linux didn't yet have multisampling support.
  // Mac OS X essentially always behaves as though it's rendering offscreen.
  // Multisampling has a heavy cost especially on devices with relatively low
  // fill rate like most notebooks, and the Mac implementation would need to
  // be optimized to resolve directly into the IOSurface shared between the
  // GPU and browser processes. For these reasons and to avoid platform
  // disparities we explicitly disable antialiasing.
  WebKit::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;
  WebGraphicsContext3DCommandBufferImpl* context =
      CreateGraphicsContext3D(attributes);
  if (!context)
    return scoped_ptr<cc::OutputSurface>();

#if defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kEnableSynchronousRendererCompositor)) {
    // TODO(joth): Move above the |context| creation step above when the
    // SynchronousCompositor no longer depends on externally created context.
    return scoped_ptr<cc::OutputSurface>(
        new SynchronousCompositorOutputSurface(routing_id(),
                                               context));
  }
#endif

  bool composite_to_mailbox =
      command_line.HasSwitch(cc::switches::kCompositeToMailbox);
  DCHECK(!composite_to_mailbox || command_line.HasSwitch(
      cc::switches::kEnableCompositorFrameMessage));
  // No swap throttling yet when compositing on the main thread.
  DCHECK(!composite_to_mailbox || is_threaded_compositing_enabled_);
  return scoped_ptr<cc::OutputSurface>(composite_to_mailbox ?
      new MailboxOutputSurface(routing_id(), context, NULL) :
          new CompositorOutputSurface(routing_id(), context, NULL));
}

void RenderWidget::OnViewContextSwapBuffersAborted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersAborted");
  while (!updates_pending_swap_.empty()) {
    ViewHostMsg_UpdateRect* msg = updates_pending_swap_.front();
    updates_pending_swap_.pop_front();
    // msg can be NULL if the swap doesn't correspond to an DoDeferredUpdate
    // compositing pass, hence doesn't require an UpdateRect message.
    if (msg)
      Send(msg);
  }
  num_swapbuffers_complete_pending_ = 0;
  using_asynchronous_swapbuffers_ = false;
  // Schedule another frame so the compositor learns about it.
  scheduleComposite();
}

void RenderWidget::OnViewContextSwapBuffersPosted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersPosted");

  if (using_asynchronous_swapbuffers_) {
    ViewHostMsg_UpdateRect* msg = NULL;
    // pending_update_params_ can be NULL if the swap doesn't correspond to an
    // DoDeferredUpdate compositing pass, hence doesn't require an UpdateRect
    // message.
    if (pending_update_params_) {
      msg = new ViewHostMsg_UpdateRect(routing_id_, *pending_update_params_);
      pending_update_params_.reset();
    }
    updates_pending_swap_.push_back(msg);
    num_swapbuffers_complete_pending_++;
  }
}

void RenderWidget::OnViewContextSwapBuffersComplete() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersComplete");

  // Notify subclasses that composited rendering was flushed to the screen.
  DidFlushPaint();

  // When compositing deactivates, we reset the swapbuffers pending count.  The
  // swapbuffers acks may still arrive, however.
  if (num_swapbuffers_complete_pending_ == 0) {
    TRACE_EVENT0("renderer", "EarlyOut_ZeroSwapbuffersPending");
    return;
  }
  DCHECK(!updates_pending_swap_.empty());
  ViewHostMsg_UpdateRect* msg = updates_pending_swap_.front();
  updates_pending_swap_.pop_front();
  // msg can be NULL if the swap doesn't correspond to an DoDeferredUpdate
  // compositing pass, hence doesn't require an UpdateRect message.
  if (msg)
    Send(msg);
  num_swapbuffers_complete_pending_--;

  // If update reply is still pending, then defer the update until that reply
  // occurs.
  if (update_reply_pending_) {
    TRACE_EVENT0("renderer", "EarlyOut_UpdateReplyPending");
    return;
  }

  // If we are not accelerated rendering, then this is a stale swapbuffers from
  // when we were previously rendering. However, if an invalidation task is not
  // posted, there may be software rendering work pending. In that case, don't
  // early out.
  if (!is_accelerated_compositing_active_ && invalidation_task_posted_) {
    TRACE_EVENT0("renderer", "EarlyOut_AcceleratedCompositingOff");
    return;
  }

  // Do not call DoDeferredUpdate unless there's animation work to be done or
  // a real invalidation. This prevents rendering in response to a swapbuffers
  // callback coming back after we've navigated away from the page that
  // generated it.
  if (!animation_update_pending_ && !paint_aggregator_.HasPendingUpdate()) {
    TRACE_EVENT0("renderer", "EarlyOut_NoPendingUpdate");
    return;
  }

  // Continue painting if necessary...
  DoDeferredUpdateAndSendInputAck();
}

void RenderWidget::OnHandleInputEvent(const WebKit::WebInputEvent* input_event,
                                      bool is_keyboard_shortcut) {
  TRACE_EVENT1("renderer", "RenderWidget::OnHandleInputEvent", "type", (input_event ? input_event->type : -1));

  handling_input_event_ = true;
  if (!input_event) {
    handling_input_event_ = false;
    return;
  }

  base::TimeDelta now = base::TimeDelta::FromInternalValue(
      base::TimeTicks::Now().ToInternalValue());

  int64 delta = static_cast<int64>(
      (now.InSecondsF() - input_event->timeStampSeconds) *
          base::Time::kMicrosecondsPerSecond);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.Renderer", delta, 0, 1000000, 100);
  std::string name_for_event =
      base::StringPrintf("Event.Latency.Renderer.%s",
                         GetEventName(input_event->type));
  base::HistogramBase* counter_for_type =
      base::Histogram::FactoryGet(
          name_for_event,
          0,
          1000000,
          100,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter_for_type->Add(delta);

  bool prevent_default = false;
  if (WebInputEvent::isMouseEventType(input_event->type)) {
    const WebMouseEvent& mouse_event =
        *static_cast<const WebMouseEvent*>(input_event);
    TRACE_EVENT2("renderer", "HandleMouseMove",
                 "x", mouse_event.x, "y", mouse_event.y);
    prevent_default = WillHandleMouseEvent(mouse_event);
  }

  if (WebInputEvent::isGestureEventType(input_event->type)) {
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(input_event);
    prevent_default = prevent_default || WillHandleGestureEvent(gesture_event);
  }
#if defined(SBROWSER_IMAGEDRAG_IMPL)
if( input_event->type == WebInputEvent::GestureLongPress)
{
    int img_width=0,img_height = 0;
    SkBitmap bitmap;
    WebKit::WebView* wv = static_cast<WebKit::WebView*>(webwidget());
    const WebKit:: WebGestureEvent* gesture_eventTest =
        static_cast< const WebKit:: WebGestureEvent*>(input_event);
    wv->getImageBitmapForDragging(gesture_eventTest->x ,gesture_eventTest->y,&img_width,&img_height,&bitmap);
	if(((img_width > 0) && (img_height >0)) && bitmap.getSize()
#if defined(SBROWSER_CQ_MPSG100013581)
	&& !bitmap.isNull()
#endif
)
	{
		Send( new ViewHostMsg_setBitmapForDragging(routing_id_,bitmap,img_width,img_height));
	}
}
#endif

    if (input_event->type == WebInputEvent::GestureTap ||
      input_event->type == WebInputEvent::GestureLongPress){
#if defined(SBROWSER_SEARCH_SCROLL_FIX) 
      resetInputMethodOnTap(*input_event);
#else	  
      resetInputMethod();
#endif
	}

  bool processed = prevent_default;
  if (input_event->type != WebInputEvent::Char || !suppress_next_char_events_) {
    suppress_next_char_events_ = false;
    if (!processed && webwidget_){
      processed = webwidget_->handleInputEvent(*input_event);
    }
  }

  // If this RawKeyDown event corresponds to a browser keyboard shortcut and
  // it's not processed by webkit, then we need to suppress the upcoming Char
  // events.
  if (!processed && is_keyboard_shortcut)
    suppress_next_char_events_ = true;

  InputEventAckState ack_result = processed ?
      INPUT_EVENT_ACK_STATE_CONSUMED : INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  if (!processed &&  input_event->type == WebInputEvent::TouchStart) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(input_event);
    ack_result = HasTouchEventHandlersAt(touch_event.touches[0].position) ?
        INPUT_EVENT_ACK_STATE_NOT_CONSUMED :
        INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  }

  IPC::Message* response =
      new InputHostMsg_HandleInputEvent_ACK(routing_id_, input_event->type,
                                                ack_result);
  bool event_type_gets_rate_limited =
      input_event->type == WebInputEvent::MouseMove ||
      input_event->type == WebInputEvent::MouseWheel
#if defined(SBROWSER_DEFER_COMMITS_ON_GESTURE)
      //|| input_event->type == WebInputEvent::TouchMove
      ;
#else
      || WebInputEvent::isTouchEventType(input_event->type);
#endif

  bool frame_pending = paint_aggregator_.HasPendingUpdate();
  if (is_accelerated_compositing_active_) {
    frame_pending = compositor_ &&
                    compositor_->commitRequested();
  }

  bool is_input_throttled =
      throttle_input_events_ &&
      frame_pending
#if defined(SBROWSER_DEFER_COMMITS_ON_GESTURE)
      && !defer_commits_
#endif
      ;

  if (event_type_gets_rate_limited && is_input_throttled && !is_hidden_) {
    // We want to rate limit the input events in this case, so we'll wait for
    // painting to finish before ACKing this message.
    if (pending_input_event_ack_) {
      // As two different kinds of events could cause us to postpone an ack
      // we send it now, if we have one pending. The Browser should never
      // send us the same kind of event we are delaying the ack for.
      Send(pending_input_event_ack_.release());
    }
    pending_input_event_ack_.reset(response);
  } else {
    Send(response);
  }
  
#if defined(SBROWSER_BLOCK_UPDATING_INPUT_AT_TOUCHEND)
  if (input_event->type == WebInputEvent::TouchStart){
      block_updating_input_state_ = false;
  }
  if(input_event->type == WebInputEvent::TouchMove && processed){
      block_updating_input_state_ = true;
  }
  // Allow the IME to be shown when the focus changes as a consequence
  // of a processed touch end event.
  if (input_event->type == WebInputEvent::TouchEnd && processed && !block_updating_input_state_)
      UpdateTextInputState(SHOW_IME_IF_NEEDED);
#else
#if defined(OS_ANDROID)
  // Allow the IME to be shown when the focus changes as a consequence
  // of a processed touch end event.
  if (input_event->type == WebInputEvent::TouchEnd && processed)
    UpdateTextInputState(SHOW_IME_IF_NEEDED);
#endif
#endif

  handling_input_event_ = false;

  if (!prevent_default) {
    if (WebInputEvent::isKeyboardEventType(input_event->type)){
// SAMSUNG CHANGE : For keylagging perfomance >>
#if defined (SBROWSER_KEYLAGGING_PERFORMANCE)
      //LOG(INFO) << __FUNCTION__<<"input_event->type: "<<input_event->type<<" WebInputEvent::KeyDown:"<<WebInputEvent::KeyDown;
      if(input_event->type == WebInputEvent::RawKeyDown) {
          DidKeyDownEvent();
      }
#endif
// SAMSUNG CHANGE : For keylagging perfomance <<
      DidHandleKeyEvent();
    }
    if (WebInputEvent::isMouseEventType(input_event->type))
      DidHandleMouseEvent(*(static_cast<const WebMouseEvent*>(input_event)));
    if (WebInputEvent::isTouchEventType(input_event->type))
      DidHandleTouchEvent(*(static_cast<const WebTouchEvent*>(input_event)));
  }
}

#if defined(SBROWSER_IMAGEDRAG_IMPL)
void RenderWidget::OnGetBitmapForCopyImage(int x,int y)
{
    int img_width=0,img_height = 0;
    SkBitmap bitmap;
    WebKit::WebView* wv = static_cast<WebKit::WebView*>(webwidget());
    wv->getImageBitmapForDragging(x ,y,&img_width,&img_height,&bitmap);
    if(((img_width > 0) && (img_height >0)) && bitmap.getSize()){
Send( new ViewHostMsg_setBitmapForDragging(routing_id_,bitmap,img_width,img_height));
}
}
#endif
void RenderWidget::OnCursorVisibilityChange(bool is_visible) {
  if (webwidget_)
    webwidget_->setCursorVisibilityState(is_visible);
}

void RenderWidget::OnMouseCaptureLost() {
  if (webwidget_)
    webwidget_->mouseCaptureLost();
}

void RenderWidget::OnSetFocus(bool enable) {
  has_focus_ = enable;
  if (webwidget_)
    webwidget_->setFocus(enable);
}

void RenderWidget::ClearFocus() {
  // We may have got the focus from the browser before this gets processed, in
  // which case we do not want to unfocus ourself.
  if (!has_focus_ && webwidget_)
    webwidget_->setFocus(false);
}

void RenderWidget::PaintRect(const gfx::Rect& rect,
                             const gfx::Point& canvas_origin,
                             skia::PlatformCanvas* canvas) {
  TRACE_EVENT2("renderer", "PaintRect",
               "width", rect.width(), "height", rect.height());

  const bool kEnableGpuBenchmarking =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGpuBenchmarking);
  canvas->save();

  // Bring the canvas into the coordinate system of the paint rect.
  canvas->translate(static_cast<SkScalar>(-canvas_origin.x()),
                    static_cast<SkScalar>(-canvas_origin.y()));

  // If there is a custom background, tile it.
  if (!background_.empty()) {
    SkPaint paint;
    skia::RefPtr<SkShader> shader = skia::AdoptRef(
        SkShader::CreateBitmapShader(background_,
                                     SkShader::kRepeat_TileMode,
                                     SkShader::kRepeat_TileMode));
    paint.setShader(shader.get());

    // Use kSrc_Mode to handle background_ transparency properly.
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);

    // Canvas could contain multiple update rects. Clip to given rect so that
    // we don't accidentally clear other update rects.
    canvas->save();
    canvas->scale(device_scale_factor_, device_scale_factor_);
    canvas->clipRect(gfx::RectToSkRect(rect));
    canvas->drawPaint(paint);
    canvas->restore();
  }

  // First see if this rect is a plugin that can paint itself faster.
  TransportDIB* optimized_dib = NULL;
  gfx::Rect optimized_copy_rect, optimized_copy_location;
  float dib_scale_factor;
  webkit::ppapi::PluginInstance* optimized_instance =
      GetBitmapForOptimizedPluginPaint(rect, &optimized_dib,
                                       &optimized_copy_location,
                                       &optimized_copy_rect,
                                       &dib_scale_factor);
  if (optimized_instance) {
    // This plugin can be optimize-painted and we can just ask it to paint
    // itself. We don't actually need the TransportDIB in this case.
    //
    // This is an optimization for PPAPI plugins that know they're on top of
    // the page content. If this rect is inside such a plugin, we can save some
    // time and avoid re-rendering the page content which we know will be
    // covered by the plugin later (this time can be significant, especially
    // for a playing movie that is invalidating a lot).
    //
    // In the plugin movie case, hopefully the similar call to
    // GetBitmapForOptimizedPluginPaint in DoDeferredUpdate handles the
    // painting, because that avoids copying the plugin image to a different
    // paint rect. Unfortunately, if anything on the page is animating other
    // than the movie, it break this optimization since the union of the
    // invalid regions will be larger than the plugin.
    //
    // This code optimizes that case, where we can still avoid painting in
    // WebKit and filling the background (which can be slow) and just painting
    // the plugin. Unlike the DoDeferredUpdate case, an extra copy is still
    // required.
    base::TimeTicks paint_begin_ticks;
    if (kEnableGpuBenchmarking)
      paint_begin_ticks = base::TimeTicks::HighResNow();

    SkAutoCanvasRestore auto_restore(canvas, true);
    canvas->scale(device_scale_factor_, device_scale_factor_);
    optimized_instance->Paint(webkit_glue::ToWebCanvas(canvas),
                              optimized_copy_location, rect);
    canvas->restore();
    if (kEnableGpuBenchmarking) {
      base::TimeDelta paint_time =
          base::TimeTicks::HighResNow() - paint_begin_ticks;
      if (!is_accelerated_compositing_active_)
        software_stats_.total_paint_time += paint_time;
    }
  } else {
    // Normal painting case.
    base::TimeTicks paint_begin_ticks;
    if (kEnableGpuBenchmarking)
      paint_begin_ticks = base::TimeTicks::HighResNow();

    webwidget_->paint(webkit_glue::ToWebCanvas(canvas), rect);

    if (kEnableGpuBenchmarking) {
      base::TimeDelta paint_time =
          base::TimeTicks::HighResNow() - paint_begin_ticks;
      if (!is_accelerated_compositing_active_)
        software_stats_.total_paint_time += paint_time;
    }

    // Flush to underlying bitmap.  TODO(darin): is this needed?
    skia::GetTopDevice(*canvas)->accessBitmap(false);
  }

  PaintDebugBorder(rect, canvas);
  canvas->restore();

  if (kEnableGpuBenchmarking) {
    int64 num_pixels_processed = rect.width() * rect.height();
    software_stats_.total_pixels_painted += num_pixels_processed;
  }
}

void RenderWidget::PaintDebugBorder(const gfx::Rect& rect,
                                    skia::PlatformCanvas* canvas) {
  static bool kPaintBorder =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kShowPaintRects);
  if (!kPaintBorder)
    return;

  // Cycle through these colors to help distinguish new paint rects.
  const SkColor colors[] = {
    SkColorSetARGB(0x3F, 0xFF, 0, 0),
    SkColorSetARGB(0x3F, 0xFF, 0, 0xFF),
    SkColorSetARGB(0x3F, 0, 0, 0xFF),
  };
  static int color_selector = 0;

  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(colors[color_selector++ % arraysize(colors)]);
  paint.setStrokeWidth(1);

  SkIRect irect;
  irect.set(rect.x(), rect.y(), rect.right() - 1, rect.bottom() - 1);
  canvas->drawIRect(irect, paint);
}

void RenderWidget::AnimationCallback() {
  TRACE_EVENT0("renderer", "RenderWidget::AnimationCallback");
  if (!animation_update_pending_) {
    TRACE_EVENT0("renderer", "EarlyOut_NoAnimationUpdatePending");
    return;
  }
  if (!animation_floor_time_.is_null() && IsRenderingVSynced()) {
    // Record when we fired (according to base::Time::Now()) relative to when
    // we posted the task to quantify how much the base::Time/base::TimeTicks
    // skew is affecting animations.
    base::TimeDelta animation_callback_delay = base::Time::Now() -
        (animation_floor_time_ - base::TimeDelta::FromMilliseconds(16));
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.AnimationCallbackDelayTime",
                               animation_callback_delay,
                               base::TimeDelta::FromMilliseconds(0),
                               base::TimeDelta::FromMilliseconds(30),
                               25);
  }
  DoDeferredUpdateAndSendInputAck();
}

void RenderWidget::AnimateIfNeeded() {
  if (!animation_update_pending_)
    return;

  // Target 60FPS if vsync is on. Go as fast as we can if vsync is off.
  base::TimeDelta animationInterval = IsRenderingVSynced() ?
      base::TimeDelta::FromMilliseconds(16) : base::TimeDelta();

  base::Time now = base::Time::Now();

  // animation_floor_time_ is the earliest time that we should animate when
  // using the dead reckoning software scheduler. If we're using swapbuffers
  // complete callbacks to rate limit, we can ignore this floor.
  if (now >= animation_floor_time_ || num_swapbuffers_complete_pending_ > 0) {
    TRACE_EVENT0("renderer", "RenderWidget::AnimateIfNeeded")
    animation_floor_time_ = now + animationInterval;
    // Set a timer to call us back after animationInterval before
    // running animation callbacks so that if a callback requests another
    // we'll be sure to run it at the proper time.
    animation_timer_.Stop();
    animation_timer_.Start(FROM_HERE, animationInterval, this,
                           &RenderWidget::AnimationCallback);
    animation_update_pending_ = false;
    if (is_accelerated_compositing_active_ && compositor_) {
      compositor_->Animate(base::TimeTicks::Now());
    } else {
      double frame_begin_time =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      webwidget_->animate(frame_begin_time);
    }
    return;
  }
  TRACE_EVENT0("renderer", "EarlyOut_AnimatedTooRecently");
  if (!animation_timer_.IsRunning()) {
    // This code uses base::Time::Now() to calculate the floor and next fire
    // time because javascript's Date object uses base::Time::Now().  The
    // message loop uses base::TimeTicks, which on windows can have a
    // different granularity than base::Time.
    // The upshot of all this is that this function might be called before
    // base::Time::Now() has advanced past the animation_floor_time_.  To
    // avoid exposing this delay to javascript, we keep posting delayed
    // tasks until base::Time::Now() has advanced far enough.
    base::TimeDelta delay = animation_floor_time_ - now;
    animation_timer_.Start(FROM_HERE, delay, this,
                           &RenderWidget::AnimationCallback);
  }
}

bool RenderWidget::IsRenderingVSynced() {
  // TODO(nduca): Forcing a driver to disable vsync (e.g. in a control panel) is
  // not caught by this check. This will lead to artificially low frame rates
  // for people who force vsync off at a driver level and expect Chrome to speed
  // up.
  return !has_disable_gpu_vsync_switch_;
}

void RenderWidget::InvalidationCallback() {
  TRACE_EVENT0("renderer", "RenderWidget::InvalidationCallback");
  invalidation_task_posted_ = false;
  DoDeferredUpdateAndSendInputAck();
}

void RenderWidget::DoDeferredUpdateAndSendInputAck() {
  DoDeferredUpdate();

  if (pending_input_event_ack_)
    Send(pending_input_event_ack_.release());
}

void RenderWidget::DoDeferredUpdate() {
  TRACE_EVENT0("renderer", "RenderWidget::DoDeferredUpdate");

  if (!webwidget_)
    return;

  if (!init_complete_) {
    TRACE_EVENT0("renderer", "EarlyOut_InitNotComplete");
    return;
  }
  if (update_reply_pending_) {
    TRACE_EVENT0("renderer", "EarlyOut_UpdateReplyPending");
    return;
  }
  if (is_accelerated_compositing_active_ &&
      num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending) {
    TRACE_EVENT0("renderer", "EarlyOut_MaxSwapBuffersPending");
    return;
  }

  // Suppress updating when we are hidden.
  if (is_hidden_ || size_.IsEmpty() || is_swapped_out_) {
    paint_aggregator_.ClearPendingUpdate();
    needs_repainting_on_restore_ = true;
    TRACE_EVENT0("renderer", "EarlyOut_NotVisible");
    return;
  }

  if (is_accelerated_compositing_active_)
    using_asynchronous_swapbuffers_ = SupportsAsynchronousSwapBuffers();

  // Tracking of frame rate jitter
  base::TimeTicks frame_begin_ticks = base::TimeTicks::Now();
  InstrumentWillBeginFrame();
  AnimateIfNeeded();

  // Layout may generate more invalidation.  It may also enable the
  // GPU acceleration, so make sure to run layout before we send the
  // GpuRenderingActivated message.
  webwidget_->layout();

  // The following two can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputState(DO_NOT_SHOW_IME);
  UpdateSelectionBounds();

  // Suppress painting if nothing is dirty.  This has to be done after updating
  // animations running layout as these may generate further invalidations.
  if (!paint_aggregator_.HasPendingUpdate()) {
    TRACE_EVENT0("renderer", "EarlyOut_NoPendingUpdate");
    InstrumentDidCancelFrame();
    return;
  }

  if (!is_accelerated_compositing_active_ &&
      !is_threaded_compositing_enabled_ &&
      ForceCompositingModeEnabled()) {
    webwidget_->enterForceCompositingMode(true);
  }

  if (!last_do_deferred_update_time_.is_null()) {
    base::TimeDelta delay = frame_begin_ticks - last_do_deferred_update_time_;
    if (is_accelerated_compositing_active_) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.AccelDoDeferredUpdateDelay",
                                 delay,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMilliseconds(120),
                                 60);
    } else {
      UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.SoftwareDoDeferredUpdateDelay",
                                 delay,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMilliseconds(120),
                                 60);
    }

    // Calculate filtered time per frame:
    float frame_time_elapsed = static_cast<float>(delay.InSecondsF());
    filtered_time_per_frame_ =
        0.9f * filtered_time_per_frame_ + 0.1f * frame_time_elapsed;
  }
  last_do_deferred_update_time_ = frame_begin_ticks;

  if (!is_accelerated_compositing_active_) {
    software_stats_.animation_frame_count++;
    software_stats_.screen_frame_count++;
  }

  // OK, save the pending update to a local since painting may cause more
  // invalidation.  Some WebCore rendering objects only layout when painted.
  PaintAggregator::PendingUpdate update;
  paint_aggregator_.PopPendingUpdate(&update);

  gfx::Rect scroll_damage = update.GetScrollDamage();
  gfx::Rect bounds = gfx::UnionRects(update.GetPaintBounds(), scroll_damage);

  // Notify derived classes that we're about to initiate a paint.
  WillInitiatePaint();

  // A plugin may be able to do an optimized paint. First check this, in which
  // case we can skip all of the bitmap generation and regular paint code.
  // This optimization allows PPAPI plugins that declare themselves on top of
  // the page (like a traditional windowed plugin) to be able to animate (think
  // movie playing) without repeatedly re-painting the page underneath, or
  // copying the plugin backing store (since we can send the plugin's backing
  // store directly to the browser).
  //
  // This optimization only works when the entire invalid region is contained
  // within the plugin. There is a related optimization in PaintRect for the
  // case where there may be multiple invalid regions.
  TransportDIB* dib = NULL;
  gfx::Rect optimized_copy_rect, optimized_copy_location;
  float dib_scale_factor = 1;
  DCHECK(!pending_update_params_.get());
  pending_update_params_.reset(new ViewHostMsg_UpdateRect_Params);
  pending_update_params_->scroll_delta = update.scroll_delta;
  pending_update_params_->scroll_rect = update.scroll_rect;
  pending_update_params_->view_size = size_;
  pending_update_params_->plugin_window_moves.swap(plugin_window_moves_);
  pending_update_params_->flags = next_paint_flags_;
  pending_update_params_->scroll_offset = GetScrollOffset();
  pending_update_params_->needs_ack = true;
  pending_update_params_->scale_factor = device_scale_factor_;
  next_paint_flags_ = 0;
  need_update_rect_for_auto_resize_ = false;

  if (update.scroll_rect.IsEmpty() &&
      !is_accelerated_compositing_active_ &&
      GetBitmapForOptimizedPluginPaint(bounds, &dib, &optimized_copy_location,
                                       &optimized_copy_rect,
                                       &dib_scale_factor)) {
    // Only update the part of the plugin that actually changed.
    optimized_copy_rect.Intersect(bounds);
    pending_update_params_->bitmap = dib->id();
    pending_update_params_->bitmap_rect = optimized_copy_location;
    pending_update_params_->copy_rects.push_back(optimized_copy_rect);
    pending_update_params_->scale_factor = dib_scale_factor;
  } else if (!is_accelerated_compositing_active_) {
    // Compute a buffer for painting and cache it.

    bool fractional_scale = device_scale_factor_ -
        static_cast<int>(device_scale_factor_) != 0;
    if (fractional_scale) {
      // Damage might not be DIP aligned. Inflate damage to compensate.
      bounds.Inset(-1, -1);
      bounds.Intersect(gfx::Rect(size_));
    }

    gfx::Rect pixel_bounds = gfx::ToEnclosingRect(
        gfx::ScaleRect(bounds, device_scale_factor_));

    scoped_ptr<skia::PlatformCanvas> canvas(
        RenderProcess::current()->GetDrawingCanvas(&current_paint_buf_,
                                                   pixel_bounds));
    if (!canvas) {
      NOTREACHED();
      return;
    }

    // We may get back a smaller canvas than we asked for.
    // TODO(darin): This seems like it could cause painting problems!
    DCHECK_EQ(pixel_bounds.width(), canvas->getDevice()->width());
    DCHECK_EQ(pixel_bounds.height(), canvas->getDevice()->height());
    pixel_bounds.set_width(canvas->getDevice()->width());
    pixel_bounds.set_height(canvas->getDevice()->height());
    bounds.set_width(pixel_bounds.width() / device_scale_factor_);
    bounds.set_height(pixel_bounds.height() / device_scale_factor_);

    HISTOGRAM_COUNTS_100("MPArch.RW_PaintRectCount", update.paint_rects.size());

    pending_update_params_->bitmap = current_paint_buf_->id();
    pending_update_params_->bitmap_rect = bounds;

    std::vector<gfx::Rect>& copy_rects = pending_update_params_->copy_rects;
    // The scroll damage is just another rectangle to paint and copy.
    copy_rects.swap(update.paint_rects);
    if (!scroll_damage.IsEmpty())
      copy_rects.push_back(scroll_damage);

    for (size_t i = 0; i < copy_rects.size(); ++i) {
      gfx::Rect rect = copy_rects[i];
      if (fractional_scale) {
        // Damage might not be DPI aligned.  Inflate rect to compensate.
        rect.Inset(-1, -1);
      }
      PaintRect(rect, pixel_bounds.origin(), canvas.get());
    }

    // Software FPS tick for performance tests. The accelerated path traces the
    // frame events in didCommitAndDrawCompositorFrame. See throughput_tests.cc.
    // NOTE: Tests may break if this event is renamed or moved.
    UNSHIPPED_TRACE_EVENT_INSTANT0("test_fps", "TestFrameTickSW",
                                   TRACE_EVENT_SCOPE_THREAD);
  } else {  // Accelerated compositing path
    // Begin painting.
    // If painting is done via the gpu process then we don't set any damage
    // rects to save the browser process from doing unecessary work.
    pending_update_params_->bitmap_rect = bounds;
    pending_update_params_->scroll_rect = gfx::Rect();
    // We don't need an ack, because we're not sharing a DIB with the browser.
    // If it needs to (e.g. composited UI), the GPU process does its own ACK
    // with the browser for the GPU surface.
    pending_update_params_->needs_ack = false;
    Composite(frame_begin_ticks);
  }

  // If we're holding a pending input event ACK, send the ACK before sending the
  // UpdateReply message so we can receive another input event before the
  // UpdateRect_ACK on platforms where the UpdateRect_ACK is sent from within
  // the UpdateRect IPC message handler.
  if (pending_input_event_ack_)
    Send(pending_input_event_ack_.release());

  // If Composite() called SwapBuffers, pending_update_params_ will be reset (in
  // OnSwapBuffersPosted), meaning a message has been added to the
  // updates_pending_swap_ queue, that will be sent later. Otherwise, we send
  // the message now.
  if (pending_update_params_) {
    // sending an ack to browser process that the paint is complete...
    update_reply_pending_ = pending_update_params_->needs_ack;
    Send(new ViewHostMsg_UpdateRect(routing_id_, *pending_update_params_));
    pending_update_params_.reset();
  }

  // If we're software rendering then we're done initiating the paint.
  if (!is_accelerated_compositing_active_)
    DidInitiatePaint();
}

void RenderWidget::Composite(base::TimeTicks frame_begin_time) {
  DCHECK(is_accelerated_compositing_active_);
  if (compositor_)  // TODO(jamesr): Figure out how this can be null.
    compositor_->Composite(frame_begin_time);
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::didInvalidateRect(const WebRect& rect) {
  TRACE_EVENT2("renderer", "RenderWidget::didInvalidateRect",
               "width", rect.width, "height", rect.height);
  // The invalidated rect might be outside the bounds of the view.
  gfx::Rect view_rect(size_);
  gfx::Rect damaged_rect = gfx::IntersectRects(view_rect, rect);
  if (damaged_rect.IsEmpty())
    return;

  paint_aggregator_.InvalidateRect(damaged_rect);

  // We may not need to schedule another call to DoDeferredUpdate.
  if (invalidation_task_posted_)
    return;
  if (!paint_aggregator_.HasPendingUpdate())
    return;
  if (update_reply_pending_ ||
      num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending)
    return;

  // When GPU rendering, combine pending animations and invalidations into
  // a single update.
  if (is_accelerated_compositing_active_ &&
      animation_update_pending_ &&
      animation_timer_.IsRunning())
    return;

  // Perform updating asynchronously.  This serves two purposes:
  // 1) Ensures that we call WebView::Paint without a bunch of other junk
  //    on the call stack.
  // 2) Allows us to collect more damage rects before painting to help coalesce
  //    the work that we will need to do.
  invalidation_task_posted_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::InvalidationCallback, this));
}

void RenderWidget::didScrollRect(int dx, int dy,
                                 const WebRect& clip_rect) {
  // Drop scrolls on the floor when we are in compositing mode.
  // TODO(nduca): stop WebViewImpl from sending scrolls in the first place.
  if (is_accelerated_compositing_active_)
    return;

  // The scrolled rect might be outside the bounds of the view.
  gfx::Rect view_rect(size_);
  gfx::Rect damaged_rect = gfx::IntersectRects(view_rect, clip_rect);
  if (damaged_rect.IsEmpty())
    return;

  paint_aggregator_.ScrollRect(gfx::Vector2d(dx, dy), damaged_rect);

  // We may not need to schedule another call to DoDeferredUpdate.
  if (invalidation_task_posted_)
    return;
  if (!paint_aggregator_.HasPendingUpdate())
    return;
  if (update_reply_pending_ ||
      num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending)
    return;

  // When GPU rendering, combine pending animations and invalidations into
  // a single update.
  if (is_accelerated_compositing_active_ &&
      animation_update_pending_ &&
      animation_timer_.IsRunning())
    return;

  // Perform updating asynchronously.  This serves two purposes:
  // 1) Ensures that we call WebView::Paint without a bunch of other junk
  //    on the call stack.
  // 2) Allows us to collect more damage rects before painting to help coalesce
  //    the work that we will need to do.
  invalidation_task_posted_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::InvalidationCallback, this));
}

void RenderWidget::didAutoResize(const WebSize& new_size) {
  if (size_.width() != new_size.width || size_.height() != new_size.height) {
    size_ = new_size;
    // If we don't clear PaintAggregator after changing autoResize state, then
    // we might end up in a situation where bitmap_rect is larger than the
    // view_size. By clearing PaintAggregator, we ensure that we don't end up
    // with invalid damage rects.
    paint_aggregator_.ClearPendingUpdate();

    if (auto_resize_mode_)
      AutoResizeCompositor();

    if (RenderThreadImpl::current()->short_circuit_size_updates()) {
      setWindowRect(WebRect(rootWindowRect().x,
                            rootWindowRect().y,
                            new_size.width,
                            new_size.height));
    } else {
      need_update_rect_for_auto_resize_ = true;
    }
  }
}

void RenderWidget::AutoResizeCompositor()  {
  physical_backing_size_ = gfx::ToCeiledSize(gfx::ScaleSize(size_,
      device_scale_factor_));
  if (compositor_)
    compositor_->setViewportSize(size_, physical_backing_size_);
}

void RenderWidget::didActivateCompositor(int input_handler_identifier) {
  TRACE_EVENT0("gpu", "RenderWidget::didActivateCompositor");

#if !defined(OS_MACOSX)
  if (!is_accelerated_compositing_active_) {
    // When not in accelerated compositing mode, in certain cases (e.g. waiting
    // for a resize or if no backing store) the RenderWidgetHost is blocking the
    // browser's UI thread for some time, waiting for an UpdateRect. If we are
    // going to switch to accelerated compositing, the GPU process may need
    // round-trips to the browser's UI thread before finishing the frame,
    // causing deadlocks if we delay the UpdateRect until we receive the
    // OnSwapBuffersComplete.  So send a dummy message that will unblock the
    // browser's UI thread. This is not necessary on Mac, because SwapBuffers
    // now unblocks GetBackingStore on Mac.
    Send(new ViewHostMsg_UpdateIsDelayed(routing_id_));
  }
#endif

  is_accelerated_compositing_active_ = true;
  Send(new ViewHostMsg_DidActivateAcceleratedCompositing(
      routing_id_, is_accelerated_compositing_active_));
}

void RenderWidget::didDeactivateCompositor() {
  TRACE_EVENT0("gpu", "RenderWidget::didDeactivateCompositor");

  is_accelerated_compositing_active_ = false;
  Send(new ViewHostMsg_DidActivateAcceleratedCompositing(
      routing_id_, is_accelerated_compositing_active_));

  if (using_asynchronous_swapbuffers_)
    using_asynchronous_swapbuffers_ = false;

  // In single-threaded mode, we exit force compositing mode and re-enter in
  // DoDeferredUpdate() if appropriate. In threaded compositing mode,
  // DoDeferredUpdate() is bypassed and WebKit is responsible for exiting and
  // entering force compositing mode at the appropriate times.
  if (!is_threaded_compositing_enabled_)
    webwidget_->enterForceCompositingMode(false);
}

void RenderWidget::initializeLayerTreeView() {
  compositor_ = RenderWidgetCompositor::Create(this);
  if (!compositor_)
    return;

  compositor_->setViewportSize(size_, physical_backing_size_);
  if (init_complete_)
    compositor_->setSurfaceReady();
}

WebKit::WebLayerTreeView* RenderWidget::layerTreeView() {
  return compositor_.get();
}

void RenderWidget::suppressCompositorScheduling(bool enable) {
  if (compositor_)
    compositor_->SetSuppressScheduleComposite(enable);
}

void RenderWidget::willBeginCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::willBeginCompositorFrame");

  DCHECK(RenderThreadImpl::current()->compositor_message_loop_proxy());

  // The following two can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputState(DO_NOT_SHOW_IME);
  UpdateSelectionBounds();

  WillInitiatePaint();
}

void RenderWidget::didBecomeReadyForAdditionalInput() {
  TRACE_EVENT0("renderer", "RenderWidget::didBecomeReadyForAdditionalInput");
  if (pending_input_event_ack_)
    Send(pending_input_event_ack_.release());
}

void RenderWidget::DidCommitCompositorFrame() {
}

void RenderWidget::didCommitAndDrawCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::didCommitAndDrawCompositorFrame");
  // Accelerated FPS tick for performance tests. See throughput_tests.cc.
  // NOTE: Tests may break if this event is renamed or moved.
  UNSHIPPED_TRACE_EVENT_INSTANT0("test_fps", "TestFrameTickGPU",
                                 TRACE_EVENT_SCOPE_THREAD);
  // Notify subclasses that we initiated the paint operation.
  DidInitiatePaint();
}

void RenderWidget::didCompleteSwapBuffers() {
  TRACE_EVENT0("renderer", "RenderWidget::didCompleteSwapBuffers");

  // Notify subclasses threaded composited rendering was flushed to the screen.
  DidFlushPaint();

  if (update_reply_pending_)
    return;

  if (!next_paint_flags_ &&
      !need_update_rect_for_auto_resize_ &&
      !plugin_window_moves_.size()) {
    return;
  }

  ViewHostMsg_UpdateRect_Params params;
  params.view_size = size_;
  params.plugin_window_moves.swap(plugin_window_moves_);
  params.flags = next_paint_flags_;
  params.scroll_offset = GetScrollOffset();
  params.needs_ack = false;
  params.scale_factor = device_scale_factor_;

  Send(new ViewHostMsg_UpdateRect(routing_id_, params));
  next_paint_flags_ = 0;
  need_update_rect_for_auto_resize_ = false;
}

void RenderWidget::scheduleComposite() {
  TRACE_EVENT0("gpu", "RenderWidget::scheduleComposite");
  if (RenderThreadImpl::current()->compositor_message_loop_proxy() &&
      compositor_) {
    compositor_->setNeedsRedraw();
  } else {
    // TODO(nduca): replace with something a little less hacky.  The reason this
    // hack is still used is because the Invalidate-DoDeferredUpdate loop
    // contains a lot of host-renderer synchronization logic that is still
    // important for the accelerated compositing case. The option of simply
    // duplicating all that code is less desirable than "faking out" the
    // invalidation path using a magical damage rect.
    didInvalidateRect(WebRect(0, 0, 1, 1));
  }
}

void RenderWidget::scheduleAnimation() {
  if (animation_update_pending_)
    return;

  TRACE_EVENT0("gpu", "RenderWidget::scheduleAnimation");
  animation_update_pending_ = true;
  if (!animation_timer_.IsRunning()) {
    animation_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(0), this,
                           &RenderWidget::AnimationCallback);
  }
}

void RenderWidget::didChangeCursor(const WebCursorInfo& cursor_info) {
  // TODO(darin): Eliminate this temporary.
  WebCursor cursor(cursor_info);

  // Only send a SetCursor message if we need to make a change.
  if (!current_cursor_.IsEqual(cursor)) {
    current_cursor_ = cursor;
    Send(new ViewHostMsg_SetCursor(routing_id_, cursor));
  }
}

// We are supposed to get a single call to Show for a newly created RenderWidget
// that was created via RenderWidget::CreateWebView.  So, we wait until this
// point to dispatch the ShowWidget message.
//
// This method provides us with the information about how to display the newly
// created RenderWidget (i.e., as a blocked popup or as a new tab).
//
void RenderWidget::show(WebNavigationPolicy) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK(routing_id_ != MSG_ROUTING_NONE);
  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  if (did_show_)
    return;

  did_show_ = true;
  // NOTE: initial_pos_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if as_popup is false, or the browser
  // process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowWidget(opener_id_, routing_id_, initial_pos_));
  SetPendingWindowRect(initial_pos_);
}

void RenderWidget::didFocus() {
}

void RenderWidget::didBlur() {
}

void RenderWidget::DoDeferredClose() {
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidget::closeWidgetSoon() {
  if (is_swapped_out_) {
    // This widget is currently swapped out, and the active widget is in a
    // different process.  Have the browser route the close request to the
    // active widget instead, so that the correct unload handlers are run.
    Send(new ViewHostMsg_RouteCloseEvent(routing_id_));
    return;
  }

  // If a page calls window.close() twice, we'll end up here twice, but that's
  // OK.  It is safe to send multiple Close messages.

  // Ask the RenderWidgetHost to initiate close.  We could be called from deep
  // in Javascript.  If we ask the RendwerWidgetHost to close now, the window
  // could be closed before the JS finishes executing.  So instead, post a
  // message back to the message loop, which won't run until the JS is
  // complete, and then the Close message can be sent.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::DoDeferredClose, this));
}

void RenderWidget::Close() {
  if (webwidget_) {
    webwidget_->willCloseLayerTreeView();
    compositor_.reset();
    webwidget_->close();
    webwidget_ = NULL;
  }
}

WebRect RenderWidget::windowRect() {
  if (pending_window_rect_count_)
    return pending_window_rect_;

  return view_screen_rect_;
}

void RenderWidget::setToolTipText(const WebKit::WebString& text,
                                  WebTextDirection hint) {
  Send(new ViewHostMsg_SetTooltipText(routing_id_, text, hint));
}

void RenderWidget::setWindowRect(const WebRect& pos) {
  if (did_show_) {
    if (!RenderThreadImpl::current()->short_circuit_size_updates()) {
      Send(new ViewHostMsg_RequestMove(routing_id_, pos));
      SetPendingWindowRect(pos);
    } else {
      WebSize new_size(pos.width, pos.height);
      Resize(new_size, new_size, overdraw_bottom_height_,
             WebRect(), is_fullscreen_, NO_RESIZE_ACK);
      view_screen_rect_ = pos;
      window_screen_rect_ = pos;
    }
  } else {
    initial_pos_ = pos;
  }
}

void RenderWidget::SetPendingWindowRect(const WebRect& rect) {
  pending_window_rect_ = rect;
  pending_window_rect_count_++;
}

WebRect RenderWidget::rootWindowRect() {
  if (pending_window_rect_count_) {
    // NOTE(mbelshe): If there is a pending_window_rect_, then getting
    // the RootWindowRect is probably going to return wrong results since the
    // browser may not have processed the Move yet.  There isn't really anything
    // good to do in this case, and it shouldn't happen - since this size is
    // only really needed for windowToScreen, which is only used for Popups.
    return pending_window_rect_;
  }

  return window_screen_rect_;
}

WebRect RenderWidget::windowResizerRect() {
  return resizer_rect_;
}

void RenderWidget::OnSetInputMethodActive(bool is_active) {
  // To prevent this renderer process from sending unnecessary IPC messages to
  // a browser process, we permit the renderer process to send IPC messages
  // only during the input method attached to the browser process is active.
  input_method_is_active_ = is_active;
}

void RenderWidget::UpdateCompositionInfo(
    const ui::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (!ShouldUpdateCompositionInfo(range, character_bounds))
    return;
  composition_character_bounds_ = character_bounds;
  composition_range_ = range;
  Send(new ViewHostMsg_ImeCompositionRangeChanged(
      routing_id(), composition_range_, composition_character_bounds_));
}

void RenderWidget::OnImeSetComposition(
    const string16& text,
    const std::vector<WebCompositionUnderline>& underlines,
    int selection_start, int selection_end) {
  if (!webwidget_)
    return;
  DCHECK(!handling_ime_event_);
  handling_ime_event_ = true;
  if (webwidget_->setComposition(
      text, WebVector<WebCompositionUnderline>(underlines),
      selection_start, selection_end)) {
    // Setting the IME composition was successful. Send the new composition
    // range to the browser.
    ui::Range range(ui::Range::InvalidRange());
    size_t location, length;
    if (webwidget_->compositionRange(&location, &length)) {
      range.set_start(location);
      range.set_end(location + length);
    }
    // The IME was cancelled via the Esc key, so just send back the caret.
    else if (webwidget_->caretOrSelectionRange(&location, &length)) {
      range.set_start(location);
      range.set_end(location + length);
    }
    std::vector<gfx::Rect> character_bounds;
    GetCompositionCharacterBounds(&character_bounds);
    UpdateCompositionInfo(range, character_bounds);
  } else {
    // If we failed to set the composition text, then we need to let the browser
    // process to cancel the input method's ongoing composition session, to make
    // sure we are in a consistent state.
    Send(new ViewHostMsg_ImeCancelComposition(routing_id()));

    // Send an updated IME range with just the caret range.
    ui::Range range(ui::Range::InvalidRange());
    size_t location, length;
    if (webwidget_->caretOrSelectionRange(&location, &length)) {
      range.set_start(location);
      range.set_end(location + length);
    }
    UpdateCompositionInfo(range, std::vector<gfx::Rect>());
  }
  handling_ime_event_ = false;
  UpdateTextInputState(DO_NOT_SHOW_IME);
}

void RenderWidget::OnImeConfirmComposition(
    const string16& text, const ui::Range& replacement_range) {
  if (!webwidget_)
    return;
  DCHECK(!handling_ime_event_);
  handling_ime_event_ = true;
  handling_input_event_ = true;
  webwidget_->confirmComposition(text);
  handling_input_event_ = false;

  // Send an updated IME range with just the caret range.
  ui::Range range(ui::Range::InvalidRange());
  size_t location, length;
  if (webwidget_->caretOrSelectionRange(&location, &length)) {
    range.set_start(location);
    range.set_end(location + length);
  }
  UpdateCompositionInfo(range, std::vector<gfx::Rect>());
  handling_ime_event_ = false;
  UpdateTextInputState(DO_NOT_SHOW_IME);
}

// This message causes the renderer to render an image of the
// desired_size, regardless of whether the tab is hidden or not.
void RenderWidget::OnPaintAtSize(const TransportDIB::Handle& dib_handle,
                                 int tag,
                                 const gfx::Size& page_size,
                                 const gfx::Size& desired_size) {
  if (!webwidget_ || !TransportDIB::is_valid_handle(dib_handle)) {
    if (TransportDIB::is_valid_handle(dib_handle)) {
      // Close our unused handle.
#if defined(OS_WIN)
      ::CloseHandle(dib_handle);
#elif defined(OS_MACOSX)
      base::SharedMemory::CloseHandle(dib_handle);
#endif
    }
    return;
  }

  if (page_size.IsEmpty() || desired_size.IsEmpty()) {
    // If one of these is empty, then we just return the dib we were
    // given, to avoid leaking it.
    Send(new ViewHostMsg_PaintAtSize_ACK(routing_id_, tag, desired_size));
    return;
  }

  // Map the given DIB ID into this process, and unmap it at the end
  // of this function.
  scoped_ptr<TransportDIB> paint_at_size_buffer(
      TransportDIB::CreateWithHandle(dib_handle));

  gfx::Size page_size_in_pixel = gfx::ToFlooredSize(
      gfx::ScaleSize(page_size, device_scale_factor_));
  gfx::Size desired_size_in_pixel = gfx::ToFlooredSize(
      gfx::ScaleSize(desired_size, device_scale_factor_));
  gfx::Size canvas_size = page_size_in_pixel;
  float x_scale = static_cast<float>(desired_size_in_pixel.width()) /
                  static_cast<float>(canvas_size.width());
  float y_scale = static_cast<float>(desired_size_in_pixel.height()) /
                  static_cast<float>(canvas_size.height());

  gfx::Rect orig_bounds(canvas_size);
  canvas_size.set_width(static_cast<int>(canvas_size.width() * x_scale));
  canvas_size.set_height(static_cast<int>(canvas_size.height() * y_scale));
  gfx::Rect bounds(canvas_size);

  scoped_ptr<skia::PlatformCanvas> canvas(
      paint_at_size_buffer->GetPlatformCanvas(canvas_size.width(),
                                              canvas_size.height()));
  if (!canvas) {
    NOTREACHED();
    return;
  }

  // Reset bounds to what we actually received, but they should be the
  // same.
  DCHECK_EQ(bounds.width(), canvas->getDevice()->width());
  DCHECK_EQ(bounds.height(), canvas->getDevice()->height());
  bounds.set_width(canvas->getDevice()->width());
  bounds.set_height(canvas->getDevice()->height());

  canvas->save();
  // Add the scale factor to the canvas, so that we'll get the desired size.
  canvas->scale(SkFloatToScalar(x_scale), SkFloatToScalar(y_scale));

  // Have to make sure we're laid out at the right size before
  // rendering.
  gfx::Size old_size = webwidget_->size();
  webwidget_->resize(page_size);
  webwidget_->layout();

  // Paint the entire thing (using original bounds, not scaled bounds).
  PaintRect(orig_bounds, orig_bounds.origin(), canvas.get());
  canvas->restore();

  // Return the widget to its previous size.
  webwidget_->resize(old_size);

  Send(new ViewHostMsg_PaintAtSize_ACK(routing_id_, tag, bounds.size()));
}

void RenderWidget::OnSnapshot(const gfx::Rect& src_subrect) {
  SkBitmap snapshot;

  if (OnSnapshotHelper(src_subrect, &snapshot)) {
    Send(new ViewHostMsg_Snapshot(routing_id(), true, snapshot));
  } else {
    Send(new ViewHostMsg_Snapshot(routing_id(), false, SkBitmap()));
  }
}

bool RenderWidget::OnSnapshotHelper(const gfx::Rect& src_subrect,
                                    SkBitmap* snapshot) {
  base::TimeTicks beginning_time = base::TimeTicks::Now();

  if (!webwidget_ || src_subrect.IsEmpty())
    return false;

  gfx::Rect viewport_size = gfx::IntersectRects(
      src_subrect, gfx::Rect(physical_backing_size_));

  skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(
      skia::CreatePlatformCanvas(viewport_size.width(),
                                 viewport_size.height(),
                                 true,
                                 NULL,
                                 skia::RETURN_NULL_ON_FAILURE));
  if (!canvas)
    return false;

  canvas->save();
  webwidget_->layout();

  PaintRect(viewport_size, viewport_size.origin(), canvas.get());
  canvas->restore();

  const SkBitmap& bitmap = skia::GetTopDevice(*canvas)->accessBitmap(false);
  if (!bitmap.copyTo(snapshot, SkBitmap::kARGB_8888_Config))
    return false;

  UMA_HISTOGRAM_TIMES("Renderer4.Snapshot",
                      base::TimeTicks::Now() - beginning_time);
  return true;
}

void RenderWidget::OnRepaint(gfx::Size size_to_paint) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // Even if the browser provides an empty damage rect, it's still expecting to
  // receive a repaint ack so just damage the entire widget bounds.
  if (size_to_paint.IsEmpty()) {
    size_to_paint = size_;
  }

  set_next_paint_is_repaint_ack();
  if (is_accelerated_compositing_active_ && compositor_) {
    compositor_->SetNeedsRedrawRect(gfx::Rect(size_to_paint));
  } else {
    gfx::Rect repaint_rect(size_to_paint.width(), size_to_paint.height());
    didInvalidateRect(repaint_rect);
  }
}

void RenderWidget::OnSmoothScrollCompleted() {
  pending_smooth_scroll_gesture_.Run();
}

void RenderWidget::OnSetTextDirection(WebTextDirection direction) {
  if (!webwidget_)
    return;
  webwidget_->setTextDirection(direction);
}

void RenderWidget::OnScreenInfoChanged(
    const WebKit::WebScreenInfo& screen_info) {
  screen_info_ = screen_info;
  SetDeviceScaleFactor(screen_info.deviceScaleFactor);
}

void RenderWidget::OnUpdateScreenRects(const gfx::Rect& view_screen_rect,
                                       const gfx::Rect& window_screen_rect) {
  view_screen_rect_ = view_screen_rect;
  window_screen_rect_ = window_screen_rect;
  Send(new ViewHostMsg_UpdateScreenRects_ACK(routing_id()));
}

#if defined(OS_ANDROID)
void RenderWidget::OnImeBatchStateChanged(bool is_begin) {
  Send(new ViewHostMsg_ImeBatchStateChanged_ACK(routing_id(), is_begin));
}

void RenderWidget::OnShowImeIfNeeded() {
  UpdateTextInputState(SHOW_IME_IF_NEEDED);
}
#endif


#if defined(SBROWSER_SBR_NATIVE_CONTENTVIEW)  && defined(SBROWSER_ENTERKEY_LONGPRESS)
void RenderWidget::OnLongPressOnFocussed(const WebKit::WebInputEvent* longpress_event){

  if (!webwidget_)
    return;

  WebKit::WebGestureEvent* lp_event = (WebKit::WebGestureEvent*)longpress_event;

  WebRect boxrect = webwidget_->focusedNodeBounds();

  if(webwidget_->isFocusedNodeSelectInput() || webwidget_->isFocusedNodeTextInput()){
	  LOG(INFO)<<"returning because focussed is input element";
	  return;
  }


  LOG(INFO)<<"longpress node bounds ("<<boxrect.x<<","<<boxrect.y<<")";
  LOG(INFO)<<"longpress node bounds ("<<boxrect.width<<","<<boxrect.height<<")";

  lp_event->x = boxrect.x + (boxrect.width/2);
  lp_event->y = boxrect.y + (boxrect.height/2);

  OnHandleInputEvent(lp_event, false);
}
#endif


void RenderWidget::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;

  device_scale_factor_ = device_scale_factor;

  if (!is_accelerated_compositing_active_) {
    didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
  } else {
    scheduleComposite();
  }
}

webkit::ppapi::PluginInstance* RenderWidget::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip,
    float* scale_factor) {
  // Bare RenderWidgets don't support optimized plugin painting.
  return NULL;
}

gfx::Vector2d RenderWidget::GetScrollOffset() {
  // Bare RenderWidgets don't support scroll offset.
  return gfx::Vector2d();
}

void RenderWidget::SetHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it.
  is_hidden_ = hidden;
#if defined(SBROWSER_ENABLE_APPNAP)  
  webwidget_->shouldEnableAppNap(is_hidden_);
#endif  
  if (is_hidden_)
    RenderThread::Get()->WidgetHidden();
  else
    RenderThread::Get()->WidgetRestored();
}

void RenderWidget::WillToggleFullscreen() {
  if (!webwidget_)
    return;

  if (is_fullscreen_) {
    webwidget_->willExitFullScreen();
  } else {
    webwidget_->willEnterFullScreen();
  }
}

void RenderWidget::DidToggleFullscreen() {
  if (!webwidget_)
    return;

  if (is_fullscreen_) {
    webwidget_->didEnterFullScreen();
  } else {
    webwidget_->didExitFullScreen();
  }
}

void RenderWidget::SetBackground(const SkBitmap& background) {
  background_ = background;

  // Generate a full repaint.
  didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
}

bool RenderWidget::next_paint_is_resize_ack() const {
  return ViewHostMsg_UpdateRect_Flags::is_resize_ack(next_paint_flags_);
}

bool RenderWidget::next_paint_is_restore_ack() const {
  return ViewHostMsg_UpdateRect_Flags::is_restore_ack(next_paint_flags_);
}

void RenderWidget::set_next_paint_is_resize_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
}

void RenderWidget::set_next_paint_is_restore_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_RESTORE_ACK;
}

void RenderWidget::set_next_paint_is_repaint_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK;
}

static bool IsDateTimeInput(ui::TextInputType type) {
  return type == ui::TEXT_INPUT_TYPE_DATE ||
      type == ui::TEXT_INPUT_TYPE_DATE_TIME ||
      type == ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL ||
      type == ui::TEXT_INPUT_TYPE_MONTH ||
      type == ui::TEXT_INPUT_TYPE_TIME ||
      type == ui::TEXT_INPUT_TYPE_WEEK;
}

void RenderWidget::UpdateTextInputState(ShowIme show_ime) {
  if (handling_ime_event_)
    return;
  bool show_ime_if_needed = (show_ime == SHOW_IME_IF_NEEDED);
  if (!show_ime_if_needed && !input_method_is_active_)
    return;
  ui::TextInputType new_type = GetTextInputType();
  if (IsDateTimeInput(new_type))
    return;  // Not considered as a text input field in WebKit/Chromium.

  WebKit::WebTextInputInfo new_info;
  if (webwidget_)
    new_info = webwidget_->textInputInfo();

  bool new_can_compose_inline = CanComposeInline();

  // Only sends text input params if they are changed or if the ime should be
  // shown.
  if (show_ime_if_needed || (text_input_type_ != new_type
      || text_input_info_ != new_info
      || can_compose_inline_ != new_can_compose_inline)) {
    ViewHostMsg_TextInputState_Params p;
    p.type = new_type;
    p.value = new_info.value.utf8();
    p.selection_start = new_info.selectionStart;
    p.selection_end = new_info.selectionEnd;
    p.composition_start = new_info.compositionStart;
    p.composition_end = new_info.compositionEnd;
#if defined(SBROWSER_KEYLAG)
	 change_listener_registered_ = new_info.changeAttrEnabled;
#endif
    p.can_compose_inline = new_can_compose_inline;
    p.show_ime_if_needed = show_ime_if_needed;
	#if defined(SBROWSER_FORM_NAVIGATION)
    if (webwidget_)
      p.privateIMEOptions = new_info.privateIMEOptions;
    else {
      p.privateIMEOptions = 0;
      new_info.privateIMEOptions = 0;
    }
#if defined(SBROWSER_ENABLE_JPN_SUPPORT_SUGGESTION_OPTION)
    p.spellCheckingEnabled = new_info.spellCheckingEnabled;//SAMSUNG For JPN IME
#endif
#endif
	LOG(INFO) <<__FUNCTION__<<" value of showme if needed "<< p.show_ime_if_needed;
    Send(new ViewHostMsg_TextInputStateChanged(routing_id(), p));

    text_input_info_ = new_info;
    text_input_type_ = new_type;
    can_compose_inline_ = new_can_compose_inline;
  }
}

void RenderWidget::GetSelectionBounds(gfx::Rect* focus, gfx::Rect* anchor) {
#if defined(SBROWSER_TEXT_SELECTION)
  WebRect focus_webrect(0,0,0,0);
  WebRect anchor_webrect(0,0,0,0);
#else
  WebRect focus_webrect;
  WebRect anchor_webrect;
#endif
  webwidget_->selectionBounds(focus_webrect, anchor_webrect);
  *focus = focus_webrect;
  *anchor = anchor_webrect;
}

void RenderWidget::UpdateSelectionBounds() {
  if (!webwidget_)
    return;

  ViewHostMsg_SelectionBounds_Params params;
  GetSelectionBounds(&params.anchor_rect, &params.focus_rect);
  if (selection_anchor_rect_ != params.anchor_rect ||
      selection_focus_rect_ != params.focus_rect) {
    selection_anchor_rect_ = params.anchor_rect;
    selection_focus_rect_ = params.focus_rect;
    webwidget_->selectionTextDirection(params.focus_dir, params.anchor_dir);
    params.is_anchor_first = webwidget_->isSelectionAnchorFirst();
    Send(new ViewHostMsg_SelectionBoundsChanged(routing_id_, params));
  }

  std::vector<gfx::Rect> character_bounds;
  GetCompositionCharacterBounds(&character_bounds);
  UpdateCompositionInfo(composition_range_, character_bounds);
}

#if defined(SBROWSER_TEXT_SELECTION)	
void RenderWidget::onUnmarkSelection(){
  if (webwidget_)
    webwidget_->unmarkSelection();
}
#endif 

bool RenderWidget::ShouldUpdateCompositionInfo(
    const ui::Range& range,
    const std::vector<gfx::Rect>& bounds) {
  if (composition_range_ != range)
    return true;
  if (bounds.size() != composition_character_bounds_.size())
    return true;
  for (size_t i = 0; i < bounds.size(); ++i) {
    if (bounds[i] != composition_character_bounds_[i])
      return true;
  }
  return false;
}

// Check WebKit::WebTextInputType and ui::TextInputType is kept in sync.
COMPILE_ASSERT(int(WebKit::WebTextInputTypeNone) == \
               int(ui::TEXT_INPUT_TYPE_NONE), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeText) == \
               int(ui::TEXT_INPUT_TYPE_TEXT), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypePassword) == \
               int(ui::TEXT_INPUT_TYPE_PASSWORD), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeSearch) == \
               int(ui::TEXT_INPUT_TYPE_SEARCH), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeEmail) == \
               int(ui::TEXT_INPUT_TYPE_EMAIL), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeNumber) == \
               int(ui::TEXT_INPUT_TYPE_NUMBER), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeTelephone) == \
               int(ui::TEXT_INPUT_TYPE_TELEPHONE), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeURL) == \
               int(ui::TEXT_INPUT_TYPE_URL), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeDate) == \
               int(ui::TEXT_INPUT_TYPE_DATE), mismatching_enum);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeDateTime) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME), mismatching_enum);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeDateTimeLocal) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL), mismatching_enum);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeMonth) == \
               int(ui::TEXT_INPUT_TYPE_MONTH), mismatching_enum);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeTime) == \
               int(ui::TEXT_INPUT_TYPE_TIME), mismatching_enum);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeWeek) == \
               int(ui::TEXT_INPUT_TYPE_WEEK), mismatching_enum);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeTextArea) == \
               int(ui::TEXT_INPUT_TYPE_TEXT_AREA), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeContentEditable) == \
               int(ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeDateTimeField) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD), mismatching_enums);

ui::TextInputType RenderWidget::WebKitToUiTextInputType(
    WebKit::WebTextInputType type) {
  // Check the type is in the range representable by ui::TextInputType.
  DCHECK_LE(type, static_cast<int>(ui::TEXT_INPUT_TYPE_MAX)) <<
    "WebKit::WebTextInputType and ui::TextInputType not synchronized";
  return static_cast<ui::TextInputType>(type);
}

ui::TextInputType RenderWidget::GetTextInputType() {
  if (webwidget_)
    return WebKitToUiTextInputType(webwidget_->textInputInfo().type);
  return ui::TEXT_INPUT_TYPE_NONE;
}

void RenderWidget::GetCompositionCharacterBounds(
    std::vector<gfx::Rect>* bounds) {
  DCHECK(bounds);
  bounds->clear();
}

bool RenderWidget::CanComposeInline() {
  return true;
}

WebScreenInfo RenderWidget::screenInfo() {
  return screen_info_;
}

float RenderWidget::deviceScaleFactor() {
  return device_scale_factor_;
}

#if defined(SBROWSER_KEYLAG)
#if defined(OS_ANDROID)
void RenderWidget::SuspendDiferredInputJS() {
  
    TRACE_EVENT0("renderer", "RenderWidget::SuspendDiferredInputJS(0)");
  

  WebKit::WebView* wv = static_cast<WebKit::WebView*>(webwidget());
  WebKit::WebFrame* main_frame = wv->mainFrame();

  
    if( main_frame ){
        int delay  = 170;
        if (deferred_input_js_timer_.IsRunning()) {
            TRACE_EVENT0("renderer", "RenderWidget::timer(reset)");
            deferred_input_js_timer_.Reset();
        }else{
            TRACE_EVENT0("renderer", "RenderWidget::suspend(start)");
            main_frame->suspendInputJS();
            TRACE_EVENT0("renderer", "RenderWidget::timer(start)");
            deferred_input_js_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(delay), this,
                    &RenderWidget::ResumeDiferredInputJS);
        }
    }
    TRACE_EVENT0("renderer", "RenderWidget::SuspendDiferredInputJS(1)");
}

void RenderWidget::ResumeDiferredInputJS() {
    TRACE_EVENT0("renderer", "RenderWidget::ResumeDiferredInputJS(0)");
   WebKit::WebView* wv = static_cast<WebKit::WebView*>(webwidget());
   WebKit::WebFrame* main_frame = wv->mainFrame();
    if( main_frame ){
        TRACE_EVENT0("renderer", "RenderWidget::ResumeDiferredInputJS(1)");
        main_frame->resumeInputJS();
    }
}
void RenderWidget::OnDeferredInputJS(bool isJsDeferred) {	
    WebKit::WebView* wv = static_cast<WebKit::WebView*>(webwidget());
    if(wv && wv->mainFrame()){	
        if(isJsDeferred){
            wv->mainFrame()->suspendInputJS();
        }
        else {
            wv->mainFrame()->resumeInputJS();
        }
	}
}

#endif
#endif

#if defined(SBROWSER_SEARCH_SCROLL_FIX) 
void RenderWidget::resetInputMethodOnTap(const WebKit::WebInputEvent &input_event){
	if (!input_method_is_active_)
	  return;
	
	// If the last text input type is not None, then we should finish any
	// ongoing composition regardless of the new text input type.
	if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE) {
	  // If a composition text exists, then we need to let the browser process
	  // to cancel the input method's ongoing composition session.
	  if (webwidget_->confirmCompositionOnTap(input_event))
		  Send(new ViewHostMsg_ImeCancelComposition(routing_id()));
	}
	
	// Send an updated IME range with the current caret rect.
	ui::Range range(ui::Range::InvalidRange());
	size_t location, length;
	if (webwidget_->caretOrSelectionRange(&location, &length)) {
	  range.set_start(location);
	  range.set_end(location + length);
	}
	
	UpdateCompositionInfo(range, std::vector<gfx::Rect>());

}
#endif

void RenderWidget::resetInputMethod() {
  if (!input_method_is_active_)
    return;

  // If the last text input type is not None, then we should finish any
  // ongoing composition regardless of the new text input type.
  if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE) {
    // If a composition text exists, then we need to let the browser process
    // to cancel the input method's ongoing composition session.
    if (webwidget_->confirmComposition())
		Send(new ViewHostMsg_ImeCancelComposition(routing_id()));
  }

  // Send an updated IME range with the current caret rect.
  ui::Range range(ui::Range::InvalidRange());
  size_t location, length;
  if (webwidget_->caretOrSelectionRange(&location, &length)) {
    range.set_start(location);
    range.set_end(location + length);
  }

  UpdateCompositionInfo(range, std::vector<gfx::Rect>());
}

void RenderWidget::didHandleGestureEvent(
    const WebGestureEvent& event,
    bool event_cancelled) {
#if defined(OS_ANDROID)
  if (event_cancelled)
    return;

  if (event.type == WebInputEvent::GestureTap ||
      event.type == WebInputEvent::GestureLongPress) {
    UpdateTextInputState(SHOW_IME_IF_NEEDED);
  }
#endif
}

void RenderWidget::SchedulePluginMove(
    const webkit::npapi::WebPluginGeometry& move) {
  size_t i = 0;
  for (; i < plugin_window_moves_.size(); ++i) {
    if (plugin_window_moves_[i].window == move.window) {
      if (move.rects_valid) {
        plugin_window_moves_[i] = move;
      } else {
        plugin_window_moves_[i].visible = move.visible;
      }
      break;
    }
  }

  if (i == plugin_window_moves_.size())
    plugin_window_moves_.push_back(move);
}

void RenderWidget::CleanupWindowInPluginMoves(gfx::PluginWindowHandle window) {
  for (WebPluginGeometryVector::iterator i = plugin_window_moves_.begin();
       i != plugin_window_moves_.end(); ++i) {
    if (i->window == window) {
      plugin_window_moves_.erase(i);
      break;
    }
  }
}

void RenderWidget::GetRenderingStats(
    WebKit::WebRenderingStatsImpl& stats) const {
  if (compositor_)
    compositor_->GetRenderingStats(&stats.rendering_stats);

  stats.rendering_stats.animation_frame_count +=
      software_stats_.animation_frame_count;
  stats.rendering_stats.screen_frame_count +=
      software_stats_.screen_frame_count;
  stats.rendering_stats.total_paint_time +=
      software_stats_.total_paint_time;
  stats.rendering_stats.total_pixels_painted +=
      software_stats_.total_pixels_painted;
}

bool RenderWidget::GetGpuRenderingStats(GpuRenderingStats* stats) const {
  GpuChannelHost* gpu_channel = RenderThreadImpl::current()->GetGpuChannel();
  if (!gpu_channel)
    return false;

  return gpu_channel->CollectRenderingStatsForSurface(surface_id(), stats);
}

RenderWidgetCompositor* RenderWidget::compositor() const {
  return compositor_.get();
}

void RenderWidget::BeginSmoothScroll(
    bool down,
    const SmoothScrollCompletionCallback& callback,
    int pixels_to_scroll,
    int mouse_event_x,
    int mouse_event_y) {
  DCHECK(!callback.is_null());

  ViewHostMsg_BeginSmoothScroll_Params params;
  params.scroll_down = down;
  params.pixels_to_scroll = pixels_to_scroll;
  params.mouse_event_x = mouse_event_x;
  params.mouse_event_y = mouse_event_y;

  Send(new ViewHostMsg_BeginSmoothScroll(routing_id_, params));
  pending_smooth_scroll_gesture_ = callback;
}

bool RenderWidget::WillHandleMouseEvent(const WebKit::WebMouseEvent& event) {
  return false;
}

bool RenderWidget::WillHandleGestureEvent(
    const WebKit::WebGestureEvent& event) {
  return false;
}

void RenderWidget::hasTouchEventHandlers(bool has_handlers) {
  Send(new ViewHostMsg_HasTouchEventHandlers(routing_id_, has_handlers));
}

bool RenderWidget::HasTouchEventHandlersAt(const gfx::Point& point) const {
  return true;
}

WebGraphicsContext3DCommandBufferImpl* RenderWidget::CreateGraphicsContext3D(
    const WebKit::WebGraphicsContext3D::Attributes& attributes) {
  if (!webwidget_)
    return NULL;
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(
          surface_id(),
          GetURLForGraphicsContext3D(),
          RenderThreadImpl::current(),
          weak_ptr_factory_.GetWeakPtr()));

  if (!context->Initialize(
          attributes,
          false /* bind generates resources */,
          CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE))
    return NULL;
  return context.release();
}

#if defined(SBROWSER_DEFER_COMMITS_ON_GESTURE)
void RenderWidget::SetDeferCommitsOnGesture(bool deferCommits) {
	defer_commits_ = deferCommits;
}
#endif

#if defined(SBROWSER_HIDE_URLBAR)
void RenderWidget::OnHideUrlBarCmd(ViewMsg_HideUrlBarCmd_Params params) {
  if(compositor_){
    switch(params.cmd_type){
      case cc::SET_URLBAR_BITMAP:{
        compositor_->SetUrlBarBitmap(&params.urlbar_bitmap);
      }
      break;
      case cc::SET_URLBAR_ACTIVE:{
        compositor_->SetUrlBarActive(params.urlbar_active, params.override_allowed);
      }
      break;
      default:
      break;
    }
  }
}
void RenderWidget::HideUrlBarEventNotify(int event, int event_data1, int event_data2) {
  ViewHostMsg_HideUrlBarCmd_Params params;
  params.cmd_type = event;
  params.cmd_data1 = event_data1;
  params.cmd_data2 = event_data2;
  switch(event){
    case cc::URLBAR_BITMAP_ACK:
    case cc::URLBAR_ACTIVE_ACK:
    case cc::SCROLL_BEGIN_NOTIFY:
    case cc::SCROLL_END_NOTIFY:{
           Send(new ViewHostMsg_HideUrlBarCmd(routing_id_, params));
         }
        break;
#if defined(SBROWSER_HIDE_URLBAR_WITHOUT_RESIZING)
    case cc::CONTENT_OFFSET_CHANGE_NOTIFY:{
           if (webwidget_)
             webwidget_->setContentTopOffset(event_data1, event_data2);
        }
        break;
#endif
    default:
        break;
    }
}
#endif

#if defined(SBROWSER_FLUSH_PENDING_MESSAGES)
void RenderWidget::OnFlushPendingMessages() {
	// send back an ack
	LOG(INFO) <<"Received ViewMsg_FlushPendingMessages";
	Send(new ViewHostMsg_DidFlushPendingMessages(routing_id_));
}
#endif

#if defined(SBROWSER_FLING_END_NOTIFICATION_CANE)
void RenderWidget::FlingAnimationCompleted(bool page_end){
  Send(new ViewMsg_FlingAnimationCompleted(routing_id_, page_end));
}
#endif
}  // namespace content
