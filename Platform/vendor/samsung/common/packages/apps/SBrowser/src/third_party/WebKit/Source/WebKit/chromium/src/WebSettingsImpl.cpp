/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebSettingsImpl.h"

#include "core/page/Settings.h"
#include "core/platform/graphics/FontRenderingMode.h"
#include "core/platform/graphics/chromium/DeferredImageDecoder.h"
#include <public/WebString.h>
#include <public/WebURL.h>
#include <wtf/UnusedParam.h>
#if defined(SBROWSER_POWER_SAVER_MODE_IMPL)
#include "core/platform/graphics/BitmapImage.h"
#endif

#if defined(OS_WIN)
#include "core/rendering/RenderThemeChromiumWin.h"
#endif

using namespace WebCore;

namespace WebKit {

WebSettingsImpl::WebSettingsImpl(Settings* settings)
    : m_settings(settings)
    , m_showFPSCounter(false)
    , m_showPaintRects(false)
    , m_renderVSyncNotificationEnabled(false)
    , m_viewportEnabled(false)
    , m_initializeAtMinimumPageScale(true)
    , m_useWideViewport(true)
    , m_gestureTapHighlightEnabled(true)
    , m_autoZoomFocusedNodeToLegibleScale(false)
    , m_deferredImageDecodingEnabled(false)
    , m_doubleTapToZoomEnabled(false)
    , m_supportDeprecatedTargetDensityDPI(false)
#if defined (SBROWSER_SAVEPAGE_IMPL)
    , m_allowContentURLAccess(false)
#endif
{
    ASSERT(settings);
}

void WebSettingsImpl::setStandardFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setStandardFontFamily(font, script);
}

void WebSettingsImpl::setFixedFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setFixedFontFamily(font, script);
}

void WebSettingsImpl::setSerifFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setSerifFontFamily(font, script);
}

void WebSettingsImpl::setSansSerifFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setSansSerifFontFamily(font, script);
}

void WebSettingsImpl::setCursiveFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setCursiveFontFamily(font, script);
}

void WebSettingsImpl::setFantasyFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setFantasyFontFamily(font, script);
}

void WebSettingsImpl::setPictographFontFamily(const WebString& font, UScriptCode script)
{
    m_settings->setPictographFontFamily(font, script);
}

void WebSettingsImpl::setDefaultFontSize(int size)
{
    m_settings->setDefaultFontSize(size);
#if defined(OS_WIN)
    // RenderTheme is a singleton that needs to know the default font size to
    // draw some form controls.  We let it know each time the size changes.
    WebCore::RenderThemeChromiumWin::setDefaultFontSize(size);
#endif
}

void WebSettingsImpl::setDefaultFixedFontSize(int size)
{
    m_settings->setDefaultFixedFontSize(size);
}

void WebSettingsImpl::setDefaultVideoPosterURL(const WebString& url)
{
    m_settings->setDefaultVideoPosterURL(url);
}

void WebSettingsImpl::setMinimumFontSize(int size)
{
    m_settings->setMinimumFontSize(size);
}

void WebSettingsImpl::setMinimumLogicalFontSize(int size)
{
    m_settings->setMinimumLogicalFontSize(size);
}

void WebSettingsImpl::setDeviceSupportsTouch(bool deviceSupportsTouch)
{
    m_settings->setDeviceSupportsTouch(deviceSupportsTouch);
}

void WebSettingsImpl::setDeviceSupportsMouse(bool deviceSupportsMouse)
{
    m_settings->setDeviceSupportsMouse(deviceSupportsMouse);
}

bool WebSettingsImpl::deviceSupportsTouch()
{
    return m_settings->deviceSupportsTouch();
}

void WebSettingsImpl::setAutoZoomFocusedNodeToLegibleScale(bool autoZoomFocusedNodeToLegibleScale)
{
    m_autoZoomFocusedNodeToLegibleScale = autoZoomFocusedNodeToLegibleScale;
}

