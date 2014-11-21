// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/debug/trace_event.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/android/content_view_util.h"
#include "chrome/browser/android/dev_tools_server.h"
#include "chrome/browser/android/intent_helper.h"
#include "chrome/browser/android/provider/chrome_browser_provider.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/history/android/sqlite_cursor.h"
#include "chrome/browser/lifetime/application_lifetime_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search_engines/template_url_service_android.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_view_android.h"
#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/ui/android/javascript_app_modal_dialog_android.h"
#include "chrome/browser/ui/android/navigation_popup.h"
#if defined(SBROWSER_CAC)
#include "chrome/browser/android/sbr/ui/android/sbr_ssl_client_certificate_request.h"
#else
#include "chrome/browser/ui/android/ssl_client_certificate_request.h"
#endif
#include "chrome/browser/ui/android/status_tray_android.h"
#include "chrome/browser/ui/android/website_settings_popup_android.h"
#include "components/autofill/browser/android/component_jni_registrar.h"
#include "components/navigation_interception/component_jni_registrar.h"
#include "components/web_contents_delegate_android/component_jni_registrar.h"

#ifdef SBROWSER_BOOKMARKS
#include "chrome/browser/android/sbr/provider/sbr_chrome_browser_provider.h"
#endif
#ifdef SBROWSER_CHROME_NATIVE_PREFERENCES
#include "chrome/browser/android/sbr/preferences/sbr_chrome_native_preferences.h"
#include "chrome/browser/android/sbr/preferences/sbr_website_settings_utils.h"
#endif //SBROWSER_CHROME_NATIVE_PREFERENCES
#if defined(SBROWSER_HISTORY)
#include "chrome/browser/android/sbr/history/history_model.h"
#include "chrome/browser/android/sbr/history/mostvisted_model.h"
#endif
#if defined(SBROWSER_CHROME_NATIVE_PREFERENCES)
#include "chrome/browser/android/sbr/locationbar/sbr_location_bar.h"
#include "chrome/browser/android/sbr/delegate/sbr_web_contents_delegate_android.h"
#include "chrome/browser/android/sbr/autocomplete/sbr_autocomplete_bridge.h"
#include "chrome/browser/android/sbr/favicon/sbr_favicon_controller.h"
#include "chrome/browser/android/sbr/infobar/sbr_infobar_android.h"
#include "chrome/browser/android/sbr/URLUtilities/sbr_url_utilities.h"
#include "chrome/browser/android/sbr/tab/sbr_tab_model.h"
#include "chrome/browser/android/sbr/touchicon/sbr_touchicon_controller.h"

#endif //SBROWSER_SUPPORT
#if defined(SBROWSER_HTML5_WEBNOTIFICATION)
#include "chrome/browser/android/sbr/notification/sbr_notification_ui_manager_android.h"
#endif
#if defined(SBROWSER_CUSTOM_HTTP_REQUEST_HEADER)
#include "chrome/browser/android/sbr/net/sbr_custom_http_request_header.h"
#endif
#if defined(SBROWSER_MEDIAPLAYER_MOTION_LISTENER)
#include "chrome/browser/android/sbr/media/sbr_media_player_motion_listener.h"
#endif
#ifdef SBROWSER_FP_SUPPORT_PART2
#include "chrome/browser/android/sbr/passwordmanager/sbr_password_manager_utility.h"
#endif
#if defined(SBROWSER_CAC)
#include "chrome/browser/android/sbr/net/sbr_smartcard_helper.h"
#endif
#if defined(SBROWSER_QUICK_LAUNCH)
#include "chrome/browser/android/sbr/quicklaunch/quicklaunch_model.h"
#endif
#if defined(SBROWSER_CHECK_REVOKED_CERTIFICATE)
#include "chrome/browser/android/sbr/net/sbr_android_network_library.h"
#endif
bool RegisterCertificateViewer(JNIEnv* env);

