// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/port/browser/web_contents_view_port.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/context_menu_params.h"
#include "ui/gfx/rect_f.h"

namespace content {
class ContentViewCoreImpl;

// Android-specific implementation of the WebContentsView.
class WebContentsViewAndroid : public WebContentsViewPort,
                               public RenderViewHostDelegateView {
 public:
  WebContentsViewAndroid(WebContentsImpl* web_contents,
                         WebContentsViewDelegate* delegate);
  virtual ~WebContentsViewAndroid();

  // Sets the interface to the view system. ContentViewCoreImpl is owned
  // by its Java ContentViewCore counterpart, whose lifetime is managed
  // by the UI frontend.
  void SetContentViewCore(ContentViewCoreImpl* content_view_core);

#if defined(GOOGLE_TV)
  void RequestExternalVideoSurface(int player_id);
  void NotifyGeometryChange(int player_id, const gfx::RectF& rect);
#endif

  // WebContentsView implementation --------------------------------------------
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual WebDropData* GetDropData() const OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
#if defined(SBROWSER_TEXT_SELECTION)   
  virtual void SelectedBitmap(const SkBitmap& Bitmap)OVERRIDE;
  virtual void PointOnRegion(bool isOnRegion)OVERRIDE;
  virtual void SetSelectionVisibilty(bool isVisible)OVERRIDE;
  virtual void updateSelectionRect(const gfx::Rect& selectionRect)OVERRIDE;
#endif 
#if defined(SBROWSER_FP_SUPPORT)
  virtual void NotifyAutofillBegin()OVERRIDE;
#endif
#if defined (SBROWSER_SCROLL_INPUTTEXT)
virtual void OnTextFieldBoundsChanged(const gfx::Rect&  input_edit_rect, bool isScrollEnable)OVERRIDE;
#endif
#if defined (SBROWSER_HANDLE_MOUSECLICK_CTRL)
virtual void OnOpenUrlInNewTab(const string16& url)OVERRIDE;
#endif

#if defined(SBROWSER_TEXT_SELECTION) && defined(SBROWSER_HTML_DRAG_N_DROP)
  virtual void SelectedMarkup(const string16& markup) OVERRIDE; 
#endif
// SAMSUNG : SmartClip >>
#if defined(SBROWSER_SMART_CLIP)  
  virtual void OnSmartClipDataExtracted(const string16& result) OVERRIDE;
#endif
// SAMSUNG : SmartClip <<
  // WebContentsViewPort implementation ----------------------------------------
  virtual void CreateView(
      const gfx::Size& initial_size, gfx::NativeView context) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* host) OVERRIDE;
  virtual void RenderViewSwappedIn(RenderViewHost* host) OVERRIDE;
  virtual void SetOverscrollControllerEnabled(bool enabled) OVERRIDE;

  // Backend implementation of RenderViewHostDelegateView.
  virtual void ShowContextMenu(const ContextMenuParams& params,
                               ContextMenuSourceType type) OVERRIDE;
 #if defined (SBROWSER_SUPPORT) && defined(SBROWSER_FORM_NAVIGATION)
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection, int privateimeoptions=0) OVERRIDE;
  #else
    virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection) OVERRIDE;
 #endif
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops,
                             const gfx::ImageSkia& image,
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info) OVERRIDE;
#if defined(SBROWSER_IMAGEDRAG_IMPL)
  virtual void SetBitmapForDragging(const SkBitmap& bitmap,int width, int height) OVERRIDE;
#endif
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;
#if defined (SBROWSER_SAVEPAGE_IMPL)
  virtual void OnBitmapForScrap(const SkBitmap& bitmap, bool response);
  virtual void ScrapPageSavedFileName ( const base::FilePath::StringType& pure_file_name);
#endif
#if defined (SBROWSER_SOFTBITMAP_IMPL)
  virtual void UpdateSoftBitmap(const SkBitmap& bitmap);
  virtual void UpdateReCaptureSoftBitmap(const SkBitmap& bitmap, bool fromPageLoadFinish);
#endif
#if defined (SBROWSER_PRINT_IMPL)
  virtual void OnBitmapForPrint(const SkBitmap& bitmap, int responseCode);
  virtual void OnPrintBegin(int totalPageCount);
#endif
#if defined (SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT)
virtual void SetBingSearchEngineAsDefault(bool enable) ;
#endif

#if defined(SBROWSER_FPAUTH_IMPL)
 virtual bool OnCheckFPAuth();
virtual void OnLaunchFingerPrintActivity();
#endif

 private:
  // The WebContents whose contents we display.
  WebContentsImpl* web_contents_;

  // ContentViewCoreImpl is our interface to the view system.
  ContentViewCoreImpl* content_view_core_;

  // Interface for extensions to WebContentsView. Used to show the context menu.
  scoped_ptr<WebContentsViewDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAndroid);
};

} // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_
