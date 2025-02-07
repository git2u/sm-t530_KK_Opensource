// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines various defaults whose values varies depending upon the OS.

#ifndef CHROME_BROWSER_DEFAULTS_H_
#define CHROME_BROWSER_DEFAULTS_H_

#include "build/build_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "ui/base/resource/resource_bundle.h"

namespace browser_defaults {

#if defined(USE_X11)

// Size of the font used in the autocomplete box for normal windows, in pixels.
extern const int kAutocompleteEditFontPixelSize;

// Size of the font used in the autocomplete box for popup windows, in pixels.
extern const int kAutocompleteEditFontPixelSizeInPopup;

// Can the user toggle the system title bar?
extern const bool kCanToggleSystemTitleBar;


#endif

#if defined(SBROWSER_CHROME_NATIVE_PREFERENCES)
extern std::string kCountryCodeAtInstall;
#endif //#if defined(SBROWSER_CHROME_NATIVE_PREFERENCES)

// Width of mini-tabs.
extern const int kMiniTabWidth;

// Should session restore restore popup windows?
extern const bool kRestorePopups;

// Can the browser be alive without any browser windows?
extern const bool kBrowserAliveWithNoWindows;

// Should a link be shown on the bookmark bar allowing the user to import
// bookmarks?
extern const bool kShowImportOnBookmarkBar;

// Whether various menu items are shown.
extern const bool kShowExitMenuItem;
extern const bool kShowFeedbackMenuItem;
extern const bool kShowHelpMenuItemIcon;
extern const bool kShowUpgradeMenuItem;

// Does the OS support other browsers? If not, operations such as default
// browser check are not done.
extern const bool kOSSupportsOtherBrowsers;

// Does the download page have the show in folder option?
extern const bool kDownloadPageHasShowInFolder;

// Should the tab strip be sized to the top of the tab strip?
extern const bool kSizeTabButtonToTopOfTabStrip;

// If true, we want to automatically start sync signin whenever we have
// credentials (user doesn't need to go through the startup flow). This is
// typically enabled on platforms (like ChromeOS) that have their own
// distinct signin flow.
extern const bool kSyncAutoStarts;

// Should other browsers be shown in about:memory page?
extern const bool kShowOtherBrowsersInAboutMemory;

// Should always open incognito windows when started with --incognito switch?
extern const bool kAlwaysOpenIncognitoWindow;

// Should the close button be shown in the Task Manager dialog?
extern const bool kShowCancelButtonInTaskManager;

// Preferred height of the bookmarks bar when shown on every page.
extern const int kBookmarkBarHeight;

// ChromiumOS network menu font
extern const ui::ResourceBundle::FontStyle kAssociatedNetworkFontStyle;

// Preferred infobar border padding in pixels.
extern const int kInfoBarBorderPaddingVertical;

// Last character display for passwords.
extern const bool kPasswordEchoEnabled;

// Indicates whether session restore should always create a new
// tabbed browser. This is true every where except on ChromeOS
// where we want the desktop to show through in this situation.
extern const bool kAlwaysCreateTabbedBrowserOnSessionRestore;

//=============================================================================
// Runtime "const" - set only once after parsing command line option and should
// never be modified after that.

// Are bookmark enabled? True by default.
extern bool bookmarks_enabled;

// Whether HelpApp is enabled. True by default. This is only used by Chrome OS
// today.
extern bool enable_help_app;

// SAMSUNG_CHANGE : Reset to default setting
#if defined(SBROWSER_ENABLE_RESET_DEFAULT_SETTINGS)
 // separate pref member for remembering form data for autofilling
 extern const bool kRememberFormDataEnabled;
extern const bool kAutofillEnabled;
extern const bool kResolveNavigationErrorEnabled;
extern const bool kSearchSuggestEnabled;
extern const bool kNetworkPredictionEnabled;
extern const float kFontScaleFactor;
extern const bool kForceZoomEnabled;
extern const bool kJavascriptEnabled;
// ------------------- kikin Modification Starts --------------------
#if defined(SBROWSER_KIKIN)
extern const bool kKikinEnabled;
#endif
// -------------------- kikin Modification Ends ---------------------
extern const bool kPopupEnabled;
extern const bool kCookiesEnabled;
extern const bool kLocationEnabled;
extern const bool kRememberPasswordEnabled;
extern const int  kWebNotificationType;
extern const bool kLoadImagesAutomaticallyEnabled;
extern const bool kRemoteDebuggingEnabled;
extern const std::string kCharset;
extern const bool kBlockzoomEnabled;
extern const bool kOverviewModeEnabled;

#if defined(SBROWSER_FONT_BOOSTING_PREFERENCES)
extern const bool kFontBoostingModeEnabled;
#endif /* SBROWSER_FONT_BOOSTING_PREFERENCES */
extern const int  kSearchEngine;
extern const int kHomeScreenMode;
#if defined(SBROWSER_IMIDEO_PREFERENCES)
extern const int kImideoDebugMode; // Imideo
#endif
#endif // SBROWSER_ENABLE_RESET_DEFAULT_SETTINGS
}  // namespace browser_defaults

#endif  // CHROME_BROWSER_DEFAULTS_H_
