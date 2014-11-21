/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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
#include "WebViewImpl.h"

#include "AutofillPopupMenuClient.h"
#include "BatteryClientImpl.h"
#include "CSSValueKeywords.h"
#include "CompositionUnderlineVectorBuilder.h"
#include "ContextFeaturesClientImpl.h"
#include "DeviceOrientationClientProxy.h"
#if defined(SBROWSER_DEVICEMOTION_IMPL)
#include "SbrDeviceMotionClientProxy.h"
#endif
#include "GeolocationClientProxy.h"
#include "GraphicsLayerFactoryChromium.h"
#include "HTMLNames.h"
#include "LinkHighlight.h"
#include "NonCompositedContentHost.h"
#include "PageWidgetDelegate.h"
#include "PrerendererClientImpl.h"
#include "SpeechInputClientImpl.h"
#include "SpeechRecognitionClientProxy.h"
#include "TextFieldDecoratorImpl.h"
#include "ValidationMessageClientImpl.h"
#include "ViewportAnchor.h"
#include "WebAccessibilityObject.h"
#include "WebActiveWheelFlingParameters.h"
#include "WebAutofillClient.h"
#include "WebCompositorInputHandlerImpl.h"
#include "WebDevToolsAgentImpl.h"
#include "WebDevToolsAgentPrivate.h"
#include "WebFrameImpl.h"
#include "WebHelperPluginImpl.h"
#include "WebHitTestResult.h"
#include "WebInputElement.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebMediaPlayerAction.h"
#include "WebNode.h"
#include "WebPagePopupImpl.h"
#include "WebPlugin.h"
#include "WebPluginAction.h"
#include "WebPluginContainerImpl.h"
#include "WebPopupMenuImpl.h"
#include "WebRange.h"
#include "WebRuntimeFeatures.h"
#include "WebSettingsImpl.h"
#include "WebTextInputInfo.h"
#include "WebViewClient.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/css/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/KeyboardEvent.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/SimulatedClickOptions.h"
#include "core/dom/Text.h"
#include "core/dom/WheelEvent.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/TextIterator.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/page/Chrome.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DragController.h"
#include "core/page/DragSession.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/PageGroup.h"
#include "core/page/PageGroupLoadDeferrer.h"
#include "core/page/PagePopupClient.h"
#include "core/page/PointerLockController.h"
#include "core/page/SecurityOrigin.h"
#include "core/page/SecurityPolicy.h"
#include "core/page/Settings.h"
#include "core/page/TouchDisambiguation.h"
#include "core/platform/ContextMenu.h"
#include "core/platform/ContextMenuItem.h"
#include "core/platform/Cursor.h"
#include "core/platform/DragData.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/NotImplemented.h"
#include "core/platform/PlatformGestureEvent.h"
#include "core/platform/PlatformKeyboardEvent.h"
#include "core/platform/PlatformMouseEvent.h"
#include "core/platform/PlatformWheelEvent.h"
#include "core/platform/PopupMenuClient.h"
#include "core/platform/SchemeRegistry.h"
#include "core/platform/Timer.h"
#include "core/platform/chromium/KeyboardCodes.h"
#include "core/platform/chromium/PopupContainer.h"
#include "core/platform/chromium/TraceEvent.h"
#include "core/platform/chromium/support/GraphicsContext3DPrivate.h"
#include "core/platform/chromium/support/WebActiveGestureAnimation.h"
#include "core/platform/graphics/Color.h"
#include "core/platform/graphics/ColorSpace.h"
#include "core/platform/graphics/Extensions3D.h"
#include "core/platform/graphics/FontDescription.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsContext3D.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/chromium/LayerPainterChromium.h"
#include "core/platform/graphics/gpu/SharedGraphicsContext3D.h"
#include "core/platform/graphics/skia/PlatformContextSkia.h"
#include "core/platform/network/ResourceHandle.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderWidget.h"
#include "modules/battery/BatteryController.h"
#include "modules/geolocation/GeolocationController.h"
#include "painting/ContinuousPainter.h"
#include <public/Platform.h>
#include <public/WebCompositorOutputSurface.h>
#include <public/WebCompositorSupport.h>
#include <public/WebDragData.h>
#include <public/WebFloatPoint.h>
#include <public/WebGraphicsContext3D.h>
#include <public/WebImage.h>
#include <public/WebLayer.h>
#include <public/WebLayerTreeView.h>
#include <public/WebPoint.h>
#include <public/WebRect.h>
#include <public/WebString.h>
#include <public/WebVector.h>
//SBROWSER_DOUBLETAP_PATENT_CHANGE >>
#if defined (SBROWSER_DOUBLETAP_PATENT_CHANGE)
#include "cc/base/switches.h"
#include "base/command_line.h"
#endif
//SBROWSER_DOUBLETAP_PATENT_CHANGE <<
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/RefPtr.h>
#include <wtf/TemporaryChange.h>
#include <wtf/Uint8ClampedArray.h>
#if defined(SBROWSER_TEXT_SELECTION) 
#include "core/platform/chromium/ClipboardChromium.h"

#endif

#if defined(SBROWSER_FPAUTH_IMPL)
#include "core/html/HTMLFormElement.h"
#endif


#if ENABLE(DEFAULT_RENDER_THEME)
#include "core/platform/chromium/PlatformThemeChromiumDefault.h"
#include "core/rendering/RenderThemeChromiumDefault.h"
#endif

#if OS(WINDOWS)
#if !ENABLE(DEFAULT_RENDER_THEME)
#include "core/rendering/RenderThemeChromiumWin.h"
#endif
#else
#include "core/rendering/RenderTheme.h"
#endif

#if defined(SBROWSER_FORM_NAVIGATION) 
#include "core/dom/NodeTraversal.h"
#include "core/html/HTMLFrameOwnerElement.h"
#endif
#if defined (SBROWSER_SCROLL_INPUTTEXT)
#include "core/rendering/RenderTextControl.h"
#endif
#if defined(SBROWSER_DPAD_CENTER_KEY_HANDLING)
#include <android/keycodes.h>
#endif

#include "chrome/common/chrome_switches.h"
#if defined(SBROWSER_ENABLE_APPNAP)  
#include "core/loader/ProgressTracker.h"
#include "bindings/v8/ScriptController.h"
#endif
#undef LOG
#include "base/logging.h"

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

#if defined (SBROWSER_SAVEPAGE_IMPL)
#include "core/html/HTMLCollection.h"
#include "core/loader/cache/CachedImage.h"
#include "core/rendering/RenderImage.h"
#endif

#if defined (SBROWSER_HANDLE_MOUSECLICK_CTRL)
#include "core/html/HTMLAnchorElement.h"
#include "core/dom/Element.h"
#include "core/platform/KURL.h"
#include "base/utf_string_conversions.h"
#endif

using namespace WebCore;
using namespace std;

// The following constants control parameters for automated scaling of webpages
// (such as due to a double tap gesture or find in page etc.). These are
// experimentally determined.
static const int touchPointPadding = 2;
static const int nonUserInitiatedPointPadding = 11;
static const float minScaleDifference = 0.01f;
static const float doubleTapZoomContentDefaultMargin = 2;
static const float doubleTapZoomContentMinimumMargin = 2;
static const double doubleTapZoomAnimationDurationInSeconds = 0.35;
static const float doubleTapZoomAlreadyLegibleRatio = 1.2f;

// Constants for viewport anchoring on resize.
static const float viewportAnchorXCoord = 0.5f;
static const float viewportAnchorYCoord = 0;

// Constants for zooming in on a focused text field.
static const double scrollAndScaleAnimationDurationInSeconds = 0.2;
static const int minReadableCaretHeight = 18;
static const float minScaleChangeToTriggerZoom = 1.05f;
static const float leftBoxRatio = 0.3f;
static const int caretPadding = 10;

namespace WebKit {

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
const double WebView::textSizeMultiplierRatio = 1.2;
const double WebView::minTextSizeMultiplier = 0.5;
const double WebView::maxTextSizeMultiplier = 3.0;
const float WebView::minPageScaleFactor = 0.25f;
const float WebView::maxPageScaleFactor = 4.0f;

// Used to defer all page activity in cases where the embedder wishes to run
// a nested event loop. Using a stack enables nesting of message loop invocations.
static Vector<PageGroupLoadDeferrer*>& pageGroupLoadDeferrerStack()
{
    DEFINE_STATIC_LOCAL(Vector<PageGroupLoadDeferrer*>, deferrerStack, ());
    return deferrerStack;
}

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
#define COMPILE_ASSERT_MATCHING_ENUM(coreName) \
    COMPILE_ASSERT(int(coreName) == int(Web##coreName), dummy##coreName)
COMPILE_ASSERT_MATCHING_ENUM(DragOperationNone);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationCopy);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationLink);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationGeneric);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationPrivate);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationMove);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationDelete);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationEvery);

static const PopupContainerSettings autofillPopupSettings = {
    false, // setTextOnIndexChange
    false, // acceptOnAbandon
    true, // loopSelectionNavigation
    false // restrictWidthOfListBox (For security reasons show the entire entry
          // so the user doesn't enter information he did not intend to.)
};

static bool shouldUseExternalPopupMenus = false;