void WebSettingsImpl::setTextAutosizingEnabled(bool enabled)
{
    m_settings->setTextAutosizingEnabled(enabled);
}

void WebSettingsImpl::setTextAutosizingFontScaleFactor(float fontScaleFactor)
{
    m_settings->setTextAutosizingFontScaleFactor(fontScaleFactor);
}

void WebSettingsImpl::setDefaultTextEncodingName(const WebString& encoding)
{
    m_settings->setDefaultTextEncodingName((String)encoding);
}

void WebSettingsImpl::setJavaScriptEnabled(bool enabled)
{
    m_settings->setScriptEnabled(enabled);
}

// ------------------- kikin Modification Starts --------------------
#if defined(SBROWSER_KIKIN)
void WebSettingsImpl::setKikinEnabled(bool enabled)
{
    m_settings->setKikinSearchEnabled(enabled);
}
#endif
// -------------------- kikin Modification Ends ---------------------

void WebSettingsImpl::setWebSecurityEnabled(bool enabled)
{
    m_settings->setWebSecurityEnabled(enabled);
}

void WebSettingsImpl::setJavaScriptCanOpenWindowsAutomatically(bool canOpenWindows)
{
    m_settings->setJavaScriptCanOpenWindowsAutomatically(canOpenWindows);
}

void WebSettingsImpl::setSupportDeprecatedTargetDensityDPI(bool supportDeprecatedTargetDensityDPI)
{
    m_supportDeprecatedTargetDensityDPI = supportDeprecatedTargetDensityDPI;
}

void WebSettingsImpl::setSupportsMultipleWindows(bool supportsMultipleWindows)
{
    m_settings->setSupportsMultipleWindows(supportsMultipleWindows);
}

void WebSettingsImpl::setLoadsImagesAutomatically(bool loadsImagesAutomatically)
{
    m_settings->setLoadsImagesAutomatically(loadsImagesAutomatically);
}

#if defined(SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT)

bool WebSettingsImpl::setBingAsDefaultSearchEngine(bool enable)
{  
    m_settings->setBingAsDefaultSearchEngine(enable);
    return true;	
}
#endif

void WebSettingsImpl::setImagesEnabled(bool enabled)
{
    m_settings->setImagesEnabled(enabled);
}

void WebSettingsImpl::setInitializeAtMinimumPageScale(bool enabled)
{
    m_initializeAtMinimumPageScale = enabled;
}

void WebSettingsImpl::setPluginsEnabled(bool enabled)
{
    m_settings->setPluginsEnabled(enabled);
}

void WebSettingsImpl::setDOMPasteAllowed(bool enabled)
{
    m_settings->setDOMPasteAllowed(enabled);
}

void WebSettingsImpl::setNeedsSiteSpecificQuirks(bool enabled)
{
    m_settings->setNeedsSiteSpecificQuirks(enabled);
}

void WebSettingsImpl::setShrinksStandaloneImagesToFit(bool shrinkImages)
{
    m_settings->setShrinksStandaloneImagesToFit(shrinkImages);
}

void WebSettingsImpl::setSpatialNavigationEnabled(bool enabled)
{
    m_settings->setSpatialNavigationEnabled(enabled);
}

void WebSettingsImpl::setUsesEncodingDetector(bool usesDetector)
{
    m_settings->setUsesEncodingDetector(usesDetector);
}

void WebSettingsImpl::setTextAreasAreResizable(bool areResizable)
{
    m_settings->setTextAreasAreResizable(areResizable);
}

void WebSettingsImpl::setJavaEnabled(bool enabled)
{
    m_settings->setJavaEnabled(enabled);
}

void WebSettingsImpl::setAllowScriptsToCloseWindows(bool allow)
{
    m_settings->setAllowScriptsToCloseWindows(allow);
}

