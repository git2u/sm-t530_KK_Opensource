// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <android/bitmap.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "cc/layers/layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/surface_texture_transport_client_android.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"
#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
#include "base/command_line.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "content/common/gpu/sbr/SurfaceViewManager.h"
#endif
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
#include "base/sbr/msc.h"
#endif

namespace content {

namespace {

void InsertSyncPointAndAckForGpu(
    int gpu_host_id, int route_id, const std::string& return_mailbox) {
  uint32 sync_point =
      ImageTransportFactoryAndroid::GetInstance()->InsertSyncPoint();
  AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
  ack_params.mailbox_name = return_mailbox;
  ack_params.sync_point = sync_point;
  RenderWidgetHostImpl::AcknowledgeBufferPresent(
      route_id, gpu_host_id, ack_params);
}

void InsertSyncPointAndAckForCompositor(
    int renderer_host_id,
    int route_id,
    const gpu::Mailbox& return_mailbox,
    const gfx::Size return_size) {
  cc::CompositorFrameAck ack;
  ack.gl_frame_data.reset(new cc::GLFrameData());
  if (!return_mailbox.IsZero()) {
    ack.gl_frame_data->mailbox = return_mailbox;
    ack.gl_frame_data->size = return_size;
    ack.gl_frame_data->sync_point =
        ImageTransportFactoryAndroid::GetInstance()->InsertSyncPoint();
  }
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(
      route_id, renderer_host_id, ack);
}

}  // anonymous namespace

#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
extern int single_compositor ;
#endif

RenderWidgetHostViewAndroid::RenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host,
    ContentViewCoreImpl* content_view_core)
    : host_(widget_host),
      is_layer_attached_(true),
      content_view_core_(NULL),
      ime_adapter_android_(this),
      cached_background_color_(SK_ColorWHITE),
      texture_id_in_layer_(0) {
  if (CompositorImpl::UsesDirectGL()) {
    surface_texture_transport_.reset(new SurfaceTextureTransportClient());
    layer_ = surface_texture_transport_->Initialize();
    layer_->SetIsDrawable(true);
  } else {
    texture_layer_ = cc::TextureLayer::Create(this);
    layer_ = texture_layer_;
  }
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
 MSC_METHOD("WC","RWHVA","Create",widget_host);
 MSC_METHOD("RWHVA","RWHVA","SetContentViewCore",content_view_core);
#endif
  layer_->SetContentsOpaque(true);

  host_->SetView(this);
  SetContentViewCore(content_view_core);
   #if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
   //Needed for SBrowser because SBrowser creates the Contents even before creating the RenderViews or Compositor
   //Not required in case of Chrome
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  single_compositor=command_line->HasSwitch(switches::kEnableSingleCompositor);
  #endif
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
  MSC_METHOD("RWHVA","RWHVA","SetContentViewCore","NULL");
#endif
  SetContentViewCore(NULL);
  DCHECK(ack_callbacks_.empty());
  if (texture_id_in_layer_ || !last_mailbox_.IsZero()) {
    ImageTransportFactoryAndroid* factory =
        ImageTransportFactoryAndroid::GetInstance();
    // TODO: crbug.com/230137 - make workaround obsolete with refcounting.
    // Don't let the last frame we sent leak in the mailbox.
    if (!last_mailbox_.IsZero()) {
      if (!texture_id_in_layer_)
        texture_id_in_layer_ = factory->CreateTexture();
      factory->AcquireTexture(texture_id_in_layer_, last_mailbox_.name);
      factory->GetContext3D()->getError();  // Clear error if mailbox was empty.
    }
    factory->DeleteTexture(texture_id_in_layer_);
  }

  if (texture_layer_)
    texture_layer_->ClearClient();
}


bool RenderWidgetHostViewAndroid::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewAndroid, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeBatchStateChanged_ACK,
                        OnProcessImeBatchStateAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartContentIntent, OnStartContentIntent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeBodyBackgroundColor,
                        OnDidChangeBodyBackgroundColor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetVSyncNotificationEnabled,
                        OnSetVSyncNotificationEnabled)
// --------------------------------- kikin Modification Starts -------------------------------
    #if defined (SBROWSER_KIKIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionContextExtracted, OnSelectionContextExtracted)
    #endif
// ---------------------------------- kikin Modification Ends --------------------------------
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderWidgetHostViewAndroid::InitAsChild(gfx::NativeView parent_view) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTIMPLEMENTED();
}

RenderWidgetHost*
RenderWidgetHostViewAndroid::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAndroid::WasShown() {
  if (!host_->is_hidden())
    return;
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
  MSC_METHOD("WC","RWHVA",__FUNCTION__,"");
  MSC_METHOD("RWHVA","RVH",__FUNCTION__,"");
#endif
  host_->WasShown();
}

void RenderWidgetHostViewAndroid::WasHidden() {
  RunAckCallbacks();

  if (host_->is_hidden())
    return;
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
  MSC_METHOD("WC","RWHVA",__FUNCTION__,"");
  MSC_METHOD("RWHVA","RVH",__FUNCTION__,"");
#endif
  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host_->WasHidden();
}