static int webInputEventKeyStateToPlatformEventKeyState(int webInputEventKeyState)
{
    int platformEventKeyState = 0;
    if (webInputEventKeyState & WebInputEvent::ShiftKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::ShiftKey;
    if (webInputEventKeyState & WebInputEvent::ControlKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::CtrlKey;
    if (webInputEventKeyState & WebInputEvent::AltKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::AltKey;
    if (webInputEventKeyState & WebInputEvent::MetaKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::MetaKey;
    return platformEventKeyState;
}

// WebView ----------------------------------------------------------------

WebView* WebView::create(WebViewClient* client)
{
#if defined(SBROWSER_DEVICEMOTION_IMPL)
     WebRuntimeFeatures::enableDeviceMotion(true); //SISO - Device Motion  Support 
#else
    // Keep runtime flag for device motion turned off until it's implemented.
    WebRuntimeFeatures::enableDeviceMotion(false);
#endif	
    // Pass the WebViewImpl's self-reference to the caller.
    return adoptRef(new WebViewImpl(client)).leakRef();
}

void WebView::setUseExternalPopupMenus(bool useExternalPopupMenus)
{
    shouldUseExternalPopupMenus = useExternalPopupMenus;
}

void WebView::updateVisitedLinkState(unsigned long long linkHash)
{
    Page::visitedStateChanged(PageGroup::sharedGroup(), linkHash);
}

void WebView::resetVisitedLinkState()
{
    Page::allVisitedStateChanged(PageGroup::sharedGroup());
}

void WebView::willEnterModalLoop()
{
    PageGroup* pageGroup = PageGroup::sharedGroup();
    if (pageGroup->pages().isEmpty())
        pageGroupLoadDeferrerStack().append(static_cast<PageGroupLoadDeferrer*>(0));
    else {
        // Pick any page in the page group since we are deferring all pages.
        pageGroupLoadDeferrerStack().append(new PageGroupLoadDeferrer(*pageGroup->pages().begin(), true));
    }
}

void WebView::didExitModalLoop()
{
    ASSERT(pageGroupLoadDeferrerStack().size());

    delete pageGroupLoadDeferrerStack().last();
    pageGroupLoadDeferrerStack().removeLast();
}

void WebViewImpl::initializeMainFrame(WebFrameClient* frameClient)
{
    // NOTE: The WebFrameImpl takes a reference to itself within InitMainFrame
    // and releases that reference once the corresponding Frame is destroyed.
    RefPtr<WebFrameImpl> frame = WebFrameImpl::create(frameClient);

    frame->initializeAsMainFrame(page());

    // Restrict the access to the local file system
    // (see WebView.mm WebView::_commonInitializationWithFrameName).
    SecurityPolicy::setLocalLoadPolicy(SecurityPolicy::AllowLocalLoadsForLocalOnly);
}
#if defined(SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT)
void WebViewImpl::setBingAsDefaultSearchEngineFromJSinWebView(bool enable){   
    m_client->setBingSearchEngineAsDefaultSearch(true);	  
}
#endif	


#if defined (SBROWSER_ENABLE_ECHO_PWD)
void WebViewImpl::SetPasswordEchoEnabled(bool enable) {
    if (!m_page)
        return;
    
    if (!m_page->settings())
        return;

    m_page->settings()->setPasswordEchoEnabled(enable);
}
#endif

#if defined(SBROWSER_OVERVIEW_MODE)
float WebViewImpl::getScreenPixelDensityScale() const {
	if(m_client)
		return  (float) (m_client->getDeviceDpi()/ 160.0f);
	else
		return 1.0f;
}
#endif

#if defined(SBROWSER_CHROME_NATIVE_PREFERENCES)
void WebViewImpl::setOverviewModeEnabled(bool enable)
{
    if (!m_page)
        return;
    
    if (!m_page->settings())
        return;

    m_page->settings()->setOverviewModeEnabled(enable);
}

bool WebViewImpl::overviewModeEnabled()const
{
    if (!m_page)
        return false;

    if (!m_page->settings())
        return false;

    return m_page->settings()->overviewModeEnabled();
}
#endif // SBROWSER_CHROME_NATIVE_PREFERENCES

// SAMSUNG_CHANGE : FontBoosting mode
#if defined(SBROWSER_FONT_BOOSTING_PREFERENCES)
void WebViewImpl::setFontBoostingModeEnabled(bool enable)
{
	if ((m_page != NULL) && (m_page->settings() != NULL))
	{
        	m_page->settings()->setFontBoostingModeEnabled(enable);
	}/* end if */
}

bool WebViewImpl::fontboostingModeEnabled() const
{
	if ((m_page != NULL) && (m_page->settings() != NULL))
	{
	    return m_page->settings()->fontboostingModeEnabled();
	}
	return false;
}
#endif /* SBROWSER_FONT_BOOSTING_PREFERENCES */
// SAMSUNG_CHANGE : FontBoosting mode

void WebViewImpl::initializeHelperPluginFrame(WebFrameClient* client)
{
    RefPtr<WebFrameImpl> frame = WebFrameImpl::create(client);
}

void WebViewImpl::setAutofillClient(WebAutofillClient* autofillClient)
{
    m_autofillClient = autofillClient;
}

void WebViewImpl::setDevToolsAgentClient(WebDevToolsAgentClient* devToolsClient)
{
    if (devToolsClient)
        m_devToolsAgent = adoptPtr(new WebDevToolsAgentImpl(this, devToolsClient));
    else
        m_devToolsAgent.clear();
}

void WebViewImpl::setValidationMessageClient(WebValidationMessageClient* client)
{
    ASSERT(client);
    m_validationMessage = ValidationMessageClientImpl::create(*client);
    m_page->setValidationMessageClient(m_validationMessage.get());
}

void WebViewImpl::setPermissionClient(WebPermissionClient* permissionClient)
{
    m_permissionClient = permissionClient;
    m_featureSwitchClient->setPermissionClient(permissionClient);
}

void WebViewImpl::setPrerendererClient(WebPrerendererClient* prerendererClient)
{
    providePrerendererClientTo(m_page.get(), new PrerendererClientImpl(prerendererClient));
}

void WebViewImpl::setSpellCheckClient(WebSpellCheckClient* spellCheckClient)
{
    m_spellCheckClient = spellCheckClient;
}

void WebViewImpl::addTextFieldDecoratorClient(WebTextFieldDecoratorClient* client)
{
    ASSERT(client);
    // We limit the number of decorators because it affects performance of text
    // field creation. If you'd like to add more decorators, consider moving
    // your decorator or existing decorators to WebCore.
    const unsigned maximumNumberOfDecorators = 8;
    if (m_textFieldDecorators.size() >= maximumNumberOfDecorators)
        CRASH();
    m_textFieldDecorators.append(TextFieldDecoratorImpl::create(client));
}

WebViewImpl::WebViewImpl(WebViewClient* client)
    : m_client(client)
    , m_autofillClient(0)
    , m_permissionClient(0)
    , m_spellCheckClient(0)
    , m_chromeClientImpl(this)
    , m_contextMenuClientImpl(this)
    , m_dragClientImpl(this)
    , m_editorClientImpl(this)
    , m_inspectorClientImpl(this)
    , m_backForwardClientImpl(this)
    , m_shouldAutoResize(false)
    , m_observedNewNavigation(false)
#ifndef NDEBUG
    , m_newNavigationLoader(0)
#endif
    , m_zoomLevel(0)
    , m_minimumZoomLevel(zoomFactorToZoomLevel(minTextSizeMultiplier))
    , m_maximumZoomLevel(zoomFactorToZoomLevel(maxTextSizeMultiplier))
    , m_pageDefinedMinimumPageScaleFactor(-1)
    , m_pageDefinedMaximumPageScaleFactor(-1)
    , m_minimumPageScaleFactor(minPageScaleFactor)
    , m_maximumPageScaleFactor(maxPageScaleFactor)
    , m_initialPageScaleFactorOverride(-1)
    , m_initialPageScaleFactor(-1)
    , m_ignoreViewportTagMaximumScale(false)
    , m_pageScaleFactorIsSet(false)
    , m_savedPageScaleFactor(0)
    , m_doubleTapZoomPageScaleFactor(0)
    , m_doubleTapZoomPending(false)
    , m_enableFakeDoubleTapAnimationForTesting(false)
    , m_fakeDoubleTapPageScaleFactor(0)
    , m_fakeDoubleTapUseAnchor(false)
    , m_contextMenuAllowed(false)
    , m_doingDragAndDrop(false)
    , m_ignoreInputEvents(false)
    , m_suppressNextKeypressEvent(false)
    , m_initialNavigationPolicy(WebNavigationPolicyIgnore)
    , m_imeAcceptEvents(true)
    , m_operationsAllowed(WebDragOperationNone)
    , m_dragOperation(WebDragOperationNone)
    , m_featureSwitchClient(adoptPtr(new ContextFeaturesClientImpl()))
    , m_autofillPopupShowing(false)
    , m_autofillPopup(0)
    , m_isTransparent(false)
    , m_tabsToLinks(false)
    , m_isCancelingFullScreen(false)
    , m_benchmarkSupport(this)
    , m_layerTreeView(0)
    , m_rootLayer(0)
    , m_rootGraphicsLayer(0)
    , m_graphicsLayerFactory(adoptPtr(new GraphicsLayerFactoryChromium(this)))
    , m_isAcceleratedCompositingActive(false)
    , m_layerTreeViewCommitsDeferred(false)
    , m_compositorCreationFailed(false)
    , m_recreatingGraphicsContext(false)
    , m_inputHandlerIdentifier(-1)
#if ENABLE(INPUT_SPEECH)
    , m_speechInputClient(SpeechInputClientImpl::create(client))
#endif
    , m_speechRecognitionClient(SpeechRecognitionClientProxy::create(client ? client->speechRecognizer() : 0))
    , m_deviceOrientationClientProxy(adoptPtr(new DeviceOrientationClientProxy(client ? client->deviceOrientationClient() : 0)))
#if defined(SBROWSER_DEVICEMOTION_IMPL)
, m_deviceMotionClientProxy(adoptPtr(new DeviceMotionClientProxy(client ? client->deviceMotionClient() : 0)))
#endif
    , m_geolocationClientProxy(adoptPtr(new GeolocationClientProxy(client ? client->geolocationClient() : 0)))
#if ENABLE(BATTERY_STATUS)
    , m_batteryClient(adoptPtr(new BatteryClientImpl(client ? client->batteryStatusClient() : 0)))
#endif
    , m_emulatedTextZoomFactor(1)
    , m_userMediaClientImpl(this)
#if ENABLE(NAVIGATOR_CONTENT_UTILS)
    , m_navigatorContentUtilsClient(NavigatorContentUtilsClientImpl::create(this))
#endif
#if defined(SBROWSER_OVERVIEW_MODE)
    , m_isResizeHandled(false)
#endif
    , m_flingModifier(0)
    , m_flingSourceDevice(false)
    , m_showFPSCounter(false)
    , m_showPaintRects(false)
    , m_showDebugBorders(false)
    , m_continuousPaintingEnabled(false)
#if defined(SBROWSER_HOVER_HIGHLIGHT)
    , m_prevHoverNode(0)
#endif
#if defined(SBROWSER_TABLET_SWITCH)
    ,m_isTabletMode(0)
#endif
#if defined(SBROWSER_ADJUST_WORKER_THREAD_PRIORITY)
    , m_adjustWorkerThreadPriorityFlag(false)
#endif
#if defined(SBROWSER_OVERVIEW_MODE)
    , m_oldContentWidth(0)
    , m_activateOverview(false)
#endif
{
    // WebKit/win/WebView.cpp does the same thing, except they call the
    // KJS specific wrapper around this method. We need to have threading
    // initialized because CollatorICU requires it.
    WTF::initializeThreading();
    WTF::initializeMainThread();

    Page::PageClients pageClients;
    pageClients.chromeClient = &m_chromeClientImpl;
    pageClients.contextMenuClient = &m_contextMenuClientImpl;
    pageClients.editorClient = &m_editorClientImpl;
    pageClients.dragClient = &m_dragClientImpl;
    pageClients.inspectorClient = &m_inspectorClientImpl;
    pageClients.backForwardClient = &m_backForwardClientImpl;

    m_page = adoptPtr(new Page(pageClients));
    provideUserMediaTo(m_page.get(), &m_userMediaClientImpl);
#if ENABLE(INPUT_SPEECH)
    provideSpeechInputTo(m_page.get(), m_speechInputClient.get());
#endif
    provideSpeechRecognitionTo(m_page.get(), m_speechRecognitionClient.get());
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    provideNotification(m_page.get(), notificationPresenterImpl());
#endif
#if ENABLE(NAVIGATOR_CONTENT_UTILS)
    provideNavigatorContentUtilsTo(m_page.get(), m_navigatorContentUtilsClient.get());
#endif

    provideContextFeaturesTo(m_page.get(), m_featureSwitchClient.get());
    provideDeviceOrientationTo(m_page.get(), m_deviceOrientationClientProxy.get());
#if defined(SBROWSER_DEVICEMOTION_IMPL)
    provideDeviceMotionTo(m_page.get(), m_deviceMotionClientProxy.get());
#endif
    provideGeolocationTo(m_page.get(), m_geolocationClientProxy.get());
    m_geolocationClientProxy->setController(GeolocationController::from(m_page.get()));

#if ENABLE(BATTERY_STATUS)
    provideBatteryTo(m_page.get(), m_batteryClient.get());
    m_batteryClient->setController(BatteryController::from(m_page.get()));
#endif

    m_page->setGroupType(Page::SharedPageGroup);

    unsigned layoutMilestones = DidFirstLayout | DidFirstVisuallyNonEmptyLayout;
    m_page->addLayoutMilestones(static_cast<LayoutMilestones>(layoutMilestones));

    if (m_client)
        setVisibilityState(m_client->visibilityState(), true);

    m_inspectorSettingsMap = adoptPtr(new SettingsMap);
}

WebViewImpl::~WebViewImpl()
{
    ASSERT(!m_page);
}

RenderTheme* WebViewImpl::theme() const
{
    return m_page ? m_page->theme() : RenderTheme::defaultTheme().get();
}

WebFrameImpl* WebViewImpl::mainFrameImpl()
{
    return m_page ? WebFrameImpl::fromFrame(m_page->mainFrame()) : 0;
}

bool WebViewImpl::tabKeyCyclesThroughElements() const
{
    ASSERT(m_page);
    return m_page->tabKeyCyclesThroughElements();
}

void WebViewImpl::setTabKeyCyclesThroughElements(bool value)
{
    if (m_page)
        m_page->setTabKeyCyclesThroughElements(value);
}

void WebViewImpl::handleMouseLeave(Frame& mainFrame, const WebMouseEvent& event)
{
    m_client->setMouseOverURL(WebURL());
    PageWidgetEventHandler::handleMouseLeave(mainFrame, event);
}

void WebViewImpl::handleMouseDown(Frame& mainFrame, const WebMouseEvent& event)
{
    // If there is a popup open, close it as the user is clicking on the page (outside of the
    // popup). We also save it so we can prevent a click on an element from immediately
    // reopening the same popup.
    RefPtr<WebCore::PopupContainer> selectPopup;
#if ENABLE(PAGE_POPUP)
    RefPtr<WebPagePopupImpl> pagePopup;
#endif
    if (event.button == WebMouseEvent::ButtonLeft) {
        selectPopup = m_selectPopup;
#if ENABLE(PAGE_POPUP)
        pagePopup = m_pagePopup;
#endif
        hidePopups();
        ASSERT(!m_selectPopup);
#if ENABLE(PAGE_POPUP)
        ASSERT(!m_pagePopup);
#endif
    }

    m_lastMouseDownPoint = WebPoint(event.x, event.y);

    if (event.button == WebMouseEvent::ButtonLeft) {
        IntPoint point(event.x, event.y);
        point = m_page->mainFrame()->view()->windowToContents(point);
        HitTestResult result(m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(point));
        Node* hitNode = result.innerNonSharedNode();

        // Take capture on a mouse down on a plugin so we can send it mouse events.
        if (hitNode && hitNode->renderer() && hitNode->renderer()->isEmbeddedObject()) {
            m_mouseCaptureNode = hitNode;
            TRACE_EVENT_ASYNC_BEGIN0("webkit", "capturing mouse", this);
        }
    }

    PageWidgetEventHandler::handleMouseDown(mainFrame, event);

    if (m_selectPopup && m_selectPopup == selectPopup) {
        // That click triggered a select popup which is the same as the one that
        // was showing before the click.  It means the user clicked the select
        // while the popup was showing, and as a result we first closed then
        // immediately reopened the select popup.  It needs to be closed.
        hideSelectPopup();
    }

#if ENABLE(PAGE_POPUP)
    if (m_pagePopup && pagePopup && m_pagePopup->hasSamePopupClient(pagePopup.get())) {
        // That click triggered a page popup that is the same as the one we just closed.
        // It needs to be closed.
        closePagePopup(m_pagePopup.get());
    }
#endif

    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Windows, we handle it on mouse up, not down.
#if OS(DARWIN)
    if (event.button == WebMouseEvent::ButtonRight
        || (event.button == WebMouseEvent::ButtonLeft
            && event.modifiers & WebMouseEvent::ControlKey))
        mouseContextMenu(event);
#elif OS(UNIX)
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

void WebViewImpl::mouseContextMenu(const WebMouseEvent& event)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    m_page->contextMenuController()->clearContextMenu();

    PlatformMouseEventBuilder pme(mainFrameImpl()->frameView(), event);

    // Find the right target frame. See issue 1186900.
    HitTestResult result = hitTestResultForWindowPos(pme.position());
    Frame* targetFrame;
    if (result.innerNonSharedNode())
        targetFrame = result.innerNonSharedNode()->document()->frame();
    else
        targetFrame = m_page->focusController()->focusedOrMainFrame();

#if OS(WINDOWS)
    targetFrame->view()->setCursor(pointerCursor());
#endif

    m_contextMenuAllowed = true;
    targetFrame->eventHandler()->sendContextMenuEvent(pme);
    m_contextMenuAllowed = false;
    // Actually showing the context menu is handled by the ContextMenuClient
    // implementation...
}

void WebViewImpl::handleMouseUp(Frame& mainFrame, const WebMouseEvent& event)
{
    PageWidgetEventHandler::handleMouseUp(mainFrame, event);

#if OS(WINDOWS)
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

bool WebViewImpl::handleMouseWheel(Frame& mainFrame, const WebMouseWheelEvent& event)
{
    hidePopups();
    return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
}

void WebViewImpl::scrollBy(const WebFloatSize& delta)
{
    if (m_flingSourceDevice == WebGestureEvent::Touchpad) {
        WebMouseWheelEvent syntheticWheel;
        const float tickDivisor = WebCore::WheelEvent::TickMultiplier;

        syntheticWheel.deltaX = delta.width;
        syntheticWheel.deltaY = delta.height;
        syntheticWheel.wheelTicksX = delta.width / tickDivisor;
        syntheticWheel.wheelTicksY = delta.height / tickDivisor;
        syntheticWheel.hasPreciseScrollingDeltas = true;
        syntheticWheel.x = m_positionOnFlingStart.x;
        syntheticWheel.y = m_positionOnFlingStart.y;
        syntheticWheel.globalX = m_globalPositionOnFlingStart.x;
        syntheticWheel.globalY = m_globalPositionOnFlingStart.y;
        syntheticWheel.modifiers = m_flingModifier;

        if (m_page && m_page->mainFrame() && m_page->mainFrame()->view())
            handleMouseWheel(*m_page->mainFrame(), syntheticWheel);
    } else {
        WebGestureEvent syntheticGestureEvent;

        syntheticGestureEvent.type = WebInputEvent::GestureScrollUpdateWithoutPropagation;
        syntheticGestureEvent.data.scrollUpdate.deltaX = delta.width;
        syntheticGestureEvent.data.scrollUpdate.deltaY = delta.height;
        syntheticGestureEvent.x = m_positionOnFlingStart.x;
        syntheticGestureEvent.y = m_positionOnFlingStart.y;
        syntheticGestureEvent.globalX = m_globalPositionOnFlingStart.x;
        syntheticGestureEvent.globalY = m_globalPositionOnFlingStart.y;
        syntheticGestureEvent.modifiers = m_flingModifier;
        syntheticGestureEvent.sourceDevice = WebGestureEvent::Touchscreen;

        if (m_page && m_page->mainFrame() && m_page->mainFrame()->view())
            handleGestureEvent(syntheticGestureEvent);
    }
}

bool WebViewImpl::handleGestureEvent(const WebGestureEvent& event)
{
    bool eventSwallowed = false;
    bool eventCancelled = false; // for disambiguation

    // Special handling for slow-path fling gestures, which have no PlatformGestureEvent equivalent.
    switch (event.type) {
    case WebInputEvent::GestureFlingStart: {
        if (mainFrameImpl()->frame()->eventHandler()->isScrollbarHandlingGestures()) {
            m_client->didHandleGestureEvent(event, eventCancelled);
            return eventSwallowed;
        }
        m_client->cancelScheduledContentIntents();
        m_positionOnFlingStart = WebPoint(event.x / pageScaleFactor(), event.y / pageScaleFactor());
        m_globalPositionOnFlingStart = WebPoint(event.globalX, event.globalY);
        m_flingModifier = event.modifiers;
        m_flingSourceDevice = event.sourceDevice;
        OwnPtr<WebGestureCurve> flingCurve = adoptPtr(Platform::current()->createFlingAnimationCurve(event.sourceDevice, WebFloatPoint(event.data.flingStart.velocityX, event.data.flingStart.velocityY), WebSize()));
        m_gestureAnimation = WebActiveGestureAnimation::createAtAnimationStart(flingCurve.release(), this);
        scheduleAnimation();
        eventSwallowed = true;

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventSwallowed;
    }
    case WebInputEvent::GestureFlingCancel:
        if (m_gestureAnimation) {
            m_gestureAnimation.clear();
            if (m_layerTreeView)
                m_layerTreeView->didStopFlinging();
            eventSwallowed = true;
        }

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventSwallowed;
    default:
        break;
    }

    PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), event);

    // Handle link highlighting outside the main switch to avoid getting lost in the
    // complicated set of cases handled below.
    switch (event.type) {
    case WebInputEvent::GestureTapDown:
        // Queue a highlight animation, then hand off to regular handler.
#if OS(LINUX) 
#if !defined(SBROWSER_FOCUS_RING_FIX)
        if (settingsImpl()->gestureTapHighlightEnabled())
            enableTapHighlight(platformEvent);
#endif		
#endif
        break;
#if !defined(SBROWSER_FOCUS_RING_FIX)
    case WebInputEvent::GestureTapCancel:
#endif		
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureLongPress:
#if defined(SBROWSER_LINKIFY_TEXT_SELECTION)
    if(mainFrameImpl())
        mainFrameImpl()->setContentDetectionResult(WebContentDetectionResult());
#endif
#if defined(SBROWSER_FOCUS_RING_FIX)		
		if (settingsImpl()->gestureTapHighlightEnabled())
            enableTapHighlight(platformEvent);
#endif		
        if (m_linkHighlight)
			m_linkHighlight->startHighlightAnimationIfNeeded();
   
        break;
    default:
        break;
    }

    switch (event.type) {
    case WebInputEvent::GestureTap: {
        m_client->cancelScheduledContentIntents();
        if (detectContentOnTouch(platformEvent.position())) {
            eventSwallowed = true;
            break;
        }

        RefPtr<WebCore::PopupContainer> selectPopup;
        selectPopup = m_selectPopup;
        hideSelectPopup();
        ASSERT(!m_selectPopup);

        // Don't trigger a disambiguation popup on sites designed for mobile devices.
        // Instead, assume that the page has been designed with big enough buttons and links.
        if (event.data.tap.width > 0 && !shouldDisableDesktopWorkarounds()) {
            // FIXME: didTapMultipleTargets should just take a rect instead of
            // an event.
            WebGestureEvent scaledEvent = event;
            scaledEvent.x = event.x / pageScaleFactor();
            scaledEvent.y = event.y / pageScaleFactor();
            scaledEvent.data.tap.width = event.data.tap.width / pageScaleFactor();
            scaledEvent.data.tap.height = event.data.tap.height / pageScaleFactor();
            IntRect boundingBox(scaledEvent.x - scaledEvent.data.tap.width / 2, scaledEvent.y - scaledEvent.data.tap.height / 2, scaledEvent.data.tap.width, scaledEvent.data.tap.height);
            Vector<IntRect> goodTargets;
            findGoodTouchTargets(boundingBox, mainFrameImpl()->frame(), goodTargets);
            // FIXME: replace touch adjustment code when numberOfGoodTargets == 1?
            // Single candidate case is currently handled by: https://bugs.webkit.org/show_bug.cgi?id=85101
            if (goodTargets.size() >= 2 && m_client && m_client->didTapMultipleTargets(scaledEvent, goodTargets)) {
                eventSwallowed = true;
                eventCancelled = true;
                break;
            }
        }

        eventSwallowed = mainFrameImpl()->frame()->eventHandler()->handleGestureEvent(platformEvent);

        if (m_selectPopup && m_selectPopup == selectPopup) {
            // That tap triggered a select popup which is the same as the one that
            // was showing before the tap. It means the user tapped the select
            // while the popup was showing, and as a result we first closed then
            // immediately reopened the select popup. It needs to be closed.
            hideSelectPopup();
        }

        break;
    }
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap: {
        if (!mainFrameImpl() || !mainFrameImpl()->frameView())
            break;
#if defined(SBROWSER_LINKIFY_TEXT_SELECTION)
    if(mainFrameImpl())
        mainFrameImpl()->setContentDetectionResult(WebContentDetectionResult());
#endif
        m_client->cancelScheduledContentIntents();
        m_page->contextMenuController()->clearContextMenu();
        m_contextMenuAllowed = true;
        eventSwallowed = mainFrameImpl()->frame()->eventHandler()->handleGestureEvent(platformEvent);
        m_contextMenuAllowed = false;

        break;
    }
    case WebInputEvent::GestureTapDown: {
        m_client->cancelScheduledContentIntents();
        eventSwallowed = mainFrameImpl()->frame()->eventHandler()->handleGestureEvent(platformEvent);
        break;
    }
    case WebInputEvent::GestureDoubleTap:
        if (m_webSettings->doubleTapToZoomEnabled() && m_minimumPageScaleFactor != m_maximumPageScaleFactor) {
            m_client->cancelScheduledContentIntents();
            animateZoomAroundPoint(platformEvent.position(), DoubleTap);
        }
        // GestureDoubleTap is currently only used by Android for zooming. For WebCore,
        // GestureTap with tap count = 2 is used instead. So we drop GestureDoubleTap here.
        eventSwallowed = true;
        break;
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GesturePinchBegin:
        m_client->cancelScheduledContentIntents();
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureScrollUpdateWithoutPropagation:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate: {
        eventSwallowed = mainFrameImpl()->frame()->eventHandler()->handleGestureEvent(platformEvent);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    m_client->didHandleGestureEvent(event, eventCancelled);
    return eventSwallowed;
}

void WebViewImpl::transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters& parameters)
{
    TRACE_EVENT0("webkit", "WebViewImpl::transferActiveWheelFlingAnimation");
    ASSERT(!m_gestureAnimation);
    m_positionOnFlingStart = parameters.point;
    m_globalPositionOnFlingStart = parameters.globalPoint;
    m_flingModifier = parameters.modifiers;
    OwnPtr<WebGestureCurve> curve = adoptPtr(Platform::current()->createFlingAnimationCurve(parameters.sourceDevice, WebFloatPoint(parameters.delta), parameters.cumulativeScroll));
    m_gestureAnimation = WebActiveGestureAnimation::createWithTimeOffset(curve.release(), this, parameters.startTime);
    scheduleAnimation();
}

bool WebViewImpl::startPageScaleAnimation(const IntPoint& targetPosition, bool useAnchor, float newScale, double durationInSeconds)
{
    WebPoint clampedPoint = targetPosition;
    if (!useAnchor) {
        clampedPoint = clampOffsetAtScale(targetPosition, newScale);
        if (!durationInSeconds) {
            setPageScaleFactor(newScale, clampedPoint);
            return false;
        }
    }
    if (useAnchor && newScale == pageScaleFactor())
        return false;

    if (m_enableFakeDoubleTapAnimationForTesting) {
        m_fakeDoubleTapTargetPosition = targetPosition;
        m_fakeDoubleTapUseAnchor = useAnchor;
        m_fakeDoubleTapPageScaleFactor = newScale;
    } else {
        if (!m_layerTreeView)
            return false;
        m_layerTreeView->startPageScaleAnimation(targetPosition, useAnchor, newScale, durationInSeconds);
    }
    return true;
}

void WebViewImpl::enableFakeDoubleTapAnimationForTesting(bool enable)
{
    m_enableFakeDoubleTapAnimationForTesting = enable;
}

WebViewBenchmarkSupport* WebViewImpl::benchmarkSupport()
{
    return &m_benchmarkSupport;
}

void WebViewImpl::setShowFPSCounter(bool show)
{
    if (isAcceleratedCompositingActive()) {
        TRACE_EVENT0("webkit", "WebViewImpl::setShowFPSCounter");
        m_layerTreeView->setShowFPSCounter(show);
    }
    m_showFPSCounter = show;
}

void WebViewImpl::setShowPaintRects(bool show)
{
    if (isAcceleratedCompositingActive()) {
        TRACE_EVENT0("webkit", "WebViewImpl::setShowPaintRects");
        m_layerTreeView->setShowPaintRects(show);
    }
    m_showPaintRects = show;
}

void WebViewImpl::setShowDebugBorders(bool show)
{
    if (isAcceleratedCompositingActive())
        m_layerTreeView->setShowDebugBorders(show);
    m_showDebugBorders = show;
}

void WebViewImpl::setContinuousPaintingEnabled(bool enabled)
{
    if (isAcceleratedCompositingActive()) {
        TRACE_EVENT0("webkit", "WebViewImpl::setContinuousPaintingEnabled");
        m_layerTreeView->setContinuousPaintingEnabled(enabled);
    }
    m_continuousPaintingEnabled = enabled;
    m_client->scheduleAnimation();
}
#if 0
#if defined(SBROWSER_FPAUTH_IMPL)
void WebViewImpl :: generateEnterEvent(WebInputElement& element){
    Node* node = focusedWebCoreNode();
    bool isOnClick = false; 

    if(node && node->hasTagName(HTMLNames::inputTag)){
        HTMLInputElement *inputElement = static_cast<HTMLInputElement*>(node);
        LOG(INFO)<<"FP: WebViewImpl :: generateEnterEvent node ="<<node;
        isOnClick = hasOnClickListener(inputElement->form());
        if(!isOnClick)
            inputElement->form()->submit();
    }
    else {
        HTMLInputElement *inputElement =element.operator PassRefPtr<HTMLInputElement>().get();
        LOG(INFO)<<"FP: WebViewImpl :: generateEnterEvent else ";//
	if(inputElement){
            isOnClick = hasOnClickListener(inputElement->form());
            if(!isOnClick)
                inputElement->form()->submit();
        }
    }
}
bool WebViewImpl::hasOnClickListener(HTMLFormElement* form){
    HTMLFormControlElement* firstSubmitButton =0;
    const Vector<FormAssociatedElement*>& formElements = form->associatedElements();
    for (size_t i = 0; i < formElements.size(); i++) {
        if (!formElements[i]->isFormControlElement())
            continue;
        HTMLFormControlElement* formElement = static_cast<HTMLFormControlElement*>(formElements[i]);
        if (!formElement->hasTagName(HTMLNames::inputTag))
            continue;
		if(!firstSubmitButton && formElement->isSuccessfulSubmitButton()){
			firstSubmitButton = formElement; //This gives first submit button if multiple submits exist
			HTMLInputElement* inputElement = formElement->toInputElement();
			if(inputElement && inputElement->hasEventListeners("click")){
			    LOG(INFO)<<"FP:has ONCLICK listener";
			    return true;
			}
		}
	}
	return false;
}
#endif
#endif

#if defined(SBROWSER_FPAUTH_IMPL)
void WebViewImpl :: generateEnterEvent(WebInputElement& element){
	/* Node* node = focusedWebCoreNode();	

	 if(node && node->hasTagName(HTMLNames::inputTag)) { 
	
			HTMLInputElement *inputElement = static_cast<HTMLInputElement*>(node);
			LOG(INFO)<<"FP: WebViewImpl :: generateEnterEvent node ="<<node;
			triggerClickOnSubmit(inputElement->form());			 
		
	 }
	else */{
			HTMLInputElement *inputElement =element.operator PassRefPtr<HTMLInputElement>().get();
			LOG(INFO)<<"FP: WebViewImpl :: generateEnterEvent else ";//
			triggerClickOnSubmit(inputElement->form());		
		 
	}
}

void WebViewImpl::triggerClickOnSubmit(HTMLFormElement* form)
{
    HTMLFormControlElement* firstSubmitButton =0;
    const Vector<FormAssociatedElement*>& formElements = form->associatedElements();

    bool hasSubmittedForm = false;
   
    for (size_t i = 0; i < formElements.size(); i++) {	
        if (!formElements[i]->isFormControlElement()){
            continue;
			}
        HTMLFormControlElement* formElement = static_cast<HTMLFormControlElement*>(formElements[i]);
		if (!formElement->hasTagName(HTMLNames::inputTag)){
                       
		        LOG(INFO)<<"FP: !formElement->hasTagName(HTMLNames::inputTag)";
		        if (formElement->hasTagName(HTMLNames::buttonTag)){
			          formElement->focus();
			          formElement->click();
			          hasSubmittedForm = true;
			          LOG(INFO)<<"FP:BUTTON click GENERATE";
				  break;
			}
			continue;
		}
		if(!firstSubmitButton && formElement->isSuccessfulSubmitButton()){
			formElement->focus();
			formElement->click();
			hasSubmittedForm = true;
			LOG(INFO)<<"FP:SUBMIT BUTTON click GENERATE";
			break;
		}
    }

    if (!hasSubmittedForm){
        form->submit();
        LOG(INFO)<<"FP:Form Submit Called";
    }
        
}


#endif
bool WebViewImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    ASSERT((event.type == WebInputEvent::RawKeyDown)
        || (event.type == WebInputEvent::KeyDown)
        || (event.type == WebInputEvent::KeyUp));


#if defined(SBROWSER_DPAD_CENTER_KEY_HANDLING)
    if (AKEYCODE_DPAD_CENTER == event.nativeKeyCode && isFocusedNodeTextInput()) {
        if (WebInputEvent::RawKeyDown == event.type && m_client)
            m_client->performMouseClick(0, 0);
        return true;
    }
#endif

    // Halt an in-progress fling on a key event.
    if (m_gestureAnimation) {
        m_gestureAnimation.clear();
        if (m_layerTreeView)
            m_layerTreeView->didStopFlinging();
    }

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.
    // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
    // Webkit. A keyDown event is typically associated with a keyPress(char)
    // event and a keyUp event. We reset this flag here as this is a new keyDown
    // event.
    m_suppressNextKeypressEvent = false;

    // If there is a select popup, it should be the one processing the event,
    // not the page.
    if (m_selectPopup)
        return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
#if ENABLE(PAGE_POPUP)
    if (m_pagePopup) {
        m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
        // We need to ignore the next Char event after this otherwise pressing
        // enter when selecting an item in the popup will go to the page.
        if (WebInputEvent::RawKeyDown == event.type)
            m_suppressNextKeypressEvent = true;
        return true;
    }
#endif

    // Give Autocomplete a chance to consume the key events it is interested in.
    if (autocompleteHandleKeyEvent(event))
        return true;

    RefPtr<Frame> frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    EventHandler* handler = frame->eventHandler();
    if (!handler)
        return keyEventDefault(event);

#if !OS(DARWIN)
    const WebInputEvent::Type contextMenuTriggeringEventType =
#if OS(WINDOWS)
        WebInputEvent::KeyUp;
#elif OS(UNIX)
        WebInputEvent::RawKeyDown;
#endif

    bool isUnmodifiedMenuKey = !(event.modifiers & WebInputEvent::InputModifiers) && event.windowsKeyCode == VKEY_APPS;
    bool isShiftF10 = event.modifiers == WebInputEvent::ShiftKey && event.windowsKeyCode == VKEY_F10;
    if ((isUnmodifiedMenuKey || isShiftF10) && event.type == contextMenuTriggeringEventType) {
        sendContextMenuEvent(event);
        return true;
    }
#endif // !OS(DARWIN)

    PlatformKeyboardEventBuilder evt(event);

    if (handler->keyEvent(evt)) {
        if (WebInputEvent::RawKeyDown == event.type) {
            // Suppress the next keypress event unless the focused node is a plug-in node.
            // (Flash needs these keypress events to handle non-US keyboards.)
            Node* node = focusedWebCoreNode();
            if (!node || !node->renderer() || !node->renderer()->isEmbeddedObject())
                m_suppressNextKeypressEvent = true;
        }
        return true;
    }

    return keyEventDefault(event);
}

bool WebViewImpl::autocompleteHandleKeyEvent(const WebKeyboardEvent& event)
{
    if (!m_autofillPopupShowing
        // Home and End should be left to the text field to process.
        || event.windowsKeyCode == VKEY_HOME
        || event.windowsKeyCode == VKEY_END)
      return false;

    // Pressing delete triggers the removal of the selected suggestion from the DB.
    if (event.windowsKeyCode == VKEY_DELETE
        && m_autofillPopup->selectedIndex() != -1) {
        Node* node = focusedWebCoreNode();
        if (!node || (node->nodeType() != Node::ELEMENT_NODE)) {
            ASSERT_NOT_REACHED();
            return false;
        }
        Element* element = toElement(node);
        if (!element->hasTagName(HTMLNames::inputTag)) {
            ASSERT_NOT_REACHED();
            return false;
        }

        int selectedIndex = m_autofillPopup->selectedIndex();

        if (!m_autofillPopupClient->canRemoveSuggestionAtIndex(selectedIndex))
            return false;

        WebString name = WebInputElement(element->toInputElement()).nameForAutofill();
        WebString value = m_autofillPopupClient->itemText(selectedIndex);
        m_autofillClient->removeAutocompleteSuggestion(name, value);
        // Update the entries in the currently showing popup to reflect the
        // deletion.
        m_autofillPopupClient->removeSuggestionAtIndex(selectedIndex);
        refreshAutofillPopup();
        return false;
    }

    if (!m_autofillPopup->isInterestedInEventForKey(event.windowsKeyCode))
        return false;

    if (m_autofillPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event))) {
        // We need to ignore the next Char event after this otherwise pressing
        // enter when selecting an item in the menu will go to the page.
        if (WebInputEvent::RawKeyDown == event.type)
            m_suppressNextKeypressEvent = true;
        return true;
    }

    return false;
}

bool WebViewImpl::handleCharEvent(const WebKeyboardEvent& event)
{
    ASSERT(event.type == WebInputEvent::Char);

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
    // handled by Webkit. A keyDown event is typically associated with a
    // keyPress(char) event and a keyUp event. We reset this flag here as it
    // only applies to the current keyPress event.
    bool suppress = m_suppressNextKeypressEvent;
    m_suppressNextKeypressEvent = false;

    // If there is a select popup, it should be the one processing the event,
    // not the page.
    if (m_selectPopup)
        return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
#if ENABLE(PAGE_POPUP)
    if (m_pagePopup)
        return m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
#endif

    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return suppress;

    EventHandler* handler = frame->eventHandler();
    if (!handler)
        return suppress || keyEventDefault(event);

    PlatformKeyboardEventBuilder evt(event);
    if (!evt.isCharacterKey())
        return true;

    // Accesskeys are triggered by char events and can't be suppressed.
    if (handler->handleAccessKey(evt))
        return true;

    // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
    // the eventHandler::keyEvent. We mimic this behavior on all platforms since
    // for now we are converting other platform's key events to windows key
    // events.
    if (evt.isSystemKey())
        return false;

    if (!suppress && !handler->keyEvent(evt))
        return keyEventDefault(event);

    return true;
}

WebRect WebViewImpl::computeBlockBounds(const WebRect& rect, AutoZoomType zoomType)
{
    if (!mainFrameImpl())
        return WebRect();

    // Use the rect-based hit test to find the node.
    IntPoint point = mainFrameImpl()->frameView()->windowToContents(IntPoint(rect.x, rect.y));
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent
            | ((zoomType == FindInPage) ? HitTestRequest::IgnoreClipping : 0);
    HitTestResult result = mainFrameImpl()->frame()->eventHandler()->hitTestResultAtPoint(point, hitType, IntSize(rect.width, rect.height));

    Node* node = result.innerNonSharedNode();
    if (!node)
        return WebRect();

    // Find the block type node based on the hit node.
    while (node && (!node->renderer() || node->renderer()->isInline()))
        node = node->parentNode();

    // Return the bounding box in the window coordinate system.
    if (node) {
        IntRect rect = node->Node::pixelSnappedBoundingBox();
        Frame* frame = node->document()->frame();
        return frame->view()->contentsToWindow(rect);
    }
    return WebRect();
}

WebRect WebViewImpl::widenRectWithinPageBounds(const WebRect& source, int targetMargin, int minimumMargin)
{
    WebSize maxSize;
    if (mainFrame())
        maxSize = mainFrame()->contentsSize();
    IntSize scrollOffset;
    if (mainFrame())
        scrollOffset = mainFrame()->scrollOffset();
    int leftMargin = targetMargin;
    int rightMargin = targetMargin;

    const int absoluteSourceX = source.x + scrollOffset.width();
    if (leftMargin > absoluteSourceX) {
        leftMargin = absoluteSourceX;
        rightMargin = max(leftMargin, minimumMargin);
    }

    const int maximumRightMargin = maxSize.width - (source.width + absoluteSourceX);
    if (rightMargin > maximumRightMargin) {
        rightMargin = maximumRightMargin;
        leftMargin = min(leftMargin, max(rightMargin, minimumMargin));
    }

    const int newWidth = source.width + leftMargin + rightMargin;
    const int newX = source.x - leftMargin;

    ASSERT(newWidth >= 0);
    ASSERT(scrollOffset.width() + newX + newWidth <= maxSize.width);

    return WebRect(newX, source.y, newWidth, source.height);
}

void WebViewImpl::computeScaleAndScrollForHitRect(const WebRect& hitRect, AutoZoomType zoomType, float& scale, WebPoint& scroll, bool& isAnchor)
{
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    scale = pageScaleFactor();
    scroll.x = scroll.y = 0;
    WebRect targetRect = hitRect;
    // Padding only depends on page scale when triggered by manually tapping
    int padding = (zoomType == DoubleTap) ? touchPointPadding : nonUserInitiatedPointPadding;
    if (targetRect.isEmpty())
        targetRect.width = targetRect.height = padding;
    WebRect rect = computeBlockBounds(targetRect, zoomType);
    if (zoomType == FindInPage && rect.isEmpty()) {
        // Keep current scale (no need to scroll as x,y will normally already
        // be visible). FIXME: Revisit this if it isn't always true.
        return;
    }
	
//SBROWSER_DOUBLETAP_PATENT_CHANGE >>
#if defined (SBROWSER_DOUBLETAP_PATENT_CHANGE)
	bool over_view_mode  = true;
	bool enable_doubleTap_patent = false;
	if(command_line.HasSwitch(cc::switches::kEnableDoubleTapPatent)) {
		std::string double_tappatent_str(command_line.GetSwitchValueASCII(cc::switches::kEnableDoubleTapPatent));
		enable_doubleTap_patent = (double_tappatent_str == "1");
    }
		  
    if (enable_doubleTap_patent) {
        // Somtimes scale is not set to minimum scale when do doubleTap for overview
        // So adding tolerance(0.05)
        if (scale > 0 && scale > m_minimumPageScaleFactor + 0.05) {
            over_view_mode = false;
        }
    }
#endif	
//SBROWSER_DOUBLETAP_PATENT_CHANGE <<
	bool scaleUnchanged = true;

    if (!rect.isEmpty()) {
        // Pages should be as legible as on desktop when at dpi scale, so no
        // need to zoom in further when automatically determining zoom level
        // (after double tap, find in page, etc), though the user should still
        // be allowed to manually pinch zoom in further if they desire.
        const float defaultScaleWhenAlreadyLegible = m_minimumPageScaleFactor * doubleTapZoomAlreadyLegibleRatio;
        // SAMSUNG: Block zoom scale >>
        // There was a complain from the testing team that block zoomed scale is small.
        // WAS: float legibleScale = 1;
        float legibleScale = 1.2;
        bool isTablet = command_line.HasSwitch(switches::kTabletUI);
        if(isTablet)
            legibleScale = 1.8;
        // SAMSUNG: Block zoom scale <<
        if (page() && page()->settings())
            legibleScale *= page()->settings()->textAutosizingFontScaleFactor();
        if (legibleScale < defaultScaleWhenAlreadyLegible)
            legibleScale = (scale == m_minimumPageScaleFactor) ? defaultScaleWhenAlreadyLegible : m_minimumPageScaleFactor;

        float defaultMargin = doubleTapZoomContentDefaultMargin;
        float minimumMargin = doubleTapZoomContentMinimumMargin;
        // We want the margins to have the same physical size, which means we
        // need to express them in post-scale size. To do that we'd need to know
        // the scale we're scaling to, but that depends on the margins. Instead
        // we express them as a fraction of the target rectangle: this will be
        // correct if we end up fully zooming to it, and won't matter if we
        // don't.
        rect = widenRectWithinPageBounds(rect,
                static_cast<int>(defaultMargin * rect.width / m_size.width),
                static_cast<int>(minimumMargin * rect.width / m_size.width));
        // Fit block to screen, respecting limits.
        scale = static_cast<float>(m_size.width) / rect.width;
        scale = min(scale, legibleScale);
        scale = clampPageScaleFactorToLimits(scale);
	scaleUnchanged = fabs(pageScaleFactor() - scale) < minScaleDifference;

//SBROWSER_DOUBLETAP_PATENT_CHANGE >>
#if defined (SBROWSER_DOUBLETAP_PATENT_CHANGE)
	if( enable_doubleTap_patent && over_view_mode && scaleUnchanged)
		scale = 1.4;  //force to higher zoom scale
#endif
//SBROWSER_DOUBLETAP_PATENT_CHANGE <<
    }
    bool stillAtPreviousDoubleTapScale = (pageScaleFactor() == m_doubleTapZoomPageScaleFactor
        && m_doubleTapZoomPageScaleFactor != m_minimumPageScaleFactor)
        || m_doubleTapZoomPending;
	
//SBROWSER_DOUBLETAP_PATENT_CHANGE >>
 #if !defined(SBROWSER_DOUBLETAP_PATENT_CHANGE)
    if (zoomType == DoubleTap && (rect.isEmpty() || scaleUnchanged || stillAtPreviousDoubleTapScale)) {
        // Zoom out to minimum scale.
       	scale = m_minimumPageScaleFactor;
        scroll = WebPoint(hitRect.x, hitRect.y);
        isAnchor = true;
    }
#else
	if (!enable_doubleTap_patent && (zoomType == DoubleTap && (rect.isEmpty() || scaleUnchanged || stillAtPreviousDoubleTapScale))) {
	   	scale = m_minimumPageScaleFactor; // Zoom out to minimum scale.
        scroll = WebPoint(hitRect.x, hitRect.y);
        isAnchor = true;
	}	
	else if (enable_doubleTap_patent && (zoomType == DoubleTap && (rect.isEmpty() || scaleUnchanged || stillAtPreviousDoubleTapScale || (over_view_mode == false)))) {
		if(!over_view_mode)
			scale = m_minimumPageScaleFactor; // Zoom out to minimum scale.
        scroll = WebPoint(hitRect.x, hitRect.y);
        isAnchor = true;
    }
#endif
//SBROWSER_DOUBLETAP_PATENT_CHANGE <<
	else {
        // FIXME: If this is being called for auto zoom during find in page,
        // then if the user manually zooms in it'd be nice to preserve the
        // relative increase in zoom they caused (if they zoom out then it's ok
        // to zoom them back in again). This isn't compatible with our current
        // double-tap zoom strategy (fitting the containing block to the screen)
        // though.

        float screenWidth = m_size.width / scale;
        float screenHeight = m_size.height / scale;

		// Scroll to vertically align the block.
		if (rect.height < screenHeight) {
//SBROWSER_DOUBLETAP_PATENT_CHANGE >>
#if defined (SBROWSER_DOUBLETAP_PATENT_CHANGE)		
			if(enable_doubleTap_patent && scale) 
				rect.y = rect.y - (rect.y * pageScaleFactor()/scale); //Maintain Vertical Scroll Offset of Block
			else
#endif			
				// Vertically center short blocks.
			rect.y -= 0.5 * (screenHeight - rect.height);
		}
		else {
			// Ensure position we're zooming to (+ padding) isn't off the bottom of
			// the screen.
			rect.y = max<float>(rect.y, hitRect.y + padding - screenHeight);
		} // Otherwise top align the block.

        // Do the same thing for horizontal alignment.
        if (rect.width < screenWidth)
            rect.x -= 0.01 * (screenWidth - rect.width);
        else
            rect.x = max<float>(rect.x, hitRect.x + padding - screenWidth);
        scroll.x = rect.x;
        scroll.y = rect.y;
        isAnchor = false;

    }

    scale = clampPageScaleFactorToLimits(scale);
    scroll = mainFrameImpl()->frameView()->windowToContents(scroll);
    if (!isAnchor)
        scroll = clampOffsetAtScale(scroll, scale);
}

static bool invokesHandCursor(Node* node, bool shiftKey, Frame* frame)
{
    if (!node || !node->renderer())
        return false;

    ECursor cursor = node->renderer()->style()->cursor();
#if defined(SBROWSER_CQ_MPSG100012310)
  	bool isLink = false;

	if (node->isLink() || (node->isElementNode() && node->hasTagName(HTMLNames::aTag))){
		isLink = true;	
	}
  
    return cursor == CURSOR_POINTER
        || (cursor == CURSOR_AUTO && frame->eventHandler()->useHandCursor(node,isLink, shiftKey));
#else
    return cursor == CURSOR_POINTER
        || (cursor == CURSOR_AUTO && frame->eventHandler()->useHandCursor(node, node->isLink(), shiftKey));
#endif
}

Node* WebViewImpl::bestTapNode(const PlatformGestureEvent& tapEvent, bool Hover_light)
{
    if (!m_page || !m_page->mainFrame())
        return 0;

    Node* bestTouchNode = 0;

    IntPoint touchEventLocation(tapEvent.position());
    m_page->mainFrame()->eventHandler()->adjustGesturePosition(tapEvent, touchEventLocation);

    IntPoint hitTestPoint = m_page->mainFrame()->view()->windowToContents(touchEventLocation);

    if(Hover_light){
    	 HitTestResult result = m_page->mainFrame()->eventHandler()->HoverhitTestResultAtPointCustomized(hitTestPoint.x(), hitTestPoint.y(),4);
    	 bestTouchNode = result.targetNode();
    } else {
    	 HitTestResult result = m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(hitTestPoint, HitTestRequest::TouchEvent | HitTestRequest::DisallowShadowContent);
    	 bestTouchNode = result.targetNode();
    }

    // Make sure our highlight candidate uses a hand cursor as a heuristic to
    // choose appropriate targets.
    while (bestTouchNode && !invokesHandCursor(bestTouchNode, false, m_page->mainFrame()))
        bestTouchNode = bestTouchNode->parentNode();

    // We should pick the largest enclosing node with hand cursor set.
    while (bestTouchNode && bestTouchNode->parentNode() && invokesHandCursor(bestTouchNode->parentNode(), false, m_page->mainFrame()))
        bestTouchNode = bestTouchNode->parentNode();

    return bestTouchNode;
}

#if defined(SBROWSER_CONTENT_HIGHLIGHT)
void WebViewImpl::enableContentHighlight(WebCore::Node* touchNode){
	// Always clear any existing highlight when this is invoked, even if we don't get a new target to highlight.
		m_linkHighlight.clear();
	
		if (!touchNode || !touchNode->renderer() || !touchNode->renderer()->enclosingLayer())
			return;
	
		Color highlightColor = touchNode->renderer()->style()->tapHighlightColor();
		// Safari documentation for -webkit-tap-highlight-color says if the specified color has 0 alpha,
		// then tap highlighting is disabled.
		// http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
		if (!highlightColor.alpha())
			return;
	
		m_linkHighlight = LinkHighlight::create(touchNode, this);

}

#endif


#if defined(SBROWSER_HOVER_HIGHLIGHT)
void WebViewImpl::enableHoverHighlight(const PlatformGestureEvent& tapEvent,bool hover_highlight)
{
	Node* touchNode = bestTapNode(tapEvent,hover_highlight);

	if(touchNode && m_prevHoverNode && m_prevHoverNode == touchNode)
	{
		m_prevHoverNode = 0;
		return;
	}

	// Always clear any existing highlight when this is invoked, even if we don't get a new target to highlight.
	m_linkHighlight.clear();

	if (!touchNode || !touchNode->renderer() || !touchNode->renderer()->enclosingLayer())
	{
		m_prevHoverNode = 0;
		return;
	}

	Color highlightColor = touchNode->renderer()->style()->tapHighlightColor();
	// Safari documentation for -webkit-tap-highlight-color says if the specified color has 0 alpha,
	// then tap highlighting is disabled.
	// http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
	if (!highlightColor.alpha())
	{
		m_prevHoverNode = 0;
		return;
	}

	m_prevHoverNode = touchNode;

	m_linkHighlight = LinkHighlight::create(touchNode, this
#if defined(SBROWSER_HOVER_HIGHLIGHT)
					,true
#endif
	);
}
#endif

void WebViewImpl::enableTapHighlight(const PlatformGestureEvent& tapEvent)
{
    // Always clear any existing highlight when this is invoked, even if we don't get a new target to highlight.
    m_linkHighlight.clear();

    Node* touchNode = bestTapNode(tapEvent);

    if (!touchNode || !touchNode->renderer() || !touchNode->renderer()->enclosingLayer())
        return;

    Color highlightColor = touchNode->renderer()->style()->tapHighlightColor();
    // Safari documentation for -webkit-tap-highlight-color says if the specified color has 0 alpha,
    // then tap highlighting is disabled.
    // http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
    if (!highlightColor.alpha())
        return;

    m_linkHighlight = LinkHighlight::create(touchNode, this);
}

void WebViewImpl::animateZoomAroundPoint(const IntPoint& point, AutoZoomType zoomType)
{
    if (!mainFrameImpl())
        return;

    float scale;
    WebPoint scroll;
    bool isAnchor;
    WebPoint webPoint = point;
    computeScaleAndScrollForHitRect(WebRect(webPoint.x, webPoint.y, 0, 0), zoomType, scale, scroll, isAnchor);

    bool isDoubleTap = (zoomType == DoubleTap);
    double durationInSeconds = isDoubleTap ? doubleTapZoomAnimationDurationInSeconds : 0;
    bool isAnimating = startPageScaleAnimation(scroll, isAnchor, scale, durationInSeconds);

    if (isDoubleTap && isAnimating) {
        m_doubleTapZoomPageScaleFactor = scale;
        m_doubleTapZoomPending = true;
    }
}

void WebViewImpl::zoomToFindInPageRect(const WebRect& rect)
{
    animateZoomAroundPoint(IntRect(rect).center(), FindInPage);
}

void WebViewImpl::numberOfWheelEventHandlersChanged(unsigned numberOfWheelHandlers)
{
    if (m_client)
        m_client->numberOfWheelEventHandlersChanged(numberOfWheelHandlers);
}

void WebViewImpl::hasTouchEventHandlers(bool hasTouchHandlers)
{
    if (m_client)
        m_client->hasTouchEventHandlers(hasTouchHandlers);
}

bool WebViewImpl::hasTouchEventHandlersAt(const WebPoint& point)
{
    // FIXME: Implement this. Note that the point must be divided by pageScaleFactor.
    return true;
}

#if !OS(DARWIN)
// Mac has no way to open a context menu based on a keyboard event.
bool WebViewImpl::sendContextMenuEvent(const WebKeyboardEvent& event)
{
    // The contextMenuController() holds onto the last context menu that was
    // popped up on the page until a new one is created. We need to clear
    // this menu before propagating the event through the DOM so that we can
    // detect if we create a new menu for this event, since we won't create
    // a new menu if the DOM swallows the event and the defaultEventHandler does
    // not run.
    page()->contextMenuController()->clearContextMenu();

    m_contextMenuAllowed = true;
    Frame* focusedFrame = page()->focusController()->focusedOrMainFrame();
    bool handled = focusedFrame->eventHandler()->sendContextMenuEventForKey();
    m_contextMenuAllowed = false;
    return handled;
}
#endif

bool WebViewImpl::keyEventDefault(const WebKeyboardEvent& event)
{
#if defined(SBROWSER_ARROWKEY_FOCUS_NAVIGATION)
    FocusDirection focusDirection = FocusDirectionNone;
#endif
    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    switch (event.type) {
    case WebInputEvent::Char:
        if (event.windowsKeyCode == VKEY_SPACE) {
            int keyCode = ((event.modifiers & WebInputEvent::ShiftKey) ? VKEY_PRIOR : VKEY_NEXT);
            return scrollViewWithKeyboard(keyCode, event.modifiers);
        }
        break;
    case WebInputEvent::RawKeyDown:
        if (event.modifiers == WebInputEvent::ControlKey) {
            switch (event.windowsKeyCode) {
#if !OS(DARWIN)
            case 'A':
                focusedFrame()->executeCommand(WebString::fromUTF8("SelectAll"));
                return true;
            case VKEY_INSERT:
            case 'C':
                focusedFrame()->executeCommand(WebString::fromUTF8("Copy"));
                return true;
#endif
            // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
            // key combinations which affect scrolling. Safari is buggy in the
            // sense that it scrolls the page for all Ctrl+scrolling key
            // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
            case VKEY_HOME:
            case VKEY_END:
                break;
            default:
                return false;
            }
        }
#if defined(SBROWSER_ARROWKEY_FOCUS_NAVIGATION)
        // SBrowser Requirment Change [Arrow keys should do focus movement]
        if (!(event.modifiers & WebMouseEvent::ControlKey) && !(event.modifiers & WebInputEvent::ShiftKey) 
                                && mapKeyCodeFocusDirection(event.windowsKeyCode, &focusDirection)) {
            PlatformKeyboardEventBuilder evt(event);
            RefPtr<KeyboardEvent> webkitEvent = KeyboardEvent::create(evt, 0);
            if (page() && page()->focusController())
			    return page()->focusController()->advanceFocus(focusDirection, webkitEvent.get(), false);
        }                   
        else if (!event.isSystemKey && !(event.modifiers & WebInputEvent::ShiftKey))
#else
        if (!event.isSystemKey && !(event.modifiers & WebInputEvent::ShiftKey))
#endif
            return scrollViewWithKeyboard(event.windowsKeyCode, event.modifiers);
        break;
    default:
        break;
    }
    return false;
}

#if defined(SBROWSER_ARROWKEY_FOCUS_NAVIGATION)
bool WebViewImpl::mapKeyCodeFocusDirection(int keyCode,
                                      WebCore::FocusDirection* focusDirection)
{
    switch (keyCode) {
    case VKEY_LEFT:
        *focusDirection = FocusDirectionLeft;
        break;
    case VKEY_RIGHT:
        *focusDirection = FocusDirectionRight;
        break;
    case VKEY_UP:
        *focusDirection = FocusDirectionUp;
        break;
    case VKEY_DOWN:
        *focusDirection = FocusDirectionDown;
        break;
    default:
        return false;
    }
    return true;
}
#endif

bool WebViewImpl::scrollViewWithKeyboard(int keyCode, int modifiers)
{
    ScrollDirection scrollDirection;
    ScrollGranularity scrollGranularity;
#if OS(DARWIN)
    // Control-Up/Down should be PageUp/Down on Mac.
    if (modifiers & WebMouseEvent::ControlKey) {
      if (keyCode == VKEY_UP)
        keyCode = VKEY_PRIOR;
      else if (keyCode == VKEY_DOWN)
        keyCode = VKEY_NEXT;
    }
#endif
    if (!mapKeyCodeForScroll(keyCode, &scrollDirection, &scrollGranularity))
        return false;
    return propagateScroll(scrollDirection, scrollGranularity);
}

bool WebViewImpl::mapKeyCodeForScroll(int keyCode,
                                      WebCore::ScrollDirection* scrollDirection,
                                      WebCore::ScrollGranularity* scrollGranularity)
{
    switch (keyCode) {
    case VKEY_LEFT:
        *scrollDirection = ScrollLeft;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_RIGHT:
        *scrollDirection = ScrollRight;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_UP:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_DOWN:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_HOME:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_END:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_PRIOR:  // page up
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByPage;
        break;
    case VKEY_NEXT:  // page down
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByPage;
        break;
    default:
        return false;
    }

    return true;
}

#if defined(SBROWSER_FORM_NAVIGATION)

void WebViewImpl::scrollSelectBoxToView()
{
	WebCore::Node* node = focusedWebCoreNode();
	WebCore::Frame* frame = mainFrameImpl()->frame();
	if (!frame || !node)
        return;

    Element* elementNode = 0;
    // If not an Element, find a visible predecessor
    // Element to scroll into view.
    if (!node->isElementNode()) {
        HTMLElement* body = frame->document()->body();
        do {
            if (node == body)
                return;
            node = node->parentNode();
        } while (node && !node->isElementNode() && !isVisible(node));
    }

    // Couldn't find a visible predecessor.
    if (!node)
        return;
	elementNode = static_cast<Element*>(node);

	elementNode->scrollIntoViewIfNeeded(true);

}
bool WebViewImpl::isFocusedNodeSelectInput()
{
    Node* node = focusedWebCoreNode();
	return isSelectInput(node);

}

bool WebViewImpl::isFocusedNodeTextInput()
{
    Node* node = focusedWebCoreNode();
	return isTextInput(node);

}

bool WebViewImpl::isVisible(WebCore::Node* node)
{
    // start off an element
    Element* element = 0;
    if (node->isElementNode())
        element = static_cast<Element*>(node);
    else
        element = node->parentElement();
    // check renderer
    if (!element->renderer()) {
        return false;
    }
    // check size
    if (element->offsetHeight() == 0 || element->offsetWidth() == 0) {
        return false;
    }
    // check style
    Node* body = mainFrameImpl()->frame()->document()->body();
    Node* currentNode = element;
    while (currentNode && currentNode != body) {
        RenderStyle* style = currentNode->computedStyle();
        if (style &&
                (style->display() == WebCore::NONE || style->visibility() == WebCore::HIDDEN)) {
            return false;
        }
        currentNode = currentNode->parentNode();
    }
    return true;
}


void WebViewImpl::scrollNodeIntoView(WebCore::Frame* frame, WebCore::Node* node)
{
    if (!frame || !node)
        return;

    Element* elementNode = 0;

    // If not an Element, find a visible predecessor
    // Element to scroll into view.
    if (!node->isElementNode()) {
        HTMLElement* body = frame->document()->body();
        do {
            if (node == body)
                return;
            node = node->parentNode();
        } while (node && !node->isElementNode() && !isVisible(node));
    }

    // Couldn't find a visible predecessor.
    if (!node)
        return;

	elementNode = static_cast<Element*>(node);
// fixing for cq MAPSG100012503 since due to this focus was not going to search node
//	if(focusedWebCoreFrame() != m_page->mainFrame())
	//	elementNode->scrollIntoCenter(false);
//	else 
	elementNode->scrollIntoCenter(true);

	//IntRect bounds = getNodeBounds(node);
    
	//m_client->ScrollTo(bounds.x(), bounds.y());
}

// Returns true if the node is a textfield, textarea, or contentEditable
bool WebViewImpl::isTextInput(WebCore::Node* node)
{
    if (!node)
        return false;
    if (isContentEditable(node))
        return true;
    WebCore::RenderObject* renderer = node->renderer();
    return renderer && (renderer->isTextField() || renderer->isTextArea());
}

bool WebViewImpl::isDateTime(WebCore::Node *node)
{
    if(node && node->hasTagName(HTMLNames::inputTag))
    {
        WebCore::Element* ele = static_cast<Element*>(node);
	const AtomicString &typestr = ele->getAttribute(HTMLNames::typeAttr);		    
	//const WTF::String& typestring = typestr.string();
	if((typestr == "date") || (typestr == "datetime") || (typestr == "datetime-local") || (typestr == "time"))
            return true;	
    }
    return false;
}

bool WebViewImpl::isContentEditable(WebCore::Node* node)
{
    if (!node)
        return false;
    return node->isContentEditable();
}

bool WebViewImpl::isFormNavTextInput(WebCore::Node* node)
{
    if (!node)
        return false;
    if (node->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement *inputElement = static_cast<HTMLInputElement*>(node);
        if (inputElement->isReadOnly())
            return false;
    }
    if (isContentEditable(node))
        return true;

    if(isDateTime(node))	
        return false;

    WebCore::RenderObject* renderer = node->renderer();
    return renderer && (renderer->isTextField() || renderer->isTextArea());
}

// Returns true if the node is a Select element
bool WebViewImpl::isSelectInput(WebCore::Node* node)
{
    if (!node)
        return false;
    WebCore::RenderObject* renderer = node->renderer();
    if (renderer && (renderer->isMenuList() || renderer->isListBox())) {
        return true;
    }
    else
        return false;
}

WebCore::IntRect WebViewImpl::getNodeBounds(WebCore::Node* node){
    if (node) {
        node->document()->updateLayoutIgnorePendingStylesheets();
        IntRect absoluteRect = pixelSnappedIntRect(node->Node::boundingBox());
        return node->document()->view()->contentsToWindow(absoluteRect);
    }
    return WebCore::IntRect();
}

bool WebViewImpl::performClickOnNode(WebCore::Node* node, bool updateReq) {
    if (!node) return false;
    IntRect bounds = getNodeBounds(node);
    if (bounds.x() == 0 && bounds.y() == 0 && bounds.isEmpty()) return false;
   // SLOG(INFO) <<__FUNCTION__<<"bounds ["<<bounds.x() <<"," <<bounds.y() <<","<<bounds.maxX() <<"," <<bounds.maxY() <<"] bounds.center ["<<bounds.center().x()<<","<<bounds.center().y()<<"]" ;
    //fakeMouseClick(bounds.center().x(), bounds.center().y(), node);
    //node->setFocus(true);
    static_cast<Element*>(node)->focus(false);
    Node* focusedNode = focusedWebCoreNode();

    if(focusedNode == node) {
       LOG(INFO) <<__FUNCTION__<<"Form Navigation node focused  after setting focus";
	   focusedNode->dispatchSimulatedClick(0,SendMouseUpDownEvents);
    } else {
       LOG(INFO) <<__FUNCTION__<<"Form Navigation node not focused  after setting focus";
	   return false;
    }
    if(isFormNavTextInput(focusedNode)) {
       WebTextInputInfo info = textInputInfo();
       setEditableSelectionOffsets(info.selectionStart,info.selectionEnd);
    }
    m_client->performMouseClick(bounds.center().x(),bounds.center().y());
    return true;
}


bool WebViewImpl::fakeMouseClick(int x, int y, WebCore::Node* node){
	WebCore::IntPoint m_mouseGlobPos = WebCore::IntPoint(x,y);
	//SLOG(INFO) <<__FUNCTION__<<" Starting points [x="<<x <<",y=" <<y <<"]" ;
	//WebCore::IntPoint m_mousePos = WebCore::IntPoint(x - mainFrame()->scrollOffset().width, y - mainFrame()->scrollOffset().height);
	//SLOG(INFO) <<__FUNCTION__<<" After scroll [x="<<m_mousePos.x() <<",y=" <<m_mousePos.y() <<"]" ;
	AtomicString eventType;

    const WebCore::PlatformMouseEvent mouseEvent(m_mouseGlobPos, m_mouseGlobPos,
        WebCore::NoButton, WebCore::PlatformEvent::MouseMoved, 1, false, false, false,
        false, WTF::currentTime());
	bool handled = false;
	
       eventType = eventNames().mousemoveEvent;
	WebMouseEvent mouse_event = WebMouseEvent();	
	mouse_event.button = WebMouseEvent::ButtonNone;
	mouse_event.type = WebInputEvent::MouseMove;
	m_lastMouseDownPoint = WebPoint(x,y);
	
	mouse_event.x = x;
	mouse_event.y = y;

	//Mouse Move
	handled = mainFrameImpl()->frame()->eventHandler()->mouseMoved(
					PlatformMouseEventBuilder(mainFrameImpl()->frameView(), mouse_event));
	//int handledValue = handled?1:0;
	//SLOG(INFO) <<__FUNCTION__<<"MouseMove="<<handledValue ;

	//Mouse Down
    const WebCore::PlatformMouseEvent mouseDown(m_mouseGlobPos, m_mouseGlobPos, WebCore::LeftButton,
        WebCore::PlatformEvent::MousePressed, 1, false, false, false, false,
        WTF::currentTime());
	eventType = eventNames().mousedownEvent;
	handled = mainFrameImpl()->frame()->eventHandler()->handleMousePressEvent(mouseDown);
	//handledValue = handled?1:0;
	//SLOG(INFO) <<__FUNCTION__<<"MouseDown="<<handledValue;

	//MouseUp
    const WebCore::PlatformMouseEvent mouseUp(m_mouseGlobPos, m_mouseGlobPos, WebCore::LeftButton,
            WebCore::PlatformEvent::MouseReleased, 1, false, false, false, false,
            WTF::currentTime());
	eventType = eventNames().mouseupEvent;
	mainFrameImpl()->frame()->eventHandler()->handleMouseReleaseEvent(mouseUp);
	//handledValue = handled?1:0;
	//SLOG(INFO) <<__FUNCTION__<<"MouseUp="<<handledValue ;
	return handled;
}

bool WebViewImpl::moveFocusToNext()
{
    LOG(INFO) <<__FUNCTION__<<"ENTER";
	bool updateReq = false;
    WebCore::Node* focusNode = focusedWebCoreNode();

    if (!focusNode)
	{
	 LOG(INFO) <<__FUNCTION__<<"RETURN FALSE  focusNodeis null ";
     return false;
    }
    if(!isFormNavTextInput(focusNode) && !isSelectInput(focusNode)) {
       LOG(INFO) <<__FUNCTION__<<"RETURN FALSE focused node not a select or text";
       return false;
    }
    WebCore::Node* newNode = nextTextOrSelectNode(focusNode);
    if (!newNode)
	{
		LOG(INFO) <<__FUNCTION__<<"RETURN FALSE next  focused node not a select or text ";
		return false;
    }
	if(isSelectInput(newNode))
	{
	HTMLSelectElement* ele = static_cast<HTMLSelectElement*>(newNode);
	if(ele)
		{
		
		if(ele->length()==0)
			m_client->messageToClosePopup();
		}
	}
    if(isFormNavTextInput(newNode) && isSelectInput(focusNode))
		updateReq = true;
    scrollNodeIntoView(mainFrameImpl()->frame(), newNode);	
    bool handled =  performClickOnNode(newNode,updateReq);
	if(isFormNavTextInput( newNode) && focusedFrame())
		 focusedFrame()->executeCommand(WebString::fromUTF8("MoveToEndOfDocument"));
    LOG(INFO) <<__FUNCTION__<<"RETURN  handled : "<< handled<<"  performClickOnNode ";
    return handled;
}

bool WebViewImpl::moveFocusToPrev()
{
    	bool updateReq = false;
    WebCore::Node* focusNode = focusedWebCoreNode();
    if (!focusNode) return false;
    if(!isFormNavTextInput(focusNode) && !isSelectInput(focusNode)) {
        return false;
    }
    WebCore::Node* newNode = previousTextOrSelectNode(focusNode);
    if (!newNode) return false;
	if(isSelectInput(newNode))
	{
	HTMLSelectElement* ele = static_cast<HTMLSelectElement*>(newNode);
	if(ele)
		{
		if(ele->length()==0)
			m_client->messageToClosePopup();
		}
	}
    if(isFormNavTextInput(newNode) && isSelectInput(focusNode))
		updateReq = true;
    scrollNodeIntoView(mainFrameImpl()->frame(), newNode);	
    bool handled = performClickOnNode(newNode,updateReq);
	if(isFormNavTextInput( newNode) && focusedFrame())
		 focusedFrame()->executeCommand(WebString::fromUTF8("MoveToEndOfDocument"));
    return handled;
}

WebCore::Node* WebViewImpl::nextDocumentFocusableNode(WebCore::Node *frameOwner){
	if (NULL == frameOwner) {
	     return NULL;
	}

	HTMLFrameOwnerElement *frameOwnerElement = static_cast<HTMLFrameOwnerElement *>(frameOwner);
	Document *ownerDocument = frameOwnerElement->contentDocument();
	if (NULL == ownerDocument) {
	    return NULL;
	}

	Node *docBody = ownerDocument->body();
	if (isContentEditable(docBody)) {
		return docBody;
	}
	
	Node *nextNode = docBody;
	while(nextNode) {
		nextNode = NodeTraversal::next(nextNode);
		if (nextNode && (/* nextNode->isFrameOwnerElement() || */ nextNode->hasTagName(HTMLNames::iframeTag) ||
	     	nextNode->hasTagName(HTMLNames::frameTag))) {
	     	LOG(INFO) <<__FUNCTION__<<"  We got a frameOwner Element";
		    HTMLFrameOwnerElement *nestedFrameOwner = static_cast<HTMLFrameOwnerElement *>(nextNode);
		    if (!nestedFrameOwner->contentFrame()) {
			     LOG(INFO) <<__FUNCTION__<<"  frame doesn't have source and hence skipped";
            	 continue;
		    }

		    nextNode = nextDocumentFocusableNode(nestedFrameOwner);
		    if (NULL == nextNode) {
		        nextNode = static_cast<Node *>(nestedFrameOwner);
		        continue;
		    }
		}
		
		if(nextNode && (nextNode->isFocusable()) && (isFormNavTextInput(nextNode) || isSelectInput(nextNode))) {
		   LOG(INFO) <<__FUNCTION__<<" Found focusable node from nested";
           break;
		}
	}

	return nextNode;
}



WebCore::Node* WebViewImpl::nextTextOrSelectNode(WebCore::Node* node){
    Document* document = node->document();
    if (NULL == document) {
        LOG(INFO) <<__FUNCTION__<<"RETURN NULL   : !document";
        return NULL;
    }

    Node *nextNode = node;
    while (nextNode ) {
        nextNode = NodeTraversal::next(nextNode);
        if (nextNode && (/* nextNode->isFrameOwnerElement() || */nextNode->hasTagName(HTMLNames::iframeTag)  ||
            nextNode->hasTagName(HTMLNames::frameTag))) {
            HTMLFrameOwnerElement *frameOwner = static_cast<HTMLFrameOwnerElement *>(nextNode);
            if (!frameOwner->contentFrame()) {
                LOG(INFO) <<__FUNCTION__<<"CONTINUE:  frame doesn't have source and hence skipped";
                continue;
            }
             
            nextNode = nextDocumentFocusableNode(nextNode);
            if (NULL == nextNode) {
                nextNode = static_cast<Node *>(frameOwner);
                LOG(INFO) <<__FUNCTION__<<"CONTINUE   Assigned frameOwner  Back : " <<nextNode->nodeName().utf8().data();
                continue;
            }
        }
        
        if (NULL == nextNode) {// Couldn't find anything in the current document scope, try finding in other document scope if present any
            if (document->frame() != mainFrameImpl()->frame()) {
		        LOG(INFO) <<__FUNCTION__<<" We are checking other document scope";
                nextNode = nextTextOrSelectNode(node->document()->ownerElement());
            }
            else {
                //Only One document Scope Didn't find anything
            }
        }

        if (nextNode && (nextNode->isFocusable()) && (isFormNavTextInput(nextNode) || isSelectInput(nextNode))) {
            LOG(INFO) <<__FUNCTION__<<" Broken after finding a focusable Node";
           break;
    }
    }

    if (!nextNode) {
       // SLOG(INFO) <<__FUNCTION__<<" nextNode is NULL";
        return NULL;
    }
    else {
   	   LOG(INFO) <<__FUNCTION__<<"  Final Next Nodename = " << nextNode->nodeName().utf8().data() <<" tagname =  "<< ((Element*) nextNode)->tagName().utf8().data();
    }
    return nextNode;
}

WebCore::Node* WebViewImpl::previousDocumentFocusableNode(WebCore::Node *documentOwner){
    if (NULL == documentOwner) {
         return NULL;
    }

    HTMLFrameOwnerElement *frameOwnerElement = static_cast<HTMLFrameOwnerElement *>(documentOwner);
    Document *ownerDocument = frameOwnerElement->contentFrame()->document();
    if (NULL == ownerDocument) {
        return NULL;
    }

    Node *docLastChild = ownerDocument->lastChild();
    while (docLastChild && docLastChild->hasChildNodes()) {
        docLastChild = docLastChild->lastChild();
    }
    
    if (isContentEditable(docLastChild)) { //In case of empty body Element
        return docLastChild;
    }

    Node *previousNode = docLastChild;
    while (previousNode) {
        previousNode = NodeTraversal::previous(previousNode);
    	if (previousNode && (/* previousNode->isFrameOwnerElement() || */previousNode->hasTagName(HTMLNames::iframeTag) ||
            previousNode->hasTagName(HTMLNames::frameTag))) {
            HTMLFrameOwnerElement *nestedFrameOwner = static_cast<HTMLFrameOwnerElement *>(previousNode);
            if (!nestedFrameOwner->contentFrame()) {
                continue;
            }
            //previousNode = previousDocumentFocusableNode(documentOwner);
            previousNode = previousDocumentFocusableNode(nestedFrameOwner);
		    if (NULL == previousNode) {
		        previousNode = static_cast<Node *>(nestedFrameOwner); 
		        continue;
		    }
    	}

    	if (previousNode && (previousNode->isFocusable()) && (isFormNavTextInput(previousNode) || isSelectInput(previousNode))) {
            break;
    	}
    }

    return previousNode;
}



WebCore::Node* WebViewImpl::previousTextOrSelectNode(WebCore::Node* node){
if(!node)
return NULL;
    Document* document = node->document();
    if (!document) {
        return NULL;
    }

    Node *previousNode = node;
    while (previousNode) {
		previousNode = NodeTraversal::previous(previousNode);
        if (previousNode && (/* previousNode->isFrameOwnerElement() ||*/previousNode->hasTagName(HTMLNames::iframeTag)  ||
            previousNode->hasTagName(HTMLNames::frameTag))) {
            HTMLFrameOwnerElement *frameOwner = static_cast<HTMLFrameOwnerElement *>(previousNode);
            if (!frameOwner->contentFrame()) {
            	continue;
            }
		
            previousNode = previousDocumentFocusableNode(previousNode);
            if (NULL == previousNode) {
                previousNode = static_cast<Node *>(frameOwner);
                continue;
            }
        }

        if (NULL == previousNode) {
            if (document->frame() != mainFrameImpl()->frame()) {
                previousNode = previousTextOrSelectNode(node->document()->ownerElement());
    	    }

            else {
                //We are in Top frame nothing found, standing at first focusable Node.
            }
        }
        
        if (previousNode && (previousNode->isFocusable()) && (isFormNavTextInput(previousNode) || isSelectInput(previousNode))) {
           break;
    }
		
 
   }

    if (!previousNode) {
        return NULL;
    }
 
    return previousNode;
}

int WebViewImpl::getIMEOptions()
{
	
		
	Node* focusedNode = focusedWebCoreNode();
	Node* next;
	Node* prev;
	int action = FORM_INPUT_NONE;
	if(!focusedNode) 
	{
		LOG(INFO) <<__FUNCTION__<<" returning because !focusedNode ";
		return action;
	}

	if(focusedNode)// && focusedNode->hasTagName(HTMLNames::inputTag))
       {
        //WebCore::Element* ele = static_cast<Element*>(focusedNode);
	//const AtomicString &typestr = ele->getAttribute(HTMLNames::nameAttr);		    
	//const WTF::String& typestring = typestr.string();
	//SLOG(INFO) <<__FUNCTION__<<" 1.  Focused node name : "<<typestring.ascii().data() ;	
      }
      else
	{
	     //  SLOG(INFO) <<__FUNCTION__<<" 1. no node/input tag " ;	
	}


	if(!isSelectInput(focusedNode) && !isFormNavTextInput(focusedNode))
	{
		LOG(INFO) <<__FUNCTION__<<" not a Form Navigation element";
		return action;
	}
	
	next = nextTextOrSelectNode(focusedNode);
	if (next)
	{  
	   	if (isFormNavTextInput(next))
			action |= FORM_INPUT_NEXT_TEXT ;
		else
			action |= FORM_INPUT_NEXT_SELECT ;
	 }
	else
	{ 
		//focused node is the  last text or select field
		//action |= FORM_INPUT_GO;
	}

	prev = previousTextOrSelectNode(focusedNode);
	if (prev)
	{  
	   	if (isFormNavTextInput(prev))
			action |= FORM_INPUT_PREV_TEXT ;
		else
			action |= FORM_INPUT_PREV_SELECT ;
	 }
	else
	{ 
		////focused node is the  last text or select field
		//action |= FORM_INPUT_GO;
	}

	LOG(INFO) <<__FUNCTION__<<"WebViewimpl returning privateimeoptions as "<<action;

	return action;

}
#endif
#if defined(SBROWSER_KEYLAG)
//SAMSUNG_CHANGES : Key lagging ++ Change
bool WebViewImpl::isChangeEventListenerRegistered()
{
//LOG(INFO)<<"KEYLAG:isChangeEventListenerRegistered ";
    Node* focusedNode = focusedWebCoreNode();
    bool isEnabled = true;

    if(!focusedNode) 
    {
        return false;
    }


    isEnabled = focusedNode->hasEventListeners(eventNames().changeEvent);

    return isEnabled;
}
//SAMSUNG_CHANGES : Key lagging ++ Change
#endif
void WebViewImpl::hideSelectPopup()
{
    if (m_selectPopup)
        m_selectPopup->hidePopup();
}

bool WebViewImpl::propagateScroll(ScrollDirection scrollDirection,
                                  ScrollGranularity scrollGranularity)
{
    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    bool scrollHandled = frame->eventHandler()->scrollOverflow(scrollDirection, scrollGranularity);
    Frame* currentFrame = frame;
    while (!scrollHandled && currentFrame) {
        scrollHandled = currentFrame->view()->scroll(scrollDirection, scrollGranularity);
        currentFrame = currentFrame->tree()->parent();
    }
    return scrollHandled;
}

void  WebViewImpl::popupOpened(WebCore::PopupContainer* popupContainer)
{
    if (popupContainer->popupType() == WebCore::PopupContainer::Select) {
        ASSERT(!m_selectPopup);
        m_selectPopup = popupContainer;
    }
}

void  WebViewImpl::popupClosed(WebCore::PopupContainer* popupContainer)
{
    if (popupContainer->popupType() == WebCore::PopupContainer::Select) {
        ASSERT(m_selectPopup);
        m_selectPopup = 0;
    }
}

#if ENABLE(PAGE_POPUP)
PagePopup* WebViewImpl::openPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView)
{
    ASSERT(client);
    if (hasOpenedPopup())
        hidePopups();
    ASSERT(!m_pagePopup);

    WebWidget* popupWidget = m_client->createPopupMenu(WebPopupTypePage);
    ASSERT(popupWidget);
    m_pagePopup = static_cast<WebPagePopupImpl*>(popupWidget);
    if (!m_pagePopup->initialize(this, client, originBoundsInRootView)) {
        m_pagePopup->closePopup();
        m_pagePopup = 0;
    }
    return m_pagePopup.get();
}

