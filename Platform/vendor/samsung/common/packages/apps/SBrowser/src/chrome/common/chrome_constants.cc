// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"

#include "base/files/file_path.h"

#define FPL FILE_PATH_LITERAL

#if defined(OS_MACOSX)
#define CHROMIUM_PRODUCT_STRING "Chromium"
#if defined(GOOGLE_CHROME_BUILD)
#define PRODUCT_STRING "Google Chrome"
#elif defined(CHROMIUM_BUILD)
#define PRODUCT_STRING "Chromium"
#else
#error Unknown branding
#endif
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#if defined(GOOGLE_CHROME_BUILD)
#define PRODUCT_STRING_PATH L"Google\\Chrome"
#elif defined(CHROMIUM_BUILD)
#define PRODUCT_STRING_PATH L"Chromium"
#else
#error Unknown branding
#endif
#endif  // defined(OS_WIN)

namespace chrome {

const char kChromeVersionEnvVar[] = "CHROME_VERSION";

// The following should not be used for UI strings; they are meant
// for system strings only. UI changes should be made in the GRD.
//
// There are four constants used to locate the executable name and path:
//
//     kBrowserProcessExecutableName
//     kHelperProcessExecutableName
//     kBrowserProcessExecutablePath
//     kHelperProcessExecutablePath
//
// In one condition, our tests will be built using the Chrome branding
// though we want to actually execute a Chromium branded application.
// This happens for the reference build on Mac.  To support that case,
// we also include a Chromium version of each of the four constants and
// in the UITest class we support switching to that version when told to
// do so.

#if defined(OS_WIN)
const base::FilePath::CharType kBrowserProcessExecutableNameChromium[] =
    FPL("chrome.exe");
const base::FilePath::CharType kBrowserProcessExecutableName[] =
    FPL("chrome.exe");
const base::FilePath::CharType kHelperProcessExecutableNameChromium[] =
    FPL("chrome.exe");
const base::FilePath::CharType kHelperProcessExecutableName[] =
    FPL("chrome.exe");
#elif defined(OS_MACOSX)
const base::FilePath::CharType kBrowserProcessExecutableNameChromium[] =
    FPL(CHROMIUM_PRODUCT_STRING);
const base::FilePath::CharType kBrowserProcessExecutableName[] =
    FPL(PRODUCT_STRING);
const base::FilePath::CharType kHelperProcessExecutableNameChromium[] =
    FPL(CHROMIUM_PRODUCT_STRING " Helper");
const base::FilePath::CharType kHelperProcessExecutableName[] =
    FPL(PRODUCT_STRING " Helper");
#elif defined(OS_ANDROID)
// NOTE: Keep it synced with the process names defined in AndroidManifest.xml.
const base::FilePath::CharType kBrowserProcessExecutableName[] = FPL("chrome");
const base::FilePath::CharType kBrowserProcessExecutableNameChromium[] =
    FPL("");
const base::FilePath::CharType kHelperProcessExecutableName[] =
    FPL("sandboxed_process");
const base::FilePath::CharType kHelperProcessExecutableNameChromium[] = FPL("");
#elif defined(OS_POSIX)
const base::FilePath::CharType kBrowserProcessExecutableNameChromium[] =
    FPL("chrome");
const base::FilePath::CharType kBrowserProcessExecutableName[] = FPL("chrome");
// Helper processes end up with a name of "exe" due to execing via
// /proc/self/exe.  See bug 22703.
const base::FilePath::CharType kHelperProcessExecutableNameChromium[] =
    FPL("exe");
const base::FilePath::CharType kHelperProcessExecutableName[] = FPL("exe");
#endif  // OS_*

#if defined(OS_WIN)
const base::FilePath::CharType kBrowserProcessExecutablePathChromium[] =
    FPL("chrome.exe");
const base::FilePath::CharType kBrowserProcessExecutablePath[] =
    FPL("chrome.exe");
const base::FilePath::CharType kHelperProcessExecutablePathChromium[] =
    FPL("chrome.exe");
const base::FilePath::CharType kHelperProcessExecutablePath[] =
    FPL("chrome.exe");
#elif defined(OS_MACOSX)
const base::FilePath::CharType kBrowserProcessExecutablePathChromium[] =
    FPL(CHROMIUM_PRODUCT_STRING ".app/Contents/MacOS/" CHROMIUM_PRODUCT_STRING);
const base::FilePath::CharType kBrowserProcessExecutablePath[] =
    FPL(PRODUCT_STRING ".app/Contents/MacOS/" PRODUCT_STRING);
const base::FilePath::CharType kHelperProcessExecutablePathChromium[] =
    FPL(CHROMIUM_PRODUCT_STRING " Helper.app/Contents/MacOS/"
        CHROMIUM_PRODUCT_STRING " Helper");
const base::FilePath::CharType kHelperProcessExecutablePath[] =
    FPL(PRODUCT_STRING " Helper.app/Contents/MacOS/" PRODUCT_STRING " Helper");
#elif defined(OS_ANDROID)
const base::FilePath::CharType kBrowserProcessExecutablePath[] = FPL("chrome");
const base::FilePath::CharType kHelperProcessExecutablePath[] = FPL("chrome");
const base::FilePath::CharType kBrowserProcessExecutablePathChromium[] =
    FPL("chrome");
const base::FilePath::CharType kHelperProcessExecutablePathChromium[] =
    FPL("chrome");
#elif defined(OS_POSIX)
const base::FilePath::CharType kBrowserProcessExecutablePathChromium[] =
    FPL("chrome");
const base::FilePath::CharType kBrowserProcessExecutablePath[] = FPL("chrome");
const base::FilePath::CharType kHelperProcessExecutablePathChromium[] =
    FPL("chrome");
const base::FilePath::CharType kHelperProcessExecutablePath[] = FPL("chrome");
#endif  // OS_*

#if defined(OS_MACOSX)
const base::FilePath::CharType kFrameworkName[] =
    FPL(PRODUCT_STRING " Framework.framework");

const char* const kHelperFlavorSuffixes[] = {
  FPL("EH"),  // Executable heap
  FPL("NP"),  // No PIE
  NULL
};
#endif  // OS_MACOSX

const wchar_t kNaClAppName[] = L"nacl64";

#if defined(OS_WIN)
const base::FilePath::CharType kMetroDriverDll[] = FPL("metro_driver.dll");
const wchar_t kStatusTrayWindowClass[] = L"Chrome_StatusTrayWindow";
#endif  // defined(OS_WIN)

const wchar_t kMessageWindowClass[] = L"Chrome_MessageWindow";
const wchar_t kCrashReportLog[] = L"Reported Crashes.txt";
const wchar_t kTestingInterfaceDLL[] = L"testing_interface.dll";
const char    kInitialProfile[] = "Default";
const char    kMultiProfileDirPrefix[] = "Profile ";
const wchar_t kBrowserResourcesDll[] = L"chrome.dll";
const base::FilePath::CharType kExtensionFileExtension[] = FPL(".crx");
const base::FilePath::CharType kExtensionKeyFileExtension[] = FPL(".pem");

// filenames
#if defined(OS_ANDROID)
const base::FilePath::CharType kAndroidCacheFilename[] = FPL("AndroidCache");
#endif
const base::FilePath::CharType kArchivedHistoryFilename[] =
    FPL("Archived History");
const base::FilePath::CharType kBookmarksFileName[] = FPL("Bookmarks");
const base::FilePath::CharType kCacheDirname[] = FPL("Cache");
const base::FilePath::CharType kCookieFilename[] = FPL("Cookies");
const base::FilePath::CharType kCRLSetFilename[] =
    FPL("Certificate Revocation Lists");
const base::FilePath::CharType kCustomDictionaryFileName[] =
    FPL("Custom Dictionary.txt");
const base::FilePath::CharType kExtensionActivityLogFilename[] =
    FPL("Extension Activity");
const base::FilePath::CharType kExtensionsCookieFilename[] =
    FPL("Extension Cookies");
const base::FilePath::CharType kFaviconsFilename[] = FPL("Favicons");
const base::FilePath::CharType kFirstRunSentinel[] = FPL("First Run");
const base::FilePath::CharType kHistoryFilename[] = FPL("History");
const base::FilePath::CharType kJumpListIconDirname[] = FPL("JumpListIcons");
const base::FilePath::CharType kLocalStateFilename[] = FPL("Local State");
const base::FilePath::CharType kLocalStorePoolName[] = FPL("LocalStorePool");
const base::FilePath::CharType kLoginDataFileName[] = FPL("Login Data");
const base::FilePath::CharType kManagedModePolicyFilename[] =
    FPL("Managed Mode Settings");
const base::FilePath::CharType kMediaCacheDirname[] = FPL("Media Cache");
const base::FilePath::CharType kNewTabThumbnailsFilename[] =
    FPL("Top Thumbnails");
const base::FilePath::CharType kOBCertFilename[] = FPL("Origin Bound Certs");
const base::FilePath::CharType kPreferencesFilename[] = FPL("Preferences");
const base::FilePath::CharType kReadmeFilename[] = FPL("README");
const base::FilePath::CharType kSafeBrowsingBaseFilename[] =
    FPL("Safe Browsing");
const base::FilePath::CharType kServiceStateFileName[] = FPL("Service State");
const base::FilePath::CharType kShortcutsDatabaseName[] = FPL("Shortcuts");
const base::FilePath::CharType kSingletonCookieFilename[] =
    FPL("SingletonCookie");
const base::FilePath::CharType kSingletonLockFilename[] = FPL("SingletonLock");
const base::FilePath::CharType kSingletonSocketFilename[] =
    FPL("SingletonSocket");
const base::FilePath::CharType kSyncCredentialsFilename[] =
    FPL("Sync Credentials");
const base::FilePath::CharType kThemePackFilename[] = FPL("Cached Theme.pak");
const base::FilePath::CharType kThumbnailsFilename[] = FPL("Thumbnails");
const base::FilePath::CharType kTopSitesFilename[] = FPL("Top Sites");
const base::FilePath::CharType kWebAppDirname[] = FPL("Web Applications");

// File name of the Pepper Flash plugin on different platforms.
const base::FilePath::CharType kPepperFlashPluginFilename[] =
#if defined(OS_MACOSX)
    FPL("PepperFlashPlayer.plugin");
#elif defined(OS_WIN)
    FPL("pepflashplayer.dll");
#else  // OS_LINUX, etc.
    FPL("libpepflashplayer.so");
#endif

// directory names
const wchar_t kUserDataDirname[] = L"User Data";

#if defined(OS_CHROMEOS)
const base::FilePath::CharType kDriveCacheDirname[] = FPL("GCache");
const char kOemDeviceRequisitionKey[] = "oem_device_requisition";
const char kOemIsEnterpriseManagedKey[] = "oem_enterprise_managed";
const char kOemCanExitEnterpriseEnrollmentKey[] = "oem_can_exit_enrollment";
const char kOemKeyboardDrivenOobeKey[] = "oem_keyboard_driven_oobe";
#endif  // defined(OS_CHROMEOS)

// We don't enable record mode in the released product because users could
// potentially be tricked into running a product in record mode without
// knowing it.  Enable in debug builds.  Playback mode is allowed always,
// because it is useful for testing and not hazardous by itself.
#ifndef NDEBUG
// const bool kRecordModeEnabled = true;
#else
// const bool kRecordModeEnabled = false;
#endif

const bool kRecordModeEnabled = true;

const char* const kUnknownLanguageCode = "und";

const int kJavascriptMessageExpectedDelay = 1000;

#if defined(OS_ANDROID)
const bool kEnableTouchIcon = true;
#else
const bool kEnableTouchIcon = false;
#endif

const float kMaxShareOfExtensionProcesses = 0.30f;

#if defined(OS_LINUX)
const int kLowestRendererOomScore = 300;
const int kHighestRendererOomScore = 1000;
#endif

#if defined(OS_WIN)
// This is used by the PreRead experiment.
const char kPreReadEnvironmentVariable[] = "CHROME_PRE_READ_EXPERIMENT";
// This is used by chrome in Windows 8 metro mode.
const wchar_t kMetroChromeUserDataSubDir[] = L"Metro";
const wchar_t kMetroNavigationAndSearchMessage[] =
    L"CHROME_METRO_NAV_SEARCH_REQUEST";
const wchar_t kMetroGetCurrentTabInfoMessage[] =
    L"CHROME_METRO_GET_CURRENT_TAB_INFO";
const wchar_t kMetroRegistryPath[] =
    L"Software\\" PRODUCT_STRING_PATH L"\\Metro";
const wchar_t kLaunchModeValue[] = L"launch_mode";
#endif

#if defined(SBROWSER_SAVEPAGE_IMPL)
const char kSavePageURI[] = "content://com.sec.android.app.sbrowser/savepage";
#endif //SBROWSER_SAVEPAGE_IMPL

}  // namespace chrome

#undef FPL