void RenderWidgetHostViewAndroid::WasResized() {
  if (surface_texture_transport_.get() && content_view_core_)
    surface_texture_transport_->SetSize(
        content_view_core_->GetPhysicalBackingSize());
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
  MSC_METHOD("WC","RWHVA",__FUNCTION__,"");
  MSC_METHOD("RWHVA","RVH",__FUNCTION__,"");
#endif
  host_->WasResized();
}

void RenderWidgetHostViewAndroid::SetSize(const gfx::Size& size) {
  // Ignore the given size as only the Java code has the power to
  // resize the view on Android.
  WasResized();
}

void RenderWidgetHostViewAndroid::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}


WebKit::WebGLId RenderWidgetHostViewAndroid::GetScaledContentTexture(
    float scale,
    gfx::Size* out_size) {

#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
    if(single_compositor)
    {
    	return 0;
		/*
    	 gfx::Size size(1080,1845);

	 if (out_size)
	    *out_size = size;
        LOG(ERROR)<<"Sandeep#"<<size.width()<<"x"<<size.height();
	
	WebKit::WebGraphicsContext3D* context =
            ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
 	 if (context->isContextLost() || !context->makeContextCurrent())
    		return 0;
  	WebKit::WebGLId texture_id = context->createTexture();
  	context->bindTexture(GL_TEXTURE_2D, texture_id);
 	 context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  	context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  	context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  	context->texParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	context->texImage2D(GL_TEXTURE_2D,
                      0,
                      GL_RGBA,
                      1080,
                      1845,
                      0,
                      GL_RGBA,
                      GL_UNSIGNED_BYTE,
                      NULL);
  	context->shallowFlushCHROMIUM();
  	DCHECK(context->getError() == GL_NO_ERROR);
	MSC_METHOD("WC","RWHVA","GetScaledContentTexture",texture_id);
       return texture_id;*/
    }
#endif
	
  gfx::Size size(gfx::ToCeiledSize(
      gfx::ScaleSize(texture_size_in_layer_, scale)));

  if (!CompositorImpl::IsInitialized() ||
      texture_id_in_layer_ == 0 ||
      texture_size_in_layer_.IsEmpty() ||
      size.IsEmpty()) {
    if (out_size)
        out_size->SetSize(0, 0);

    return 0;
  }

  if (out_size)
    *out_size = size;

  GLHelper* helper = ImageTransportFactoryAndroid::GetInstance()->GetGLHelper();
  return helper->CopyAndScaleTexture(texture_id_in_layer_,
                                     texture_size_in_layer_,
                                     size,
                                     true);
}

bool RenderWidgetHostViewAndroid::PopulateBitmapWithContents(jobject jbitmap) {
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
	MSC_METHOD("UI","RWHVA",__FUNCTION__,"");
#endif	 
	
  if (!CompositorImpl::IsInitialized() ||
      texture_id_in_layer_ == 0 ||
      texture_size_in_layer_.IsEmpty())
    return false;

   gfx::JavaBitmap bitmap(jbitmap);
   
#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)	  
	if(single_compositor)
	{
		return false;
		//unsigned char*  dest = static_cast<unsigned char*> (bitmap.pixels());
		//memset(dest,0,(bitmap.size().width())*(bitmap.size().height())*4);

  		//return true;
	}
#endif
		


  // TODO(dtrainor): Eventually add support for multiple formats here.
  DCHECK(bitmap.format() == ANDROID_BITMAP_FORMAT_RGBA_8888);

  GLHelper* helper = ImageTransportFactoryAndroid::GetInstance()->GetGLHelper();

  WebKit::WebGLId texture = helper->CopyAndScaleTexture(texture_id_in_layer_,
                                                        texture_size_in_layer_,
                                                        bitmap.size(),
                                                        true);
  if (texture == 0)
    return false;

  helper->ReadbackTextureSync(texture,
                              gfx::Rect(bitmap.size()),
                              static_cast<unsigned char*> (bitmap.pixels()));

  WebKit::WebGraphicsContext3D* context =
      ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
  context->deleteTexture(texture);

  return true;
}

bool RenderWidgetHostViewAndroid::HasValidFrame() const {

#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)	  
  if(single_compositor)
  {
	if(content_view_core_)
	{
	     LOG(INFO)<<__FUNCTION__<<"HasValidFrame--->True";
	     return true;	
	}
	LOG(INFO)<<__FUNCTION__<<"HasValidFrame--->False";
	return false;
  }
#endif
  return texture_id_in_layer_ != 0 &&
     content_view_core_ &&
     !texture_size_in_layer_.IsEmpty();
}

gfx::NativeView RenderWidgetHostViewAndroid::GetNativeView() const {
  return content_view_core_->GetViewAndroid();
}

gfx::NativeViewId RenderWidgetHostViewAndroid::GetNativeViewId() const {
  return reinterpret_cast<gfx::NativeViewId>(
      const_cast<RenderWidgetHostViewAndroid*>(this));
}