void WebViewImpl::closePagePopup(PagePopup* popup)
{
    ASSERT(popup);
    WebPagePopupImpl* popupImpl = static_cast<WebPagePopupImpl*>(popup);
    ASSERT(m_pagePopup.get() == popupImpl);
    if (m_pagePopup.get() != popupImpl)
        return;
    m_pagePopup->closePopup();
    m_pagePopup = 0;
}
#endif

void WebViewImpl::hideAutofillPopup()
{
    if (m_autofillPopupShowing) {
        m_autofillPopup->hidePopup();
        m_autofillPopupShowing = false;
    }
}

WebHelperPluginImpl* WebViewImpl::createHelperPlugin(const String& pluginType, const WebDocument& hostDocument)
{
    WebWidget* popupWidget = m_client->createPopupMenu(WebPopupTypeHelperPlugin);
    ASSERT(popupWidget);
    WebHelperPluginImpl* helperPlugin = static_cast<WebHelperPluginImpl*>(popupWidget);

    if (!helperPlugin->initialize(pluginType, hostDocument, this)) {
        helperPlugin->closeHelperPlugin();
        helperPlugin = 0;
    }
    return helperPlugin;
}

Frame* WebViewImpl::focusedWebCoreFrame() const
{
    return m_page ? m_page->focusController()->focusedOrMainFrame() : 0;
}

