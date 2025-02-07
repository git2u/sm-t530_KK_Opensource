/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebHelperPluginImpl.h"

#include "PageWidgetDelegate.h"
#include "WebDocument.h"
#include "WebFrameImpl.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "WebWidgetClient.h"
#include "core/dom/NodeList.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/EmptyClients.h"
#include "core/page/FocusController.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"

using namespace WebCore;

namespace WebKit {

#define addLiteral(literal, writer)    writer.addData(literal, sizeof(literal) - 1)

static inline void addString(const String& str, DocumentWriter& writer)
{
    CString str8 = str.utf8();
    writer.addData(str8.data(), str8.length());
}

void writeDocument(const String& pluginType, const WebDocument& hostDocument, WebCore::DocumentWriter& writer)
{
    // Give the new document the same URL as the hose document so that content
    // settings and other decisions can be made based on the correct origin.
    const WebURL& url = hostDocument.url();

    writer.setMIMEType("text/html");
    writer.setEncoding("UTF-8", false);
    writer.begin(url);

    addLiteral("<!DOCTYPE html><head><meta charset='UTF-8'></head><body>\n", writer);
    String objectTag = "<object type=\"" + pluginType + "\"></object>";
    addString(objectTag, writer);
    addLiteral("</body>\n", writer);

    writer.end();
}

class HelperPluginChromeClient : public EmptyChromeClient {
    WTF_MAKE_NONCOPYABLE(HelperPluginChromeClient);
    WTF_MAKE_FAST_ALLOCATED;

public:
    explicit HelperPluginChromeClient(WebHelperPluginImpl* widget)
        : m_widget(widget)
    {
        ASSERT(m_widget->m_widgetClient);
    }

private:
    virtual void closeWindowSoon() OVERRIDE
    {
        // This should never be called since the only way to close the
        // invisible page is via closeHelperPlugin().
        ASSERT_NOT_REACHED(); 
        m_widget->closeHelperPlugin();
    }

    virtual void* webView() const OVERRIDE
    {
        return m_widget->m_webView;
    }

    WebHelperPluginImpl* m_widget;
};

// WebHelperPluginImpl ----------------------------------------------------------------

WebHelperPluginImpl::WebHelperPluginImpl(WebWidgetClient* client)
    : m_widgetClient(client)
    , m_webView(0)
{
    ASSERT(client);
}

WebHelperPluginImpl::~WebHelperPluginImpl()
{
    ASSERT(!m_page);
}

bool WebHelperPluginImpl::initialize(const String& pluginType, const WebDocument& hostDocument, WebViewImpl* webView)
{
    ASSERT(webView);
    m_webView = webView;

    return initializePage(pluginType, hostDocument);
}

void WebHelperPluginImpl::closeHelperPlugin()
{
    if (m_page) {
        m_page->clearPageGroup();
        m_page->mainFrame()->loader()->stopAllLoaders();
        m_page->mainFrame()->loader()->stopLoading(UnloadEventPolicyNone);
    }

    // We must destroy the page now in case the host page is being destroyed, in
    // which case some of the objects the page depends on may have been
    // destroyed by the time this->close() is called asynchronously.
    destroyPage();

    // m_widgetClient might be 0 because this widget might be already closed.
    if (m_widgetClient) {
        // closeWidgetSoon() will call this->close() later.
        m_widgetClient->closeWidgetSoon();
    }
}

void WebHelperPluginImpl::initializeFrame(WebFrameClient* client)
{
    ASSERT(m_page);
    RefPtr<WebFrameImpl> frame = WebFrameImpl::create(client);
    frame->initializeAsMainFrame(m_page.get());
}

// Returns a pointer to the WebPlugin by finding the single <object> tag in the page.
WebPlugin* WebHelperPluginImpl::getPlugin()
{
    ASSERT(m_page);

    RefPtr<NodeList> objectElements = m_page->mainFrame()->document()->getElementsByTagName(WebCore::HTMLNames::objectTag.localName());
    ASSERT(objectElements && objectElements->length() == 1);
    if (!objectElements || objectElements->length() < 1)
        return 0;
    Node* node = objectElements->item(0);
    ASSERT(node->hasTagName(WebCore::HTMLNames::objectTag));
    WebCore::Widget* widget = toHTMLPlugInElement(node)->pluginWidget();
    if (!widget)
        return 0;
    WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(widget)->plugin();
    ASSERT(plugin);
    // If the plugin is a placeholder, it is not useful to the caller, and it
    // could be replaced at any time. Therefore, do not return it.
    if (plugin->isPlaceholder())
        return 0;

    // The plugin was instantiated and will outlive this object.
    return plugin;
}

bool WebHelperPluginImpl::initializePage(const String& pluginType, const WebDocument& hostDocument)
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_chromeClient = adoptPtr(new HelperPluginChromeClient(this));
    pageClients.chromeClient = m_chromeClient.get();

    m_page = adoptPtr(new Page(pageClients));
    // Scripting must be enabled in ScriptController::windowScriptNPObject().
    m_page->settings()->setScriptEnabled(true);
    m_page->settings()->setPluginsEnabled(true);

    unsigned layoutMilestones = DidFirstLayout | DidFirstVisuallyNonEmptyLayout;
    m_page->addLayoutMilestones(static_cast<LayoutMilestones>(layoutMilestones));

    m_webView->client()->initializeHelperPluginWebFrame(this);

    // The page's main frame was set in initializeFrame() as a result of the above call.
    Frame* frame = m_page->mainFrame();
    ASSERT(frame);
    frame->setView(FrameView::create(frame));
    // No need to set a size or make it not transparent.

    DocumentWriter* writer = frame->loader()->activeDocumentLoader()->writer();
    writeDocument(pluginType, hostDocument, *writer);

    return true;
}

void WebHelperPluginImpl::destroyPage()
{
    if (!m_page)
        return;

    if (m_page->mainFrame())
        m_page->mainFrame()->loader()->frameDetached();

    m_page.clear();
}

void WebHelperPluginImpl::layout()
{
    PageWidgetDelegate::layout(m_page.get());
}

void WebHelperPluginImpl::setFocus(bool)
{
    ASSERT_NOT_REACHED();
}

void WebHelperPluginImpl::close()
{
    ASSERT(!m_page); // Should only be called via closePopup().
    m_widgetClient = 0;
    deref();
}

// WebHelperPlugin ----------------------------------------------------------------

WebHelperPlugin* WebHelperPlugin::create(WebWidgetClient* client)
{
    if (!client)
        CRASH();
    // A WebHelperPluginImpl instance usually has two references.
    //  - One owned by the instance itself. It represents the visible widget.
    //  - One owned by the hosting element. It's released when the hosting
    //    element asks the WebHelperPluginImpl to close.
    // We need them because the closing operation is asynchronous and the widget
    // can be closed while the hosting element is unaware of it.
    return adoptRef(new WebHelperPluginImpl(client)).leakRef();
}

} // namespace WebKit
