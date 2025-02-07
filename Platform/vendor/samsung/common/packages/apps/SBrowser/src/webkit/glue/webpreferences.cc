// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webpreferences.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNetworkStateNotifier.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSettings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/icu/public/common/unicode/uchar.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebNetworkStateNotifier;
using WebKit::WebRuntimeFeatures;
using WebKit::WebSettings;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebView;

WebPreferences::WebPreferences()
    : default_font_size(16),
      default_fixed_font_size(13),
      minimum_font_size(0),
      minimum_logical_font_size(6),
      default_encoding("ISO-8859-1"),
      apply_default_device_scale_factor_in_compositor(false),
      apply_page_scale_factor_in_compositor(false),
      javascript_enabled(true),
      // ------------------- kikin Modification Starts --------------------
      #if defined(SBROWSER_KIKIN)
      kikin_enabled(true),
      #endif
      // -------------------- kikin Modification Ends ---------------------
      web_security_enabled(true),
      javascript_can_open_windows_automatically(true),
      loads_images_automatically(true),
      images_enabled(true),
      #if defined(SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT)
      is_bing_as_default_search_engine(false), 
      #endif
      plugins_enabled(true),
      dom_paste_enabled(false),  // enables execCommand("paste")
      site_specific_quirks_enabled(false),
      shrinks_standalone_images_to_fit(true),
      #if defined(SBROWSER_TEXT_ENCODING)
      uses_universal_detector(true),
      #else
      uses_universal_detector(false),  // Disabled: page cycler regression
      #endif
      text_areas_are_resizable(true),
      java_enabled(true),
      allow_scripts_to_close_windows(false),
      remote_fonts_enabled(true),
      javascript_can_access_clipboard(false),
      xss_auditor_enabled(true),
      dns_prefetching_enabled(true),
      local_storage_enabled(false),
      databases_enabled(false),
      application_cache_enabled(false),
      tabs_to_links(true),
      caret_browsing_enabled(false),
      hyperlink_auditing_enabled(true),
      is_online(true),
      user_style_sheet_enabled(false),
      author_and_user_styles_enabled(true),
      allow_universal_access_from_file_urls(false),
      allow_file_access_from_file_urls(false),
#if defined (SBROWSER_SAVEPAGE_IMPL)
      allow_content_url_access(false),
#endif

#if defined(SBROWSER_CHROME_NATIVE_PREFERENCES)
       overview_mode_enabled(true),
#endif //SBROWSER_CHROME_NATIVE_PREFERENCES
#if defined(SBROWSER_FONT_BOOSTING_PREFERENCES)
      fontboosting_mode_enabled(true),
#endif // SBROWSER_FONT_BOOSTING_PREFERENCES

#if defined(SBROWSER_IMIDEO_PREFERENCES)
      imideo_debug_mode(0),		// default values is 0(OFF)
#endif
#if defined(SBROWSER_POWER_SAVER_MODE_IMPL)
      disable_progressive_image_decoding(false),
      disable_gif_animation(false),
      disable_filter_bitmap(false),
      enable_bgcolor_Offwhite(false),
      enable_bgcolor_lightgrey(false),
      enable_bgcolor_grey(false),
#endif
#if defined(SBROWSER_ENABLE_APPNAP)  
	  enable_appnap(false),
#endif
#if defined(SBROWSER_ENABLE_ADBLOCKER)
	  enable_adblocker(false),