WebViewImpl* WebViewImpl::fromPage(Page* page)
{
    if (!page)
        return 0;

    ChromeClientImpl* chromeClient = static_cast<ChromeClientImpl*>(page->chrome()->client());
    return static_cast<WebViewImpl*>(chromeClient->webView());
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::close()
{
    if (m_page) {
        // Initiate shutdown for the entire frameset.  This will cause a lot of
        // notifications to be sent.
        if (m_page->mainFrame())
            m_page->mainFrame()->loader()->frameDetached();

        m_page.clear();
    }

    // Should happen after m_page.clear().
    if (m_devToolsAgent)
        m_devToolsAgent.clear();

    // Reset the delegate to prevent notifications being sent as we're being
    // deleted.
    m_client = 0;

    deref();  // Balances ref() acquired in WebView::create
}

void WebViewImpl::willStartLiveResize()
{
    if (mainFrameImpl() && mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->willStartLiveResize();

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->willStartLiveResize();
}

IntSize WebViewImpl::scaledSize(float pageScaleFactor) const
{
    FloatSize scaledSize = IntSize(m_size);
    scaledSize.scale(1 / pageScaleFactor);
    return expandedIntSize(scaledSize);
}

WebSize WebViewImpl::size()
{
    return m_size;
}

#if defined(SBROWSER_HIDE_URLBAR) && defined(SBROWSER_HIDE_URLBAR_WITHOUT_RESIZING)
void WebViewImpl::setContentTopOffset(int contentTopOffset, int controlTopOffset){
	VLOG(0)<< __FUNCTION__<<"  HideUrlBar:  contentTopOffset = " << contentTopOffset << "  controlTopOffset = " << controlTopOffset;
	mainFrameImpl()->frameView()->setContentTopOffset(-controlTopOffset);
}
#endif

void WebViewImpl::resize(const WebSize& newSize)
{
    LOG(INFO)<<"WebViewImpl::resize++";
    if (m_shouldAutoResize || m_size == newSize)
        return;

    FrameView* view = mainFrameImpl()->frameView();
    if (!view)
        return;

    WebSize oldSize = m_size;
    float oldPageScaleFactor = pageScaleFactor();
    int oldContentsWidth = contentsSize().width();

    m_size = newSize;

#if defined(SBROWSER_OVERVIEW_MODE)
    m_isResizeHandled = true;
#endif
    bool shouldAnchorAndRescaleViewport = settings()->viewportEnabled() && oldSize.width && oldContentsWidth;
    ViewportAnchor viewportAnchor(mainFrameImpl()->frame()->eventHandler());
    if (shouldAnchorAndRescaleViewport) {
        viewportAnchor.setAnchor(view->visibleContentRect(),
                                 FloatSize(viewportAnchorXCoord, viewportAnchorYCoord));
    }

    ViewportArguments viewportArguments = mainFrameImpl()->frame()->document()->viewportArguments();
    m_page->chrome()->client()->dispatchViewportPropertiesDidChange(viewportArguments);

    WebDevToolsAgentPrivate* agentPrivate = devToolsAgentPrivate();
    if (agentPrivate)
        agentPrivate->webViewResized(newSize);
    if (!agentPrivate || !agentPrivate->metricsOverridden()) {
        WebFrameImpl* webFrame = mainFrameImpl();
        if (webFrame->frameView())
            webFrame->frameView()->resize(m_size);
    }

    if (settings()->viewportEnabled()) {
        // Relayout immediately to recalculate the minimum scale limit.
        if (view->needsLayout())
            view->layout();

        if (shouldAnchorAndRescaleViewport) {
            float viewportWidthRatio = static_cast<float>(newSize.width) / oldSize.width;
            float contentsWidthRatio = static_cast<float>(contentsSize().width()) / oldContentsWidth;
            float scaleMultiplier = viewportWidthRatio / contentsWidthRatio;

            IntSize viewportSize = view->visibleContentRect().size();
            if (scaleMultiplier != 1) {
                float newPageScaleFactor = oldPageScaleFactor * scaleMultiplier;
                viewportSize.scale(pageScaleFactor() / newPageScaleFactor);
                IntPoint scrollOffsetAtNewScale = viewportAnchor.computeOrigin(viewportSize);
                setPageScaleFactor(newPageScaleFactor, scrollOffsetAtNewScale);
            } else {
                IntPoint scrollOffsetAtNewScale = clampOffsetAtScale(viewportAnchor.computeOrigin(viewportSize), pageScaleFactor());
                updateMainFrameScrollPosition(scrollOffsetAtNewScale, false);
            }
        }
    }

#if defined(SBROWSER_OVERVIEW_MODE)
    m_isResizeHandled = false;
#endif
    sendResizeEventAndRepaint();
#if defined (SBROWSER_SCROLL_INPUTTEXT)
    TextFieldsBoundsChanged(true);
 #endif
    LOG(INFO)<<"WebViewImpl::resize--";

}

void WebViewImpl::willEndLiveResize()
{
    if (mainFrameImpl() && mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->willEndLiveResize();

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->willEndLiveResize();
}

void WebViewImpl::willEnterFullScreen()
{
    if (!m_provisionalFullScreenElement)
        return;

    // Ensure that this element's document is still attached.
    Document* doc = m_provisionalFullScreenElement->document();
    if (doc->frame()) {
        doc->webkitWillEnterFullScreenForElement(m_provisionalFullScreenElement.get());
        m_fullScreenFrame = doc->frame();
    }
    m_provisionalFullScreenElement.clear();
}

void WebViewImpl::didEnterFullScreen()
{
    if (!m_fullScreenFrame)
        return;

    if (Document* doc = m_fullScreenFrame->document()) {
        if (doc->webkitIsFullScreen())
            doc->webkitDidEnterFullScreenForElement(0);
    }
}

void WebViewImpl::willExitFullScreen()
{
    if (!m_fullScreenFrame)
        return;

    if (Document* doc = m_fullScreenFrame->document()) {
        if (doc->webkitIsFullScreen()) {
            // When the client exits from full screen we have to call webkitCancelFullScreen to
            // notify the document. While doing that, suppress notifications back to the client.
            m_isCancelingFullScreen = true;
            doc->webkitCancelFullScreen();
            m_isCancelingFullScreen = false;
            doc->webkitWillExitFullScreenForElement(0);
        }
    }
}

void WebViewImpl::didExitFullScreen()
{
    if (!m_fullScreenFrame)
        return;

    if (Document* doc = m_fullScreenFrame->document()) {
        if (doc->webkitIsFullScreen())
            doc->webkitDidExitFullScreenForElement(0);
    }

    m_fullScreenFrame.clear();
}

#if ENABLE(BATTERY_STATUS)
void WebViewImpl::updateBatteryStatus(const WebBatteryStatus& status)
{
    m_batteryClient->updateBatteryStatus(status);
}
#endif

void WebViewImpl::animate(double monotonicFrameBeginTime)
{
    TRACE_EVENT0("webkit", "WebViewImpl::animate");

    if (!monotonicFrameBeginTime)
        monotonicFrameBeginTime = monotonicallyIncreasingTime();

    // Create synthetic wheel events as necessary for fling.
    if (m_gestureAnimation) {
        if (m_gestureAnimation->animate(monotonicFrameBeginTime))
            scheduleAnimation();
        else {
            m_gestureAnimation.clear();
            if (m_layerTreeView)
                m_layerTreeView->didStopFlinging();

            PlatformGestureEvent endScrollEvent(PlatformEvent::GestureScrollEnd,
                m_positionOnFlingStart, m_globalPositionOnFlingStart, 0, 0, 0,
                false, false, false, false);

            mainFrameImpl()->frame()->eventHandler()->handleGestureScrollEnd(endScrollEvent);
        }
    }

    if (!m_page)
        return;

    PageWidgetDelegate::animate(m_page.get(), monotonicFrameBeginTime);

    if (m_continuousPaintingEnabled) {
        ContinuousPainter::setNeedsDisplayRecursive(m_rootGraphicsLayer, m_pageOverlays.get());
        m_client->scheduleAnimation();
    }
}

void WebViewImpl::layout()
{
    TRACE_EVENT0("webkit", "WebViewImpl::layout");
    PageWidgetDelegate::layout(m_page.get());

    if (m_linkHighlight)
        m_linkHighlight->updateGeometry();
}

#if defined(SBROWSER_CSS_ANIMATION_PERFORMANCE_IMPROVEMENT_3)
void WebViewImpl::updateCompositingLayersIfNeeded()
{
    FrameView* view = page()->mainFrame()->view();
    
    if(!view)
        return;
        
    view->updateCompositingLayerIfNeeded();
}
#endif

void WebViewImpl::enterForceCompositingMode(bool enter)
{
    if (page()->settings()->forceCompositingMode() == enter)
        return;

    TRACE_EVENT1("webkit", "WebViewImpl::enterForceCompositingMode", "enter", enter);
    settingsImpl()->setForceCompositingMode(enter);
    if (enter) {
        if (!m_page)
            return;
        Frame* mainFrame = m_page->mainFrame();
        if (!mainFrame)
            return;
        mainFrame->view()->updateCompositingLayersAfterStyleChange();
    }
}

void WebViewImpl::doPixelReadbackToCanvas(WebCanvas* canvas, const IntRect& rect)
{
    ASSERT(m_layerTreeView);

    SkBitmap target;
    target.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height(), rect.width() * 4);
    target.allocPixels();
    m_layerTreeView->compositeAndReadback(target.getPixels(), rect);
#if (!SK_R32_SHIFT && SK_B32_SHIFT == 16)
    // The compositor readback always gives back pixels in BGRA order, but for
    // example Android's Skia uses RGBA ordering so the red and blue channels
    // need to be swapped.
    uint8_t* pixels = reinterpret_cast<uint8_t*>(target.getPixels());
    for (size_t i = 0; i < target.getSize(); i += 4)
        std::swap(pixels[i], pixels[i + 2]);
#endif
    canvas->writePixels(target, rect.x(), rect.y());
}
#if defined (SBROWSER_SOFTBITMAP_IMPL)
bool WebViewImpl::isSoftwareRenderable() const {
     Document* document = page()->mainFrame()->document();
    /* if (document) {
        RenderView* renderView = document->renderView();
        if (renderView) {
           RenderLayerCompositor* compositor = renderView->compositor(); 
           if(compositor && !compositor->has3DContent())
              return true;
        }
     }*/
     if (document) {
        String url = document->documentURI();
        if (!url.isNull() && !url.isEmpty() && url.contains("m.daum.net")) {
           return false;
        }
     }
     return true;
}

void WebViewImpl::paintSoftBitmapRootImage(WebCanvas* canvas, const WebRect& rect){	
        FrameView* view = page()->mainFrame()->view();
        if (view) {
		PageWidgetDelegate::paint(m_page.get(), pageOverlays(), canvas, rect, isTransparent() ? PageWidgetDelegate::Translucent : PageWidgetDelegate::Opaque);
        }
}

void WebViewImpl::paintSoftBitmap(WebCanvas* canvas, const WebRect& rect){
        FrameView* view = page()->mainFrame()->view();
        if (view) {
		PaintBehavior oldPaintBehavior = view->paintBehavior();
		if (isAcceleratedCompositingActive()) {
		    view->setPaintBehavior(oldPaintBehavior | PaintBehaviorFlattenCompositingLayers);
		}

		PageWidgetDelegate::paint(m_page.get(), pageOverlays(), canvas, rect, isTransparent() ? PageWidgetDelegate::Translucent : PageWidgetDelegate::Opaque);

		if (isAcceleratedCompositingActive()) {
		    view->setPaintBehavior(oldPaintBehavior);
		}
        }
}
#endif

#if defined (SBROWSER_PRINT_IMPL) || defined (SBROWSER_SAVEPAGE_IMPL)
void WebViewImpl::paintImage(WebCanvas* canvas, const WebRect& rect){	
        WebFrameImpl* webframe = mainFrameImpl();
		if (webframe) {            
		        if(isAcceleratedCompositingActive()) {				
				    FrameView* view = page()->mainFrame()->view();
					if(view){
						// retain the old behavior.
						PaintBehavior oldPaintBehavior = view->paintBehavior();
						//If h/w rendering is active, disable it
						if (isAcceleratedCompositingActive()) {          
								view->setPaintBehavior(oldPaintBehavior | PaintBehaviorFlattenCompositingLayers);
						}		           	   
						m_client->didInvalidateRect(rect);
						// Force a style recalc to remove all the composited layers.
						m_page->mainFrame()->document()->scheduleForcedStyleRecalc();
						//layout();
                                                if (view->needsLayout()) {
                                                   view->layout();
                                                }
						webframe->paintForPrint(canvas,rect);
						//After painting enable again
						if (isAcceleratedCompositingActive()) {                    
							   view->setPaintBehavior(oldPaintBehavior);				     
						}
						//layout();
					}					
	    }else
	        	webframe->paintForPrint(canvas,rect);
		}
}
#endif

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect, PaintOptions option)
{
#if !OS(ANDROID)
    // ReadbackFromCompositorIfAvailable is the only option available on non-Android.
    // Ideally, Android would always use ReadbackFromCompositorIfAvailable as well.
    ASSERT(option == ReadbackFromCompositorIfAvailable);
#endif

    if (option == ReadbackFromCompositorIfAvailable && isAcceleratedCompositingActive()) {
        // If a canvas was passed in, we use it to grab a copy of the
        // freshly-rendered pixels.
        if (canvas) {
            // Clip rect to the confines of the rootLayerTexture.
            IntRect resizeRect(rect);
            resizeRect.intersect(IntRect(IntPoint(0, 0), m_layerTreeView->deviceViewportSize()));
            doPixelReadbackToCanvas(canvas, resizeRect);
        }
    } else {
        FrameView* view = page()->mainFrame()->view();
        PaintBehavior oldPaintBehavior = view->paintBehavior();
        if (isAcceleratedCompositingActive()) {
            ASSERT(option == ForceSoftwareRenderingAndIgnoreGPUResidentContent);            
            view->setPaintBehavior(oldPaintBehavior | PaintBehaviorFlattenCompositingLayers);
        }

        double paintStart = currentTime();
        PageWidgetDelegate::paint(m_page.get(), pageOverlays(), canvas, rect, isTransparent() ? PageWidgetDelegate::Translucent : PageWidgetDelegate::Opaque);
        double paintEnd = currentTime();
        double pixelsPerSec = (rect.width * rect.height) / (paintEnd - paintStart);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);

        if (isAcceleratedCompositingActive()) {
            ASSERT(option == ForceSoftwareRenderingAndIgnoreGPUResidentContent);            
            view->setPaintBehavior(oldPaintBehavior);
        }
    }
}

bool WebViewImpl::isTrackingRepaints() const
{
    if (!page())
        return false;
    FrameView* view = page()->mainFrame()->view();
    return view->isTrackingRepaints();
}

void WebViewImpl::themeChanged()
{
    if (!page())
        return;
    FrameView* view = page()->mainFrame()->view();

    WebRect damagedRect(0, 0, m_size.width, m_size.height);
    view->invalidateRect(damagedRect);
}

void WebViewImpl::setNeedsRedraw()
{
    if (m_layerTreeView && isAcceleratedCompositingActive())
        m_layerTreeView->setNeedsRedraw();
}

void WebViewImpl::enterFullScreenForElement(WebCore::Element* element)
{
    // We are already transitioning to fullscreen for a different element.
    if (m_provisionalFullScreenElement) {
        m_provisionalFullScreenElement = element;
        return;
    }

    // We are already in fullscreen mode.
    if (m_fullScreenFrame) {
        m_provisionalFullScreenElement = element;
        willEnterFullScreen();
        didEnterFullScreen();
        return;
    }

#if USE(NATIVE_FULLSCREEN_VIDEO)
    if (element && element->isMediaElement()) {
        HTMLMediaElement* mediaElement = toMediaElement(element);
        if (mediaElement->player() && mediaElement->player()->canEnterFullscreen()) {
            mediaElement->player()->enterFullscreen();
            m_provisionalFullScreenElement = element;
        }
        return;
    }
#endif

    // We need to transition to fullscreen mode.
    if (m_client && m_client->enterFullScreen())
        m_provisionalFullScreenElement = element;
}

void WebViewImpl::exitFullScreenForElement(WebCore::Element* element)
{
    // The client is exiting full screen, so don't send a notification.
    if (m_isCancelingFullScreen)
        return;
#if USE(NATIVE_FULLSCREEN_VIDEO)
    if (element && element->isMediaElement()) {
        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(element);
        if (mediaElement->player())
            mediaElement->player()->exitFullscreen();
        return;
    }
#endif
    if (m_client)
        m_client->exitFullScreen();
}

bool WebViewImpl::hasHorizontalScrollbar()
{
    return mainFrameImpl()->frameView()->horizontalScrollbar();
}

bool WebViewImpl::hasVerticalScrollbar()
{
    return mainFrameImpl()->frameView()->verticalScrollbar();
}

const WebInputEvent* WebViewImpl::m_currentInputEvent = 0;

