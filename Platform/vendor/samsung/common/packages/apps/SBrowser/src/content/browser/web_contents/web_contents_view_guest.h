// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/port/browser/web_contents_view_port.h"

namespace content {

class WebContents;
class WebContentsImpl;
class BrowserPluginGuest;

class CONTENT_EXPORT WebContentsViewGuest
    : public WebContentsViewPort,
      public RenderViewHostDelegateView {
 public:
  // The corresponding WebContentsImpl is passed in the constructor, and manages
  // our lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  // WebContentsViewGuest always has a backing platform dependent view,
  // |platform_view|.
  WebContentsViewGuest(WebContentsImpl* web_contents,
                       BrowserPluginGuest* guest,
                       WebContentsViewPort* platform_view);
  virtual ~WebContentsViewGuest();

  WebContents* web_contents();

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
#if defined (SBROWSER_SAVEPAGE_IMPL)
  void OnBitmapForScrap(const SkBitmap& bitmap, bool response)OVERRIDE;
  void ScrapPageSavedFileName ( const base::FilePath::StringType& pure_file_name)OVERRIDE;
  void OnReceiveBitmapFromCache(const SkBitmap& bitmap)OVERRIDE;
#endif
#if defined (SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT)
void SetBingSearchEngineAsDefault(bool enable) OVERRIDE;
#endif
#if defined(OS_MACOSX)
  virtual void SetAllowOverlappingViews(bool overlapping) OVERRIDE;
#endif
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
  virtual void OnTextFieldBoundsChanged(const gfx::Rect&  input_edit_rect,bool isScrollEnable)OVERRIDE;
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
  virtual void CreateView(const gfx::Size& initial_size,
                          gfx::NativeView context) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* host) OVERRIDE;
  virtual void RenderViewSwappedIn(RenderViewHost* host) OVERRIDE;
  virtual void SetOverscrollControllerEnabled(bool enabled) OVERRIDE;
#if defined(OS_MACOSX)
  virtual bool IsEventTracking() const OVERRIDE;
  virtual void CloseTabAfterEventTracking() OVERRIDE;
#endif

  // Backend implementation of RenderViewHostDelegateView.
  virtual void ShowContextMenu(
      const ContextMenuParams& params,
      ContextMenuSourceType type) OVERRIDE;
  #if defined (SBROWSER_SUPPORT) && defined(SBROWSER_FORM_NAVIGATION)
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection,
                             int privateimeoptions=0) OVERRIDE; 
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
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;

 private:
  // The WebContentsImpl whose contents we display.
  WebContentsImpl* web_contents_;
  BrowserPluginGuest* guest_;
  // The platform dependent view backing this WebContentsView.
  // Calls to this WebContentsViewGuest are forwarded to |platform_view_|.
  WebContentsViewPort* platform_view_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_
