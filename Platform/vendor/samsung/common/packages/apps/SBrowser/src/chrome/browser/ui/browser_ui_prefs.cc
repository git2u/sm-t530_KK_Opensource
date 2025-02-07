// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_ui_prefs.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

namespace chrome {

void RegisterBrowserPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kOptionsWindowLastTabIndex, 0);
  registry->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);
  registry->RegisterIntegerPref(prefs::kShowFirstRunBubbleOption,
                             first_run::FIRST_RUN_BUBBLE_DONT_SHOW);
}

void RegisterBrowserUserPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kHomePageChanged,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kHomePageIsNewTabPage,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kShowHomeButton,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(OS_MACOSX)
  // This really belongs in platform code, but there's no good place to
  // initialize it between the time when the AppController is created
  // (where there's no profile) and the time the controller gets another
  // crack at the start of the main event loop. By that time,
  // StartupBrowserCreator has already created the browser window, and it's too
  // late: we need the pref to be already initialized. Doing it here also saves
  // us from having to hard-code pref registration in the several unit tests
  // that use this preference.
  registry->RegisterBooleanPref(
      prefs::kShowUpdatePromotionInfoBar,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
  registry->RegisterBooleanPref(
      prefs::kDeleteBrowsingHistory,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDeleteDownloadHistory,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDeleteCache,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDeleteCookies,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDeletePasswords,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDeleteFormData,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDeleteHostedAppsData,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kDeleteTimePeriod,
      0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterInt64Pref(
      prefs::kLastClearBrowsingDataTime,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kModuleConflictBubbleShown,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kCheckDefaultBrowser,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#if defined(OS_WIN)
  // As with Mac-spacific code above, it should be in a platform-specific
  // section somewhere, but there is no good place for it.
  registry->RegisterBooleanPref(
      prefs::kSuppressSwitchToMetroModeOnSetDefault,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
  registry->RegisterBooleanPref(
      prefs::kShowOmniboxSearchHint,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebAppCreateOnDesktop,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebAppCreateInAppsMenu,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kWebAppCreateInQuickLaunchBar,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEnableTranslate,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kCloudPrintEmail,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kCloudPrintProxyEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kCloudPrintSubmitEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDevToolsDisabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kDevToolsHSplitLocation,
      -1,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kDevToolsVSplitLocation,
      -1,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kBrowserWindowPlacement,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kPreferencesWindowPlacement,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kImportBookmarks,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kImportHistory,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kImportHomepage,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kImportSearchEngine,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kImportSavedPasswords,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEnableDoNotTrack,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Dictionaries to keep track of default tasks in the file browser.
  registry->RegisterDictionaryPref(
      prefs::kDefaultTasksByMimeType,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDefaultTasksBySuffix,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // We need to register the type of these preferences in order to query
  // them even though they're only typically controlled via policy.
  registry->RegisterBooleanPref(
      prefs::kPluginsAllowOutdated,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPluginsAlwaysAuthorize,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kClearPluginLSODataEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kHideWebStoreIcon,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void RegisterAppPrefs(const std::string& app_name, Profile* profile) {
  // We need to register the window position pref.
  //
  // TODO(mnissler): Use a separate pref name pointing to a single
  // dictionary instead. Also tracked as http://crbug.com/167256
  std::string window_pref(prefs::kBrowserWindowPlacement);
  window_pref.append("_");
  window_pref.append(app_name);
  PrefService* prefs = profile->GetPrefs();
  if (!prefs->FindPreference(window_pref.c_str())) {
    // TODO(joi): Do all registration up front.
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        static_cast<user_prefs::PrefRegistrySyncable*>(
            prefs->DeprecatedGetPrefRegistry()));
    registry->RegisterDictionaryPref(
        window_pref.c_str(), user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  }
}


}  // namespace chrome