void WebSettingsImpl::setUserStyleSheetLocation(const WebURL& location)
{
    m_settings->setUserStyleSheetLocation(location);
}

void WebSettingsImpl::setAuthorAndUserStylesEnabled(bool enabled)
{
    m_settings->setAuthorAndUserStylesEnabled(enabled);
}

void WebSettingsImpl::setUseWideViewport(bool useWideViewport)
{
    m_useWideViewport = useWideViewport;
#if defined(SBROWSER_USE_WIDE_VIEWPORT)
    m_settings->setUseWideViewport(useWideViewport);
#endif
}

void WebSettingsImpl::setDoubleTapToZoomEnabled(bool doubleTapToZoomEnabled)
{
    m_doubleTapToZoomEnabled = doubleTapToZoomEnabled;
}

void WebSettingsImpl::setDownloadableBinaryFontsEnabled(bool enabled)
{
    m_settings->setDownloadableBinaryFontsEnabled(enabled);
}

void WebSettingsImpl::setJavaScriptCanAccessClipboard(bool enabled)
{
    m_settings->setJavaScriptCanAccessClipboard(enabled);
}

void WebSettingsImpl::setXSSAuditorEnabled(bool enabled)
{
    m_settings->setXSSAuditorEnabled(enabled);
}

void WebSettingsImpl::setUnsafePluginPastingEnabled(bool enabled)
{
    m_settings->setUnsafePluginPastingEnabled(enabled);
}

void WebSettingsImpl::setDNSPrefetchingEnabled(bool enabled)
{
    m_settings->setDNSPrefetchingEnabled(enabled);
}

void WebSettingsImpl::setLocalStorageEnabled(bool enabled)
{
    m_settings->setLocalStorageEnabled(enabled);
}

void WebSettingsImpl::setEditableLinkBehaviorNeverLive()
{
    // FIXME: If you ever need more behaviors than this, then we should probably
    //        define an enum in WebSettings.h and have a switch statement that
    //        translates.  Until then, this is probably fine, though.
    m_settings->setEditableLinkBehavior(WebCore::EditableLinkNeverLive);
}

void WebSettingsImpl::setFrameFlatteningEnabled(bool enabled)
{
    m_settings->setFrameFlatteningEnabled(enabled);
}

void WebSettingsImpl::setFontRenderingModeNormal()
{
    // FIXME: If you ever need more behaviors than this, then we should probably
    //        define an enum in WebSettings.h and have a switch statement that
    //        translates.  Until then, this is probably fine, though.
    m_settings->setFontRenderingMode(WebCore::NormalRenderingMode);
}

void WebSettingsImpl::setAllowUniversalAccessFromFileURLs(bool allow)
{
    m_settings->setAllowUniversalAccessFromFileURLs(allow);
}

void WebSettingsImpl::setAllowFileAccessFromFileURLs(bool allow)
{
    m_settings->setAllowFileAccessFromFileURLs(allow);
}

void WebSettingsImpl::setTextDirectionSubmenuInclusionBehaviorNeverIncluded()
{
    // FIXME: If you ever need more behaviors than this, then we should probably
    //        define an enum in WebSettings.h and have a switch statement that
    //        translates.  Until then, this is probably fine, though.
    m_settings->setTextDirectionSubmenuInclusionBehavior(WebCore::TextDirectionSubmenuNeverIncluded);
}

void WebSettingsImpl::setTouchDragDropEnabled(bool enabled)
{
    m_settings->setTouchDragDropEnabled(enabled);
}

void WebSettingsImpl::setTouchEditingEnabled(bool enabled)
{
    m_settings->setTouchEditingEnabled(enabled);
}

void WebSettingsImpl::setThreadedHTMLParser(bool enabled)
{
    m_settings->setThreadedHTMLParser(enabled);
}

void WebSettingsImpl::setOfflineWebApplicationCacheEnabled(bool enabled)
{
    m_settings->setOfflineWebApplicationCacheEnabled(enabled);
}

