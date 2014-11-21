// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PORT_BROWSER_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
#define CONTENT_PORT_BROWSER_RENDER_VIEW_HOST_DELEGATE_VIEW_H_

#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"
#include "content/public/common/context_menu_source_type.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#if defined (SBROWSER_SAVEPAGE_IMPL)
#include "base/files/file_path.h"
#endif

class SkBitmap;
struct WebDropData;
struct WebMenuItem;

namespace gfx {
class ImageSkia;
class Rect;
class Vector2d;
}

namespace content {

struct ContextMenuParams;

// This class provides a way for the RenderViewHost to reach out to its
// delegate's view. It only needs to be implemented by embedders if they don't
// use the default WebContentsView implementations.
class CONTENT_EXPORT RenderViewHostDelegateView {
 public:
  // A context menu should be shown, to be built using the context information
  // provided in the supplied params.
  virtual void ShowContextMenu(const ContextMenuParams& params,
                               ContextMenuSourceType type) {}

  // Shows a popup menu with the specified items.
  // This method should call RenderViewHost::DidSelectPopupMenuItem[s]() or
  // RenderViewHost::DidCancelPopupMenu() based on the user action.
    #if defined(SBROWSER_FORM_NAVIGATION)
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection,
                             int privateimeoptions=0) = 0;
  #else
    virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection) = 0;
  #endif

  // The user started dragging content of the specified type within the
  // RenderView. Contextual information about the dragged content is supplied
  // by WebDropData. If the delegate's view cannot start the drag for /any/
  // reason, it must inform the renderer that the drag has ended; otherwise,
  // this results in bugs like http://crbug.com/157134.
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops,
                             const gfx::ImageSkia& image,
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info) {}

#if defined(SBROWSER_IMAGEDRAG_IMPL)
	virtual void SetBitmapForDragging(const SkBitmap& bitmap,int width, int height){}
#endif
#if defined (SBROWSER_SCROLL_INPUTTEXT)
   virtual void OnTextFieldBoundsChanged(const gfx::Rect&  input_edit_rect, bool isScrollEnable) = 0;
#endif
#if defined (SBROWSER_HANDLE_MOUSECLICK_CTRL)
   virtual void OnOpenUrlInNewTab(const string16& url) = 0;
#endif
  // The page wants to update the mouse cursor during a drag & drop operation.
  // |operation| describes the current operation (none, move, copy, link.)
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) {}

  // Notification that view for this delegate got the focus.
  virtual void GotFocus() {}

  // Callback to inform the browser that the page is returning the focus to
  // the browser's chrome. If reverse is true, it means the focus was
  // retrieved by doing a Shift-Tab.
  virtual void TakeFocus(bool reverse) {}

#if defined(SBROWSER_TEXT_SELECTION)
  virtual void SelectedBitmap(const SkBitmap& Bitmap) {};
  virtual void PointOnRegion(bool isOnRegion) {};
  virtual void SetSelectionVisibilty(bool isVisible) {};
  virtual void updateSelectionRect(const gfx::Rect& selectionRect){};
#endif

#if defined(SBROWSER_FP_SUPPORT)
  virtual void NotifyAutofillBegin(){};
#endif

#if defined(SBROWSER_TEXT_SELECTION) && defined(SBROWSER_HTML_DRAG_N_DROP)
  virtual void SelectedMarkup(const string16& markup) {};
#endif

// SAMSUNG : SmartClip >>
#if defined(SBROWSER_SMART_CLIP)  
  virtual void OnSmartClipDataExtracted(const string16& result) {};
#endif
// SAMSUNG : SmartClip <<
#if defined (SBROWSER_SAVEPAGE_IMPL)
  virtual void OnBitmapForScrap(const SkBitmap& bitmap, bool response){} ;
  virtual  void ScrapPageSavedFileName(const base::FilePath::StringType& pure_file_name){} ;
  virtual void OnReceiveBitmapFromCache(const SkBitmap& bitmap){};
#endif
#if defined (SBROWSER_SOFTBITMAP_IMPL)
virtual void UpdateSoftBitmap(const SkBitmap& bitmap) {};
virtual void UpdateReCaptureSoftBitmap(const SkBitmap& bitmap, bool fromPageLoadFinish) {};
#endif

#if defined (SBROWSER_PRINT_IMPL)
  virtual void OnPrintBegin(int totalPageCount ) {};
  virtual void OnBitmapForPrint(const SkBitmap& bitmap, int responseCode ) {};
#endif
 #if defined (SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT)
  virtual void SetBingSearchEngineAsDefault(bool enable) {};
 #endif
 
#if defined(SBROWSER_FPAUTH_IMPL)
 virtual bool OnCheckFPAuth(){return false;};
 virtual void OnLaunchFingerPrintActivity(){};
#endif

 protected:
  virtual ~RenderViewHostDelegateView() {}
};

}  // namespace content

#endif  // CONTENT_PORT_BROWSER_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
