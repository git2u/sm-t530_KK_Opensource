// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sender.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNavigationPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageVisibilityState.h"
#include "ui/gfx/native_widget_types.h"
#if defined(SBROWSER_FPAUTH_IMPL)
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#endif
class SkPicture;
struct WebPreferences;

namespace WebKit {
class WebFrame;
class WebNode;
class WebPlugin;
class WebString;
class WebURLRequest;
class WebView;
struct WebContextMenuData;
struct WebPluginParams;
}

namespace gfx {
class Size;
}

namespace webkit {
struct WebPluginInfo;
}

namespace content {

class ContextMenuClient;
class RenderViewVisitor;
struct ContextMenuParams;
struct SSLStatus;

class CONTENT_EXPORT RenderView : public IPC::Sender {
 public:
  // Returns the RenderView containing the given WebView.
  static RenderView* FromWebView(WebKit::WebView* webview);

  // Returns the RenderView for the given routing ID.
  static RenderView* FromRoutingID(int routing_id);

  // Visit all RenderViews with a live WebView (i.e., RenderViews that have
  // been closed but not yet destroyed are excluded).
  static void ForEach(RenderViewVisitor* visitor);

  // Get the routing ID of the view.
  virtual int GetRoutingID() const = 0;

  // Page IDs allow the browser to identify pages in each renderer process for
  // keeping back/forward history in sync.
  // Note that this is NOT updated for every main frame navigation, only for
  // "regular" navigations that go into session history. In particular, client
  // redirects, like the page cycler uses (document.location.href="foo") do not
  // count as regular navigations and do not increment the page id.
  virtual int GetPageId() const = 0;

  // Returns the size of the view.
  virtual gfx::Size GetSize() const = 0;

  // Gets WebKit related preferences associated with this view.
  virtual WebPreferences& GetWebkitPreferences() = 0;

  // Overrides the WebKit related preferences associated with this view. Note
  // that the browser process may update the preferences at any time.
  virtual void SetWebkitPreferences(const WebPreferences& preferences) = 0;

  // Returns the associated WebView. May return NULL when the view is closing.
  virtual WebKit::WebView* GetWebView() = 0;

#if defined(SBROWSER_FP_SUPPORT)
  virtual void OnNotifyAutofillBeginMain() = 0;
#endif

#if defined(SBROWSER_FPAUTH_IMPL)
  virtual void SubmitForm(WebKit::WebInputElement& pwd)=0;
#endif

  // Gets the focused node. If no such node exists then the node will be isNull.
  virtual WebKit::WebNode GetFocusedNode() const = 0;

  // Gets the node that the context menu was pressed over.
  virtual WebKit::WebNode GetContextMenuNode() const = 0;

  // Returns true if the parameter node is a textfield, text area, a content
  // editable div, or has an ARIA role of textbox.
  virtual bool IsEditableNode(const WebKit::WebNode& node) const = 0;

  // Create a new NPAPI/Pepper plugin depending on |info|. Returns NULL if no
  // plugin was found.
  virtual WebKit::WebPlugin* CreatePlugin(
      WebKit::WebFrame* frame,
      const webkit::WebPluginInfo& info,
      const WebKit::WebPluginParams& params) = 0;

  // Evaluates a string of JavaScript in a particular frame.
  virtual void EvaluateScript(const string16& frame_xpath,
                              const string16& jscript,
                              int id,
                              bool notify_result) = 0;

  // Returns true if we should display scrollbars for the given view size and
  // false if the scrollbars should be hidden.
  virtual bool ShouldDisplayScrollbars(int width, int height) const = 0;

  // Bitwise-ORed set of extra bindings that have been enabled.  See
  // BindingsPolicy for details.
  virtual int GetEnabledBindings() const = 0;

  // Whether content state (such as form state, scroll position and page
  // contents) should be sent to the browser immediately. This is normally
  // false, but set to true by some tests.
  virtual bool GetContentStateImmediately() const = 0;

  // Filtered time per frame based on UpdateRect messages.
  virtual float GetFilteredTimePerFrame() const = 0;

  // Shows a context menu with the given information. The given client will
  // be called with the result.
  //
  // The request ID will be returned by this function. This is passed to the
  // client functions for identification.
  //
  // If the client is destroyed, CancelContextMenu() should be called with the
  // request ID returned by this function.
  //
  // Note: if you end up having clients outliving the RenderView, we should add
  // a CancelContextMenuCallback function that takes a request id.
  virtual int ShowContextMenu(ContextMenuClient* client,
                              const ContextMenuParams& params) = 0;

  // Cancels a context menu in the event that the client is destroyed before the
  // menu is closed.
  virtual void CancelContextMenu(int request_id) = 0;

  // Returns the current visibility of the WebView.
  virtual WebKit::WebPageVisibilityState GetVisibilityState() const = 0;

  // Displays a modal alert dialog containing the given message.  Returns
  // once the user dismisses the dialog.
  virtual void RunModalAlertDialog(WebKit::WebFrame* frame,
                                   const WebKit::WebString& message) = 0;

  // The client should handle the navigation externally.
  virtual void LoadURLExternally(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationPolicy policy) = 0;

  // Notifies the renderer that a paint is to be generated for the size
  // passed in.
  virtual void Repaint(const gfx::Size& size) = 0;

  // Inject edit commands to be used for the next keyboard event.
  virtual void SetEditCommandForNextKeyEvent(const std::string& name,
                                             const std::string& value) = 0;
  virtual void ClearEditCommands() = 0;

  // Returns a collection of security info about |frame|.
  virtual SSLStatus GetSSLStatusOfFrame(WebKit::WebFrame* frame) const = 0;

#if defined(OS_ANDROID)
  // Returns a SkPicture with the full contents of the current frame as part of
  // the legacy Android WebView capture picture API. As it involves playing back
  // all the drawing commands of the current frame it can have an important
  // performance impact and should not be used for other purposes.
  // Requires enabling the impl-side painting feature in the compositor.
  virtual skia::RefPtr<SkPicture> CapturePicture() = 0;
#endif

 protected:
  virtual ~RenderView() {}

 private:
  // This interface should only be implemented inside content.
  friend class RenderViewImpl;
  RenderView() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_
