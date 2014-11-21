#ifndef CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_PREF_NAMES
#define CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_PREF_NAMES
#include <stddef.h>
#include "build/build_config.h"
namespace sbrprefs {
extern const char kWebkitOverviewModeEnabled[];
extern const char kRememberFormDataEnabled[];
extern const char kWebkitHomeScreenMode[];
//extern const char kSaveFileDefaultDirectory[];
extern const char kBandwidthConservationSetting[];
#if defined(SBROWSER_USE_WIDE_VIEWPORT)
extern const char kWebKitUseWideViewport[];
#endif
#if defined(SBROWSER_SAVEPAGE_IMPL)
extern const char kWebKitAllowContentURLAccess[];
#endif
#if defined(SBROWSER_FONT_BOOSTING_PREFERENCES)
extern const char kWebKitFontBoostingModeEnabled[];
#endif /* SBROWSER_FONT_BOOSTING_PREFERENCES */
#if defined(SBROWSER_IMIDEO_PREFERENCES)
//extern const char kWebkitImideoDebugMode[];
#endif
#if defined(SBROWSER_ENABLE_APPNAP) 
extern const char kEnableAppNap[];
#endif
#if defined(SBROWSER_POWER_SAVER_MODE_IMPL)
extern const char kEnableBgColorOffWhite[];
extern const char kEnableBgColorLightGrey[];
extern const char kEnableBgColorGrey[];
extern const char kDisableGifAnimation[];
extern const char kDisableProgressiveImageDecode[];
extern const char kDisableFilterBitmap[];
#endif
#if defined(SBROWSER_ENABLE_ADBLOCKER)
extern const char kEnableAdBlocker[];
#endif
}  // namespace prefs

#endif// CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_PREF_NAMES
