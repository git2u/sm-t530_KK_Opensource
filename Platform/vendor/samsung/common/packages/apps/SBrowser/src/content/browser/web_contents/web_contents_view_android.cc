// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_android.h"

#include "base/logging.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/media_player_manager_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/interstitial_page_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {
WebContentsViewPort* CreateWebContentsView(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  WebContentsViewAndroid* rv = new WebContentsViewAndroid(
      web_contents, delegate);
  *render_view_host_delegate_view = rv;
  return rv;
}

WebContentsViewAndroid::WebContentsViewAndroid(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate)
    : web_contents_(web_contents),
      content_view_core_(NULL),
      delegate_(delegate) {
}

WebContentsViewAndroid::~WebContentsViewAndroid() {
}

void WebContentsViewAndroid::SetContentViewCore(
    ContentViewCoreImpl* content_view_core) {
  content_view_core_ = content_view_core;
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      web_contents_->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->SetContentViewCore(content_view_core_);

  if (web_contents_->ShowingInterstitialPage()) {
    rwhv = static_cast<RenderWidgetHostViewAndroid*>(
        static_cast<InterstitialPageImpl*>(
            web_contents_->GetInterstitialPage())->
                GetRenderViewHost()->GetView());
    if (rwhv)
      rwhv->SetContentViewCore(content_view_core_);
  }
}

#if defined(GOOGLE_TV)
void WebContentsViewAndroid::RequestExternalVideoSurface(int player_id) {
  if (content_view_core_)
    content_view_core_->RequestExternalVideoSurface(player_id);
}

void WebContentsViewAndroid::NotifyGeometryChange(int player_id,
                                                  const gfx::RectF& rect) {
  if (content_view_core_)
    content_view_core_->NotifyGeometryChange(player_id, rect);
}
#endif

#if defined(SBROWSER_TEXT_SELECTION)  
void WebContentsViewAndroid::SelectedBitmap(const SkBitmap& Bitmap)
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
  if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
    content_view_core_->GetSbrContentViewCore()->SelectedBitmap(Bitmap);
#endif	
}

void WebContentsViewAndroid::PointOnRegion(bool isOnRegion)
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
  if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
    content_view_core_->GetSbrContentViewCore()->PointOnRegion(isOnRegion);
#endif
}

void WebContentsViewAndroid::SetSelectionVisibilty(bool isVisible)
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
  if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
    content_view_core_->GetSbrContentViewCore()->SetSelectionVisibilty(isVisible);
#endif
}
void WebContentsViewAndroid::updateSelectionRect(const gfx::Rect& selectionRect)
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
  if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
    content_view_core_->GetSbrContentViewCore()->UpdateCurrentSelectionRect(selectionRect);
#endif
}
#endif
#if defined(SBROWSER_FP_SUPPORT)
void WebContentsViewAndroid::NotifyAutofillBegin()
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
	if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
		content_view_core_->GetSbrContentViewCore()->NotifyAutofillBegin();
#endif
}
#endif
#if defined (SBROWSER_SCROLL_INPUTTEXT)
void WebContentsViewAndroid::OnTextFieldBoundsChanged(const gfx::Rect&  input_edit_rect,bool isScrollEnable) {
#if defined (SBROWSER_SBRNATIVE_IMPL)
  if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
    content_view_core_->GetSbrContentViewCore()->OnTextFieldBoundsChanged(input_edit_rect, isScrollEnable);
#endif
}
#endif

#if defined (SBROWSER_HANDLE_MOUSECLICK_CTRL)
void WebContentsViewAndroid::OnOpenUrlInNewTab(const string16& url) {
#if defined (SBROWSER_SBRNATIVE_IMPL)
  if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
    content_view_core_->GetSbrContentViewCore()->OnOpenUrlInNewTab(url);
#endif
}
#endif

#if defined(SBROWSER_TEXT_SELECTION) && defined(SBROWSER_HTML_DRAG_N_DROP)
void WebContentsViewAndroid::SelectedMarkup(const string16& markup)
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
  if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
    content_view_core_->GetSbrContentViewCore()->SelectedMarkup(markup);
#endif
}
#endif
// SAMSUNG : SmartClip >>
#if defined(SBROWSER_SMART_CLIP)  
void WebContentsViewAndroid::OnSmartClipDataExtracted(const string16& result)
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
    if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
        content_view_core_->GetSbrContentViewCore()->OnSmartClipDataExtracted(result);