gfx::NativeViewAccessible
RenderWidgetHostViewAndroid::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::MovePluginWindows(
    const gfx::Vector2d& scroll_offset,
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  // We don't have plugin windows on Android. Do nothing. Note: this is called
  // from RenderWidgetHost::OnUpdateRect which is itself invoked while
  // processing the corresponding message from Renderer.
}

void RenderWidgetHostViewAndroid::Focus() {
  host_->Focus();
  host_->SetInputMethodActive(true);
  ResetClipping();
}

void RenderWidgetHostViewAndroid::Blur() {
  host_->Send(new InputMsg_ExecuteEditCommand(
      host_->GetRoutingID(), "Unselect", ""));
  host_->SetInputMethodActive(false);
  host_->Blur();
}

bool RenderWidgetHostViewAndroid::HasFocus() const {
  if (!content_view_core_)
    return false;  // ContentViewCore not created yet.

  return content_view_core_->HasFocus();
}

bool RenderWidgetHostViewAndroid::IsSurfaceAvailableForCopy() const {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAndroid::Show() {
  if (is_layer_attached_)
    return;

  is_layer_attached_ = true;
  if (content_view_core_)
    content_view_core_->AttachLayer(layer_);
}

void RenderWidgetHostViewAndroid::Hide() {
  if (!is_layer_attached_)
    return;

  is_layer_attached_ = false;
  if (content_view_core_)
    content_view_core_->RemoveLayer(layer_);
}

bool RenderWidgetHostViewAndroid::IsShowing() {
  // ContentViewCoreImpl represents the native side of the Java
  // ContentViewCore.  It being NULL means that it is not attached
  // to the View system yet, so we treat this RWHVA as hidden.
  return is_layer_attached_ && content_view_core_;
}

gfx::Rect RenderWidgetHostViewAndroid::GetViewBounds() const {
  if (!content_view_core_)
    return gfx::Rect();

  // If the backing hasn't been initialized yet, report empty view bounds
  // as well. Otherwise, we may end up stuck in a white-screen state because
  // the resize ack is sent after swapbuffers.
  if (GetPhysicalBackingSize().IsEmpty())
    return gfx::Rect();

  gfx::Size size = content_view_core_->GetViewportSizeDip();
  gfx::Size offset = content_view_core_->GetViewportSizeOffsetDip();
  size.Enlarge(-offset.width(), -offset.height());

  return gfx::Rect(size);
}

gfx::Size RenderWidgetHostViewAndroid::GetPhysicalBackingSize() const {
  if (!content_view_core_)
    return gfx::Size();

  return content_view_core_->GetPhysicalBackingSize();
}

#if defined (SBROWSER_MULTIINSTANCE_TAB_DRAG_AND_DROP)
bool RenderWidgetHostViewAndroid::GetTabDragAndDropIsInProgress() const {
  if (!content_view_core_)
    return false;

  return content_view_core_->GetSbrContentViewCore()->GetTabDragAndDropIsInProgress();
}
#endif

float RenderWidgetHostViewAndroid::GetOverdrawBottomHeight() const {
  if (!content_view_core_)
    return 0.f;

  return content_view_core_->GetOverdrawBottomHeightDip();
}

// for closing the webselct dialog when there is no 
#if defined(SBROWSER_FORM_NAVIGATION)
void RenderWidgetHostViewAndroid::selectPopupClose(){
	if(content_view_core_)
		content_view_core_->GetSbrContentViewCore()->selectPopupClose();

}

void RenderWidgetHostViewAndroid::selectPopupCloseZero(){
	if(content_view_core_)
		content_view_core_->GetSbrContentViewCore()->selectPopupCloseZero();

}

#endif

void RenderWidgetHostViewAndroid::UpdateCursor(const WebCursor& cursor) {
  // There are no cursors on Android.
}

void RenderWidgetHostViewAndroid::SetIsLoading(bool is_loading) {
  // Do nothing. The UI notification is handled through ContentViewClient which
  // is TabContentsDelegate.
}

void RenderWidgetHostViewAndroid::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  if (!IsShowing())
    return;

  #if defined(SBROWSER_FORM_NAVIGATION)
  content_view_core_->GetSbrContentViewCore()->UpdateImeAdapter(
      GetNativeImeAdapter(),
      static_cast<int>(params.type),
      params.value, params.selection_start, params.selection_end,
      params.composition_start, params.composition_end,
      params.show_ime_if_needed,
      params.privateIMEOptions
#if defined (SBROWSER_ENABLE_JPN_SUPPORT_SUGGESTION_OPTION)
      ,params.spellCheckingEnabled
#endif
      );
  #else
    content_view_core_->UpdateImeAdapter(
      GetNativeImeAdapter(),
      static_cast<int>(params.type),
      params.value, params.selection_start, params.selection_end,
      params.composition_start, params.composition_end,
      params.show_ime_if_needed);
  #endif
}

