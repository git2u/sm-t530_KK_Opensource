// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_guest.h"

#include "build/build_config.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_guest.h"
#include "content/browser/web_contents/interstitial_page_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/drag_messages.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webdropdata.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;

namespace content {

WebContentsViewGuest::WebContentsViewGuest(
    WebContentsImpl* web_contents,
    BrowserPluginGuest* guest,
    WebContentsViewPort* platform_view)
    : web_contents_(web_contents),
      guest_(guest),
      platform_view_(platform_view) {
}

WebContentsViewGuest::~WebContentsViewGuest() {
}

gfx::NativeView WebContentsViewGuest::GetNativeView() const {
  return platform_view_->GetNativeView();
}

gfx::NativeView WebContentsViewGuest::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return NULL;
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewGuest::GetTopLevelNativeWindow() const {
  return guest_->embedder_web_contents()->GetView()->GetTopLevelNativeWindow();
}

void WebContentsViewGuest::GetContainerBounds(gfx::Rect* out) const {
  out->SetRect(0, 0, size_.width(), size_.height());
}

void WebContentsViewGuest::SizeContents(const gfx::Size& size) {
  size_ = size;
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void WebContentsViewGuest::SetInitialFocus() {
  platform_view_->SetInitialFocus();
}

gfx::Rect WebContentsViewGuest::GetViewBounds() const {
  return gfx::Rect(size_);
}

#if defined(OS_MACOSX)
void WebContentsViewGuest::SetAllowOverlappingViews(bool overlapping) {
  platform_view_->SetAllowOverlappingViews(overlapping);
}
#endif

void WebContentsViewGuest::CreateView(const gfx::Size& initial_size,
                                      gfx::NativeView context) {
  platform_view_->CreateView(initial_size, context);
  size_ = initial_size;
}
#if defined(SBROWSER_TEXT_SELECTION)   
void WebContentsViewGuest::SelectedBitmap(const SkBitmap& Bitmap)
{

}
void WebContentsViewGuest::PointOnRegion(bool isOnRegion)
{

}
void WebContentsViewGuest::SetSelectionVisibilty(bool isVisible){}
void WebContentsViewGuest::updateSelectionRect(const gfx::Rect& selectionRect){}
#endif  

#if defined(SBROWSER_FP_SUPPORT)
void WebContentsViewGuest::NotifyAutofillBegin()
{
}
#endif

#if defined (SBROWSER_SCROLL_INPUTTEXT)
void WebContentsViewGuest::OnTextFieldBoundsChanged(const gfx::Rect&  input_edit_rect,bool isScrollEnable) {
}
#endif

#if defined (SBROWSER_HANDLE_MOUSECLICK_CTRL)
void WebContentsViewGuest::OnOpenUrlInNewTab(const string16& url) {
}
#endif

#if defined(SBROWSER_TEXT_SELECTION) && defined(SBROWSER_HTML_DRAG_N_DROP)
void WebContentsViewGuest::SelectedMarkup(const string16& markup)
{

}
#endif
// SAMSUNG : SmartClip >>
#if defined(SBROWSER_SMART_CLIP)  
void WebContentsViewGuest::OnSmartClipDataExtracted(const string16& result) {}
#endif
// SAMSUNG : SmartClip <<
#if defined (SBROWSER_SAVEPAGE_IMPL)
void WebContentsViewGuest::OnBitmapForScrap(const SkBitmap& bitmap, bool response){}
void WebContentsViewGuest::ScrapPageSavedFileName(const base::FilePath::StringType& pure_file_name){}
void WebContentsViewGuest::OnReceiveBitmapFromCache(const SkBitmap& bitmap){}
#endif

#if defined (SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT)
void WebContentsViewGuest::SetBingSearchEngineAsDefault(bool enable) {}
#endif

RenderWidgetHostView* WebContentsViewGuest::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->GetView();
  }

  RenderWidgetHostView* platform_widget = NULL;
  platform_widget = platform_view_->CreateViewForWidget(render_widget_host);