#endif
}
#endif
// SAMSUNG : SmartClip <<
gfx::NativeView WebContentsViewAndroid::GetNativeView() const {
  return content_view_core_ ? content_view_core_->GetViewAndroid() : NULL;
}

gfx::NativeView WebContentsViewAndroid::GetContentNativeView() const {
  return content_view_core_ ? content_view_core_->GetViewAndroid() : NULL;
}

gfx::NativeWindow WebContentsViewAndroid::GetTopLevelNativeWindow() const {
  return content_view_core_ ? content_view_core_->GetWindowAndroid() : NULL;
}

void WebContentsViewAndroid::GetContainerBounds(gfx::Rect* out) const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    *out = rwhv->GetViewBounds();
}

void WebContentsViewAndroid::SetPageTitle(const string16& title) {
  if (content_view_core_)
    content_view_core_->SetTitle(title);
}

void WebContentsViewAndroid::OnTabCrashed(base::TerminationStatus status,
                                          int error_code) {
LOG(INFO)  << "TouchLockup WebContentsViewAndroid::OnTabCrashed";                                  
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      web_contents_->GetRenderViewHost());
  if (rvh->media_player_manager())
    rvh->media_player_manager()->DestroyAllMediaPlayers();
  if (content_view_core_)
    content_view_core_->OnTabCrashed();
}

void WebContentsViewAndroid::SizeContents(const gfx::Size& size) {
  // TODO(klobag): Do we need to do anything else?
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void WebContentsViewAndroid::Focus() {
  if (web_contents_->ShowingInterstitialPage())
    web_contents_->GetInterstitialPage()->Focus();
  else
    web_contents_->GetRenderWidgetHostView()->Focus();
}

void WebContentsViewAndroid::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar(false);
  else
    Focus();
}

void WebContentsViewAndroid::StoreFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::RestoreFocus() {
  NOTIMPLEMENTED();
}

WebDropData* WebContentsViewAndroid::GetDropData() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::Rect WebContentsViewAndroid::GetViewBounds() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    return rwhv->GetViewBounds();
  else
    return gfx::Rect();
}

void WebContentsViewAndroid::CreateView(
    const gfx::Size& initial_size, gfx::NativeView context) {
}

RenderWidgetHostView* WebContentsViewAndroid::CreateViewForWidget(
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
  // Note that while this instructs the render widget host to reference
  // |native_view_|, this has no effect without also instructing the
  // native view (i.e. ContentView) how to obtain a reference to this widget in
  // order to paint it. See ContentView::GetRenderWidgetHostViewAndroid for an
  // example of how this is achieved for InterstitialPages.
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(render_widget_host);
  RenderWidgetHostView* view = new RenderWidgetHostViewAndroid(
      rwhi, content_view_core_);
  return view;
}

RenderWidgetHostView* WebContentsViewAndroid::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return RenderWidgetHostViewPort::CreateViewForWidget(render_widget_host);
}

void WebContentsViewAndroid::RenderViewCreated(RenderViewHost* host) {
}

void WebContentsViewAndroid::RenderViewSwappedIn(RenderViewHost* host) {
}

void WebContentsViewAndroid::SetOverscrollControllerEnabled(bool enabled) {
}

void WebContentsViewAndroid::ShowContextMenu(
    const ContextMenuParams& params,
    ContextMenuSourceType type) {
  if (delegate_)
    delegate_->ShowContextMenu(params, type);
#if defined(SBROWSER_CONTEXT_MENU_FLAG) && defined(SBROWSER_SBR_NATIVE_CONTENTVIEW)
  if (content_view_core_ && content_view_core_->GetSbrContentViewCore()){
     LOG(INFO)<<"WebContentsViewAndroid::ShowContextMenu";
     content_view_core_->GetSbrContentViewCore()->ShowContextMenu(params);
                }
#endif	
}

void WebContentsViewAndroid::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<WebMenuItem>& items,
    bool right_aligned,
    bool allow_multiple_selection
     #if defined (SBROWSER_SUPPORT) && defined(SBROWSER_FORM_NAVIGATION)
    , int privateimeoptions)
    #else
	)
    #endif 
	 {
	 #if defined (SBROWSER_SUPPORT) && defined(SBROWSER_FORM_NAVIGATION) 
		 if (content_view_core_->GetSbrContentViewCore()) {   // done changes contentview_core
  	
    content_view_core_->GetSbrContentViewCore()->ShowSelectPopupMenu(
        items, selected_item, allow_multiple_selection,privateimeoptions);
  		}
	#else
	  if (content_view_core_) {
	    content_view_core_->ShowSelectPopupMenu(
	        items, selected_item, allow_multiple_selection);
	  }
    #endif
}