int RenderWidgetHostViewAndroid::GetNativeImeAdapter() {
  return reinterpret_cast<int>(&ime_adapter_android_);
}

void RenderWidgetHostViewAndroid::OnProcessImeBatchStateAck(bool is_begin) {
  if (content_view_core_)
    content_view_core_->ProcessImeBatchStateAck(is_begin);
}

void RenderWidgetHostViewAndroid::OnDidChangeBodyBackgroundColor(
    SkColor color) {
  if (cached_background_color_ == color)
    return;

  cached_background_color_ = color;
  if (content_view_core_)
    content_view_core_->OnBackgroundColorChanged(color);
}

void RenderWidgetHostViewAndroid::SendVSync(base::TimeTicks frame_time) {
  host_->Send(new ViewMsg_DidVSync(host_->GetRoutingID(), frame_time));
}

#if defined(SBROWSER_BLOCK_SENDING_FRAME_MESSAGE)
void RenderWidgetHostViewAndroid::NeedToUpdateFrameInfo(bool enabled) {
  host_->Send(new ViewMsg_NeedToUpdateFrameInfo(host_->GetRoutingID(), enabled));
}
#endif

void RenderWidgetHostViewAndroid::OnSetVSyncNotificationEnabled(bool enabled) {
  if (content_view_core_)
    content_view_core_->SetVSyncNotificationEnabled(enabled);
}

void RenderWidgetHostViewAndroid::OnStartContentIntent(
    const GURL& content_url) {
  if (content_view_core_)
    content_view_core_->StartContentIntent(content_url);
}

void RenderWidgetHostViewAndroid::ImeCancelComposition() {
  ime_adapter_android_.CancelComposition();
}

void RenderWidgetHostViewAndroid::ImeCompositionRangeChanged(
    const ui::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
}

void RenderWidgetHostViewAndroid::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect,
    const gfx::Vector2d& scroll_delta,
    const std::vector<gfx::Rect>& copy_rects) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::RenderViewGone(
    base::TerminationStatus status, int error_code) {
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
     MSC_MESSAGE("RVH","RWHVA","Delete",status);
#endif
  Destroy();
}

void RenderWidgetHostViewAndroid::Destroy() {
  if (content_view_core_) {
    content_view_core_->RemoveLayer(layer_);
    content_view_core_ = NULL;
  }

  // The RenderWidgetHost's destruction led here, so don't call it.
  host_ = NULL;

  delete this;
}

void RenderWidgetHostViewAndroid::SetTooltipText(
    const string16& tooltip_text) {
  // Tooltips don't makes sense on Android.
}

void RenderWidgetHostViewAndroid::SelectionChanged(const string16& text,
                                                   size_t offset,
                                                   const ui::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

  if (text.empty() || range.is_empty() || !content_view_core_)
    return;
  size_t pos = range.GetMin() - offset;
  size_t n = range.length();

  DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
  if (pos >= text.length()) {
    NOTREACHED() << "The text can not cover range.";
    return;
  }

  std::string utf8_selection = UTF16ToUTF8(text.substr(pos, n));

  content_view_core_->OnSelectionChanged(utf8_selection);
}

void RenderWidgetHostViewAndroid::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (content_view_core_) {
    content_view_core_->OnSelectionBoundsChanged(params);
  }
}

void RenderWidgetHostViewAndroid::ScrollOffsetChanged() {
}