namespace chrome {
namespace android {

static base::android::RegistrationMethod kChromeRegisteredMethods[] = {
  // Register JNI for components we depend on.
  { "NavigationInterception", components::RegisterNavigationInterceptionJni },
  { "WebContentsDelegateAndroid",
      components::RegisterWebContentsDelegateAndroidJni },
  { "RegisterAuxiliaryProfileLoader",
      autofill::RegisterAutofillAndroidJni },
  // Register JNI for chrome classes.
  { "ApplicationLifetime", RegisterApplicationLifetimeAndroid},
  { "AutofillDialog",
      autofill::AutofillDialogViewAndroid::RegisterAutofillDialogViewAndroid},
  { "AutofillPopup",
      autofill::AutofillPopupViewAndroid::RegisterAutofillPopupViewAndroid},
  { "CertificateViewer", RegisterCertificateViewer},
  { "ChromeBrowserProvider",
      ChromeBrowserProvider::RegisterChromeBrowserProvider },
  { "ChromeHttpAuthHandler",
      ChromeHttpAuthHandler::RegisterChromeHttpAuthHandler },
#if defined(SBROWSER_CHROME_NATIVE_PREFERENCES)
  { "SbrWebContentsDelegateAndroid",RegisterSbrWebContentsDelegateAndroid },
  { "SbrAutocompleteBridge",RegisterSbrAutocompleteBridge },
  {"SbrLocationBar",RegisterSbrLocationBar},
  { "SbrFaviconController", RegisterSbrFaviconController },
  { "SbrTouchiconController",   RegisterSbrTouchiconController },
  {"SbrInfoBarContainerAndroid",RegisterInfoBarContainer},
  {"SbrURLUtilities",RegisterSbrURLUtilities},
  {"SbrTabModel",SbrTabModel::RegisterSbrTabModel},
#endif // SBROWSER_SUPPORT		
#if defined(SBROWSER_HTML5_WEBNOTIFICATION)
	{ "WebNotificationUIManager",RegisterNotificationUIManagerImpl },
#endif //	SBROWSER_HTML5_WEBNOTIFICATION	
// Sbrowser changes end      
  { "ContentViewUtil", RegisterContentViewUtil },
  { "DevToolsServer", RegisterDevToolsServer },
  { "IntentHelper", RegisterIntentHelper },
  { "JavascriptAppModalDialog",
      JavascriptAppModalDialogAndroid::RegisterJavascriptAppModalDialog },
  { "NavigationPopup", NavigationPopup::RegisterNavigationPopup },
  { "PersonalDataManagerAndroid",
      autofill::PersonalDataManagerAndroid::Register},
  { "ProfileAndroid", ProfileAndroid::RegisterProfileAndroid },
  { "ProfileSyncService", ProfileSyncServiceAndroid::Register },
  { "SqliteCursor", SQLiteCursor::RegisterSqliteCursor },
#if defined(SBROWSER_CAC)
  { "SbrSSLClientCertificateRequest",
      RegisterSbrSSLClientCertificateRequestAndroid },
#else
  { "SSLClientCertificateRequest",
      RegisterSSLClientCertificateRequestAndroid },
#endif
  { "StatusTray", StatusTrayAndroid::Register },
  { "TabAndroid", TabAndroid::RegisterTabAndroid },
  { "TemplateUrlServiceAndroid", TemplateUrlServiceAndroid::Register },
  { "WebsiteSettingsPopupAndroid",
      WebsiteSettingsPopupAndroid::RegisterWebsiteSettingsPopupAndroid },
#ifdef SBROWSER_BOOKMARKS
  { "SbrChromeBrowserProvider",
      SbrChromeBrowserProvider::RegisterSbrChromeBrowserProvider },
#endif //SBROWSER_BOOKMARKS
#ifdef SBROWSER_CHROME_NATIVE_PREFERENCES
  { "SbrChromeNativePreferences",
      RegisterSbrChromeNativePreferences},
  { "SbrWebSiteSettingsUtils",
      RegisterSbrWebsiteSettingsUtils},      
#endif //SBROWSER_CHROME_NATIVE_PREFERENCES
#if defined(SBROWSER_HISTORY)
  { "HistoryModel",
      HistoryModel::RegisterHistoryModel },
  { "MostVisitedModel",
      RegisterMostVisitedModel},
#endif
#if defined(SBROWSER_CUSTOM_HTTP_REQUEST_HEADER)
  { "CustomHttpRequestHeader",
      net::RegisterCustomHttpRequestHeader },
#endif
#if defined(SBROWSER_MEDIAPLAYER_MOTION_LISTENER)
  { "SbrMediaPlayerMotionListener",
      media::SbrMediaPlayerMotionListener::RegisterSbrMediaPlayerMotionListener },
#endif
 #ifdef SBROWSER_FP_SUPPORT_PART2
  {"SbrPasswordManagerUtility",RegisterSbrPasswordManagerUtility},
 #endif
#if defined(SBROWSER_CAC)
  { "SmartcardHelper",net::RegisterSmartcardHelper },
#endif
#if defined(SBROWSER_QUICK_LAUNCH)
  { "QuickLaunchModel",
      QuickLaunchModel::RegisterQuickLaunchModel },
#endif
#if defined(SBROWSER_CHECK_REVOKED_CERTIFICATE)
  { "SbrAndroidNetworkLibrary",
    RegisterSbrAndroidNetowrkLibrary },
#endif
};

bool RegisterJni(JNIEnv* env) {
  TRACE_EVENT0("startup", "chrome_android::RegisterJni");
  return RegisterNativeMethods(env, kChromeRegisteredMethods,
                               arraysize(kChromeRegisteredMethods));
}

} // namespace android
} // namespace chrome