void WebContentsViewAndroid::StartDragging(
    const WebDropData& drop_data,
    WebKit::WebDragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info) {
  NOTIMPLEMENTED();
}

#if defined(SBROWSER_IMAGEDRAG_IMPL)
void WebContentsViewAndroid::SetBitmapForDragging(const SkBitmap& bitmap,int width, int height){
LOG(INFO)<<"SCRAP ::WebContentsViewAndroid::SetBitmapForDragging";
#if defined (SBROWSER_SBRNATIVE_IMPL)
	if (content_view_core_ && content_view_core_->GetSbrContentViewCore()){
    		content_view_core_->GetSbrContentViewCore()->SetBitmapForDragging(bitmap,width,height);
		}	
#endif	
}
#endif
void WebContentsViewAndroid::UpdateDragCursor(WebKit::WebDragOperation op) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::GotFocus() {
  // This is only used in the views FocusManager stuff but it bleeds through
  // all subclasses. http://crbug.com/21875
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void WebContentsViewAndroid::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate() &&
      web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse))
    return;
  web_contents_->GetRenderWidgetHostView()->Focus();
}

#if defined (SBROWSER_SAVEPAGE_IMPL)
void WebContentsViewAndroid::OnBitmapForScrap(const SkBitmap& bitmap, bool response)
{
LOG(INFO)<<"SCRAP ::WebContentsViewAndroid::OnBitmapForScrap";
#if defined (SBROWSER_SBRNATIVE_IMPL)
	if (content_view_core_&& content_view_core_->GetSbrContentViewCore())
		content_view_core_->GetSbrContentViewCore()->OnBitmapForScrap(bitmap, response); 
#endif	
}
void WebContentsViewAndroid::ScrapPageSavedFileName ( const base::FilePath::StringType& pure_file_name)
{
LOG(INFO)<<"SCRAP ::WebContentsViewAndroid::ScrapPageSavedFileName :: pure_file_name :: "<<pure_file_name;
#if defined (SBROWSER_SBRNATIVE_IMPL)
	if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
		content_view_core_->GetSbrContentViewCore()->ScrapPageSavedFileName(pure_file_name); 
#endif	
}
#endif
#if defined (SBROWSER_SOFTBITMAP_IMPL)
void WebContentsViewAndroid::UpdateSoftBitmap(const SkBitmap& bitmap)
{
      #if defined (SBROWSER_SBRNATIVE_IMPL)
	if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
		content_view_core_->GetSbrContentViewCore()->UpdateSoftBitmap(bitmap);
      #endif
}

void WebContentsViewAndroid::UpdateReCaptureSoftBitmap(const SkBitmap& bitmap, bool fromPageLoadFinish)
{
      #if defined (SBROWSER_SBRNATIVE_IMPL)
	if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
		content_view_core_->GetSbrContentViewCore()->UpdateReCaptureSoftBitmap(bitmap, fromPageLoadFinish);
      #endif
}
#endif

#if defined (SBROWSER_PRINT_IMPL)
void WebContentsViewAndroid::OnBitmapForPrint(const SkBitmap& bitmap, int responseCode)
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
	if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
		content_view_core_->GetSbrContentViewCore()->OnBitmapForPrint(bitmap, responseCode);
#endif		
}

void WebContentsViewAndroid::OnPrintBegin(int totalPageCount)
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
	if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
		content_view_core_->GetSbrContentViewCore()->OnPrintBegin(totalPageCount);
#endif
}

#endif
#if defined (SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT)
void WebContentsViewAndroid::SetBingSearchEngineAsDefault(bool enable) {
      
      if (content_view_core_ && content_view_core_->GetSbrContentViewCore()){
 	   content_view_core_->GetSbrContentViewCore()->SetBingSearchEngineAsDefault(enable);
      }	
	
}
#endif

#if defined(SBROWSER_FPAUTH_IMPL)
bool WebContentsViewAndroid::OnCheckFPAuth()
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
	if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
		return content_view_core_->GetSbrContentViewCore()->OnCheckFPAuth();
	return false;
#endif	
}

void WebContentsViewAndroid::OnLaunchFingerPrintActivity()
{
#if defined (SBROWSER_SBRNATIVE_IMPL)
	if (content_view_core_ && content_view_core_->GetSbrContentViewCore())
		content_view_core_->GetSbrContentViewCore()->OnLaunchFingerPrintActivity();
#endif	
}



#endif

} // namespace content