BackingStore* RenderWidgetHostViewAndroid::AllocBackingStore(
    const gfx::Size& size) {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::SetBackground(const SkBitmap& background) {
  RenderWidgetHostViewBase::SetBackground(background);
  host_->Send(new ViewMsg_SetBackground(host_->GetRoutingID(), background));
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(false, SkBitmap());
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

bool RenderWidgetHostViewAndroid::CanCopyToVideoFrame() const {
  return false;
}

void RenderWidgetHostViewAndroid::ShowDisambiguationPopup(
    const gfx::Rect& target_rect, const SkBitmap& zoomed_bitmap) {
  if (!content_view_core_)
    return;

  content_view_core_->ShowDisambiguationPopup(target_rect, zoomed_bitmap);
}

void RenderWidgetHostViewAndroid::OnAcceleratedCompositingStateChange() {
}

void RenderWidgetHostViewAndroid::OnSwapCompositorFrame(
    scoped_ptr<cc::CompositorFrame> frame) {
  // Always let ContentViewCore know about the new frame first, so it can decide
  // to schedule a Draw immediately when it sees the texture layer invalidation.
  if (content_view_core_) {
    // All offsets and sizes are in CSS pixels.
#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
	//Hack for proper link click in single compositor
#endif
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
  MSC_DATA_METHOD("RVH","RWHVA","OnSwapCompositorFrame","");
  MSC_DATA_CALLBACK("RWHVA","UI","UpdateFrameInfo",frame->metadata.viewport_size.ToString());
#endif
    content_view_core_->UpdateFrameInfo(
        frame->metadata.root_scroll_offset,
        frame->metadata.page_scale_factor,
        gfx::Vector2dF(frame->metadata.min_page_scale_factor,
                       frame->metadata.max_page_scale_factor),
        frame->metadata.root_layer_size,
        frame->metadata.viewport_size,
        frame->metadata.location_bar_offset,
        frame->metadata.location_bar_content_translation,
        frame->metadata.overdraw_bottom_height);
  }

  if (!frame->gl_frame_data || frame->gl_frame_data->mailbox.IsZero())
    return;

  base::Closure callback = base::Bind(&InsertSyncPointAndAckForCompositor,
                                      host_->GetProcess()->GetID(),
                                      host_->GetRoutingID(),
                                      current_mailbox_,
                                      texture_size_in_layer_);
  ImageTransportFactoryAndroid::GetInstance()->WaitSyncPoint(
      frame->gl_frame_data->sync_point);
  const gfx::Size& texture_size = frame->gl_frame_data->size;

  last_mailbox_ = current_mailbox_;

  // Calculate the content size.  This should be 0 if the texture_size is 0.
  float dp2px = frame->metadata.device_scale_factor;
  gfx::Vector2dF offset;
  if (texture_size.GetArea() > 0)
    offset = frame->metadata.location_bar_content_translation;
  offset.set_y(offset.y() + frame->metadata.overdraw_bottom_height);
  gfx::SizeF content_size(texture_size.width() - offset.x() * dp2px,
                          texture_size.height() - offset.y() * dp2px);
  BuffersSwapped(frame->gl_frame_data->mailbox,
                 texture_size,
                 content_size,
                 callback);

}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  NOTREACHED() << "Deprecated. Use --composite-to-mailbox.";

  if (params.mailbox_name.empty())
    return;

  std::string return_mailbox;
  if (!current_mailbox_.IsZero()) {
    return_mailbox.assign(
        reinterpret_cast<const char*>(current_mailbox_.name),
        sizeof(current_mailbox_.name));
  }

  base::Closure callback = base::Bind(&InsertSyncPointAndAckForGpu,
                                      gpu_host_id, params.route_id,
                                      return_mailbox);

  gpu::Mailbox mailbox;
  std::copy(params.mailbox_name.data(),
            params.mailbox_name.data() + params.mailbox_name.length(),
            reinterpret_cast<char*>(mailbox.name));

  BuffersSwapped(mailbox, params.size, params.size, callback);
}

void RenderWidgetHostViewAndroid::BuffersSwapped(
    const gpu::Mailbox& mailbox,
    const gfx::Size texture_size,
    const gfx::SizeF content_size,
    const base::Closure& ack_callback) {
  ImageTransportFactoryAndroid* factory =
      ImageTransportFactoryAndroid::GetInstance();

  // TODO(sievers): When running the impl thread in the browser we
  // need to delay the ACK until after commit and use more than a single
  // texture.
  DCHECK(!CompositorImpl::IsThreadingEnabled());

  if (texture_id_in_layer_) {
    DCHECK(!current_mailbox_.IsZero());
    ImageTransportFactoryAndroid::GetInstance()->ReleaseTexture(
        texture_id_in_layer_, current_mailbox_.name);
  } else {
    texture_id_in_layer_ = factory->CreateTexture();
    texture_layer_->SetIsDrawable(true);
  }

  ImageTransportFactoryAndroid::GetInstance()->AcquireTexture(
      texture_id_in_layer_, mailbox.name);

  texture_size_in_layer_ = texture_size;
  content_size_in_layer_ = gfx::Size(content_size.width(),
                                     content_size.height());

  ResetClipping();

  current_mailbox_ = mailbox;

  if (host_->is_hidden())
    ack_callback.Run();
  else
    ack_callbacks_.push(ack_callback);
}

void RenderWidgetHostViewAndroid::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceSuspend() {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceRelease() {
  // This tells us we should free the frontbuffer.
  if (texture_id_in_layer_) {
    texture_layer_->SetTextureId(0);
    texture_layer_->SetIsDrawable(false);
    ImageTransportFactoryAndroid::GetInstance()->DeleteTexture(
        texture_id_in_layer_);
    texture_id_in_layer_ = 0;
    current_mailbox_ = gpu::Mailbox();
  }
}

bool RenderWidgetHostViewAndroid::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  NOTREACHED();
  return false;
}

void RenderWidgetHostViewAndroid::GetScreenInfo(WebKit::WebScreenInfo* result) {
  // ScreenInfo isn't tied to the widget on Android. Always return the default.
  RenderWidgetHostViewBase::GetDefaultScreenInfo(result);
}

// TODO(jrg): Find out the implications and answer correctly here,
// as we are returning the WebView and not root window bounds.
gfx::Rect RenderWidgetHostViewAndroid::GetBoundsInRootWindow() {
  return GetViewBounds();
}

gfx::GLSurfaceHandle RenderWidgetHostViewAndroid::GetCompositingSurface() {
  if (surface_texture_transport_) {
    return surface_texture_transport_->GetCompositingSurface(
        host_->surface_id());
  } else {
  #if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
  	if(!single_compositor)
  		return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::TEXTURE_TRANSPORT);
  	else
  	{
  		#ifdef SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL_MULTINSTANT
  	       if(content_view_core_ &&(content_view_core_->GetSbrContentViewCore()))
  	       {
	  	       #if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL_MULTI_DRAG)
			   gfx::AcceleratedWidget old_window = GpuSurfaceTracker::Get()->GetNativeWidget(host_->surface_id());
			   GpuSurfaceTracker::Get()->SetNativeWidget(host_->surface_id(),content_view_core_->GetSbrContentViewCore()->GetWindow(),NULL);
  			   LOG(INFO)<<" Single_Compositor: Setting for "<<host_->surface_id()<<" to "<<content_view_core_->GetSbrContentViewCore()->GetWindow();
			   if((old_window !=content_view_core_->GetSbrContentViewCore()->GetWindow()))
			   {
			   	SurfaceViewManager::GetInstance()->MigrateActiveWindow(host_->surface_id());
				LOG(INFO)<<" Single_Compositor:Migrating surface "<<host_->surface_id();
			   }
			#else
			GpuSurfaceTracker::Get()->SetNativeWidget(host_->surface_id(),content_view_core_->GetSbrContentViewCore()->GetWindow(),NULL);
			LOG(INFO)<<" Single_Compositor: Setting for "<<host_->surface_id()<<" to "<<content_view_core_->GetSbrContentViewCore()->GetWindow();
			#endif
  	       }
		#else
		GpuSurfaceTracker::Get()->SetNativeWidget(host_->surface_id(),NULL,NULL);
		#endif
    		return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NATIVE_DIRECT);
  	}
  #else
  	return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::TEXTURE_TRANSPORT);
  #endif
  }
}