bool WebViewImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    TRACE_EVENT0("webkit", "WebViewImpl::handleInputEvent");
	LOG(INFO)<<"WebViewImpl::handleInputEvent received";
    // If we've started a drag and drop operation, ignore input events until
    // we're done.
    if (m_doingDragAndDrop)
        return true;

    if (m_devToolsAgent && m_devToolsAgent->handleInputEvent(m_page.get(), inputEvent))
        return true;

    // Report the event to be NOT processed by WebKit, so that the browser can handle it appropriately.
    if (m_ignoreInputEvents)
        return false;

    TemporaryChange<const WebInputEvent*> currentEventChange(m_currentInputEvent, &inputEvent);

    if (isPointerLocked() && WebInputEvent::isMouseEventType(inputEvent.type)) {
      pointerLockMouseEvent(inputEvent);
      return true;
    }

    if (m_mouseCaptureNode && WebInputEvent::isMouseEventType(inputEvent.type)) {
        TRACE_EVENT1("webkit", "captured mouse event", "type", inputEvent.type);
        // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
        RefPtr<Node> node = m_mouseCaptureNode;

        // Not all platforms call mouseCaptureLost() directly.
        if (inputEvent.type == WebInputEvent::MouseUp)
            mouseCaptureLost();

        AtomicString eventType;
        switch (inputEvent.type) {
        case WebInputEvent::MouseMove:
            eventType = eventNames().mousemoveEvent;
            break;
        case WebInputEvent::MouseLeave:
            eventType = eventNames().mouseoutEvent;
            break;
        case WebInputEvent::MouseDown:
            eventType = eventNames().mousedownEvent;
            break;
        case WebInputEvent::MouseUp:
            eventType = eventNames().mouseupEvent;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
#if defined(SBROWSER_CQ_MPSG100018638)
       WebFrameImpl* webFrame = mainFrameImpl();
       if (webFrame && webFrame->frameView()) {
           node->dispatchMouseEvent(
              PlatformMouseEventBuilder(webFrame->frameView(), *static_cast<const WebMouseEvent*>(&inputEvent)),
              eventType, static_cast<const WebMouseEvent*>(&inputEvent)->clickCount);
       }
          
#else
        node->dispatchMouseEvent(
              PlatformMouseEventBuilder(mainFrameImpl()->frameView(), *static_cast<const WebMouseEvent*>(&inputEvent)),
              eventType, static_cast<const WebMouseEvent*>(&inputEvent)->clickCount);
#endif
        return true;
    }

    //return PageWidgetDelegate::handleInputEvent(m_page.get(), *this, inputEvent);
    bool handled = PageWidgetDelegate::handleInputEvent(m_page.get(), *this, inputEvent);

#if defined(SBROWSER_COMMIT_BLOCK_ONTOUCH)
	if(handled)//|| inputEvent.type == 20 )//(( inputEvent.type == WebInputEvent::TouchStart)  && (m_lastInputEvent == WebInputEvent::TouchEnd)) )	//_lastInputEvent = inputEvent.type ;
	{
		if(m_layerTreeView)
			m_layerTreeView->setCommitBlock(false);
	}
#endif
    return handled;
}

void WebViewImpl::setCursorVisibilityState(bool isVisible)
{
    if (m_page)
        m_page->setIsCursorVisible(isVisible);
}

void WebViewImpl::mouseCaptureLost()
{
    TRACE_EVENT_ASYNC_END0("webkit", "capturing mouse", this);
    m_mouseCaptureNode = 0;
}

void WebViewImpl::setFocus(bool enable)
{
    m_page->focusController()->setFocused(enable);
    if (enable) {
        m_page->focusController()->setActive(true);
        RefPtr<Frame> focusedFrame = m_page->focusController()->focusedFrame();
        if (focusedFrame) {
            Node* focusedNode = focusedFrame->document()->focusedNode();
            if (focusedNode && focusedNode->isElementNode()
                && focusedFrame->selection()->selection().isNone()) {
                // If the selection was cleared while the WebView was not
                // focused, then the focus element shows with a focus ring but
                // no caret and does respond to keyboard inputs.
                Element* element = toElement(focusedNode);
                if (element->isTextFormControl())
                    element->updateFocusAppearance(true);
                else if (focusedNode->isContentEditable()) {
                    // updateFocusAppearance() selects all the text of
                    // contentseditable DIVs. So we set the selection explicitly
                    // instead. Note that this has the side effect of moving the
                    // caret back to the beginning of the text.
                    Position position(focusedNode, 0,
                                      Position::PositionIsOffsetInAnchor);
                    focusedFrame->selection()->setSelection(
                        VisibleSelection(position, SEL_DEFAULT_AFFINITY));
                }
            }
        }
        m_imeAcceptEvents = true;
    } else {
        hidePopups();

        // Clear focus on the currently focused frame if any.
        if (!m_page)
            return;

        Frame* frame = m_page->mainFrame();
        if (!frame)
            return;

        RefPtr<Frame> focusedFrame = m_page->focusController()->focusedFrame();
        if (focusedFrame) {
            // Finish an ongoing composition to delete the composition node.
            Editor* editor = focusedFrame->editor();
            if (editor && editor->hasComposition()) {
                if (m_autofillClient)
                    m_autofillClient->setIgnoreTextChanges(true);

                editor->confirmComposition();

                if (m_autofillClient)
                    m_autofillClient->setIgnoreTextChanges(false);
            }
            m_imeAcceptEvents = false;
        }
    }
}

bool WebViewImpl::setComposition(
    const WebString& text,
    const WebVector<WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;
    Editor* editor = focused->editor();
    if (!editor)
        return false;

    // The input focus has been moved to another WebWidget object.
    // We should use this |editor| object only to complete the ongoing
    // composition.
    if (!editor->canEdit() && !editor->hasComposition())
        return false;

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    PassRefPtr<Range> range = editor->compositionRange();
    if (range) {
        Node* node = range->startContainer();
        if (!node || !node->isContentEditable())
            editor->cancelComposition(); //modifying this part like SBrowser18 due to input issue in cafe.naver.com issue.
            //return false;
    }

    // If we're not going to fire a keypress event, then the keydown event was
    // canceled.  In that case, cancel any existing composition.
    if (text.isEmpty() || m_suppressNextKeypressEvent) {
        // A browser process sent an IPC message which does not contain a valid
        // string, which means an ongoing composition has been canceled.
        // If the ongoing composition has been canceled, replace the ongoing
        // composition string with an empty string and complete it.
        String emptyString;
        Vector<CompositionUnderline> emptyUnderlines;
        editor->setComposition(emptyString, emptyUnderlines, 0, 0);
        return text.isEmpty();
    }

    // When the range of composition underlines overlap with the range between
    // selectionStart and selectionEnd, WebKit somehow won't paint the selection
    // at all (see InlineTextBox::paint() function in InlineTextBox.cpp).
    // But the selection range actually takes effect.
    editor->setComposition(String(text),
                           CompositionUnderlineVectorBuilder(underlines),
                           selectionStart, selectionEnd);

    return editor->hasComposition();
}

bool WebViewImpl::confirmComposition()
{
    return confirmComposition(WebString());
}

#if defined(SBROWSER_ENABLE_APPNAP)  
void WebViewImpl::shouldPauseScript(bool pause){
	Frame* frame = mainFrameImpl()->frame();
	
	if(frame)
		frame->script()->setPaused(pause);
} 

bool WebViewImpl::canEnableAppNapp(){
	const KURL docUrl = page()->mainFrame()->document()->url();
	//LOG(INFO)<<"POWERSAVER :: canAppNap :: docurl ::"<<docUrl.string().characters()<<":issecure:"<<SecurityOrigin::isSecure(docUrl);
	
	return ((page()->progress()->estimatedProgress() ==0) && !SecurityOrigin::isSecure(docUrl));
}

void WebViewImpl::shouldEnableAppNap(bool pause){
	Settings* settings = page()->settings();
	if(settings && settings->isAppNapEnabled()){
		if(pause && canEnableAppNapp() ){
			shouldPauseScript(pause);		
			}
		else if(!pause)
			shouldPauseScript(pause);
		}
}
#endif
#if defined(SBROWSER_SEARCH_SCROLL_FIX) 
bool  WebViewImpl::confirmCompositionOnTap(const WebInputEvent& input_event)
{	 
    Frame* focused = focusedWebCoreFrame();
      
    if (!focused || !m_imeAcceptEvents)
		return false;
    Editor* editor = focused->editor();
    if (!editor || (!editor->hasComposition()))
        return false;

	PlatformGestureEventBuilder gestureEvent(mainFrameImpl()->frameView(), *static_cast<const WebGestureEvent*>(&input_event));
	IntPoint adjustedPoint = gestureEvent.position();

	IntPoint hitTestPoint = m_page->mainFrame()->view()->windowToContents(adjustedPoint);
    HitTestResult result(m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(hitTestPoint));
    Node* hitNode = result.targetNode();
	if (hitNode &&( hitNode->isContentEditable() || (hitNode->hasTagName(WebCore::HTMLNames::inputTag))
	 												|| (hitNode->hasTagName(WebCore::HTMLNames::textareaTag)))){											
        return confirmComposition();
	 }
	editor->cancelCompositionOnTap();
	return true;

}
#endif

// SAMSUNG CHANGE : Patch for appeared ime when select not editable >>
#if defined (SBROWSER_SUPPORT)
bool WebViewImpl::isEditableOnInputNode(const WebInputEvent& input_event)
{
    Frame* focused = focusedWebCoreFrame();

    if (!focused || !m_imeAcceptEvents)
		return false;
    Editor* editor = focused->editor();
    if (!editor)
        return false;

	PlatformGestureEventBuilder gestureEvent(mainFrameImpl()->frameView(), *static_cast<const WebGestureEvent*>(&input_event));
	IntPoint adjustedPoint = gestureEvent.position();

	IntPoint hitTestPoint = m_page->mainFrame()->view()->windowToContents(adjustedPoint);
    HitTestResult result(m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(hitTestPoint));
    Node* hitNode = result.targetNode();
	if (hitNode &&( hitNode->isContentEditable() || (hitNode->hasTagName(WebCore::HTMLNames::inputTag))
		|| (hitNode->hasTagName(WebCore::HTMLNames::textareaTag)))){
        return true;
	 }
	return false;

}
#endif
// SAMSUNG CHANGE : Patch for appeared ime when select not editable <<

bool WebViewImpl::confirmComposition(const WebString& text)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;
    Editor* editor = focused->editor();
    if (!editor || (!editor->hasComposition() && !text.length()))
        return false;

    // 3x4 keypad passowrd issue ++
    if((focused->selection()->isInPasswordField()) && (!(text.length()) && editor->hasComposition())) {
        editor->cancelComposition();
        return false;
    }
    // 3x4 keypad passowrd issue --

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    PassRefPtr<Range> range = editor->compositionRange();
    if (range) {
        Node* node = range->startContainer();
        if (!node || !node->isContentEditable())
            editor->cancelComposition();
	    //return false;
    }

    if (editor->hasComposition()) {
        if (text.length())
            editor->confirmComposition(String(text));
        else
            editor->confirmComposition();
    } else
        editor->insertText(String(text), 0);
 #if defined (SBROWSER_SCROLL_INPUTTEXT)
     TextFieldsBoundsChanged();
 #endif

    return true;
}

bool WebViewImpl::compositionRange(size_t* location, size_t* length)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !focused->selection() || !m_imeAcceptEvents)
        return false;
    Editor* editor = focused->editor();
    if (!editor || !editor->hasComposition())
        return false;

    RefPtr<Range> range = editor->compositionRange();
    if (!range)
        return false;

    if (TextIterator::getLocationAndLengthFromRange(focused->selection()->rootEditableElementOrDocumentElement(), range.get(), *location, *length))
        return true;
    return false;
}
#if defined(SBROWSER_FORM_NAVIGATION)&&defined(SBROWSER_ENABLE_JPN_SUPPORT_SUGGESTION_OPTION)
bool WebViewImpl::isSpellCheckEnabled()
{
	Node* focusedNode = focusedWebCoreNode();
	bool isEnabled = true;

	if(!focusedNode)
	{
		return false;
	}
	if (focusedNode->isElementNode()) {
		WebCore::Element* ele = static_cast<Element*>(focusedNode);
		isEnabled = ele->isSpellCheckingEnabled();
	}
	return isEnabled;
}
#endif

WebTextInputInfo WebViewImpl::textInputInfo()
{
    WebTextInputInfo info;

	#if defined(SBROWSER_FORM_NAVIGATION)
	//info.privateIMEOptions = getIMEOptions();
        info.privateIMEOptions = m_client->getPrivateIMEOptionsValue();
#if defined(SBROWSER_ENABLE_JPN_SUPPORT_SUGGESTION_OPTION)
    info.spellCheckingEnabled = isSpellCheckEnabled(); //SAMSUNG for JPN IME
#endif
#endif
	
    Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return info;

    FrameSelection* selection = focused->selection();
    if (!selection)
        return info;

    Node* node = selection->selection().rootEditableElement();
    if (!node)
        return info;

    info.type = textInputType();
    if (info.type == WebTextInputTypeNone)
        return info;

    Editor* editor = focused->editor();
    if (!editor || !editor->canEdit())
        return info;

    info.value = plainText(rangeOfContents(node).get());

    if (info.value.isEmpty())
        return info;

    size_t location;
    size_t length;
    RefPtr<Range> range = selection->selection().firstRange();
    if (range && TextIterator::getLocationAndLengthFromRange(selection->rootEditableElement(), range.get(), location, length)) {
        info.selectionStart = location;
        info.selectionEnd = location + length;
    }
    range = editor->compositionRange();
    if (range && TextIterator::getLocationAndLengthFromRange(selection->rootEditableElement(), range.get(), location, length)) {
        info.compositionStart = location;
        info.compositionEnd = location + length;
    }
#if defined(SBROWSER_KEYLAG)
    info.changeAttrEnabled = isChangeEventListenerRegistered(); //SAMSUNG_CHANGES : Key lagging ++ Change
#endif
	
    return info;
}

WebTextInputType WebViewImpl::textInputType()
{
    Node* node = focusedWebCoreNode();
    if (!node)
        return WebTextInputTypeNone;

    if (node->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(node);

        if (input->isDisabledOrReadOnly())
            return WebTextInputTypeNone;

        if (input->isPasswordField())
            return WebTextInputTypePassword;
        if (input->isSearchField())
            return WebTextInputTypeSearch;
        if (input->isEmailField())
            return WebTextInputTypeEmail;
        if (input->isNumberField())
            return WebTextInputTypeNumber;
        if (input->isTelephoneField())
            return WebTextInputTypeTelephone;
        if (input->isURLField())
            return WebTextInputTypeURL;
        if (input->isDateField())
            return WebTextInputTypeDate;
        if (input->isDateTimeField())
            return WebTextInputTypeDateTime;
        if (input->isDateTimeLocalField())
            return WebTextInputTypeDateTimeLocal;
        if (input->isMonthField())
            return WebTextInputTypeMonth;
        if (input->isTimeField())
            return WebTextInputTypeTime;
        if (input->isWeekField())
            return WebTextInputTypeWeek;
        if (input->isTextField())
            return WebTextInputTypeText;

        return WebTextInputTypeNone;
    }

    if (node->hasTagName(HTMLNames::textareaTag)) {
        if (static_cast<HTMLTextAreaElement*>(node)->isDisabledOrReadOnly())
            return WebTextInputTypeNone;
        return WebTextInputTypeTextArea;
    }

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    if (node->isHTMLElement()) {
        HTMLElement* element = toHTMLElement(node);
        if (element->isDateTimeFieldElement())
            return WebTextInputTypeDateTimeField;
    }
#endif

    if (node->shouldUseInputMethod())
        return WebTextInputTypeContentEditable;

    return WebTextInputTypeNone;
}

bool WebViewImpl::selectionBounds(WebRect& anchor, WebRect& focus) const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection* selection = frame->selection();
    if (!selection)
        return false;

    if (selection->isCaret()
#if defined (SBROWSER_TEXT_SELECTION) //P130724-1254
	&& selection->isContentEditable()
#endif
	)
        anchor = focus = selection->absoluteCaretBounds();
    else {
        RefPtr<Range> selectedRange = frame->selection()->toNormalizedRange();
        if (!selectedRange)
            return false;

#if defined (SBROWSER_TEXT_SELECTION)//PLM:P130318-0228
    String text = frame->editor()->selectedText();

    if (""== text || text.isEmpty() /*|| "\n" == text || "\n\n" == text*/)
	return false;
#endif

#if defined (SBROWSER_TEXT_SELECTION)
    anchor = frame->editor()->firstRectForRange(selectedRange.get());
    focus =frame->editor()->lastRectForRange(selectedRange.get());
#else
        RefPtr<Range> range(Range::create(selectedRange->startContainer()->document(),
            selectedRange->startContainer(),
            selectedRange->startOffset(),
            selectedRange->startContainer(),
            selectedRange->startOffset()));
        anchor = frame->editor()->firstRectForRange(range.get());

        range = Range::create(selectedRange->endContainer()->document(),
            selectedRange->endContainer(),
            selectedRange->endOffset(),
            selectedRange->endContainer(),
            selectedRange->endOffset());
        focus = frame->editor()->firstRectForRange(range.get());
#endif
    }

    IntRect scaledAnchor(frame->view()->contentsToWindow(anchor));
    IntRect scaledFocus(frame->view()->contentsToWindow(focus));
    scaledAnchor.scale(pageScaleFactor());
    scaledFocus.scale(pageScaleFactor());
    anchor = scaledAnchor;
    focus = scaledFocus;

    if (!frame->selection()->selection().isBaseFirst())
        std::swap(anchor, focus);
    return true;
}

#if defined (SBROWSER_TEXT_SELECTION)
void WebViewImpl::OnGetSelectionBitmap(SkBitmap* SelectedRegion)
{

	Frame* frame_p =  m_page->mainFrame();

	RefPtr<WebCore::ChromiumDataObject> selectDataObject = ChromiumDataObject::create();
	m_selectionClipboard = ClipboardChromium::create(Clipboard::CopyAndPaste, selectDataObject.get(), ClipboardWritable, frame_p);
	 RefPtr<Range> selectionRange = frame_p->selection()->toNormalizedRange();
        ASSERT(selectionRange);
                m_selectionClipboard->writeRange(selectionRange.get(), frame_p); 
                
	DragImageRef selectedImage = frame_p->dragImageForSelection();
	if(selectedImage)
		*SelectedRegion = *selectedImage->bitmap; 
	deleteDragImage(selectedImage);
}
WebRect WebViewImpl::currentSelectionRect()
{
    Frame* frame_p =  page()->focusController()->focusedOrMainFrame();
    if(frame_p == NULL)
        return WebRect(0,0,0,0);
    WebCore::IntRect box, selectionrect;
    WTF::Vector<IntRect> boxVector;
    RefPtr<Range> range;
	
    if(frame_p->selection()->isRange() &&  (range = frame_p->selection()->toNormalizedRange()))
    {
        ExceptionCode ec = 0;
        RefPtr<Range> tempRange = range->cloneRange(ec);
        // consider image selection also while getting the bounds.
        boxVector = tempRange->boundingBoxEx();
        if (!boxVector.isEmpty())
        {
            box=boxVector[0];
            for (size_t i = 1; i < boxVector.size(); ++i){
                box.unite(boxVector[i]);
            }
            LOG(INFO)<<"selectionrect box: "<<"  "<<box.x()<<"  "<<box.y()<<"  x2="<<box.width()<<"  "<<box.height();	       
	        selectionrect = frame_p->view()->contentsToWindow(IntRect(box.x(),box.y(),box.width(), box.height()));
	        selectionrect.scale(pageScaleFactor());
	        boxVector.clear();
	        LOG(INFO)<<"selectionrect finalrect box: "<<selectionrect.x()<<"  "<<selectionrect.y()<<"  "<<selectionrect.width()<<"  "<<selectionrect.height();
		}
	}
	else
		return WebRect(0,0,0,0);  	
   return WebRect(selectionrect.x(),selectionrect.y(),selectionrect.width(),selectionrect.height());   
}
bool WebViewImpl::IsSelectionWithinVisibleRect() 
{
    bool isVisible = false;
    WTF::Vector<IntRect> boxVector;
    IntRect visibleRect ; 
    RefPtr<Range> range;

#if defined(SBROWSER_CQ_MPSG100012711)
    Frame* frame_p =  page()->focusController()->focusedOrMainFrame();

    if (NULL == frame_p)
	return false;

    visibleRect = frame_p ->view()->visibleContentRect();
#else
        Frame* frame_p =  m_page->mainFrame();
	 visibleRect = page()->mainFrame()->view()->visibleContentRect();
#endif

  	if(frame_p != NULL){
	if ( frame_p->selection() && frame_p->selection()->isRange() &&  (range = frame_p->selection()->toNormalizedRange()) )
		{
			ExceptionCode ec = 0;
			RefPtr<Range> tempRange = range->cloneRange(ec);
			boxVector = tempRange->textBoundingBoxEx();	
			
			if(!boxVector.isEmpty())
			{
				for (size_t i = 0; i < boxVector.size(); i++) 
				{
				if(visibleRect.intersects(boxVector[i])) ////3.Check if these two has any common intersection
				{
				  isVisible = true;
				  break;
				}
				}
			}
		
		}
	
	}

return isVisible;
}

void WebViewImpl::unmarkSelection(){
    VisibleSelection newSelection;
    RefPtr<Frame> focusedFrame = m_page->focusController()->focusedFrame();
    if (focusedFrame) {		
        Node* focusedNode = focusedFrame->document()->focusedNode();
        if(focusedNode){
            bool caretBrowsing = focusedFrame->settings() && focusedFrame->settings()->caretBrowsingEnabled();
				
            if ( focusedFrame->selection() && (caretBrowsing || static_cast<Element*>(focusedNode)->isTextFormControl()) && focusedNode->renderer()) {
                VisiblePosition pos = focusedFrame->selection()->end();					
                newSelection = VisibleSelection(pos);
                if (focusedFrame->selection()->selection() != newSelection && focusedFrame->selection()->shouldChangeSelection(newSelection))
                    focusedFrame->selection()->setSelection(newSelection);
				
                focusedFrame->selection()->notifyRendererOfSelectionChange(UserTriggered);
            }
        }
    }
}
#endif
#if defined (SBROWSER_TABLET_SWITCH)
void WebViewImpl::SetTabletModeEnabled(bool tabletMode)
{
	m_isTabletMode = tabletMode;
}
#endif
#if defined (SBROWSER_SCROLL_INPUTTEXT)

void WebViewImpl::ScrollEditWithCursor(int scrollX) {
    WebCore::Node* focus = focusedWebCoreNode();
    if (!focus)
        return;
    RenderTextControl* rtc = 0;
    RenderObject* renderer = focus->renderer();
    if (renderer && renderer->isTextControl()) {
        rtc = toRenderTextControl(renderer);
    }
    if (!rtc)
        return;
    int contentScrollX = (int)(scrollX/pageScaleFactor());
    rtc->setScrollLeft(contentScrollX + rtc->scrollLeft());
    rtc->setNeedsLayout(true);
    focus->document()->frame()->selection()->recomputeCaretRect();
    m_client->didChangeSelection(true);
}

void WebViewImpl::TextFieldsBoundsChanged(bool focusOn)
{
    WebCore::Node* node = focusedWebCoreNode();
    if(!node)
        return;
    if (!node->hasTagName(HTMLNames::inputTag))
        return;	
// =====================InputBox Bounds ===================
    IntRect inputBoxRect = node->Node::pixelSnappedBoundingBox();
    inputBoxRect = node->document()->view()->contentsToWindow(inputBoxRect);
// ====================================================
// =====================InputText Bounds ===================
    IntRect inputTextRect = getEditFieldTextRectBounds(node);
// ====================================================

//Check if textRect Width > EditField width to enable scrolling inside EditField
    bool isScrollEnable = EnableScrollingInEditField(inputTextRect.width());
  m_client->updateTextFieldBounds(WebRect(inputBoxRect.x(),inputBoxRect.y(),inputBoxRect.width(),inputBoxRect.height()),isScrollEnable,focusOn);
}

 IntRect WebViewImpl::getEditFieldTextRectBounds(WebCore::Node* node) {
    if((node->hasTagName(HTMLNames::inputTag)) && (node->renderer()))	{
        RenderTextControl* renderText = WebCore::toRenderTextControl(node->renderer());
        if(!renderText)
            return WebCore::IntRect();
        IntRect textRect;
	    float width = renderText->scrollWidth() ;
        if(width <= 0)
            return WebCore::IntRect();
	    float height = renderText->scrollHeight() ;
        textRect.setWidth((int)width);
        textRect.setHeight((int)height);
        textRect.move(-renderText->scrollLeft(), -renderText->scrollTop());
        return textRect;
      }
    else
        return WebCore::IntRect();
}

 bool WebViewImpl::EnableScrollingInEditField(int textRectWidth)
 {
    LayoutRect clientRect;
    Node* node = focusedWebCoreNode();
	if(!node)
	    return false;
	if((node->hasTagName(HTMLNames::inputTag)) && (node->renderer()) && (node->renderer()->isBox()))
		{
		RenderBox* renderBox = toRenderBox(node->renderer());
		if(!renderBox)
			return false;
		clientRect = renderBox->clientBoxRect();
		LayoutRect contentBox = renderBox->contentBoxRect();
		clientRect.setX(contentBox.x());
		clientRect.setWidth(contentBox.width());
		if(textRectWidth > clientRect.width())
            return true;
		}
	return false;	
 }
#endif

#if defined (SBROWSER_MULTIINSTANCE_DRAG_DROP)
void WebViewImpl::OnHandleSelectionDrop(int x, int y, const WebString& text) {
    IntPoint point(x, y);
    point = m_page->mainFrame()->view()->windowToContents(point);
    HitTestResult result(m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(point));
    Node* hitNode = result.innerNonSharedNode();
    if (hitNode && isFormNavTextInput(hitNode)) {
        IntRect bounds = getNodeBounds(hitNode);
        if (bounds.x() == 0 && bounds.y() == 0 && bounds.isEmpty()) return;
        fakeMouseClick(x, y, hitNode);
        hitNode->setFocus(true);   

        Frame* focused = focusedWebCoreFrame();
        if (!focused)
            return;
         Editor* editor = focused->editor();
        if (!editor)
            return;

        if (editor->hasComposition()) {
            editor->confirmComposition(String(text));
        } else
            editor->insertText(String(text), 0);    
    }
}
#endif

#if defined (SBROWSER_HANDLE_MOUSECLICK_CTRL)
void WebViewImpl::OnHandleMouseClickWithCtrlkey(int x, int y) {	
	LOG(INFO)<<"WebViewImpl::OnHandleMouseClickWithCtrlkey";
	IntPoint point(x, y);	
	point = m_page->mainFrame()->view()->windowToContents(point);	
	HitTestResult result(m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(point));
	KURL linkURL = result.absoluteLinkURL();
	if (linkURL.isEmpty()) {
       	linkURL = result.absoluteImageURL();
	}
	if (!linkURL.isEmpty()) {
		 LOG(INFO)<<"WebViewImpl::OnHandleMouseClickWithCtrlkey:: url ="<<linkURL.string().utf8().data();
		m_client->OpenUrlInNewTab(UTF8ToUTF16(linkURL.string().utf8().data()));
	}
}

string16 WebViewImpl::getUrlFromElement( const WebElement& url_element )
{
  if( url_element.isNull())
  	return string16();

  WebElement element = url_element;
  while( element.isNull() == false && !element.hasTagName("body") )
  {
      WebString hrefstr = element.getAttribute( "href" ) ;
      if( hrefstr.length() != 0 )
      {
      	 LOG(INFO)<<"WebViewImpl::getUrlFromElement:: url ="<<hrefstr.utf8().data();
        return UTF8ToUTF16(hrefstr.utf8().data());
        break;
      }    
      element = element.parentNode().to<WebElement>();
  }
  return string16();
}
#endif

bool WebViewImpl::selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection* selection = frame->selection();
    if (!selection)
        return false;
    if (!selection->toNormalizedRange())
        return false;
    start = selection->start().primaryDirection() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    end = selection->end().primaryDirection() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    return true;
}

bool WebViewImpl::isSelectionAnchorFirst() const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection* selection = frame->selection();
    if (!selection)
        return false;
    return selection->selection().isBaseFirst();
}

bool WebViewImpl::setEditableSelectionOffsets(int start, int end)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;

    Editor* editor = focused->editor();
    if (!editor || !editor->canEdit())
        return false;

    return editor->setSelectionOffsets(start, end);
}

bool WebViewImpl::setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;

    //Composition from existing text is not needed in password fields.
    if(focused->selection()->isInPasswordField())
        return false;

    Editor* editor = focused->editor();
    if (!editor || !editor->canEdit())
        return false;

    editor->cancelComposition();

    if (compositionStart == compositionEnd)
        return true;

    size_t location;
    size_t length;
    caretOrSelectionRange(&location, &length);
    editor->setIgnoreCompositionSelectionChange(true);
    editor->setSelectionOffsets(compositionStart, compositionEnd);
    String text = editor->selectedText();
    focused->document()->execCommand("delete", true);
    editor->setComposition(text, CompositionUnderlineVectorBuilder(underlines), 0, 0);
    // Need to set setIgnoreCompositionSelectionChange(true) again because setComposition resets it to false.
    editor->setIgnoreCompositionSelectionChange(true);
    editor->setSelectionOffsets(location, location + length);
    editor->setIgnoreCompositionSelectionChange(false);

    return true;
}