#endif
      webaudio_enabled(false),
      experimental_webgl_enabled(false),
      flash_3d_enabled(true),
      flash_stage3d_enabled(false),
      flash_stage3d_baseline_enabled(false),
      gl_multisampling_enabled(true),
      privileged_webgl_extensions_enabled(false),
      webgl_errors_to_console_enabled(true),
      accelerated_compositing_for_overflow_scroll_enabled(false),
      accelerated_compositing_for_scrollable_frames_enabled(false),
      composited_scrolling_for_frames_enabled(false),
      mock_scrollbars_enabled(false),
      threaded_html_parser(true),
      show_paint_rects(false),
      asynchronous_spell_checking_enabled(true),
      unified_textchecker_enabled(false),
      accelerated_compositing_enabled(false),
      force_compositing_mode(false),
      accelerated_compositing_for_3d_transforms_enabled(false),
      accelerated_compositing_for_animation_enabled(false),
      accelerated_compositing_for_video_enabled(false),
      accelerated_2d_canvas_enabled(false),
      minimum_accelerated_2d_canvas_size(257 * 256),
      antialiased_2d_canvas_disabled(false),
      accelerated_filters_enabled(false),
      gesture_tap_highlight_enabled(false),
      accelerated_compositing_for_plugins_enabled(false),
      memory_info_enabled(false),
      fullscreen_enabled(false),
      allow_displaying_insecure_content(true),
      allow_running_insecure_content(false),
      password_echo_enabled(false),
      should_print_backgrounds(false),
      enable_scroll_animator(false),
      visual_word_movement_enabled(false),
      css_sticky_position_enabled(false),
      css_shaders_enabled(false),
      css_variables_enabled(false),
      css_grid_layout_enabled(false),
      lazy_layout_enabled(false),
      touch_enabled(false),
      device_supports_touch(false),
      device_supports_mouse(true),
      touch_adjustment_enabled(true),
      touch_drag_drop_enabled(false),
      fixed_position_creates_stacking_context(false),
      sync_xhr_in_documents_enabled(true),
      deferred_image_decoding_enabled(false),
      should_respect_image_orientation(false),
      number_of_cpu_cores(1),
#if defined(OS_MACOSX)
      editing_behavior(webkit_glue::EDITING_BEHAVIOR_MAC),
#elif defined(OS_WIN)
      editing_behavior(webkit_glue::EDITING_BEHAVIOR_WIN),
#elif defined(OS_ANDROID)
      editing_behavior(webkit_glue::EDITING_BEHAVIOR_ANDROID),
#elif defined(OS_POSIX)
      editing_behavior(webkit_glue::EDITING_BEHAVIOR_UNIX),
#else
      editing_behavior(webkit_glue::EDITING_BEHAVIOR_MAC),
#endif
      supports_multiple_windows(true),
      viewport_enabled(false),
      initialize_at_minimum_page_scale(true),
#if defined(OS_MACOSX)
      smart_insert_delete_enabled(true),
#else
      smart_insert_delete_enabled(false),
#endif
      spatial_navigation_enabled(false),
      cookie_enabled(true),
#if defined(OS_ANDROID)
      text_autosizing_enabled(true),
      font_scale_factor(1.0f),
      force_enable_zoom(false),
      double_tap_to_zoom_enabled(true),
      user_gesture_required_for_media_playback(true),
      support_deprecated_target_density_dpi(false),  // SAMSUNG CAHNGE : enable target-densitydpi spec //reverting back to false which is default PLM P131212-00258 tinyrul.com/vrz21
      use_wide_viewport(true)
#endif
{
  standard_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Times New Roman");
  fixed_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Courier New");
  serif_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Times New Roman");
  sans_serif_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Arial");
  cursive_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Script");
  fantasy_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Impact");
  pictograph_font_family_map[webkit_glue::kCommonScript] =
      ASCIIToUTF16("Times New Roman");
}

WebPreferences::~WebPreferences() {
}

namespace {

void setStandardFontFamilyWrapper(WebSettings* settings,
                                  const base::string16& font,
                                  UScriptCode script) {
  settings->setStandardFontFamily(font, script);
}

void setFixedFontFamilyWrapper(WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->setFixedFontFamily(font, script);
}

void setSerifFontFamilyWrapper(WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->setSerifFontFamily(font, script);
}

void setSansSerifFontFamilyWrapper(WebSettings* settings,
                                   const base::string16& font,
                                   UScriptCode script) {
  settings->setSansSerifFontFamily(font, script);
}

void setCursiveFontFamilyWrapper(WebSettings* settings,
                                 const base::string16& font,
                                 UScriptCode script) {
  settings->setCursiveFontFamily(font, script);
}

void setFantasyFontFamilyWrapper(WebSettings* settings,
                                 const base::string16& font,
                                 UScriptCode script) {
  settings->setFantasyFontFamily(font, script);
}

void setPictographFontFamilyWrapper(WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->setPictographFontFamily(font, script);
}

typedef void (*SetFontFamilyWrapper)(
    WebKit::WebSettings*, const base::string16&, UScriptCode);

// If |scriptCode| is a member of a family of "similar" script codes, returns
// the script code in that family that is used by WebKit for font selection
// purposes.  For example, USCRIPT_KATAKANA_OR_HIRAGANA and USCRIPT_JAPANESE are
// considered equivalent for the purposes of font selection.  WebKit uses the
// script code USCRIPT_KATAKANA_OR_HIRAGANA.  So, if |scriptCode| is
// USCRIPT_JAPANESE, the function returns USCRIPT_KATAKANA_OR_HIRAGANA.  WebKit
// uses different scripts than the ones in Chrome pref names because the version
// of ICU included on certain ports does not have some of the newer scripts.  If
// |scriptCode| is not a member of such a family, returns |scriptCode|.
UScriptCode GetScriptForWebSettings(UScriptCode scriptCode) {
  switch (scriptCode) {
  case USCRIPT_HIRAGANA:
  case USCRIPT_KATAKANA:
  case USCRIPT_JAPANESE:
    return USCRIPT_KATAKANA_OR_HIRAGANA;
  case USCRIPT_KOREAN:
    return USCRIPT_HANGUL;
  default:
    return scriptCode;
  }
}

void ApplyFontsFromMap(const webkit_glue::ScriptFontFamilyMap& map,
                       SetFontFamilyWrapper setter,
                       WebSettings* settings) {
  for (webkit_glue::ScriptFontFamilyMap::const_iterator it = map.begin();
       it != map.end(); ++it) {
    int32 script = u_getPropertyValueEnum(UCHAR_SCRIPT, (it->first).c_str());
    if (script >= 0 && script < USCRIPT_CODE_LIMIT) {
      UScriptCode code = static_cast<UScriptCode>(script);
      (*setter)(settings, it->second, GetScriptForWebSettings(code));
    }
  }
}

}  // namespace