void WebSettingsImpl::setWebAudioEnabled(bool enabled)
{
    m_settings->setWebAudioEnabled(enabled);
}

#if defined(SBROWSER_POWER_SAVER_MODE_IMPL)
void WebSettingsImpl::setDisableProgressiveImageDecoding(bool enabled)
{
    DeferredImageDecoder::setDisableProgressiveImageDecoding(enabled);
}
void WebSettingsImpl::setDisableGIFAnimation(bool enabled)
{
    DeferredImageDecoder::setDisableGIFAnimation(enabled);
}

void WebSettingsImpl::setDisableFilterBitmap(bool enabled)
{
    BitmapImage::setDisableFilterBitmap(enabled);
}

void WebSettingsImpl::setEnableBgColorOffWhite(bool enabled)
{
    m_settings->setEnableBgColorOffWhite(enabled);
}

void WebSettingsImpl::setEnableBgColorLightGrey(bool enabled)
{
    m_settings->setEnableBgColorLightGrey(enabled);
}

void WebSettingsImpl::setEnableBgColorGrey(bool enabled)
{
    m_settings->setEnableBgColorGrey(enabled);
}

#endif

#if defined(SBROWSER_ENABLE_APPNAP) 
void WebSettingsImpl::setEnableAppNap(bool enableAppNap){
	m_settings->setEnableAppNap(enableAppNap);
}
#endif
void WebSettingsImpl::setExperimentalWebGLEnabled(bool enabled)
{
    m_settings->setWebGLEnabled(enabled);
}

void WebSettingsImpl::setCSSStickyPositionEnabled(bool enabled)
{
    m_settings->setCSSStickyPositionEnabled(enabled);
}

void WebSettingsImpl::setExperimentalCSSGridLayoutEnabled(bool enabled)
{
    m_settings->setCSSGridLayoutEnabled(enabled);
}

void WebSettingsImpl::setExperimentalCSSCustomFilterEnabled(bool enabled)
{
    m_settings->setCSSCustomFilterEnabled(enabled);
}

void WebSettingsImpl::setExperimentalCSSVariablesEnabled(bool enabled)
{
    m_settings->setCSSVariablesEnabled(enabled);
}

void WebSettingsImpl::setOpenGLMultisamplingEnabled(bool enabled)
{
    m_settings->setOpenGLMultisamplingEnabled(enabled);
}

void WebSettingsImpl::setPrivilegedWebGLExtensionsEnabled(bool enabled)
{
    m_settings->setPrivilegedWebGLExtensionsEnabled(enabled);
}

void WebSettingsImpl::setRenderVSyncNotificationEnabled(bool enabled)
{
    m_renderVSyncNotificationEnabled = enabled;
}

void WebSettingsImpl::setWebGLErrorsToConsoleEnabled(bool enabled)
{
    m_settings->setWebGLErrorsToConsoleEnabled(enabled);
}

void WebSettingsImpl::setShowDebugBorders(bool show)
{
    m_settings->setShowDebugBorders(show);
}

void WebSettingsImpl::setShowFPSCounter(bool show)
{
    m_showFPSCounter = show;
}

void WebSettingsImpl::setShowPaintRects(bool show)
{
    m_showPaintRects = show;
}

void WebSettingsImpl::setEditingBehavior(EditingBehavior behavior)
{
    m_settings->setEditingBehaviorType(static_cast<WebCore::EditingBehaviorType>(behavior));
}

void WebSettingsImpl::setAcceleratedCompositingEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingEnabled(enabled);
    m_settings->setScrollingCoordinatorEnabled(enabled);
}

void WebSettingsImpl::setForceCompositingMode(bool enabled)
{
    m_settings->setForceCompositingMode(enabled);
}