void WebViewImpl::extendSelectionAndDelete(int before, int after)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return;

    Editor* editor = focused->editor();
    if (!editor || !editor->canEdit())
        return;

    FrameSelection* selection = focused->selection();
    if (!selection)
        return;

    size_t location;
    size_t length;
    RefPtr<Range> range = selection->selection().firstRange();
    if (range && TextIterator::getLocationAndLengthFromRange(selection->rootEditableElement(), range.get(), location, length)) {
        editor->setSelectionOffsets(max(static_cast<int>(location) - before, 0), location + length + after);
        focused->document()->execCommand("delete", true);
    }
}

bool WebViewImpl::isSelectionEditable() const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    return frame->selection()->isContentEditable();
}

WebColor WebViewImpl::backgroundColor() const
{
    if (!m_page)
        return Color::white;
    FrameView* view = m_page->mainFrame()->view();
    Color backgroundColor = view->documentBackgroundColor();
    if (!backgroundColor.isValid())
        return Color::white;
    return backgroundColor.rgb();
}

bool WebViewImpl::caretOrSelectionRange(size_t* location, size_t* length)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;

    FrameSelection* selection = focused->selection();
    if (!selection)
        return false;

    RefPtr<Range> range = selection->selection().firstRange();
    if (!range)
        return false;

    if (TextIterator::getLocationAndLengthFromRange(selection->rootEditableElementOrTreeScopeRootNode(), range.get(), *location, *length))
        return true;
    return false;
}

void WebViewImpl::setTextDirection(WebTextDirection direction)
{
    // The Editor::setBaseWritingDirection() function checks if we can change
    // the text direction of the selected node and updates its DOM "dir"
    // attribute and its CSS "direction" property.
    // So, we just call the function as Safari does.
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return;

    Editor* editor = focused->editor();
    if (!editor || !editor->canEdit())
        return;

    switch (direction) {
    case WebTextDirectionDefault:
        editor->setBaseWritingDirection(NaturalWritingDirection);
        break;

    case WebTextDirectionLeftToRight:
        editor->setBaseWritingDirection(LeftToRightWritingDirection);
        break;

    case WebTextDirectionRightToLeft:
        editor->setBaseWritingDirection(RightToLeftWritingDirection);
        break;

    default:
        notImplemented();
        break;
    }
}

bool WebViewImpl::isAcceleratedCompositingActive() const
{
    return m_isAcceleratedCompositingActive;
}

void WebViewImpl::willCloseLayerTreeView()
{
    setIsAcceleratedCompositingActive(false);
    m_layerTreeView = 0;
}

void WebViewImpl::didAcquirePointerLock()
{
    if (page())
        page()->pointerLockController()->didAcquirePointerLock();
}

void WebViewImpl::didNotAcquirePointerLock()
{
    if (page())
        page()->pointerLockController()->didNotAcquirePointerLock();
}

void WebViewImpl::didLosePointerLock()
{
    if (page())
        page()->pointerLockController()->didLosePointerLock();
}

void WebViewImpl::didChangeWindowResizerRect()
{
    if (mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->windowResizerRectChanged();
}

// WebView --------------------------------------------------------------------

WebSettingsImpl* WebViewImpl::settingsImpl()
{
    if (!m_webSettings)
        m_webSettings = adoptPtr(new WebSettingsImpl(m_page->settings()));
    ASSERT(m_webSettings);
    return m_webSettings.get();
}

WebSettings* WebViewImpl::settings()
{
    return settingsImpl();
}

WebString WebViewImpl::pageEncoding() const
{
    if (!m_page)
        return WebString();

    // FIXME: Is this check needed?
    if (!m_page->mainFrame()->document()->loader())
        return WebString();

    return m_page->mainFrame()->document()->encoding();
}

void WebViewImpl::setPageEncoding(const WebString& encodingName)
{
    if (!m_page)
        return;

    // Only change override encoding, don't change default encoding.
    // Note that the new encoding must be 0 if it isn't supposed to be set.
    String newEncodingName;
    if (!encodingName.isEmpty())
        newEncodingName = encodingName;
    m_page->mainFrame()->loader()->reloadWithOverrideEncoding(newEncodingName);
}

bool WebViewImpl::dispatchBeforeUnloadEvent()
{
    // FIXME: This should really cause a recursive depth-first walk of all
    // frames in the tree, calling each frame's onbeforeunload.  At the moment,
    // we're consistent with Safari 3.1, not IE/FF.
    Frame* frame = m_page->mainFrame();
    if (!frame)
        return true;

    return frame->loader()->shouldClose();
}

void WebViewImpl::dispatchUnloadEvent()
{
    // Run unload handlers.
    m_page->mainFrame()->loader()->closeURL();
}

WebFrame* WebViewImpl::mainFrame()
{
    return mainFrameImpl();
}

WebFrame* WebViewImpl::findFrameByName(
    const WebString& name, WebFrame* relativeToFrame)
{
    if (!relativeToFrame)
        relativeToFrame = mainFrame();
    Frame* frame = static_cast<WebFrameImpl*>(relativeToFrame)->frame();
    frame = frame->tree()->find(name);
    return WebFrameImpl::fromFrame(frame);
}

WebFrame* WebViewImpl::focusedFrame()
{
    return WebFrameImpl::fromFrame(focusedWebCoreFrame());
}

void WebViewImpl::setFocusedFrame(WebFrame* frame)
{
    if (!frame) {
        // Clears the focused frame if any.
        Frame* frame = focusedWebCoreFrame();
        if (frame)
            frame->selection()->setFocused(false);
        return;
    }
    WebFrameImpl* frameImpl = static_cast<WebFrameImpl*>(frame);
    Frame* webcoreFrame = frameImpl->frame();
    webcoreFrame->page()->focusController()->setFocusedFrame(webcoreFrame);
}

void WebViewImpl::setInitialFocus(bool reverse)
{
    if (!m_page)
        return;

    // Since we don't have a keyboard event, we'll create one.
    WebKeyboardEvent keyboardEvent;
    keyboardEvent.type = WebInputEvent::RawKeyDown;
    if (reverse)
        keyboardEvent.modifiers = WebInputEvent::ShiftKey;

    // VK_TAB which is only defined on Windows.
    keyboardEvent.windowsKeyCode = 0x09;
    PlatformKeyboardEventBuilder platformEvent(keyboardEvent);
    RefPtr<KeyboardEvent> webkitEvent = KeyboardEvent::create(platformEvent, 0);

    Frame* frame = page()->focusController()->focusedOrMainFrame();
    if (Document* document = frame->document())
        document->setFocusedNode(0);
    page()->focusController()->setInitialFocus(
        reverse ? FocusDirectionBackward : FocusDirectionForward,
        webkitEvent.get());
}

void WebViewImpl::clearFocusedNode()
{
    RefPtr<Frame> frame = focusedWebCoreFrame();
    if (!frame)
        return;

    RefPtr<Document> document = frame->document();
    if (!document)
        return;

    RefPtr<Node> oldFocusedNode = document->focusedNode();

    // Clear the focused node.
    document->setFocusedNode(0);

    if (!oldFocusedNode)
        return;

    // If a text field has focus, we need to make sure the selection controller
    // knows to remove selection from it. Otherwise, the text field is still
    // processing keyboard events even though focus has been moved to the page and
    // keystrokes get eaten as a result.
    if (oldFocusedNode->isContentEditable()
        || (oldFocusedNode->isElementNode()
            && toElement(oldFocusedNode.get())->isTextFormControl())) {
        frame->selection()->clear();
    }
}

void WebViewImpl::scrollFocusedNodeIntoView()
{
    Node* focusedNode = focusedWebCoreNode();
    if (focusedNode && focusedNode->isElementNode()) {
        Element* elementNode = toElement(focusedNode);
        elementNode->scrollIntoViewIfNeeded(true);
    }
}

#if OS(ANDROID)
WebRect WebViewImpl::focusedNodeBounds()
{
    Node* focusedNode = focusedWebCoreNode();
    if (focusedNode && focusedNode->document() && focusedNode->document()->view()) {
        focusedNode->document()->updateLayoutIgnorePendingStylesheets();
        IntRect absoluteRect = enclosingIntRect(focusedNode->Node::boundingBox());
        return focusedNode->document()->view()->contentsToWindow(absoluteRect);
    }
    return WebRect();
}
#endif

void WebViewImpl::scrollFocusedNodeIntoRect(const WebRect& rect)
{
    Frame* frame = page()->mainFrame();
    Node* focusedNode = focusedWebCoreNode();
    if (!frame || !frame->view() || !focusedNode || !focusedNode->isElementNode())
        return;

    if (!m_webSettings->autoZoomFocusedNodeToLegibleScale()) {
        Element* elementNode = toElement(focusedNode);
        frame->view()->scrollElementToRect(elementNode, IntRect(rect.x, rect.y, rect.width, rect.height));
        return;
    }

    float scale;
    IntPoint scroll;
    bool needAnimation;
    computeScaleAndScrollForFocusedNode(focusedNode, scale, scroll, needAnimation);
    if (needAnimation)
        startPageScaleAnimation(scroll, false, scale, scrollAndScaleAnimationDurationInSeconds);
#if defined (SBROWSER_SCROLL_INPUTTEXT)
    TextFieldsBoundsChanged(true);
#endif
}

void WebViewImpl::computeScaleAndScrollForFocusedNode(Node* focusedNode, float& newScale, IntPoint& newScroll, bool& needAnimation)
{
    focusedNode->document()->updateLayoutIgnorePendingStylesheets();

    // 'caret' is rect encompassing the blinking cursor.
    IntRect textboxRect = focusedNode->document()->view()->contentsToWindow(pixelSnappedIntRect(focusedNode->Node::boundingBox()));
    WebRect caret, unusedEnd;
    selectionBounds(caret, unusedEnd);
    IntRect unscaledCaret = caret;
    unscaledCaret.scale(1 / pageScaleFactor());
    caret = unscaledCaret;

    // Pick a scale which is reasonably readable. This is the scale at which
    // the caret height will become minReadableCaretHeight (adjusted for dpi
    // and font scale factor).
    float targetScale = 1;
	// PLM P140121-06253, For tablets zoom scale should be more
    if(CommandLine::ForCurrentProcess()->HasSwitch(switches::kTabletUI)) {
        targetScale = 1.5;	
    }
    if (page() && page()->settings())
        targetScale *= page()->settings()->textAutosizingFontScaleFactor();

    newScale = clampPageScaleFactorToLimits(minReadableCaretHeight * targetScale / caret.height);
    const float deltaScale = newScale / pageScaleFactor();

    // Convert the rects to absolute space in the new scale.
    IntRect textboxRectInDocumentCoordinates = textboxRect;
    textboxRectInDocumentCoordinates.move(mainFrame()->scrollOffset());
    IntRect caretInDocumentCoordinates = caret;
    caretInDocumentCoordinates.move(mainFrame()->scrollOffset());

    int viewWidth = m_size.width / newScale;
    int viewHeight = m_size.height / newScale;

    if (textboxRectInDocumentCoordinates.width() <= viewWidth) {
        // Field is narrower than screen. Try to leave padding on left so field's
        // label is visible, but it's more important to ensure entire field is
        // onscreen.
        int idealLeftPadding = viewWidth * leftBoxRatio;
        int maxLeftPaddingKeepingBoxOnscreen = viewWidth - textboxRectInDocumentCoordinates.width();
        newScroll.setX(textboxRectInDocumentCoordinates.x() - min<int>(idealLeftPadding, maxLeftPaddingKeepingBoxOnscreen));
    } else {
        // Field is wider than screen. Try to left-align field, unless caret would
        // be offscreen, in which case right-align the caret.
        newScroll.setX(max<int>(textboxRectInDocumentCoordinates.x(), caretInDocumentCoordinates.x() + caretInDocumentCoordinates.width() + caretPadding - viewWidth));
    }
    if (textboxRectInDocumentCoordinates.height() <= viewHeight) {
        // Field is shorter than screen. Vertically center it.
        newScroll.setY(textboxRectInDocumentCoordinates.y() - (viewHeight - textboxRectInDocumentCoordinates.height()) / 2);
    } else {
        // Field is taller than screen. Try to top align field, unless caret would
        // be offscreen, in which case bottom-align the caret.
        newScroll.setY(max<int>(textboxRectInDocumentCoordinates.y(), caretInDocumentCoordinates.y() + caretInDocumentCoordinates.height() + caretPadding - viewHeight));
    }

    needAnimation = false;
    // If we are at less than the target zoom level, zoom in.
    if (deltaScale > minScaleChangeToTriggerZoom)
        needAnimation = true;
    // If the caret is offscreen, then animate.
    IntRect sizeRect(0, 0, viewWidth, viewHeight);
    if (!sizeRect.contains(caret))
        needAnimation = true;
    // If the box is partially offscreen and it's possible to bring it fully
    // onscreen, then animate.
    if (sizeRect.contains(textboxRectInDocumentCoordinates.width(), textboxRectInDocumentCoordinates.height()) && !sizeRect.contains(textboxRect))
        needAnimation = true;
}

void WebViewImpl::advanceFocus(bool reverse)
{
    page()->focusController()->advanceFocus(reverse ? FocusDirectionBackward : FocusDirectionForward, 0);
}

double WebViewImpl::zoomLevel()
{
    return m_zoomLevel;
}

double WebViewImpl::setZoomLevel(bool textOnly, double zoomLevel)
{
    if (zoomLevel < m_minimumZoomLevel)
        m_zoomLevel = m_minimumZoomLevel;
    else if (zoomLevel > m_maximumZoomLevel)
        m_zoomLevel = m_maximumZoomLevel;
    else
        m_zoomLevel = zoomLevel;

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->plugin()->setZoomLevel(m_zoomLevel, textOnly);
    else {
        float zoomFactor = static_cast<float>(zoomLevelToZoomFactor(m_zoomLevel));
        if (textOnly)
            frame->setPageAndTextZoomFactors(1, zoomFactor * m_emulatedTextZoomFactor);
        else
            frame->setPageAndTextZoomFactors(zoomFactor, m_emulatedTextZoomFactor);
    }
    return m_zoomLevel;
}

void WebViewImpl::zoomLimitsChanged(double minimumZoomLevel,
                                    double maximumZoomLevel)
{
    m_minimumZoomLevel = minimumZoomLevel;
    m_maximumZoomLevel = maximumZoomLevel;
    m_client->zoomLimitsChanged(m_minimumZoomLevel, m_maximumZoomLevel);
}

void WebViewImpl::fullFramePluginZoomLevelChanged(double zoomLevel)
{
    if (zoomLevel == m_zoomLevel)
        return;

    m_zoomLevel = max(min(zoomLevel, m_maximumZoomLevel), m_minimumZoomLevel);
    m_client->zoomLevelChanged();
}

double WebView::zoomLevelToZoomFactor(double zoomLevel)
{
    return pow(textSizeMultiplierRatio, zoomLevel);
}

double WebView::zoomFactorToZoomLevel(double factor)
{
    // Since factor = 1.2^level, level = log(factor) / log(1.2)
    return log(factor) / log(textSizeMultiplierRatio);
}

void WebViewImpl::setInitialPageScaleOverride(float initialPageScaleFactorOverride)
{
    if (m_initialPageScaleFactorOverride == initialPageScaleFactorOverride)
        return;
    m_initialPageScaleFactorOverride = initialPageScaleFactorOverride;
    m_pageScaleFactorIsSet = false;
    computePageScaleFactorLimits();
}

float WebViewImpl::pageScaleFactor() const
{
    if (!page())
        return 1;

    return page()->pageScaleFactor();
}

bool WebViewImpl::isPageScaleFactorSet() const
{
    return m_pageScaleFactorIsSet;
}

 //SAMSUNG CHANGES - OVERVIEW MODE
#if defined(SBROWSER_OVERVIEW_MODE)
bool WebViewImpl::isResizeHandled() const
{
    return m_isResizeHandled;
}
#endif  //SAMSUNG CHANGES - OVERVIEW MODE

float WebViewImpl::clampPageScaleFactorToLimits(float scaleFactor)
{
    return min(max(scaleFactor, m_minimumPageScaleFactor), m_maximumPageScaleFactor);
}

IntPoint WebViewImpl::clampOffsetAtScale(const IntPoint& offset, float scale) const
{
    IntPoint clampedOffset = offset;
    clampedOffset = clampedOffset.shrunkTo(IntPoint(contentsSize() - scaledSize(scale)));
    clampedOffset.clampNegativeToZero();

    return clampedOffset;
}

void WebViewImpl::setPageScaleFactorPreservingScrollOffset(float scaleFactor)
{
    scaleFactor = clampPageScaleFactorToLimits(scaleFactor);

    IntPoint scrollOffset(mainFrame()->scrollOffset().width, mainFrame()->scrollOffset().height);
    scrollOffset = clampOffsetAtScale(scrollOffset, scaleFactor);

    setPageScaleFactor(scaleFactor, scrollOffset);
}

void WebViewImpl::setPageScaleFactor(float scaleFactor, const WebPoint& origin)
{
    if (!page())
        return;

    if (!scaleFactor)
        scaleFactor = 1;

    IntPoint newScrollOffset = origin;
    scaleFactor = clampPageScaleFactorToLimits(scaleFactor);
    newScrollOffset = clampOffsetAtScale(newScrollOffset, scaleFactor);

    Frame* frame = page()->mainFrame();
    FrameView* view = frame->view();
    IntPoint oldScrollOffset = view->scrollPosition();

    // Adjust the page scale in two steps. First, without change to scroll
    // position, and then with a user scroll to the desired position.
    // We do this because Page::setPageScaleFactor calls
    // FrameView::setScrollPosition which is a programmatic scroll
    // and we need this method to perform only user scrolls.
    page()->setPageScaleFactor(scaleFactor, oldScrollOffset);
    if (newScrollOffset != oldScrollOffset)
        updateMainFrameScrollPosition(newScrollOffset, false);

    m_pageScaleFactorIsSet = true;
}

float WebViewImpl::deviceScaleFactor() const
{
    if (!page())
        return 1;

    return page()->deviceScaleFactor();
}

void WebViewImpl::setDeviceScaleFactor(float scaleFactor)
{
    if (!page())
        return;

    page()->setDeviceScaleFactor(scaleFactor);

    if (m_layerTreeView)
        m_layerTreeView->setDeviceScaleFactor(scaleFactor);
}

bool WebViewImpl::isFixedLayoutModeEnabled() const
{
    if (!page())
        return false;

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return false;

    return frame->view()->useFixedLayout();
}

void WebViewImpl::enableFixedLayoutMode(bool enable)
{
    if (!page())
        return;

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return;

    frame->view()->setUseFixedLayout(enable);

    // Also notify the base layer, which RenderLayerCompositor does not see.
    if (m_nonCompositedContentHost)
        updateLayerTreeViewport();
}


void WebViewImpl::enableAutoResizeMode(const WebSize& minSize, const WebSize& maxSize)
{
    m_shouldAutoResize = true;
    m_minAutoSize = minSize;
    m_maxAutoSize = maxSize;
    configureAutoResizeMode();
}

void WebViewImpl::disableAutoResizeMode()
{
    m_shouldAutoResize = false;
    configureAutoResizeMode();
}

void WebViewImpl::setPageScaleFactorLimits(float minPageScale, float maxPageScale)
{
    if (minPageScale == m_pageDefinedMinimumPageScaleFactor && maxPageScale == m_pageDefinedMaximumPageScaleFactor)
        return;

    m_pageDefinedMinimumPageScaleFactor = minPageScale;
    m_pageDefinedMaximumPageScaleFactor = maxPageScale;

    if (settings()->viewportEnabled()) {
        // If we're in viewport tag mode, we need to layout to obtain the latest
        // contents size and compute the final limits.
        FrameView* view = mainFrameImpl()->frameView();
        if (view)
            view->setNeedsLayout();
    } else {
        // Otherwise just compute the limits immediately.
        computePageScaleFactorLimits();
    }
}

void WebViewImpl::setIgnoreViewportTagMaximumScale(bool flag)
{
    m_ignoreViewportTagMaximumScale = flag;

    if (!page() || !page()->mainFrame())
        return;

    m_page->chrome()->client()->dispatchViewportPropertiesDidChange(page()->mainFrame()->document()->viewportArguments());
}

// ------------------- kikin Modification Starts --------------------
#if defined(SBROWSER_KIKIN)
void WebViewImpl::setKikinEnabled(bool flag)
{
    if (!m_page)
        return;

    if(!m_page->settings())
        return;

    m_page->settings()->setKikinSearchEnabled(flag);
}

bool WebViewImpl::kikinEnabled() const
{
    if (!m_page)
        return false;

    if(!m_page->settings())
        return false;

    return m_page->settings()->isKikinSearchEnabled();
}
#endif
// -------------------- kikin Modification Ends ---------------------

IntSize WebViewImpl::contentsSize() const
{
    RenderView* root = page()->mainFrame()->contentRenderer();
    if (!root)
        return IntSize();
    return root->documentRect().size();
}

void WebViewImpl::computePageScaleFactorLimits()
{
    if (!mainFrame() || !page() || !page()->mainFrame() || !page()->mainFrame()->view())
        return;
#if defined(SBROWSER_PLM_P131209_05168)
       if(mainFrameImpl() && mainFrameImpl()->frame() && mainFrameImpl()->frame()->document() && mainFrameImpl()->frame()->document()->printing())
	   return;
#endif
    FrameView* view = page()->mainFrame()->view();

    if (m_pageDefinedMinimumPageScaleFactor == -1 || m_pageDefinedMaximumPageScaleFactor == -1) {
        m_minimumPageScaleFactor = minPageScaleFactor;
        m_maximumPageScaleFactor = maxPageScaleFactor;
    } else {
        m_minimumPageScaleFactor = min(max(m_pageDefinedMinimumPageScaleFactor, minPageScaleFactor), maxPageScaleFactor);
        m_maximumPageScaleFactor = max(min(m_pageDefinedMaximumPageScaleFactor, maxPageScaleFactor), minPageScaleFactor);
    }

    if (settings()->viewportEnabled()) {
        if (!contentsSize().width() || !m_size.width)
            return;

        // When viewport tag is enabled, the scale needed to see the full
        // content width is the default minimum.
        int viewWidthNotIncludingScrollbars = m_size.width;
        if (view->verticalScrollbar() && !view->verticalScrollbar()->isOverlayScrollbar())
            viewWidthNotIncludingScrollbars -= view->verticalScrollbar()->width();
        m_minimumPageScaleFactor = max(m_minimumPageScaleFactor, static_cast<float>(viewWidthNotIncludingScrollbars) / contentsSize().width());
        m_maximumPageScaleFactor = max(m_minimumPageScaleFactor, m_maximumPageScaleFactor);
        if (m_initialPageScaleFactorOverride != -1) {
            m_minimumPageScaleFactor = min(m_minimumPageScaleFactor, m_initialPageScaleFactorOverride);
            m_maximumPageScaleFactor = max(m_maximumPageScaleFactor, m_initialPageScaleFactorOverride);
        }
    }
    ASSERT(m_minimumPageScaleFactor <= m_maximumPageScaleFactor);

    // Initialize and/or clamp the page scale factor if needed.
    float initialPageScaleFactor = m_initialPageScaleFactor;
    if (!settings()->viewportEnabled())
        initialPageScaleFactor = 1;
    if (m_initialPageScaleFactorOverride != -1)
        initialPageScaleFactor = m_initialPageScaleFactorOverride;
    float newPageScaleFactor = pageScaleFactor();
#if defined(SBROWSER_OVERVIEW_MODE)
    if(m_activateOverview){
      float oldPageScaleFactor = pageScaleFactor();
      float newContentWidth = static_cast<float>(contentsSize().width());
      float contentsWidthRatio;
      if(newContentWidth > m_oldContentWidth)
        contentsWidthRatio = m_oldContentWidth /newContentWidth;
      else
        contentsWidthRatio = newContentWidth/m_oldContentWidth;
      newPageScaleFactor = oldPageScaleFactor * contentsWidthRatio;
      m_oldContentWidth=contentsSize().width();
      m_activateOverview=false;
    }
#endif	
    if (!m_pageScaleFactorIsSet && initialPageScaleFactor != -1) {
        newPageScaleFactor = initialPageScaleFactor;
        m_pageScaleFactorIsSet = true;
    }
    newPageScaleFactor = clampPageScaleFactorToLimits(newPageScaleFactor);
    if (m_layerTreeView)
        m_layerTreeView->setPageScaleFactorAndLimits(newPageScaleFactor, m_minimumPageScaleFactor, m_maximumPageScaleFactor);
    if (newPageScaleFactor != pageScaleFactor())
        setPageScaleFactorPreservingScrollOffset(newPageScaleFactor);
}

float WebViewImpl::minimumPageScaleFactor() const
{
    return m_minimumPageScaleFactor;
}

float WebViewImpl::maximumPageScaleFactor() const
{
    return m_maximumPageScaleFactor;
}

void WebViewImpl::saveScrollAndScaleState()
{
    m_savedPageScaleFactor = pageScaleFactor();
    m_savedScrollOffset = mainFrame()->scrollOffset();
}

void WebViewImpl::restoreScrollAndScaleState()
{
    if (!m_savedPageScaleFactor)
        return;

    startPageScaleAnimation(IntPoint(m_savedScrollOffset), false, m_savedPageScaleFactor, scrollAndScaleAnimationDurationInSeconds);
    resetSavedScrollAndScaleState();
}

void WebViewImpl::resetSavedScrollAndScaleState()
{
    m_savedPageScaleFactor = 0;
    m_savedScrollOffset = IntSize();
}

void WebViewImpl::resetScrollAndScaleState()
{
    page()->setPageScaleFactor(1, IntPoint());

    // Clear out the values for the current history item. This will prevent the history item from clobbering the
    // value determined during page scale initialization, which may be less than 1.
    page()->mainFrame()->loader()->history()->saveDocumentAndScrollState();
    page()->mainFrame()->loader()->history()->clearScrollPositionAndViewState();
    m_pageScaleFactorIsSet = false;

    // Clobber saved scales and scroll offsets.
    if (FrameView* view = page()->mainFrame()->document()->view())
        view->cacheCurrentScrollPosition();
    resetSavedScrollAndScaleState();
}

WebSize WebViewImpl::fixedLayoutSize() const
{
    if (!page())
        return WebSize();

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return WebSize();

    return frame->view()->fixedLayoutSize();
}

void WebViewImpl::setFixedLayoutSize(const WebSize& layoutSize)
{
    if (!page())
        return;

    Frame* frame = page()->mainFrame();
    if (!frame || !frame->view())
        return;

    frame->view()->setFixedLayoutSize(layoutSize);
}

void WebViewImpl::performMediaPlayerAction(const WebMediaPlayerAction& action,
                                           const WebPoint& location)
{
    HitTestResult result = hitTestResultForWindowPos(location);
    RefPtr<Node> node = result.innerNonSharedNode();
    if (!node->hasTagName(HTMLNames::videoTag) && !node->hasTagName(HTMLNames::audioTag))
        return;

    RefPtr<HTMLMediaElement> mediaElement =
        static_pointer_cast<HTMLMediaElement>(node);
    switch (action.type) {
    case WebMediaPlayerAction::Play:
        if (action.enable)
            mediaElement->play();
        else
            mediaElement->pause();
        break;
    case WebMediaPlayerAction::Mute:
        mediaElement->setMuted(action.enable);
        break;
    case WebMediaPlayerAction::Loop:
        mediaElement->setLoop(action.enable);
        break;
    case WebMediaPlayerAction::Controls:
        mediaElement->setControls(action.enable);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void WebViewImpl::performPluginAction(const WebPluginAction& action,
                                      const WebPoint& location)
{
    HitTestResult result = hitTestResultForWindowPos(location);
    RefPtr<Node> node = result.innerNonSharedNode();
    if (!node->hasTagName(HTMLNames::objectTag) && !node->hasTagName(HTMLNames::embedTag))
        return;

    RenderObject* object = node->renderer();
    if (object && object->isWidget()) {
        Widget* widget = toRenderWidget(object)->widget();
        if (widget && widget->isPluginContainer()) {
            WebPluginContainerImpl* plugin = static_cast<WebPluginContainerImpl*>(widget);
            switch (action.type) {
            case WebPluginAction::Rotate90Clockwise:
                plugin->plugin()->rotateView(WebPlugin::RotationType90Clockwise);
                break;
            case WebPluginAction::Rotate90Counterclockwise:
                plugin->plugin()->rotateView(WebPlugin::RotationType90Counterclockwise);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
    }
}

WebHitTestResult WebViewImpl::hitTestResultAt(const WebPoint& point)
{
    return hitTestResultForWindowPos(point);
}

#if defined(SBROWSER_HOVER_HIGHLIGHT)
void WebViewImpl::HoverHighLight(const WebGestureEvent& hover_event,bool high_light)
{
	PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), hover_event);
	if( high_light) {
	  if (settingsImpl()->gestureTapHighlightEnabled())
		  enableHoverHighlight(platformEvent,high_light);
	}
	else {
	  m_prevHoverNode = 0;
	  if (m_linkHighlight)
		  m_linkHighlight->startHighlightAnimationIfNeeded();
	}
}
#endif

void WebViewImpl::copyImageAt(const WebPoint& point)
{
    if (!m_page)
        return;

    HitTestResult result = hitTestResultForWindowPos(point);

    if (result.absoluteImageURL().isEmpty()) {
        // There isn't actually an image at these coordinates.  Might be because
        // the window scrolled while the context menu was open or because the page
        // changed itself between when we thought there was an image here and when
        // we actually tried to retreive the image.
        //
        // FIXME: implement a cache of the most recent HitTestResult to avoid having
        //        to do two hit tests.
        return;
    }

    m_page->mainFrame()->editor()->copyImage(result);
}

void WebViewImpl::dragSourceEndedAt(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperation operation)
{
    PlatformMouseEvent pme(clientPoint,
                           screenPoint,
                           LeftButton, PlatformEvent::MouseMoved, 0, false, false, false,
                           false, 0);
    m_page->mainFrame()->eventHandler()->dragSourceEndedAt(pme,
        static_cast<DragOperation>(operation));
}

void WebViewImpl::dragSourceMovedTo(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperation operation)
{
}

void WebViewImpl::dragSourceSystemDragEnded()
{
    // It's possible for us to get this callback while not doing a drag if
    // it's from a previous page that got unloaded.
    if (m_doingDragAndDrop) {
        m_page->dragController()->dragEnded();
        m_doingDragAndDrop = false;
    }
}

WebDragOperation WebViewImpl::dragTargetDragEnter(
    const WebDragData& webDragData,
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int keyModifiers)
{
    ASSERT(!m_currentDragData);

    m_currentDragData = webDragData;
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragEnter, keyModifiers);
}

WebDragOperation WebViewImpl::dragTargetDragOver(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int keyModifiers)
{
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragOver, keyModifiers);
}

void WebViewImpl::dragTargetDragLeave()
{
    ASSERT(m_currentDragData);

    DragData dragData(
        m_currentDragData.get(),
        IntPoint(),
        IntPoint(),
        static_cast<DragOperation>(m_operationsAllowed));

    m_page->dragController()->dragExited(&dragData);

    // FIXME: why is the drag scroll timer not stopped here?

    m_dragOperation = WebDragOperationNone;
    m_currentDragData = 0;
}

void WebViewImpl::dragTargetDrop(const WebPoint& clientPoint,
                                 const WebPoint& screenPoint,
                                 int keyModifiers)
{
    ASSERT(m_currentDragData);

    // If this webview transitions from the "drop accepting" state to the "not
    // accepting" state, then our IPC message reply indicating that may be in-
    // flight, or else delayed by javascript processing in this webview.  If a
    // drop happens before our IPC reply has reached the browser process, then
    // the browser forwards the drop to this webview.  So only allow a drop to
    // proceed if our webview m_dragOperation state is not DragOperationNone.

    if (m_dragOperation == WebDragOperationNone) { // IPC RACE CONDITION: do not allow this drop.
        dragTargetDragLeave();
        return;
    }

    m_currentDragData->setModifierKeyState(webInputEventKeyStateToPlatformEventKeyState(keyModifiers));
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    m_page->dragController()->performDrag(&dragData);

    m_dragOperation = WebDragOperationNone;
    m_currentDragData = 0;
}

void WebViewImpl::spellingMarkers(WebVector<uint32_t>* markers)
{
    Vector<uint32_t> result;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        const Vector<DocumentMarker*>& documentMarkers = frame->document()->markers()->markers();
        for (size_t i = 0; i < documentMarkers.size(); ++i)
            result.append(documentMarkers[i]->hash());
    }
    markers->assign(result);
}