namespace webkit_glue {

// "Zyyy" is the ISO 15924 script code for undetermined script aka Common.
const char kCommonScript[] = "Zyyy";

void ApplyWebPreferences(const WebPreferences& prefs, WebView* web_view) {
  WebSettings* settings = web_view->settings();
  ApplyFontsFromMap(prefs.standard_font_family_map,
                    setStandardFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.fixed_font_family_map,
                    setFixedFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.serif_font_family_map,
                    setSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.sans_serif_font_family_map,
                    setSansSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.cursive_font_family_map,
                    setCursiveFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.fantasy_font_family_map,
                    setFantasyFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.pictograph_font_family_map,
                    setPictographFontFamilyWrapper, settings);
  settings->setDefaultFontSize(prefs.default_font_size);
  settings->setDefaultFixedFontSize(prefs.default_fixed_font_size);
  settings->setMinimumFontSize(prefs.minimum_font_size);
  settings->setMinimumLogicalFontSize(prefs.minimum_logical_font_size);
  settings->setDefaultTextEncodingName(ASCIIToUTF16(prefs.default_encoding));
  settings->setApplyDefaultDeviceScaleFactorInCompositor(
      prefs.apply_default_device_scale_factor_in_compositor);
  settings->setApplyPageScaleFactorInCompositor(
      prefs.apply_page_scale_factor_in_compositor);
  settings->setJavaScriptEnabled(prefs.javascript_enabled);
  // ------------------- kikin Modification Starts --------------------
  #if defined(SBROWSER_KIKIN)
  web_view->setKikinEnabled(prefs.kikin_enabled);
  #endif
  // -------------------- kikin Modification Ends ---------------------
  settings->setWebSecurityEnabled(prefs.web_security_enabled);
  settings->setJavaScriptCanOpenWindowsAutomatically(
      prefs.javascript_can_open_windows_automatically);
  settings->setLoadsImagesAutomatically(prefs.loads_images_automatically);
  settings->setImagesEnabled(prefs.images_enabled);
  settings->setPluginsEnabled(prefs.plugins_enabled);
  settings->setDOMPasteAllowed(prefs.dom_paste_enabled);
  settings->setNeedsSiteSpecificQuirks(prefs.site_specific_quirks_enabled);
  settings->setShrinksStandaloneImagesToFit(
      prefs.shrinks_standalone_images_to_fit);
  settings->setUsesEncodingDetector(prefs.uses_universal_detector);
  settings->setTextAreasAreResizable(prefs.text_areas_are_resizable);
  settings->setAllowScriptsToCloseWindows(prefs.allow_scripts_to_close_windows);
  if (prefs.user_style_sheet_enabled)
    settings->setUserStyleSheetLocation(prefs.user_style_sheet_location);
  else
    settings->setUserStyleSheetLocation(WebURL());
  // SAMSUNG_CHANGE : FontBoosting mode
#if defined(SBROWSER_FONT_BOOSTING_PREFERENCES)
  web_view->setFontBoostingModeEnabled(prefs.fontboosting_mode_enabled);
#endif // SBROWSER_FONT_BOOSTING_PREFERENCES
  // SAMSUNG_CHANGE : FontBoosting mode
  settings->setAuthorAndUserStylesEnabled(prefs.author_and_user_styles_enabled);
  #if defined(SBROWSER_BINGSEARCH_ENGINE_SET_VIA_JAVASCRIPT)  
  settings->setBingAsDefaultSearchEngine(prefs.is_bing_as_default_search_engine);
  #endif
  settings->setDownloadableBinaryFontsEnabled(prefs.remote_fonts_enabled);
  settings->setJavaScriptCanAccessClipboard(
      prefs.javascript_can_access_clipboard);
  settings->setXSSAuditorEnabled(prefs.xss_auditor_enabled);
  settings->setDNSPrefetchingEnabled(prefs.dns_prefetching_enabled);
  settings->setLocalStorageEnabled(prefs.local_storage_enabled);
  settings->setSyncXHRInDocumentsEnabled(prefs.sync_xhr_in_documents_enabled);
  WebRuntimeFeatures::enableDatabase(prefs.databases_enabled);
  settings->setOfflineWebApplicationCacheEnabled(
      prefs.application_cache_enabled);
  settings->setCaretBrowsingEnabled(prefs.caret_browsing_enabled);
  settings->setHyperlinkAuditingEnabled(prefs.hyperlink_auditing_enabled);
  settings->setCookieEnabled(prefs.cookie_enabled);

  // This setting affects the behavior of links in an editable region:
  // clicking the link should select it rather than navigate to it.
  // Safari uses the same default. It is unlikley an embedder would want to
  // change this, since it would break existing rich text editors.
  settings->setEditableLinkBehaviorNeverLive();

  settings->setFontRenderingModeNormal();
  settings->setJavaEnabled(prefs.java_enabled);

  // By default, allow_universal_access_from_file_urls is set to false and thus
  // we mitigate attacks from local HTML files by not granting file:// URLs
  // universal access. Only test shell will enable this.
  settings->setAllowUniversalAccessFromFileURLs(
      prefs.allow_universal_access_from_file_urls);
  settings->setAllowFileAccessFromFileURLs(
      prefs.allow_file_access_from_file_urls);
#if defined (SBROWSER_SAVEPAGE_IMPL)
  // Apply Android content:// access restrictions.
  settings->setAllowContentURLAccess(prefs.allow_content_url_access);
#endif

#if defined(SBROWSER_CHROME_NATIVE_PREFERENCES)
  web_view->setOverviewModeEnabled(prefs.overview_mode_enabled);
#endif //SBROWSER_CHROME_NATIVE_PREFERENCES

#if defined(SBROWSER_IMIDEO_PREFERENCES) // Imideo >>
  settings->setImideoDebugMode(prefs.imideo_debug_mode);
#endif // Imideo <<
  
  // We prevent WebKit from checking if it needs to add a "text direction"
  // submenu to a context menu. it is not only because we don't need the result
  // but also because it cause a possible crash in Editor::hasBidiSelection().
  settings->setTextDirectionSubmenuInclusionBehaviorNeverIncluded();

  // Enable the web audio API if requested on the command line.
  settings->setWebAudioEnabled(prefs.webaudio_enabled);

#if defined(SBROWSER_POWER_SAVER_MODE_IMPL)
  settings->setDisableProgressiveImageDecoding(prefs.disable_progressive_image_decoding);
  settings->setDisableGIFAnimation(prefs.disable_gif_animation);
  settings->setDisableFilterBitmap(prefs.disable_filter_bitmap);
  settings->setEnableBgColorOffWhite(prefs.enable_bgcolor_Offwhite);
  settings->setEnableBgColorLightGrey(prefs.enable_bgcolor_lightgrey);
  settings->setEnableBgColorGrey(prefs.enable_bgcolor_grey);
#endif
#if defined(SBROWSER_ENABLE_APPNAP) 
	settings->setEnableAppNap(prefs.enable_appnap);
#endif
  // Enable experimental WebGL support if requested on command line
  // and support is compiled in.
  settings->setExperimentalWebGLEnabled(prefs.experimental_webgl_enabled);

  // Disable GL multisampling if requested on command line.
  settings->setOpenGLMultisamplingEnabled(prefs.gl_multisampling_enabled);

  // Enable privileged WebGL extensions for Chrome extensions or if requested
  // on command line.
  settings->setPrivilegedWebGLExtensionsEnabled(
      prefs.privileged_webgl_extensions_enabled);

  // Enable WebGL errors to the JS console if requested.
  settings->setWebGLErrorsToConsoleEnabled(
      prefs.webgl_errors_to_console_enabled);

  // Enables accelerated compositing for overflow scroll.
  settings->setAcceleratedCompositingForOverflowScrollEnabled(
      prefs.accelerated_compositing_for_overflow_scroll_enabled);

  // Enables accelerated compositing for scrollable frames if requested on
  // command line.
  settings->setAcceleratedCompositingForScrollableFramesEnabled(
      prefs.accelerated_compositing_for_scrollable_frames_enabled);

  // Enables composited scrolling for frames if requested on command line.
  settings->setCompositedScrollingForFramesEnabled(
      prefs.composited_scrolling_for_frames_enabled);

  // Uses the mock theme engine for scrollbars.
  settings->setMockScrollbarsEnabled(prefs.mock_scrollbars_enabled);

  settings->setThreadedHTMLParser(prefs.threaded_html_parser);

  // Display visualization of what has changed on the screen using an
  // overlay of rects, if requested on the command line.
  settings->setShowPaintRects(prefs.show_paint_rects);

  // Enable gpu-accelerated compositing if requested on the command line.
  settings->setAcceleratedCompositingEnabled(
      prefs.accelerated_compositing_enabled);

  // Enable gpu-accelerated 2d canvas if requested on the command line.
  settings->setAccelerated2dCanvasEnabled(prefs.accelerated_2d_canvas_enabled);

  settings->setMinimumAccelerated2dCanvasSize(
      prefs.minimum_accelerated_2d_canvas_size);

  // Disable antialiasing for 2d canvas if requested on the command line.
  settings->setAntialiased2dCanvasEnabled(
      !prefs.antialiased_2d_canvas_disabled);

  // Enable gpu-accelerated filters if requested on the command line.
  settings->setAcceleratedFiltersEnabled(prefs.accelerated_filters_enabled);

  // Enable gesture tap highlight if requested on the command line.
  settings->setGestureTapHighlightEnabled(prefs.gesture_tap_highlight_enabled);

  // Enabling accelerated layers from the command line enabled accelerated
  // 3D CSS, Video, and Animations.
  settings->setAcceleratedCompositingFor3DTransformsEnabled(
      prefs.accelerated_compositing_for_3d_transforms_enabled);
  settings->setAcceleratedCompositingForVideoEnabled(
      prefs.accelerated_compositing_for_video_enabled);
  settings->setAcceleratedCompositingForAnimationEnabled(
      prefs.accelerated_compositing_for_animation_enabled);

  // Enabling accelerated plugins if specified from the command line.
  settings->setAcceleratedCompositingForPluginsEnabled(
      prefs.accelerated_compositing_for_plugins_enabled);

  // WebGL and accelerated 2D canvas are always gpu composited.
  settings->setAcceleratedCompositingForCanvasEnabled(
      prefs.experimental_webgl_enabled || prefs.accelerated_2d_canvas_enabled);

  // Enable memory info reporting to page if requested on the command line.
  settings->setMemoryInfoEnabled(prefs.memory_info_enabled);

  settings->setAsynchronousSpellCheckingEnabled(
      prefs.asynchronous_spell_checking_enabled);
  settings->setUnifiedTextCheckerEnabled(prefs.unified_textchecker_enabled);

  for (webkit_glue::WebInspectorPreferences::const_iterator it =
           prefs.inspector_settings.begin();
       it != prefs.inspector_settings.end(); ++it)
    web_view->setInspectorSetting(WebString::fromUTF8(it->first),
                                  WebString::fromUTF8(it->second));

  // Tabs to link is not part of the settings. WebCore calls
  // ChromeClient::tabsToLinks which is part of the glue code.
  web_view->setTabsToLinks(prefs.tabs_to_links);

  settings->setInteractiveFormValidationEnabled(true);

  settings->setFullScreenEnabled(prefs.fullscreen_enabled);
  settings->setAllowDisplayOfInsecureContent(
      prefs.allow_displaying_insecure_content);
  settings->setAllowRunningOfInsecureContent(
      prefs.allow_running_insecure_content);
  settings->setPasswordEchoEnabled(prefs.password_echo_enabled);
  settings->setShouldPrintBackgrounds(prefs.should_print_backgrounds);
  settings->setEnableScrollAnimator(prefs.enable_scroll_animator);
  settings->setVisualWordMovementEnabled(prefs.visual_word_movement_enabled);

  settings->setCSSStickyPositionEnabled(prefs.css_sticky_position_enabled);
  settings->setExperimentalCSSCustomFilterEnabled(prefs.css_shaders_enabled);
  settings->setExperimentalCSSVariablesEnabled(prefs.css_variables_enabled);
  settings->setExperimentalCSSGridLayoutEnabled(prefs.css_grid_layout_enabled);

  WebRuntimeFeatures::enableLazyLayout(prefs.lazy_layout_enabled);
  WebRuntimeFeatures::enableTouch(prefs.touch_enabled);
  settings->setDeviceSupportsTouch(prefs.device_supports_touch);
  settings->setDeviceSupportsMouse(prefs.device_supports_mouse);
  settings->setEnableTouchAdjustment(prefs.touch_adjustment_enabled);
  settings->setTouchDragDropEnabled(prefs.touch_drag_drop_enabled);

  settings->setFixedPositionCreatesStackingContext(
      prefs.fixed_position_creates_stacking_context);

  settings->setDeferredImageDecodingEnabled(
      prefs.deferred_image_decoding_enabled);
  settings->setShouldRespectImageOrientation(
      prefs.should_respect_image_orientation);

  settings->setUnsafePluginPastingEnabled(false);
  settings->setEditingBehavior(
      static_cast<WebSettings::EditingBehavior>(prefs.editing_behavior));

  settings->setSupportsMultipleWindows(prefs.supports_multiple_windows);

  settings->setViewportEnabled(prefs.viewport_enabled);
  settings->setInitializeAtMinimumPageScale(
      prefs.initialize_at_minimum_page_scale);

  settings->setSmartInsertDeleteEnabled(prefs.smart_insert_delete_enabled);

  settings->setSpatialNavigationEnabled(prefs.spatial_navigation_enabled);

  settings->setSelectionIncludesAltImageText(true);

#if defined(OS_ANDROID)
  settings->setAllowCustomScrollbarInMainFrame(false);
#if defined(SBROWSER_FONT_BOOSTING_PREFERENCES)
  //settings->setFontBoostingVersion((int)fontboosting_mode_enabled);
  settings->setTextAutosizingEnabled(prefs.fontboosting_mode_enabled);
#else
  //settings->setFontBoostingVersion(font_boosting_version);
  settings->setTextAutosizingEnabled(prefs.text_autosizing_enabled);
#endif /* SBROWSER_FONT_BOOSTING_PREFERENCES */
  //settings->setTextAutosizingEnabled(prefs.text_autosizing_enabled);
  settings->setTextAutosizingFontScaleFactor(prefs.font_scale_factor);
  web_view->setIgnoreViewportTagMaximumScale(prefs.force_enable_zoom);
  settings->setAutoZoomFocusedNodeToLegibleScale(true);
  settings->setDoubleTapToZoomEnabled(prefs.double_tap_to_zoom_enabled);
  settings->setMediaPlaybackRequiresUserGesture(
      prefs.user_gesture_required_for_media_playback);
  settings->setDefaultVideoPosterURL(
        ASCIIToUTF16(prefs.default_video_poster_url.spec()));
  settings->setSupportDeprecatedTargetDensityDPI(
      prefs.support_deprecated_target_density_dpi);
  settings->setUseWideViewport(prefs.use_wide_viewport);
#endif

  WebNetworkStateNotifier::setOnLine(prefs.is_online);
}

#define COMPILE_ASSERT_MATCHING_ENUMS(webkit_glue_name, webkit_name)         \
    COMPILE_ASSERT(                                                          \
        static_cast<int>(webkit_glue_name) == static_cast<int>(webkit_name), \
        mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUMS(
    webkit_glue::EDITING_BEHAVIOR_MAC, WebSettings::EditingBehaviorMac);
COMPILE_ASSERT_MATCHING_ENUMS(
    webkit_glue::EDITING_BEHAVIOR_WIN, WebSettings::EditingBehaviorWin);
COMPILE_ASSERT_MATCHING_ENUMS(
    webkit_glue::EDITING_BEHAVIOR_UNIX, WebSettings::EditingBehaviorUnix);
COMPILE_ASSERT_MATCHING_ENUMS(
    webkit_glue::EDITING_BEHAVIOR_ANDROID, WebSettings::EditingBehaviorAndroid);

}  // namespace webkit_glue