void RenderWidgetHostViewAndroid::ProcessAckedTouchEvent(
    const WebKit::WebTouchEvent& touch_event, InputEventAckState ack_result) {
  if (content_view_core_)
    content_view_core_->ConfirmTouchEvent(ack_result);
}

void RenderWidgetHostViewAndroid::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  // intentionally empty, like RenderWidgetHostViewViews
}

void RenderWidgetHostViewAndroid::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  // intentionally empty, like RenderWidgetHostViewViews
}

void RenderWidgetHostViewAndroid::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  // intentionally empty, like RenderWidgetHostViewViews
}

InputEventAckState RenderWidgetHostViewAndroid::FilterInputEvent(
    const WebKit::WebInputEvent& input_event) {
  if (!content_view_core_)
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  return content_view_core_->FilterInputEvent(input_event);
}

void RenderWidgetHostViewAndroid::OnAccessibilityNotifications(
    const std::vector<AccessibilityHostMsg_NotificationParams>& params) {
}

bool RenderWidgetHostViewAndroid::LockMouse() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAndroid::UnlockMouse() {
  NOTIMPLEMENTED();
}

// Methods called from the host to the render

void RenderWidgetHostViewAndroid::SendKeyEvent(
    const NativeWebKeyboardEvent& event) {
  if (host_)
    host_->ForwardKeyboardEvent(event);
}

void RenderWidgetHostViewAndroid::SendTouchEvent(
    const WebKit::WebTouchEvent& event) {
  if (host_)
    host_->ForwardTouchEvent(event);
}


void RenderWidgetHostViewAndroid::SendMouseEvent(
    const WebKit::WebMouseEvent& event) {
  if (host_)
    host_->ForwardMouseEvent(event);
}

void RenderWidgetHostViewAndroid::SendMouseWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  if (host_)
    host_->ForwardWheelEvent(event);
}

void RenderWidgetHostViewAndroid::SendGestureEvent(
    const WebKit::WebGestureEvent& event) {
  if (host_)
    host_->ForwardGestureEvent(event);
}

void RenderWidgetHostViewAndroid::SelectRange(const gfx::Point& start,
                                              const gfx::Point& end) {
  if (host_)
    host_->SelectRange(start, end);
}

#if defined (SBROWSER_ENABLE_ECHO_PWD)
void RenderWidgetHostViewAndroid::SetPasswordEcho(bool pwdEchoEnabled) {
  if (host_)
    host_->SetPasswordEcho(pwdEchoEnabled);
}
#endif


#if defined (SBROWSER_SAVEPAGE_IMPL)
void  RenderWidgetHostViewAndroid::GetScrapBitmap(int bitmapWidth, int bitmapHeight){
   if (host_)  
    host_->Send(new ViewMsg_GetScrapBitmap(host_->GetRoutingID(), bitmapWidth, bitmapHeight));
}
void RenderWidgetHostViewAndroid::GetBitmapFromCachedResource(std::string imgUrl){
	if (host_)	
	   host_->Send(new ViewMsg_GetBitmapFromCachedResource(host_->GetRoutingID(), imgUrl));
}
#endif
#if defined (SBROWSER_PRINT_IMPL)
void RenderWidgetHostViewAndroid::PrintBegin() {
	if (host_) 
		host_->Send(new ViewMsg_PrintBegin(host_->GetRoutingID()));
}