  RenderWidgetHostView* view = new RenderWidgetHostViewGuest(
      render_widget_host,
      guest_,
      platform_widget);

  return view;
}

RenderWidgetHostView* WebContentsViewGuest::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return RenderWidgetHostViewPort::CreateViewForWidget(render_widget_host);
}

void WebContentsViewGuest::SetPageTitle(const string16& title) {
}

void WebContentsViewGuest::RenderViewCreated(RenderViewHost* host) {
  platform_view_->RenderViewCreated(host);
}

void WebContentsViewGuest::RenderViewSwappedIn(RenderViewHost* host) {
  platform_view_->RenderViewSwappedIn(host);
}

void WebContentsViewGuest::SetOverscrollControllerEnabled(bool enabled) {
  // This should never override the setting of the embedder view.
}

#if defined(OS_MACOSX)
bool WebContentsViewGuest::IsEventTracking() const {
  return false;
}

void WebContentsViewGuest::CloseTabAfterEventTracking() {
}
#endif

WebContents* WebContentsViewGuest::web_contents() {
  return web_contents_;
}

void WebContentsViewGuest::RestoreFocus() {
  platform_view_->RestoreFocus();
}

void WebContentsViewGuest::OnTabCrashed(base::TerminationStatus status,
                                        int error_code) {
}

void WebContentsViewGuest::Focus() {
  platform_view_->Focus();
}

void WebContentsViewGuest::StoreFocus() {
  platform_view_->StoreFocus();
}

WebDropData* WebContentsViewGuest::GetDropData() const {
  NOTIMPLEMENTED();
  return NULL;
}

void WebContentsViewGuest::UpdateDragCursor(WebDragOperation operation) {
  RenderViewHostImpl* embedder_render_view_host =
      static_cast<RenderViewHostImpl*>(
          guest_->embedder_web_contents()->GetRenderViewHost());
  CHECK(embedder_render_view_host);
  RenderViewHostDelegateView* view =
      embedder_render_view_host->GetDelegate()->GetDelegateView();
  if (view)
    view->UpdateDragCursor(operation);
}

void WebContentsViewGuest::GotFocus() {
}

void WebContentsViewGuest::TakeFocus(bool reverse) {
}

void WebContentsViewGuest::ShowContextMenu(
    const ContextMenuParams& params,
    ContextMenuSourceType type) {
  NOTIMPLEMENTED();
}

#if defined (SBROWSER_SUPPORT) && defined(SBROWSER_FORM_NAVIGATION)
void WebContentsViewGuest::ShowPopupMenu(const gfx::Rect& bounds,
                                         int item_height,
                                         double item_font_size,
                                         int selected_item,
                                         const std::vector<WebMenuItem>& items,
                                         bool right_aligned,
                                         bool allow_multiple_selection,
                                         int privateimeoptions)
 #else
 void WebContentsViewGuest::ShowPopupMenu(const gfx::Rect& bounds,
                                         int item_height,
                                         double item_font_size,
                                         int selected_item,
                                         const std::vector<WebMenuItem>& items,
                                         bool right_aligned,
                                         bool allow_multiple_selection)
 #endif
 {
  // External popup menus are only used on Mac and Android.
  NOTIMPLEMENTED();
}

void WebContentsViewGuest::StartDragging(
    const WebDropData& drop_data,
    WebDragOperationsMask ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info) {
  WebContentsImpl* embedder_web_contents = guest_->embedder_web_contents();
  embedder_web_contents->GetBrowserPluginEmbedder()->StartDrag(guest_);
  RenderViewHostImpl* embedder_render_view_host =
      static_cast<RenderViewHostImpl*>(
          embedder_web_contents->GetRenderViewHost());
  CHECK(embedder_render_view_host);
  RenderViewHostDelegateView* view =
      embedder_render_view_host->GetDelegate()->GetDelegateView();
  if (view)
    view->StartDragging(drop_data, ops, image, image_offset, event_info);
  else
    embedder_web_contents->SystemDragEnded();
}

}  // namespace content