WebDragOperation WebViewImpl::dragTargetDragEnterOrOver(const WebPoint& clientPoint, const WebPoint& screenPoint, DragAction dragAction, int keyModifiers)
{
    ASSERT(m_currentDragData);

    m_currentDragData->setModifierKeyState(webInputEventKeyStateToPlatformEventKeyState(keyModifiers));
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    DragSession dragSession;
    if (dragAction == DragEnter)
        dragSession = m_page->dragController()->dragEntered(&dragData);
    else
        dragSession = m_page->dragController()->dragUpdated(&dragData);

    DragOperation dropEffect = dragSession.operation;

    // Mask the drop effect operation against the drag source's allowed operations.
    if (!(dropEffect & dragData.draggingSourceOperationMask()))
        dropEffect = DragOperationNone;

     m_dragOperation = static_cast<WebDragOperation>(dropEffect);

    return m_dragOperation;
}

void WebViewImpl::sendResizeEventAndRepaint()
{
    if (mainFrameImpl()->frameView()) {
        // Enqueues the resize event.
        mainFrameImpl()->frame()->eventHandler()->sendResizeEvent();
    }

    if (m_client) {
        if (isAcceleratedCompositingActive()) {
            updateLayerTreeViewport();
        } else {
            WebRect damagedRect(0, 0, m_size.width, m_size.height);
            m_client->didInvalidateRect(damagedRect);
        }
    }
    if (m_pageOverlays)
        m_pageOverlays->update();
}

void WebViewImpl::configureAutoResizeMode()
{
    if (!mainFrameImpl() || !mainFrameImpl()->frame() || !mainFrameImpl()->frame()->view())
        return;

    mainFrameImpl()->frame()->view()->enableAutoSizeMode(m_shouldAutoResize, m_minAutoSize, m_maxAutoSize);
}

unsigned long WebViewImpl::createUniqueIdentifierForRequest()
{
    return createUniqueIdentifier();
}

void WebViewImpl::inspectElementAt(const WebPoint& point)
{
    if (!m_page)
        return;

    if (point.x == -1 || point.y == -1)
        m_page->inspectorController()->inspect(0);
    else {
        HitTestResult result = hitTestResultForWindowPos(point);

        if (!result.innerNonSharedNode())
            return;

        m_page->inspectorController()->inspect(result.innerNonSharedNode());
    }
}

WebString WebViewImpl::inspectorSettings() const
{
    return m_inspectorSettings;
}

void WebViewImpl::setInspectorSettings(const WebString& settings)
{
    m_inspectorSettings = settings;
}

bool WebViewImpl::inspectorSetting(const WebString& key, WebString* value) const
{
    if (!m_inspectorSettingsMap->contains(key))
        return false;
    *value = m_inspectorSettingsMap->get(key);
    return true;
}

void WebViewImpl::setInspectorSetting(const WebString& key,
                                      const WebString& value)
{
    m_inspectorSettingsMap->set(key, value);
    client()->didUpdateInspectorSetting(key, value);
}

WebDevToolsAgent* WebViewImpl::devToolsAgent()
{
    return m_devToolsAgent.get();
}

WebAccessibilityObject WebViewImpl::accessibilityObject()
{
    if (!mainFrameImpl())
        return WebAccessibilityObject();

    Document* document = mainFrameImpl()->frame()->document();
    return WebAccessibilityObject(
        document->axObjectCache()->getOrCreate(document->renderer()));
}

void WebViewImpl::applyAutofillSuggestions(
    const WebNode& node,
    const WebVector<WebString>& names,
    const WebVector<WebString>& labels,
    const WebVector<WebString>& icons,
    const WebVector<int>& itemIDs,
    int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(names.size() == itemIDs.size());

    if (names.isEmpty()) {
        hideAutofillPopup();
        return;
    }

    RefPtr<Node> focusedNode = focusedWebCoreNode();
    // If the node for which we queried the Autofill suggestions is not the
    // focused node, then we have nothing to do.  FIXME: also check the
    // caret is at the end and that the text has not changed.
    if (!focusedNode || focusedNode != PassRefPtr<Node>(node)) {
        hideAutofillPopup();
        return;
    }

    HTMLInputElement* inputElem = focusedNode->toInputElement();
    ASSERT(inputElem);

    // The first time the Autofill popup is shown we'll create the client and
    // the popup.
    if (!m_autofillPopupClient)
        m_autofillPopupClient = adoptPtr(new AutofillPopupMenuClient);

    m_autofillPopupClient->initialize(
        inputElem, names, labels, icons, itemIDs, separatorIndex);

    if (!m_autofillPopup) {
        PopupContainerSettings popupSettings = autofillPopupSettings;
        popupSettings.deviceSupportsTouch = settingsImpl()->deviceSupportsTouch();
        m_autofillPopup = PopupContainer::create(m_autofillPopupClient.get(),
                                                 PopupContainer::Suggestion,
                                                 popupSettings);
    }

    if (m_autofillPopupShowing) {
        refreshAutofillPopup();
    } else {
        m_autofillPopupShowing = true;
        m_autofillPopup->showInRect(focusedNode->pixelSnappedBoundingBox(), focusedNode->ownerDocument()->view(), 0);
    }
}

void WebViewImpl::hidePopups()
{
    hideSelectPopup();
    hideAutofillPopup();
#if ENABLE(PAGE_POPUP)
    if (m_pagePopup)
        closePagePopup(m_pagePopup.get());
#endif
}

void WebViewImpl::performCustomContextMenuAction(unsigned action)
{
    if (!m_page)
        return;
    ContextMenu* menu = m_page->contextMenuController()->contextMenu();
    if (!menu)
        return;
    const ContextMenuItem* item = menu->itemWithAction(static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + action));
    if (item)
        m_page->contextMenuController()->contextMenuItemSelected(item);
    m_page->contextMenuController()->clearContextMenu();
}

void WebViewImpl::showContextMenu()
{
    if (!page())
        return;

    page()->contextMenuController()->clearContextMenu();
    m_contextMenuAllowed = true;
    if (Frame* focusedFrame = page()->focusController()->focusedOrMainFrame())
        focusedFrame->eventHandler()->sendContextMenuEventForKey();
    m_contextMenuAllowed = false;
}

// WebView --------------------------------------------------------------------

void WebViewImpl::setIsTransparent(bool isTransparent)
{
    // Set any existing frames to be transparent.
    Frame* frame = m_page->mainFrame();
    while (frame) {
        frame->view()->setTransparent(isTransparent);
        frame = frame->tree()->traverseNext();
    }

    // Future frames check this to know whether to be transparent.
    m_isTransparent = isTransparent;

    if (m_nonCompositedContentHost)
        m_nonCompositedContentHost->setOpaque(!isTransparent);
}

bool WebViewImpl::isTransparent() const
{
    return m_isTransparent;
}

void WebViewImpl::setIsActive(bool active)
{
    if (page() && page()->focusController())
        page()->focusController()->setActive(active);
}

bool WebViewImpl::isActive() const
{
    return (page() && page()->focusController()) ? page()->focusController()->isActive() : false;
}

void WebViewImpl::setDomainRelaxationForbidden(bool forbidden, const WebString& scheme)
{
    SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(forbidden, String(scheme));
}

void WebViewImpl::setScrollbarColors(unsigned inactiveColor,
                                     unsigned activeColor,
                                     unsigned trackColor) {
#if ENABLE(DEFAULT_RENDER_THEME)
    PlatformThemeChromiumDefault::setScrollbarColors(inactiveColor, activeColor, trackColor);
#endif
}

void WebViewImpl::setSelectionColors(unsigned activeBackgroundColor,
                                     unsigned activeForegroundColor,
                                     unsigned inactiveBackgroundColor,
                                     unsigned inactiveForegroundColor) {
#if ENABLE(DEFAULT_RENDER_THEME)
    RenderThemeChromiumDefault::setSelectionColors(activeBackgroundColor, activeForegroundColor, inactiveBackgroundColor, inactiveForegroundColor);
    theme()->platformColorsDidChange();
#endif
}

void WebView::addUserStyleSheet(const WebString& sourceCode,
                                const WebVector<WebString>& patternsIn,
                                WebView::UserContentInjectIn injectIn,
                                WebView::UserStyleInjectionTime injectionTime)
{
    Vector<String> patterns;
    for (size_t i = 0; i < patternsIn.size(); ++i)
        patterns.append(patternsIn[i]);

    PageGroup* pageGroup = PageGroup::sharedGroup();

    // FIXME: Current callers always want the level to be "author". It probably makes sense to let
    // callers specify this though, since in other cases the caller will probably want "user" level.
    //
    // FIXME: It would be nice to populate the URL correctly, instead of passing an empty URL.
    pageGroup->addUserStyleSheet(sourceCode, WebURL(), patterns, Vector<String>(),
                                 static_cast<UserContentInjectedFrames>(injectIn),
                                 UserStyleAuthorLevel,
                                 static_cast<WebCore::UserStyleInjectionTime>(injectionTime));
}

void WebView::removeAllUserContent()
{
    PageGroup::sharedGroup()->removeAllUserContent();
}

void WebViewImpl::didCommitLoad(bool* isNewNavigation, bool isNavigationWithinPage)
{
    if (isNewNavigation)
        *isNewNavigation = m_observedNewNavigation;

#ifndef NDEBUG
    ASSERT(!m_observedNewNavigation
        || m_page->mainFrame()->loader()->documentLoader() == m_newNavigationLoader);
    m_newNavigationLoader = 0;
#endif
    m_observedNewNavigation = false;
    if (*isNewNavigation && !isNavigationWithinPage){
        m_pageScaleFactorIsSet = false;
#if defined(SBROWSER_OVERVIEW_MODE)
        m_oldContentWidth=0;
#endif
    }
    // Make sure link highlight from previous page is cleared.
    m_linkHighlight.clear();
    m_gestureAnimation.clear();
    if (m_layerTreeView)
        m_layerTreeView->didStopFlinging();
    resetSavedScrollAndScaleState();
}

void WebViewImpl::layoutUpdated(WebFrameImpl* webframe)
{
    if (!m_client || webframe != mainFrameImpl())
        return;

    if (m_shouldAutoResize && mainFrameImpl()->frame() && mainFrameImpl()->frame()->view()) {
        WebSize frameSize = mainFrameImpl()->frame()->view()->frameRect().size();
        if (frameSize != m_size) {
            m_size = frameSize;
            m_client->didAutoResize(m_size);
            sendResizeEventAndRepaint();
        }
    }

    if (settings()->viewportEnabled()) {
        if (!isPageScaleFactorSet()) {
            // If the viewport tag failed to be processed earlier, we need
            // to recompute it now.
            ViewportArguments viewportArguments = mainFrameImpl()->frame()->document()->viewportArguments();
            m_page->chrome()->client()->dispatchViewportPropertiesDidChange(viewportArguments);
        }

        // Contents size is an input to the page scale limits, so a good time to
        // recalculate is after layout has occurred.
        computePageScaleFactorLimits();

        // Relayout immediately to avoid violating the rule that needsLayout()
        // isn't set at the end of a layout.
        FrameView* view = mainFrameImpl()->frameView();
        if (view && view->needsLayout())
            view->layout();
    }

    m_client->didUpdateLayout();

}
#if defined(SBROWSER_OVERVIEW_MODE)
void WebViewImpl::activateOverviewMode()
{
    if(contentsSize().width()!= m_oldContentWidth){
      m_activateOverview=true;
    }
}
#endif
void WebViewImpl::didChangeContentsSize()
{
}

void WebViewImpl::deviceOrPageScaleFactorChanged()
{
    if (pageScaleFactor() && pageScaleFactor() != 1)
        enterForceCompositingMode(true);
    updateLayerTreeViewport();
}

bool WebViewImpl::useExternalPopupMenus()
{
    return shouldUseExternalPopupMenus;
}

void WebViewImpl::setEmulatedTextZoomFactor(float textZoomFactor)
{
    m_emulatedTextZoomFactor = textZoomFactor;
    Frame* frame = mainFrameImpl()->frame();
    if (frame)
        frame->setPageAndTextZoomFactors(frame->pageZoomFactor(), m_emulatedTextZoomFactor);
}

bool WebViewImpl::navigationPolicyFromMouseEvent(unsigned short button,
                                                 bool ctrl, bool shift,
                                                 bool alt, bool meta,
                                                 WebNavigationPolicy* policy)
{
#if OS(DARWIN)
    const bool newTabModifier = (button == 1) || meta;
#else
    const bool newTabModifier = (button == 1) || ctrl;
#endif
    if (!newTabModifier && !shift && !alt)
      return false;

    ASSERT(policy);
    if (newTabModifier) {
        if (shift)
          *policy = WebNavigationPolicyNewForegroundTab;
        else
          *policy = WebNavigationPolicyNewBackgroundTab;
    } else {
        if (shift)
          *policy = WebNavigationPolicyNewWindow;
        else
          *policy = WebNavigationPolicyDownload;
    }
    return true;
}

void WebViewImpl::startDragging(Frame* frame,
                                const WebDragData& dragData,
                                WebDragOperationsMask mask,
                                const WebImage& dragImage,
                                const WebPoint& dragImageOffset)
{
    if (!m_client)
        return;
    ASSERT(!m_doingDragAndDrop);
    m_doingDragAndDrop = true;
    m_client->startDragging(WebFrameImpl::fromFrame(frame), dragData, mask, dragImage, dragImageOffset);
}

void WebViewImpl::observeNewNavigation()
{
    m_observedNewNavigation = true;
#ifndef NDEBUG
    m_newNavigationLoader = m_page->mainFrame()->loader()->documentLoader();
#endif
}

void WebViewImpl::setIgnoreInputEvents(bool newValue)
{
    ASSERT(m_ignoreInputEvents != newValue);
    m_ignoreInputEvents = newValue;
}

void WebViewImpl::addPageOverlay(WebPageOverlay* overlay, int zOrder)
{
    if (!m_pageOverlays)
        m_pageOverlays = PageOverlayList::create(this);

    m_pageOverlays->add(overlay, zOrder);
}

void WebViewImpl::removePageOverlay(WebPageOverlay* overlay)
{
    if (m_pageOverlays && m_pageOverlays->remove(overlay) && m_pageOverlays->empty())
        m_pageOverlays = nullptr;
}

void WebViewImpl::setOverlayLayer(WebCore::GraphicsLayer* layer)
{
    if (m_rootGraphicsLayer) {
        if (layer->parent() != m_rootGraphicsLayer)
            m_rootGraphicsLayer->addChild(layer);
    }
}

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
NotificationPresenterImpl* WebViewImpl::notificationPresenterImpl()
{
    if (!m_notificationPresenter.isInitialized() && m_client)
        m_notificationPresenter.initialize(m_client->notificationPresenter());
    return &m_notificationPresenter;
}
#endif

#if defined(SBROWSER_IMAGEDRAG_IMPL)

void WebViewImpl::getImageBitmapForDragging(int x, int y,int *width,int *height,SkBitmap* bmp){

    ScrollView* rootView = mainFrameImpl()->frameView()->root();
    float scale = rootView?rootView->visibleContentScaleFactor():1;
    WebPoint point = mainFrameImpl()->frameView()->convertFromContainingWindow(IntPoint(x / scale, y / scale));
    IntPoint contentPoint = m_page->mainFrame()->view()->windowToContents(point);

	HitTestResult result = mainFrameImpl()->frame()->eventHandler()->hitTestResultAtPointCustomized(contentPoint.x(), contentPoint.y(), 4);
	Node* hitNode = result.innerNonSharedNode();

	if (hitNode && hitNode->renderer() && hitNode->renderer()->isImage())
	{
		RenderBox* box = toRenderBox(hitNode->renderer());
		LayoutUnit wWidth = box->contentWidth();
		LayoutUnit hHeight = box->contentHeight();
		Image* img = result.image();
		*width = wWidth;
		*height = hHeight;

		if(img != NULL){
			NativeImagePtr bitmapPtr = img->nativeImageForCurrentFrame();
			if(bitmapPtr){
                        *bmp = bitmapPtr->bitmap();
                        }
		}
	}

}
#endif

void WebViewImpl::refreshAutofillPopup()
{
    ASSERT(m_autofillPopupShowing);

    // Hide the popup if it has become empty.
    if (!m_autofillPopupClient->listSize()) {
        hideAutofillPopup();
        return;
    }

    WebRect newWidgetRect = m_autofillPopup->refresh(focusedWebCoreNode()->pixelSnappedBoundingBox());
    // Let's resize the backing window if necessary.
    WebPopupMenuImpl* popupMenu = static_cast<WebPopupMenuImpl*>(m_autofillPopup->client());
    if (popupMenu && popupMenu->client()->windowRect() != newWidgetRect)
        popupMenu->client()->setWindowRect(newWidgetRect);
}

Node* WebViewImpl::focusedWebCoreNode()
{
    Frame* frame = m_page->focusController()->focusedFrame();
    if (!frame)
        return 0;

    Document* document = frame->document();
    if (!document)
        return 0;

    return document->focusedNode();
}

HitTestResult WebViewImpl::hitTestResultForWindowPos(const IntPoint& pos)
{
    IntPoint docPoint(m_page->mainFrame()->view()->windowToContents(pos));
    return m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(docPoint);
}

void WebViewImpl::setTabsToLinks(bool enable)
{
    m_tabsToLinks = enable;
}

bool WebViewImpl::tabsToLinks() const
{
    return m_tabsToLinks;
}

void WebViewImpl::suppressInvalidations(bool enable)
{
    if (m_client)
        m_client->suppressCompositorScheduling(enable);
}

bool WebViewImpl::allowsAcceleratedCompositing()
{
    return !m_compositorCreationFailed;
}

void WebViewImpl::setRootGraphicsLayer(GraphicsLayer* layer)
{
    suppressInvalidations(true);

    m_rootGraphicsLayer = layer;
    m_rootLayer = layer ? layer->platformLayer() : 0;

    setIsAcceleratedCompositingActive(layer);
    if (m_nonCompositedContentHost) {
        GraphicsLayer* scrollLayer = 0;
        if (layer) {
            Document* document = page()->mainFrame()->document();
            RenderView* renderView = document->renderView();
            RenderLayerCompositor* compositor = renderView->compositor();
            scrollLayer = compositor->scrollLayer();
        }
        m_nonCompositedContentHost->setScrollLayer(scrollLayer);
    }

    if (m_layerTreeView) {
        if (m_rootLayer)
            m_layerTreeView->setRootLayer(*m_rootLayer);
        else
            m_layerTreeView->clearRootLayer();
    }

    suppressInvalidations(false);
}

void WebViewImpl::scheduleCompositingLayerSync()
{
    m_layerTreeView->setNeedsRedraw();
}

void WebViewImpl::scrollRootLayerRect(const IntSize&, const IntRect&)
{
    updateLayerTreeViewport();
}

void WebViewImpl::invalidateRect(const IntRect& rect)
{
    if (m_layerTreeViewCommitsDeferred) {
        // If we receive an invalidation from WebKit while in deferred commit mode,
        // that means it's time to start producing frames again so un-defer.
        if (m_layerTreeView)
            m_layerTreeView->setDeferCommits(false);
        m_layerTreeViewCommitsDeferred = false;
    }
    if (m_isAcceleratedCompositingActive) {
        ASSERT(m_layerTreeView);

        if (!page())
            return;

        FrameView* view = page()->mainFrame()->view();
        IntRect dirtyRect = view->windowToContents(rect);
        updateLayerTreeViewport();
        m_nonCompositedContentHost->invalidateRect(dirtyRect);
    } else if (m_client)
        m_client->didInvalidateRect(rect);
}

NonCompositedContentHost* WebViewImpl::nonCompositedContentHost()
{
    return m_nonCompositedContentHost.get();
}

void WebViewImpl::setBackgroundColor(const WebCore::Color& color)
{
    WebCore::Color documentBackgroundColor = color.isValid() ? color : WebCore::Color::white;
    WebColor webDocumentBackgroundColor = documentBackgroundColor.rgb();
    m_nonCompositedContentHost->setBackgroundColor(documentBackgroundColor);
    m_layerTreeView->setBackgroundColor(webDocumentBackgroundColor);
}

WebCore::GraphicsLayerFactory* WebViewImpl::graphicsLayerFactory() const
{
    return m_graphicsLayerFactory.get();
}

void WebViewImpl::registerForAnimations(WebLayer* layer)
{
    if (m_layerTreeView)
        m_layerTreeView->registerForAnimations(layer);
}

WebCore::GraphicsLayer* WebViewImpl::rootGraphicsLayer()
{
    return m_rootGraphicsLayer;
}

void WebViewImpl::scheduleAnimation()
{
    if (isAcceleratedCompositingActive()) {
        if (Platform::current()->isThreadedCompositingEnabled()) {
            ASSERT(m_layerTreeView);
            m_layerTreeView->setNeedsAnimate();
        } else
            m_client->scheduleAnimation();
    } else
            m_client->scheduleAnimation();
}