void RenderWidgetHostViewAndroid::PrintPage(int pageNum) {
	if (host_)
		host_->Send(new ViewMsg_PrintPage(host_->GetRoutingID(), pageNum));
}

void RenderWidgetHostViewAndroid::PrintEnd() {
	if (host_)
		host_->Send(new ViewMsg_PrintEnd(host_->GetRoutingID()));
}
#endif

#if defined(SBROWSER_FPAUTH_IMPL)
void RenderWidgetHostViewAndroid::FPAuthStatus(bool enabled, std::string usrName) {
	if (host_)
		host_->Send(new ViewMsg_FPAuthStatus(host_->GetRoutingID(),enabled, usrName));
}
#endif

#if defined(SBROWSER_TEXT_SELECTION)
void RenderWidgetHostViewAndroid:: GetSelectionBitmap() {
    if (host_)  
        host_->Send(new ViewMsg_GetSelectionBitmap(host_->GetRoutingID()));
}
void RenderWidgetHostViewAndroid:: selectClosestWord(int x , int y) {
    if (host_)  
        host_->Send(new ViewMsg_SelectClosestWord(host_->GetRoutingID(), x,y));
}
void RenderWidgetHostViewAndroid:: clearTextSelection() {
    if (host_)  
        host_->Send(new ViewMsg_ClearTextSelection(host_->GetRoutingID()));
}
void RenderWidgetHostViewAndroid:: CheckBelongToSelection(int TouchX,  int TouchY) {
   if (host_)  
   	 host_->Send(new ViewMsg_CheckBelongToSelection(host_->GetRoutingID(), TouchX,TouchY));
}

void RenderWidgetHostViewAndroid::RequestSelectionVisibilityStatus(){
	 if (host_)  
   	 host_->Send(new ViewMsg_getSelectionVisiblity(host_->GetRoutingID()));
}
void RenderWidgetHostViewAndroid::GetSelectionRect(){
	 if (host_)  
   	 host_->Send(new ViewMsg_getSelectionrect(host_->GetRoutingID()));
}

void RenderWidgetHostViewAndroid::UnmarkSelection() {
     if (host_)
         host_->Send(new ViewMsg_UnmarkSelection(host_->GetRoutingID()));
}
#endif

#if defined(SBROWSER_SMART_CLIP)
void RenderWidgetHostViewAndroid:: RequestSmartClipData(int x, int y, int w, int h) {
    if (host_)  
        host_->Send(new ViewMsg_RequestSmartClipData(host_->GetRoutingID(), x, y, w, h));
}
#endif
void RenderWidgetHostViewAndroid::MoveCaret(const gfx::Point& point) {
  if (host_)
    host_->MoveCaret(point);
}

// SAMSUNG : Reader >>
#if defined(SBROWSER_READER_NATIVE)
void RenderWidgetHostViewAndroid::RecognizeArticle(int mode) {
    if (host_)
        host_->Send(new ViewMsg_RecognizeArticle(host_->GetRoutingID(), mode));
}

void RenderWidgetHostViewAndroid::OnRecognizeArticleResult(bool isArticle) {
  if (content_view_core_ && content_view_core_->GetSbrContentViewCore()) {
    content_view_core_->GetSbrContentViewCore()->OnRecognizeArticleResult(isArticle);
  }
}
#endif
// SAMSUNG : Reader <<

void RenderWidgetHostViewAndroid::RequestContentClipping(
    const gfx::Rect& clipping,
    const gfx::Size& content_size) {
  // A focused view provides its own clipping.
  if (HasFocus())
    return;

  ClipContents(clipping, content_size);
}

void RenderWidgetHostViewAndroid::ResetClipping() {
  ClipContents(gfx::Rect(gfx::Point(), content_size_in_layer_),
               content_size_in_layer_);
}

void RenderWidgetHostViewAndroid::ClipContents(const gfx::Rect& clipping,
                                               const gfx::Size& content_size) {
  if (!texture_id_in_layer_ || content_size_in_layer_.IsEmpty())
    return;

  gfx::Size clipped_content(content_size_in_layer_);
  clipped_content.ClampToMax(clipping.size());
  texture_layer_->SetBounds(clipped_content);
  texture_layer_->SetNeedsDisplay();

  if (texture_size_in_layer_.IsEmpty()) {
    texture_layer_->SetUV(gfx::PointF(), gfx::PointF());
    return;
  }

  gfx::PointF offset(
      clipping.x() + content_size_in_layer_.width() - content_size.width(),
      clipping.y() + content_size_in_layer_.height() - content_size.height());
  offset.ClampToMin(gfx::PointF());

  gfx::Vector2dF uv_scale(1.f / texture_size_in_layer_.width(),
                          1.f / texture_size_in_layer_.height());
  texture_layer_->SetUV(
      gfx::PointF(offset.x() * uv_scale.x(),
                  offset.y() * uv_scale.y()),
      gfx::PointF((offset.x() + clipped_content.width()) * uv_scale.x(),
                  (offset.y() + clipped_content.height()) * uv_scale.y()));
}