void WebSettingsImpl::setMockScrollbarsEnabled(bool enabled)
{
    m_settings->setMockScrollbarsEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingFor3DTransformsEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingFor3DTransformsEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForVideoEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForVideoEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForOverflowScrollEnabled(
    bool enabled)
{
    m_settings->setAcceleratedCompositingForOverflowScrollEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForPluginsEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForPluginsEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForCanvasEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForCanvasEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForAnimationEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForAnimationEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedCompositingForScrollableFramesEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForScrollableFramesEnabled(enabled);
}

void WebSettingsImpl::setAcceleratedFiltersEnabled(bool enabled)
{
    m_settings->setAcceleratedFiltersEnabled(enabled);
}

void WebSettingsImpl::setAccelerated2dCanvasEnabled(bool enabled)
{
    m_settings->setAccelerated2dCanvasEnabled(enabled);
}

void WebSettingsImpl::setAntialiased2dCanvasEnabled(bool enabled)
{
    m_settings->setAntialiased2dCanvasEnabled(enabled);
}

void WebSettingsImpl::setDeferred2dCanvasEnabled(bool enabled)
{
}

void WebSettingsImpl::setDeferredImageDecodingEnabled(bool enabled)
{
    DeferredImageDecoder::setEnabled(enabled);
    m_deferredImageDecodingEnabled = enabled;
}

void WebSettingsImpl::setAcceleratedCompositingForFixedPositionEnabled(bool enabled)
{
    m_settings->setAcceleratedCompositingForFixedPositionEnabled(enabled);
}

void WebSettingsImpl::setMinimumAccelerated2dCanvasSize(int numPixels)
{
    m_settings->setMinimumAccelerated2dCanvasSize(numPixels);
}

void WebSettingsImpl::setMemoryInfoEnabled(bool enabled)
{
    m_settings->setMemoryInfoEnabled(enabled);
}

void WebSettingsImpl::setHyperlinkAuditingEnabled(bool enabled)
{
    m_settings->setHyperlinkAuditingEnabled(enabled);
}

void WebSettingsImpl::setLayoutFallbackWidth(int width)
{
    m_settings->setLayoutFallbackWidth(width);
}

void WebSettingsImpl::setAsynchronousSpellCheckingEnabled(bool enabled)
{
    m_settings->setAsynchronousSpellCheckingEnabled(enabled);
}

void WebSettingsImpl::setUnifiedTextCheckerEnabled(bool enabled)
{
    m_settings->setUnifiedTextCheckerEnabled(enabled);
}

void WebSettingsImpl::setCaretBrowsingEnabled(bool enabled)
{
    m_settings->setCaretBrowsingEnabled(enabled);
}

void WebSettingsImpl::setInteractiveFormValidationEnabled(bool enabled)
{
    m_settings->setInteractiveFormValidationEnabled(enabled);
}

void WebSettingsImpl::setValidationMessageTimerMagnification(int newValue)
{
    m_settings->setValidationMessageTimerMagnification(newValue);
}

void WebSettingsImpl::setMinimumTimerInterval(double interval)
{
    // FIXME: remove this once the embedder is no longer calling it.
}

void WebSettingsImpl::setFullScreenEnabled(bool enabled)
{
    m_settings->setFullScreenEnabled(enabled);
}

void WebSettingsImpl::setAllowDisplayOfInsecureContent(bool enabled)
{
    m_settings->setAllowDisplayOfInsecureContent(enabled);
}

void WebSettingsImpl::setAllowRunningOfInsecureContent(bool enabled)
{
    m_settings->setAllowRunningOfInsecureContent(enabled);
}

void WebSettingsImpl::setPasswordEchoEnabled(bool flag)
{
    m_settings->setPasswordEchoEnabled(flag);
}

void WebSettingsImpl::setPasswordEchoDurationInSeconds(double durationInSeconds)
{
    m_settings->setPasswordEchoDurationInSeconds(durationInSeconds);
}

void WebSettingsImpl::setPerTilePaintingEnabled(bool enabled)
{
    m_perTilePaintingEnabled = enabled;
}

void WebSettingsImpl::setShouldPrintBackgrounds(bool enabled)
{
    m_settings->setShouldPrintBackgrounds(enabled);
}

void WebSettingsImpl::setEnableScrollAnimator(bool enabled)
{
    m_settings->setScrollAnimatorEnabled(enabled);
}

void WebSettingsImpl::setEnableTouchAdjustment(bool enabled)
{
    m_settings->setTouchAdjustmentEnabled(enabled);
}

bool WebSettingsImpl::scrollAnimatorEnabled() const
{
    return m_settings->scrollAnimatorEnabled();
}

bool WebSettingsImpl::touchEditingEnabled() const
{
    return m_settings->touchEditingEnabled();
}

void WebSettingsImpl::setVisualWordMovementEnabled(bool enabled)
{
    m_settings->setVisualWordMovementEnabled(enabled);
}

void WebSettingsImpl::setShouldDisplaySubtitles(bool enabled)
{
    m_settings->setShouldDisplaySubtitles(enabled);
}

void WebSettingsImpl::setShouldDisplayCaptions(bool enabled)
{
    m_settings->setShouldDisplayCaptions(enabled);
}

void WebSettingsImpl::setShouldDisplayTextDescriptions(bool enabled)
{
    m_settings->setShouldDisplayTextDescriptions(enabled);
}

void WebSettingsImpl::setShouldRespectImageOrientation(bool enabled)
{
    m_settings->setShouldRespectImageOrientation(enabled);
}

void WebSettingsImpl::setMediaPlaybackRequiresUserGesture(bool required)
{
    m_settings->setMediaPlaybackRequiresUserGesture(required);
}

void WebSettingsImpl::setFixedPositionCreatesStackingContext(bool creates)
{
    m_settings->setFixedPositionCreatesStackingContext(creates);
}

void WebSettingsImpl::setViewportEnabled(bool enabled)
{
    m_viewportEnabled = enabled;
}

void WebSettingsImpl::setSyncXHRInDocumentsEnabled(bool enabled)
{
    m_settings->setSyncXHRInDocumentsEnabled(enabled);
}

void WebSettingsImpl::setCookieEnabled(bool enabled)
{
    m_settings->setCookieEnabled(enabled);
}

void WebSettingsImpl::setGestureTapHighlightEnabled(bool enableHighlight)
{
    m_gestureTapHighlightEnabled = enableHighlight;
}

void WebSettingsImpl::setAllowCustomScrollbarInMainFrame(bool enabled)
{
    m_settings->setAllowCustomScrollbarInMainFrame(enabled);
}

void WebSettingsImpl::setCompositedScrollingForFramesEnabled(bool enabled)
{
    m_settings->setCompositedScrollingForFramesEnabled(enabled);
}

void WebSettingsImpl::setSelectTrailingWhitespaceEnabled(bool enabled)
{
    m_settings->setSelectTrailingWhitespaceEnabled(enabled);
}

void WebSettingsImpl::setSelectionIncludesAltImageText(bool enabled)
{
    m_settings->setSelectionIncludesAltImageText(enabled);
}

void WebSettingsImpl::setSmartInsertDeleteEnabled(bool enabled)
{
    m_settings->setSmartInsertDeleteEnabled(enabled);
}

#if defined (SBROWSER_SAVEPAGE_IMPL)
void WebSettingsImpl::setAllowContentURLAccess(bool allow)
{
    m_allowContentURLAccess = allow;
}

bool WebSettingsImpl::allowContentURLAccess() const
{
    return true ; // m_allowContentURLAccess;
}
#endif
#if defined (SBROWSER_IMIDEO_PREFERENCES)
void WebSettingsImpl::setImideoDebugMode(int mode)
{
    m_settings->setImideoDebugMode(mode);
}
#endif
} // namespace WebKit