void WebViewImpl::paintRootLayer(GraphicsContext& context, const IntRect& contentRect)
{
    double paintStart = currentTime();
    if (!page())
        return;
    FrameView* view = page()->mainFrame()->view();
    context.setUseHighResMarkers(page()->deviceScaleFactor() > 1.5f);
    view->paintContents(&context, contentRect);
    double paintEnd = currentTime();
    double pixelsPerSec = (contentRect.width() * contentRect.height()) / (paintEnd - paintStart);
    WebKit::Platform::current()->histogramCustomCounts("Renderer4.AccelRootPaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
    WebKit::Platform::current()->histogramCustomCounts("Renderer4.AccelRootPaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);

    setBackgroundColor(view->documentBackgroundColor());
}

void WebViewImpl::setIsAcceleratedCompositingActive(bool active)
{
    WebKit::Platform::current()->histogramEnumeration("GPU.setIsAcceleratedCompositingActive", active * 2 + m_isAcceleratedCompositingActive, 4);

    if (m_isAcceleratedCompositingActive == active)
        return;

    if (!active) {
        m_isAcceleratedCompositingActive = false;
        // We need to finish all GL rendering before sending didDeactivateCompositor() to prevent
        // flickering when compositing turns off. This is only necessary if we're not in
        // force-compositing-mode.
        if (m_layerTreeView && !page()->settings()->forceCompositingMode())
            m_layerTreeView->finishAllRendering();
        m_client->didDeactivateCompositor();
        if (!m_layerTreeViewCommitsDeferred
            && WebKit::Platform::current()->isThreadedCompositingEnabled()) {
            ASSERT(m_layerTreeView);
            // In threaded compositing mode, force compositing mode is always on so setIsAcceleratedCompositingActive(false)
            // means that we're transitioning to a new page. Suppress commits until WebKit generates invalidations so
            // we don't attempt to paint too early in the next page load.
            m_layerTreeView->setDeferCommits(true);
            m_layerTreeViewCommitsDeferred = true;
        }
    } else if (m_layerTreeView) {
        m_isAcceleratedCompositingActive = true;
        updateLayerTreeViewport();

        m_client->didActivateCompositor(m_inputHandlerIdentifier);
    } else {
        TRACE_EVENT0("webkit", "WebViewImpl::setIsAcceleratedCompositingActive(true)");

        m_nonCompositedContentHost = NonCompositedContentHost::create(this, graphicsLayerFactory());
        m_nonCompositedContentHost->setShowDebugBorders(page()->settings()->showDebugBorders());
        m_nonCompositedContentHost->setOpaque(!isTransparent());

        m_client->initializeLayerTreeView();
        m_layerTreeView = m_client->layerTreeView();
        if (m_layerTreeView) {
            m_layerTreeView->setRootLayer(*m_rootLayer);

            bool visible = page()->visibilityState() == PageVisibilityStateVisible;
            m_layerTreeView->setVisible(visible);
            m_layerTreeView->setDeviceScaleFactor(page()->deviceScaleFactor());
            m_layerTreeView->setPageScaleFactorAndLimits(pageScaleFactor(), m_minimumPageScaleFactor, m_maximumPageScaleFactor);
            m_layerTreeView->setHasTransparentBackground(isTransparent());
            updateLayerTreeViewport();
            m_client->didActivateCompositor(m_inputHandlerIdentifier);
            m_isAcceleratedCompositingActive = true;
            m_compositorCreationFailed = false;
            if (m_pageOverlays)
                m_pageOverlays->update();
            m_layerTreeView->setShowFPSCounter(m_showFPSCounter);
            m_layerTreeView->setShowPaintRects(m_showPaintRects);
            m_layerTreeView->setShowDebugBorders(m_showDebugBorders);
            m_layerTreeView->setContinuousPaintingEnabled(m_continuousPaintingEnabled);
        } else {
            m_nonCompositedContentHost.clear();
            m_isAcceleratedCompositingActive = false;
            m_client->didDeactivateCompositor();
            m_compositorCreationFailed = true;
        }
    }
    if (page())
        page()->mainFrame()->view()->setClipsRepaints(!m_isAcceleratedCompositingActive);
}

WebInputHandler* WebViewImpl::createInputHandler()
{
    WebCompositorInputHandlerImpl* handler = new WebCompositorInputHandlerImpl();
    m_inputHandlerIdentifier = handler->identifier();
    return handler;
}

void WebViewImpl::updateMainFrameScrollPosition(const IntPoint& scrollPosition, bool programmaticScroll)
{
    FrameView* frameView = page()->mainFrame()->view();
    if (!frameView)
        return;

    if (frameView->scrollPosition() == scrollPosition)
        return;

    bool oldProgrammaticScroll = frameView->inProgrammaticScroll();
    frameView->setInProgrammaticScroll(programmaticScroll);
    frameView->notifyScrollPositionChanged(scrollPosition);
    frameView->setInProgrammaticScroll(oldProgrammaticScroll);
}

void WebViewImpl::applyScrollAndScale(const WebSize& scrollDelta, float pageScaleDelta)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    if (pageScaleDelta == 1) {
        TRACE_EVENT_INSTANT2("webkit", "WebViewImpl::applyScrollAndScale::scrollBy", "x", scrollDelta.width, "y", scrollDelta.height);
        WebSize webScrollOffset = mainFrame()->scrollOffset();
        IntPoint scrollOffset(webScrollOffset.width + scrollDelta.width, webScrollOffset.height + scrollDelta.height);
        updateMainFrameScrollPosition(scrollOffset, false);
    } else {
        // The page scale changed, so apply a scale and scroll in a single
        // operation.
        WebSize scrollOffset = mainFrame()->scrollOffset();
        scrollOffset.width += scrollDelta.width;
        scrollOffset.height += scrollDelta.height;

        WebPoint scrollPoint(scrollOffset.width, scrollOffset.height);
        setPageScaleFactor(pageScaleFactor() * pageScaleDelta, scrollPoint);
        m_doubleTapZoomPending = false;
    }
#if defined (SBROWSER_SCROLL_INPUTTEXT)
    TextFieldsBoundsChanged(true);
#endif
}

void WebViewImpl::didExitCompositingMode()
{
    ASSERT(m_isAcceleratedCompositingActive);
    setIsAcceleratedCompositingActive(false);
    m_compositorCreationFailed = true;
    m_client->didInvalidateRect(IntRect(0, 0, m_size.width, m_size.height));

    // Force a style recalc to remove all the composited layers.
    m_page->mainFrame()->document()->scheduleForcedStyleRecalc();

    if (m_pageOverlays)
        m_pageOverlays->update();
}

void WebViewImpl::updateLayerTreeViewport()
{
    if (!page() || !m_nonCompositedContentHost || !m_layerTreeView)
        return;

    FrameView* view = page()->mainFrame()->view();
    m_nonCompositedContentHost->setViewport(m_size, view->contentsSize(), view->scrollPosition(), view->scrollOrigin());
    m_layerTreeView->setPageScaleFactorAndLimits(pageScaleFactor(), m_minimumPageScaleFactor, m_maximumPageScaleFactor);
}

void WebViewImpl::selectAutofillSuggestionAtIndex(unsigned listIndex)
{
    if (m_autofillPopupClient && listIndex < m_autofillPopupClient->getSuggestionsCount())
        m_autofillPopupClient->valueChanged(listIndex);
}

bool WebViewImpl::detectContentOnTouch(const WebPoint& position)
{
    HitTestResult touchHit = hitTestResultForWindowPos(position);

    // samsung changes
    // in order to force to open newwindow in seperated process, href infomaiton is required to determine conditions
    //const PassRefPtr<WebCore::Element> url_element = (WebCore::Element*)touchHit.URLElement();
    //m_client->detectLinkOnTouch( url_element );

    if (touchHit.isContentEditable())
        return false;

    Node* node = touchHit.innerNode();
    if (!node || !node->isTextNode())
        return false;

    // Ignore when tapping on links or nodes listening to click events, unless the click event is on the
    // body element, in which case it's unlikely that the original node itself was intended to be clickable.
    for (; node && !node->hasTagName(HTMLNames::bodyTag); node = node->parentNode()) {
        if (node->isLink() || node->willRespondToTouchEvents() || node->willRespondToMouseClickEvents())
            return false;
    }

    WebContentDetectionResult content = m_client->detectContentAround(touchHit);
    if (!content.isValid())
        return false;
    #if defined(SBROWSER_LINKIFY_TEXT_SELECTION)
    if(mainFrameImpl())
        mainFrameImpl()->setContentDetectionResult(content);
    #endif

	#if defined(SBROWSER_CONTENT_HIGHLIGHT)
	if(!(content.range()).isNull()){
	  RefPtr<Range> range = static_cast<PassRefPtr<Range> >(content.range());
	  Node* touchNode = range->firstNode();
	  enableContentHighlight( touchNode);
	  if (m_linkHighlight)
			m_linkHighlight->startHighlightAnimationIfNeeded();
	 }
	#endif
    m_client->scheduleContentIntent(content.intent());
    return true;
}

void WebViewImpl::didProgrammaticallyScroll(const WebCore::IntPoint& scrollPoint)
{
    m_client->didProgrammaticallyScroll(scrollPoint);
}

void WebViewImpl::setVisibilityState(WebPageVisibilityState visibilityState,
                                     bool isInitialState) {
    if (!page())
        return;

    ASSERT(visibilityState == WebPageVisibilityStateVisible
           || visibilityState == WebPageVisibilityStateHidden
           || visibilityState == WebPageVisibilityStatePrerender
           || visibilityState == WebPageVisibilityStatePreview);
    m_page->setVisibilityState(static_cast<PageVisibilityState>(static_cast<int>(visibilityState)), isInitialState);

    if (m_layerTreeView) {
        bool visible = visibilityState == WebPageVisibilityStateVisible;
        m_layerTreeView->setVisible(visible);
    }
}

bool WebViewImpl::requestPointerLock()
{
    return m_client && m_client->requestPointerLock();
}

void WebViewImpl::requestPointerUnlock()
{
    if (m_client)
        m_client->requestPointerUnlock();
}

bool WebViewImpl::isPointerLocked()
{
    return m_client && m_client->isPointerLocked();
}

void WebViewImpl::pointerLockMouseEvent(const WebInputEvent& event)
{
    AtomicString eventType;
    switch (event.type) {
    case WebInputEvent::MouseDown:
        eventType = eventNames().mousedownEvent;
        break;
    case WebInputEvent::MouseUp:
        eventType = eventNames().mouseupEvent;
        break;
    case WebInputEvent::MouseMove:
        eventType = eventNames().mousemoveEvent;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    const WebMouseEvent& mouseEvent = static_cast<const WebMouseEvent&>(event);

    if (page())
        page()->pointerLockController()->dispatchLockedMouseEvent(
            PlatformMouseEventBuilder(mainFrameImpl()->frameView(), mouseEvent),
            eventType);
}

bool WebViewImpl::shouldDisableDesktopWorkarounds()
{
    ViewportArguments arguments = mainFrameImpl()->frame()->document()->viewportArguments();
    return arguments.width == ViewportArguments::ValueDeviceWidth || !arguments.userZoom
        || (arguments.minZoom == arguments.maxZoom && arguments.minZoom != ViewportArguments::ValueAuto);
}

// SAMSUNG : SmartClip >>
#if defined(SBROWSER_SMART_CLIP)
Node* WebViewImpl::findMinContainerNode(Node* pNode1, Node* pNode2)
{
    if(pNode1 == NULL && pNode2 == NULL)
        return NULL;

    if(pNode2 == NULL)
        return pNode1;
        
    if(pNode1 == NULL)
        return pNode2;
                 
    IntRect rect1 = pixelSnappedIntRect(pNode1->Node::boundingBox());
    IntRect rect2 = pixelSnappedIntRect(pNode2->Node::boundingBox());

    Node*  pareantNode1 = pNode1->parentNode();
    Node*  pareantNode2 = pNode2->parentNode();

    if(rect1.contains(rect2) == true)
    {
        if(pareantNode1 != NULL && pareantNode2 != NULL && pareantNode2->parentNode() ==pareantNode1) //fix issue in wikipedia
            return pareantNode1;
        else
            return pNode1;
    }

    if(rect2.contains(rect1) == true)
    {
        if(pareantNode1 != NULL && pareantNode2 != NULL && pareantNode1->parentNode() ==pareantNode2)
            return pareantNode2;
        else
            return pNode2;
    }


    Node* pCurNode = pNode1;
    while(pCurNode != NULL)
    {
        if(pCurNode->renderer() != NULL) 
        {
            IntRect curRect = pixelSnappedIntRect(pCurNode->Node::boundingBox());
            if(curRect.contains(rect2) == true)
            {
                return pCurNode;
            }
        }    
        pCurNode = pCurNode->parentNode();
    }

    return NULL;
}

WebString WebViewImpl::extractSmartClipData(WebRect checkRect)
{
    Frame* pFrame = focusedWebCoreFrame();
    if (!pFrame)
        return WebString();

    // Calculate absolute document coordinate
    IntRect absCheckRect(checkRect.x, checkRect.y, checkRect.width, checkRect.height);
    int x =(int)(absCheckRect.x()/pageScaleFactor());
    int y =(int)(absCheckRect.y()/pageScaleFactor());
    int w =(int)(absCheckRect.width()/pageScaleFactor());
    int h =(int)(absCheckRect.height()/pageScaleFactor());
    if(absCheckRect.width()>0 && w==0)//For drag&drop in smartclip
       w =1;
    if(absCheckRect.height()>0 && h==0)
       h =1;
     IntRect cropRect(x, y, w, h);


    String resultStr("");
    Vector<CroppedNodeInfo>* nodeInfoTable = new Vector<CroppedNodeInfo>();
    Vector<CroppedNodeInfo>* overlappedNodeInfoTable = new Vector<CroppedNodeInfo>();	

    if(pFrame != NULL && nodeInfoTable != NULL && overlappedNodeInfoTable != NULL )
    {
        Node* pBodyNode = pFrame->document()->body();
        Node* pNode = pBodyNode;
        Node* pMinContainer = NULL;
  
        pMinContainer= findMatchNodeForSmartClip(pNode, cropRect);
  
        // Set entire cropped rect & extract text which included in cropped rect.
        if(pMinContainer != NULL)
        {
            int cntOverlapNode=0; 
            IntRect containerRect(0,0,0,0);
            String  collectedText("");    // Extract all text which contains in pMinContainer.
           
            if(pMinContainer && pMinContainer->hasTagName(HTMLNames::iframeTag))
            {
                 Node* pIframeBody= getBodyNodeFromIframe(pMinContainer);
                 if(pIframeBody!=NULL)
                 {
                       Node* pMatchNode= findMatchNodeForSmartClip(pIframeBody, cropRect);
                       if(pMatchNode!=NULL)
                           pMinContainer=pMatchNode;
                 }
            }

            cntOverlapNode=findOverlappedChildNode(pMinContainer, cropRect, overlappedNodeInfoTable);
            if(cntOverlapNode == 0 ||cntOverlapNode==(int)(pMinContainer->childNodeCount()))
            {
                 collectedText = extractAllTextFromNode(pMinContainer);
                 containerRect = pixelSnappedIntRect(pMinContainer->Node::boundingBox());
            }
            else
            {
                 int tableSize=overlappedNodeInfoTable->size();
                 for (int i=0; i<tableSize; i++)
                 {
                     CroppedNodeInfo& nodeInfo = (*overlappedNodeInfoTable)[i];
                     collectedText.append(extractAllTextFromNode(nodeInfo.m_node)); 
                 }
                 containerRect= getOverlappedNodeArea(overlappedNodeInfoTable);
            }
            containerRect=convertToWindowNodeRect(pMinContainer,containerRect);
            nodeInfoTable->append(CroppedNodeInfo(pMinContainer, containerRect, collectedText));
            // Converts CroppedNodeInfo array to string which will transfer to Java layer.
            resultStr = convertCroppedNodeInfoTableToString(nodeInfoTable);
        }
        else
        {
          resultStr ="";
        }
    }    

    if (nodeInfoTable != NULL)
        delete nodeInfoTable;
    if (overlappedNodeInfoTable != NULL)
        delete overlappedNodeInfoTable;
    
    return resultStr;
}

Node* WebViewImpl::findMatchNodeForSmartClip(Node* pBodyNode, IntRect cropRect)
{
    Node* pNode = pBodyNode;
    Node* pMinContainer = NULL;
    IntRect reSizeCropRect;
    
    if(pNode!=NULL)
        reSizeCropRect= pNode->document()->view()->windowToContents(cropRect);
    
    while(pNode != NULL)
    {
        IntRect nodeRect =pixelSnappedIntRect(pNode->Node::boundingBox());
        RenderObject*  pRenderer = pNode->renderer();
        bool bAriaHiddenAttr= pNode->hasAttributes() && toElement(pNode)->getAttribute(HTMLNames::aria_hiddenAttr)=="true";
        
        if(pRenderer != NULL && (nodeRect.width() > 0 || nodeRect.height() > 0))
        {
            if(bAriaHiddenAttr) //for daum mobile stie
            {
                pNode = NodeTraversal::nextSkippingChildren(pNode, pBodyNode); 
                continue;
            }
            
            if(pRenderer->isText() || pRenderer->isRenderImage() || pNode->hasTagName(HTMLNames::iframeTag)
                || (pRenderer->style()->hasBackgroundImage() && !isSkipBackgroundImage(pNode)))
            {
                if(reSizeCropRect.intersects(nodeRect) == true)
                {
                    pMinContainer = findMinContainerNode(pMinContainer, pNode);    
                }
                else
                {
                    pNode = NodeTraversal::nextSkippingChildren(pNode, pBodyNode); 
                    continue;
                }
            }    
        }
        pNode = NodeTraversal::next(pNode, pBodyNode); //pNode->traverseNextNode(pBodyNode);
    }    
    
    return pMinContainer;
}

bool WebViewImpl::isSkipBackgroundImage(Node* pNode)
{
    if(pNode==NULL)
        return true;
    
    RenderObject* pRenderer = pNode->renderer();
    if(pRenderer && (pRenderer->style()->logicalHeight().isAuto() || pRenderer->style()->logicalWidth().isAuto()))
        return true;
    
    Node* pBody=pNode->document()->body();
    IntRect nodeRect =pixelSnappedIntRect(pNode->Node::boundingBox());
    IntRect bodyNodeRect =pixelSnappedIntRect(pBody->Node::boundingBox());
    if(nodeRect.width()==bodyNodeRect.width())
        return true;
    return false;
}

Node* WebViewImpl::getBodyNodeFromIframe(Node* iframeNode)
{
    if(NULL == iframeNode) {
        return NULL;
    }
    HTMLFrameOwnerElement* frameOwnerElement= static_cast<HTMLFrameOwnerElement *>(iframeNode);	 
    Document *ownerDocument = frameOwnerElement->contentWindow()->document();
    if (NULL == ownerDocument) 
       return iframeNode;

    Node* pBodyNode = ownerDocument->body();
    return pBodyNode;
}

int  WebViewImpl::findOverlappedChildNode(WebCore::Node* node,   IntRect cropRect, Vector<CroppedNodeInfo>* overlappedNodeInfoTable)
{
    int overlappedChildCnt=0;
    if(!node || !node->firstChild())
        return 0;
    cropRect = node->document()->view()->windowToContents(cropRect);
    for (Node* child = node->firstChild(); child; child = child->nextSibling())
    {
        IntRect renderRect = pixelSnappedIntRect(child->Node::boundingBox());
        if(cropRect.intersects(renderRect) ==true)
        {
            ++overlappedChildCnt;
            overlappedNodeInfoTable->append(CroppedNodeInfo(child, renderRect));
        }
        else
        {
            IntRect childRect = getChildNodesRect(child);
            if (cropRect.intersects(childRect) ==true)
            {
                ++overlappedChildCnt;
                overlappedNodeInfoTable->append(CroppedNodeInfo(child, childRect));
            }
        }
    }
    return overlappedChildCnt;
}

IntRect WebViewImpl::getOverlappedNodeArea(Vector<CroppedNodeInfo>* overlappedNodeInfoTable)
{
    int tableSize = overlappedNodeInfoTable->size();
    CroppedNodeInfo& nodeInfo = (*overlappedNodeInfoTable)[0];
    IntRect resultArea(nodeInfo.m_rect);

    for (int i=1; i<tableSize; i++)
    {
        nodeInfo = (*overlappedNodeInfoTable)[i];
        resultArea.unite(nodeInfo.m_rect);
    }
    return resultArea;
}

IntRect WebViewImpl::getChildNodesRect(WebCore::Node* pNode)
{
    IntRect childArea= pixelSnappedIntRect(pNode->Node::boundingBox());
    for (Node* child = pNode->firstChild(); child; child = child->nextSibling())
    {
        IntRect renderRect = pixelSnappedIntRect(child->Node::boundingBox());
        childArea.unite(renderRect);
    }
    return childArea;
}


IntRect WebViewImpl::convertToWindowNodeRect(WebCore::Node* pNode, WebCore::IntRect nodeRect)
{
    nodeRect=pNode->document()->view()->contentsToWindow(nodeRect);
    int x =(int)(nodeRect.x()*pageScaleFactor());
    int y =(int)(nodeRect.y()*pageScaleFactor());
    int w =(int)(nodeRect.width()*pageScaleFactor());
    int h =(int)(nodeRect.height()*pageScaleFactor());

    return IntRect(x,y,w,h);
}

String WebViewImpl::convertCroppedNodeInfoTableToString(Vector<CroppedNodeInfo>* nodeInfoTable)
{
    String FieldSeparator;
    String RowSeparator;
    String resultStr;
    int tableSize = nodeInfoTable->size();

    FieldSeparator.append((UChar)0xFFFE);
    RowSeparator  .append((UChar)0xFFFF);

    for(int i=0; i<tableSize; i++)
    {
        CroppedNodeInfo& nodeInfo = (*nodeInfoTable)[i];
        
        String rowStr;
        rowStr = String::number(nodeInfo.m_rect.x()) 
                    + FieldSeparator 
                    + String::number(nodeInfo.m_rect.y()) 
                    + FieldSeparator 
                    + String::number(nodeInfo.m_rect.width()) 
                    + FieldSeparator 
                    + String::number(nodeInfo.m_rect.height()) 
                    + FieldSeparator 
                    + nodeInfo.m_string 
                    + RowSeparator;
                    
        resultStr.append(rowStr); //resultStr += rowStr;
    }

    return resultStr;
}

String WebViewImpl::extractAllTextFromNode(Node* pNode)
{
    String resultStr;
    Node* pCurNode = pNode;
    int prevYPos = -99999;
    
    while(pCurNode != NULL)
    {
        Node::NodeType nodeType  = pCurNode->nodeType();
        IntRect nodeRect = pixelSnappedIntRect(pCurNode->Node::boundingBox());          
        RenderObject*  pRenderer = pCurNode->renderer();

        RenderStyle* style = pCurNode->computedStyle(); 
        if(!pCurNode->dispatchEvent(Event::create(eventNames().selectstartEvent, true, true) )|| (style &&style->userSelect() == SELECT_NONE))  
        {
            return resultStr;
        }

        if(pCurNode->hasTagName(HTMLNames::iframeTag))
        {
            Node* iFrameBody= getBodyNodeFromIframe(pCurNode);
            if(iFrameBody !=NULL){
                String str= extractAllTextFromNode(iFrameBody);
                resultStr.append(str);
            }
        } 

        if(pRenderer != NULL && nodeRect.width() > 0 && nodeRect.height() > 0)
        {
            if(nodeType == Node::TEXT_NODE)
            {
                String nodeValue = pCurNode->nodeValue();

                if(nodeValue.isNull() == true) 
                    nodeValue = "";

                if(nodeValue == "\n")
                    nodeValue = "";

                if(nodeRect.y() != prevYPos) 
                {
                   resultStr.append(String("\n") + nodeValue); // resultStr += String("\n") + nodeValue;
                    prevYPos = nodeRect.y();
                }
                else
                {
                    resultStr.append(nodeValue + " "); //resultStr += nodeValue + " ";
                }
            }
        }    

        pCurNode = NodeTraversal::next(pCurNode, pNode); //pCurNode->traverseNextNode(pNode);
    }

    return resultStr;
}

int WebViewImpl::getParentCount(Node* pNode)
{
    int parentCount = 0;
    Node* pCurNode = pNode;
    
    while(pCurNode != NULL)
    {
        parentCount++;
        pCurNode = pCurNode->parentNode();
    }

    return parentCount;
}

String WebViewImpl::getRectString(IntRect rect)
{
    String result = String::format("%4d, %4d, %4d, %4d", rect.x(), rect.y(), rect.width(), rect.height() );
    return result;
}

//debug helper function
String WebViewImpl::getNodeTypeStr(Node::NodeType nodeType)
{
    switch(nodeType)
    {
        case Node::ELEMENT_NODE                   : return String("ELEMENT");
        case Node::ATTRIBUTE_NODE                 : return String("ATTRIBUTE");
        case Node::TEXT_NODE                      : return String("TEXT   ");
        case Node::CDATA_SECTION_NODE             : return String("CDATA_SECTION");
        case Node::ENTITY_REFERENCE_NODE          : return String("ENTITY_REFERENCE");
        case Node::ENTITY_NODE                    : return String("ENTITY");
        case Node::PROCESSING_INSTRUCTION_NODE    : return String("PROCESSING_INSTRUCTION");
        case Node::COMMENT_NODE                   : return String("COMMENT");
        case Node::DOCUMENT_NODE                  : return String("DOCUMENT");
        case Node::DOCUMENT_TYPE_NODE             : return String("DOCUMENT_TYPE");
        case Node::DOCUMENT_FRAGMENT_NODE         : return String("DOCUMENT_FRAGMENT");
        case Node::NOTATION_NODE                  : return String("NOTATION");
        case Node::XPATH_NAMESPACE_NODE           : return String("XPATH_NAMESPACE");
    }
    return String("UNKNOWN");
}

//debug helper function
void WebViewImpl::dumpNodes(Node* pRootNode)
{
    //LOG(INFO) << __FUNCTION__ << "--------------- Dumping start --------------- ";
   /* 
    Node* pNode = pRootNode;

    while(pNode != NULL)
    {
        Node::NodeType nodeType = pNode->nodeType();
        IntRect nodeRect    = pNode->getRect();
        String  nodeName    = pNode->nodeName();
        String  nodeValue   = pNode->nodeValue();
        nodeValue.replace("\n", "\\n");
        
        String  nodeDumpStr = "\t";

        if(nodeName.isNull() == true)
            nodeName = "NULL";
            
        if(nodeValue.isNull() == true)
            nodeValue = "NULL";

        int parentCount = getParentCount(pNode);
        for(int cnt = 0; cnt<parentCount; cnt++)
        {
            nodeDumpStr += "\t";
        }
        nodeDumpStr += String::number(parentCount) + "\t";

        nodeDumpStr += String("Type(") +getNodeTypeStr(nodeType)+")\t";
        nodeDumpStr += String("Name(") +nodeName +")\t";
        nodeDumpStr += String("Value(")+nodeValue+")\t";
        nodeDumpStr += String("Rect(") +getRectString(nodeRect)+")\t";

        RenderObject* pRenderer = pNode->renderer();
        if(pRenderer != NULL) {
            bool bReplaced = false;
            nodeDumpStr += String("Render name(")       + pRenderer->renderName()+")\t";
            nodeDumpStr += String("Render rect(")       +getRectString(pNode->renderRect(&bReplaced))+")\t";
            nodeDumpStr += String("View rect(")         +getRectString(pRenderer->view()->viewRect())+")\t";
            nodeDumpStr += String("Abs bounding box(")  +getRectString(pRenderer->absoluteBoundingBoxRect(false))+")\t";

            IntRect paintingRootRect;
            pRenderer->paintingRootRect(paintingRootRect);
            nodeDumpStr += String("Painting root rect(")+getRectString(paintingRootRect)+")\t";
        }
        else
        {
            nodeDumpStr += "Render is NULL\t";
        }

        //LOG(INFO) << __FUNCTION__ << nodeDumpStr.utf8().data();
        
        pNode = NodeTraversal::next(pNode, pRootNode); //pNode->traverseNextNode(pRootNode);
    }    */

    //LOG(INFO) << __FUNCTION__ << "--------------- Dumping end --------------- ";
}
#endif
// SAMSUNG : SmartClip <<

#if defined(SBROWSER_ADJUST_WORKER_THREAD_PRIORITY)
void WebViewImpl::adjustWorkerThreadPriority(const std::string& url)
{         
    String _url(url.c_str());
    
    bool flag = false;
    if (!_url.isNull() && !_url.isEmpty() && _url.contains("newsstand.naver.com")) {
        TRACE_EVENT1("webkit","WebViewImpl::adjustWorkerThreadPriority:setHighPriorityToRasterWorker", "url", _url.utf8());
        flag = true;           
        m_webSettings->setDeferredImageDecodingEnabled(false);	         
    }
    else {
        m_webSettings->setDeferredImageDecodingEnabled(true);
    }
    
    setHighPriorityToRasterWorker(flag);
}

void WebViewImpl::setHighPriorityToRasterWorker(bool flag)
{
    if(m_layerTreeView) {
        if(m_adjustWorkerThreadPriorityFlag != flag) {
            m_adjustWorkerThreadPriorityFlag = flag;
            m_layerTreeView->setHighPriorityToRasterWorker(flag);
        }
    }
}
#endif
#if defined (SBROWSER_SAVEPAGE_IMPL)
static CachedImage* getCachedImage(Element* element)
{
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage())
        return 0;
    RenderImage* image = toRenderImage(renderer);
    return image->cachedImage();
}

static Image* getImage(Element* element)
{
    ASSERT(element);
    CachedImage* cachedImage = getCachedImage(element);
    
    return (cachedImage && !cachedImage->errorOccurred()) ?
        cachedImage->image() : 0;
}



void WebViewImpl::getBitmapFromCachedResource(const std::string imgurl, SkBitmap* bmp){
	LOG(INFO)<<"SCRAP :: getBitmapFromCachedResource!!";
	String imageUrl(imgurl.c_str());
	if(imageUrl.isNull())
		return;
	
	RefPtr<HTMLCollection> images = mainFrameImpl()->frame()->document()->images();
    size_t sourceLength = images->length();
    LOG(INFO)<<"SCRAP :: getBitmapFromCachedResource sourceLength :: "<<sourceLength;
    for (size_t i = 0; i < sourceLength; ++i) {
        Node* node = images->item(i);
		Element* element = toElement(node);

		if(element && (element->imageSourceURL().string() == imageUrl)){
			Image *cachedImage = getImage(element);
			LOG(INFO)<<"SCRAP :: getBitmapFromCachedResource imageUrl :: "<<element->imageSourceURL().string().utf8().data();
			if(cachedImage != NULL){
				NativeImagePtr bitmapPtr = cachedImage->nativeImageForCurrentFrame();
				if(bitmapPtr){	
					LOG(INFO)<<"SCRAP :: getBitmapFromCachedResource got bitmap!!!";
			
					*bmp = bitmapPtr->bitmap();
					return;
				}
			}	
		}
    }


}
#endif
} // namespace WebKit