SkColor RenderWidgetHostViewAndroid::GetCachedBackgroundColor() const {
  return cached_background_color_;
}

void RenderWidgetHostViewAndroid::SetContentViewCore(
    ContentViewCoreImpl* content_view_core) {
  RunAckCallbacks();

  if (content_view_core_ && is_layer_attached_)
    content_view_core_->RemoveLayer(layer_);

  content_view_core_ = content_view_core;
  if (content_view_core_ && is_layer_attached_)
    content_view_core_->AttachLayer(layer_);
}

void RenderWidgetHostViewAndroid::RunAckCallbacks() {
  while (!ack_callbacks_.empty()) {
    ack_callbacks_.front().Run();
    ack_callbacks_.pop();
  }
}

void RenderWidgetHostViewAndroid::HasTouchEventHandlers(
    bool need_touch_events) {
  if (content_view_core_)
    content_view_core_->HasTouchEventHandlers(need_touch_events);
}

unsigned RenderWidgetHostViewAndroid::PrepareTexture(
    cc::ResourceUpdateQueue* queue) {
  RunAckCallbacks();
  return texture_id_in_layer_;
}

WebKit::WebGraphicsContext3D* RenderWidgetHostViewAndroid::Context3d() {
  return ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
}

bool RenderWidgetHostViewAndroid::PrepareTextureMailbox(
    cc::TextureMailbox* mailbox) {
  return false;
}

// --------------------------- kikin Modification starts ---------------------------
#if defined (SBROWSER_KIKIN)
void RenderWidgetHostViewAndroid::GetSelectionContext(const bool shouldUpdateSelectionContext)
{
  if (host_)
    host_->GetSelectionContext(shouldUpdateSelectionContext);
}

void RenderWidgetHostViewAndroid::OnSelectionContextExtracted(const std::map<std::string, std::string>& selectionContext)
{
  if (content_view_core_)
    content_view_core_->GetSbrContentViewCore()->OnSelectionContextExtracted(selectionContext);
}

void RenderWidgetHostViewAndroid::UpdateSelection(const std::string& oldSelection, const std::string& newSelection)
{
  if (host_)
    host_->UpdateSelection(oldSelection, newSelection);
}

void RenderWidgetHostViewAndroid::RestoreSelection()
{
  if (host_)
    host_->RestoreSelection();
}

void RenderWidgetHostViewAndroid::ClearSelection()
{
  if (host_)
    host_->ClearSelection();
}

#endif
// ---------------------------  kikin Modification ends  ---------------------------

#if defined(SBROWSER_HIDE_URLBAR)
void RenderWidgetHostViewAndroid::OnHideUrlBarEventNotify(int event, int event_data1, int event_data2) {
#if defined (SBROWSER_SBR_NATIVE_CONTENTVIEW)
   if(content_view_core_){
     content_view_core_->GetSbrContentViewCore()->OnHideUrlBarEventNotify(event, event_data1, event_data2);
   }
#endif
}

void RenderWidgetHostViewAndroid::HideUrlBarCmdReq(int cmd, bool urlbar_active, bool override_allowed, SkBitmap* urlbar_bitmap) {
   if (host_){
        ViewMsg_HideUrlBarCmd_Params params;
	params.cmd_type = cmd;
	params.urlbar_active = urlbar_active;
	params.override_allowed = override_allowed;
	if(urlbar_bitmap){
	   params.urlbar_bitmap = *urlbar_bitmap;
	}
	else{
	  params.urlbar_bitmap = SkBitmap();
	}
	host_->Send(new ViewMsg_HideUrlBarCmd(host_->GetRoutingID(), params));
        if(urlbar_bitmap){
           urlbar_bitmap->reset();
        }
   }
}
#endif

#if defined(SBROWSER_FLING_END_NOTIFICATION_CANE)
void RenderWidgetHostViewAndroid::OnFlingAnimationCompleted(bool page_end){
if(content_view_core_){
	content_view_core_->OnFlingAnimationCompleted(page_end);
}
}
#endif

// static
void RenderWidgetHostViewPort::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  results->rect = display.bounds();
  // TODO(husky): Remove any system controls from availableRect.
  results->availableRect = display.work_area();
  results->deviceScaleFactor = display.device_scale_factor();
  gfx::DeviceDisplayInfo info;
  results->depth = info.GetBitsPerPixel();
  results->depthPerComponent = info.GetBitsPerComponent();
  results->isMonochrome = (results->depthPerComponent == 0);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
RenderWidgetHostView*
RenderWidgetHostView::CreateViewForWidget(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
  return new RenderWidgetHostViewAndroid(rwhi, NULL);
}

#if defined(SBROWSER_KEYLAG)
void RenderWidgetHostViewAndroid::InformJSDeferrState(bool isDeferred)
{
  if (host_) {
    host_->Send(new ViewMsg_DeferrJavaScript(host_->GetRoutingID(), isDeferred));
  }
}
#endif

} // namespace content
