// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H_
#define CHROME_COMMON_CHROME_SWITCHES_H_

#include "build/build_config.h"

#include "base/base_switches.h"
#include "components/autofill/common/autofill_switches.h"
#include "content/public/common/content_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in
// media/base/media_switches.cc or ui/gl/gl_switches.cc or one of the
// .cc files corresponding to the *_switches.h files included above
// instead.
// -----------------------------------------------------------------------------

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kActionBox[];
extern const char kAllowCrossOriginAuthPrompt[];
extern const char kAllowFileAccess[];
extern const char kAllowHTTPBackgroundPage[];
extern const char kAllowHttpScreenCapture[];
extern const char kAllowLegacyExtensionManifests[];
extern const char kAllowNaClSocketAPI[];
extern const char kAllowOutdatedPlugins[];
extern const char kAllowRunningInsecureContent[];
extern const char kAllowScriptingGallery[];
extern const char kAlwaysAuthorizePlugins[];
extern const char kAppId[];
extern const char kApp[];
extern const char kAppsDevtool[];
extern const char kAppWindowSize[];
extern const char kAppsCheckoutURL[];
extern const char kAppsGalleryDownloadURL[];
extern const char kAppsGalleryInstallAutoConfirmForTests[];
extern const char kAppsGalleryURL[];
extern const char kAppsGalleryUpdateURL[];
extern const char kAppModeAuthCode[];
extern const char kAppModeOAuth2Token[];
extern const char kAppsNewInstallBubble[];
extern const char kAppsNoThrob[];
extern const char kAuthNegotiateDelegateWhitelist[];
extern const char kAuthSchemes[];
extern const char kAuthServerWhitelist[];
extern const char kAutoLaunchAtStartup[];
extern const char kAutomationClientChannelID[];
extern const char kAutomationReinitializeOnChannelError[];
extern const char kCheckForUpdateIntervalSec[];
extern const char kCheckCloudPrintConnectorPolicy[];
extern const char kChromeFrame[];
extern const char kChromeVersion[];
extern const char kCipherSuiteBlacklist[];
extern const char kClearTokenService[];
extern const char kCloudPrintDeleteFile[];
extern const char kCloudPrintFile[];
extern const char kCloudPrintJobTitle[];
extern const char kCloudPrintFileType[];
extern const char kCloudPrintPrintTicket[];
extern const char kCloudPrintSetupProxy[];
extern const char kCloudPrintServiceURL[];
extern const char kComponentUpdaterDebug[];
extern const char kConflictingModulesCheck[];
extern const char kContentSettings2[];
extern const char kCountry[];
extern const char kCrashOnHangThreads[];
extern const char kCreateBrowserOnStartupForTests[];
extern const char kDebugEnableFrameToggle[];
extern const char kDebugPackedApps[];
extern const char kDebugPrint[];
extern const char kDeviceManagementUrl[];
extern const char kDiagnostics[];
extern const char kDisableAsyncDns[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kDisableBackgroundMode[];
extern const char kDisableBackgroundNetworking[];
extern const char kDisableBundledPpapiFlash[];
extern const char kDisableBookmarkAutocompleteProvider[];
extern const char kDisableClientSidePhishingDetection[];
extern const char kDisableCloudPolicyOnSignin[];
extern const char kDisableComponentUpdate[];
extern const char kDisableCRLSets[];
extern const char kDisableCustomJumpList[];
extern const char kDisableDefaultApps[];
extern const char kDisableDhcpWpad[];
extern const char kDisableExtensionsFileAccessCheck[];
extern const char kDisableExtensionsHttpThrottling[];
extern const char kDisableExtensionsResourceWhitelist[];
extern const char kDisableExtensions[];
extern const char kDisableFullscreenApp[];
extern const char kDisableImprovedDownloadProtection[];
extern const char kDisableInstantExtendedAPI[];
extern const char kDisableIPv6[];
extern const char kDisableIPPooling[];
extern const char kDisableLocalFirstLoadNTP[];
extern const char kDisableLocalOnlyInstantExtendedAPI[];
extern const char kDisableMinimizeOnSecondLauncherItemClick[];
extern const char kDisableNativeAutofillUi[];
extern const char kDisableNTPOtherSessionsMenu[];
extern const char kDisablePopupBlocking[];
extern const char kDisablePreconnect[];
extern const char kDisablePromptOnRepost[];
extern const char kDisableRestoreBackgroundContents[];
extern const char kDisableRestoreSessionState[];
extern const char kDisableScriptedPrintThrottling[];
extern const char kDisableSync[];
extern const char kDisableSyncAppSettings[];
extern const char kDisableSyncApps[];
extern const char kDisableSyncAutofill[];
extern const char kDisableSyncAutofillProfile[];
extern const char kDisableSyncBookmarks[];
extern const char kDisableSyncDictionary[];
extern const char kDisableSyncExtensionSettings[];
extern const char kDisableSyncExtensions[];
extern const char kDisableSyncPasswords[];
extern const char kDisableSyncPreferences[];
extern const char kDisableSyncPriorityPreferences[];
extern const char kDisableSyncSearchEngines[];
extern const char kDisableSyncSyncedNotifications[];
extern const char kDisableSyncThemes[];
extern const char kDisableSyncTypedUrls[];
extern const char kDisableTranslate[];
extern const char kDisableTLSChannelID[];
extern const char kDisableWebResources[];
extern const char kDisableZeroBrowsersOpenForTests[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kDnsLogDetails[];
extern const char kDnsPrefetchDisable[];
extern const char kDisableSyncTabs[];
extern const char kEasyOffStoreExtensionInstall[];
extern const char kEnableAdview[];
extern const char kEnableAdviewSrcAttribute[];
extern const char kEnableAsyncDns[];
extern const char kEnableAuthNegotiatePort[];
extern const char kEnableAutologin[];
extern const char kEnableBenchmarking[];
extern const char kEnableCloudPrintProxy[];
extern const char kEnableComponentCloudPolicy[];
extern const char kEnableContacts[];
extern const char kEnableDesktopGuestMode[];
extern const char kEnableDevToolsExperiments[];
extern const char kEnableExperimentalExtensionApis[];
extern const char kEnableExtensionActivityLogging[];
extern const char kEnableExtensionActivityLogTesting[];
extern const char kEnableExtensionActivityUI[];
extern const char kEnableFileCookies[];
extern const char kEnableGoogleNowIntegration[];
extern const char kEnableInstantExtendedAPI[];
extern const char kEnableInteractiveAutocomplete[];
extern const char kEnableIPCFuzzing[];
extern const char kEnableIPPooling[];
extern const char kEnableIPv6[];
extern const char kEnableLocalFirstLoadNTP[];
extern const char kEnableLocalOnlyInstantExtendedAPI[];
extern const char kEnableManagedStorage[];
extern const char kEnableManagedUsers[];
extern const char kEnableMemoryInfo[];
extern const char kEnableMemoryInternalsUI[];
extern const char kEnableMetricsReportingForTesting[];
extern const char kEnableNaCl[];
extern const char kEnableNaClDebug[];
extern const char kEnableNetBenchmarking[];
extern const char kEnableNpn[];
extern const char kEnableNpnHttpOnly[];
extern const char kEnablePanels[];
extern const char kEnablePasswordGeneration[];
extern const char kEnablePnacl[];
extern const char kEnableProfiling[];
extern const char kEnableQuic[];
extern const char kEnableResourceContentSettings[];
#if defined(SBROWSER_SESSION_CACHE)
extern const char kEnableSBrowserSessionCache[];
#endif
extern const char kEnableSdch[];
extern const char kDisableSpdy31[];
extern const char kEnableSpdy4a1[];
extern const char kEnableSpdyCredentialFrames[];
extern const char kEnableSpellingAutoCorrect[];
extern const char kEnableStackedTabStrip[];
extern const char kEnableSuggestionsTabPage[];
extern const char kEnableSyncFavicons[];
extern const char kEnableSyncSyncedNotifications[];
extern const char kEnableTabGroupsContextMenu[];
extern const char kEnableTranslateAlphaLanguages[];
extern const char kEnableUnrestrictedSSL3Fallback[];
extern const char kEnableUserAlternateProtocolPorts[];
extern const char kEnableWatchdog[];
extern const char kEnableWebSocketOverSpdy[];
extern const char kExtensionsInActionBox[];
extern const char kExtensionsOnChromeURLs[];
extern const char kEventPageIdleTime[];
extern const char kEventPageSuspendingTime[];
extern const char kExplicitlyAllowedPorts[];
extern const char kExtensionProcess[];
extern const char kExtensionsUpdateFrequency[];
extern const char kFakeVariationsChannel[];
extern const char kFlagSwitchesBegin[];
extern const char kFlagSwitchesEnd[];
extern const char kFeedbackServer[];
extern const char kFileDescriptorLimit[];
extern const char kForceAppMode[];
extern const char kForceFirstRun[];
extern const char kForceLoadCloudPolicy[];
extern const char kGaiaProfileInfo[];
extern const char kGoogleBaseSuggestURL[];
extern const char kGoogleSearchDomainCheckURL[];
extern const char kGSSAPILibraryName[];
extern const char kHelp[];
extern const char kHelpShort[];
extern const char kHideIcons[];
extern const char kHistoryDisableFullHistorySync[];
extern const char kHistoryEnableGroupByDomain[];
extern const char kHistoryWebHistoryUrl[];
extern const char kHomePage[];
extern const char kHostRules[];
extern const char kHostResolverParallelism[];
extern const char kHostResolverRetryAttempts[];
extern const char kHstsHosts[];
extern const char kIgnoreUrlFetcherCertRequests[];
extern const char kImport[];
extern const char kImportFromFile[];
extern const char kIncognito[];
extern const char kInstallFromWebstore[];
extern const char kInstantProcess[];
extern const char kInstantURL[];
extern const char kKeepAliveForTest[];
extern const char kKioskMode[];
extern const char kKioskModePrinting[];
extern const char kLimitedInstallFromWebstore[];
extern const char kLoadAndLaunchApp[];
extern const char kLoadComponentExtension[];
extern const char kLoadExtension[];
extern const char kUninstallExtension[];
extern const char kLogNetLog[];
extern const char kMakeDefaultBrowser[];
extern const char kManaged[];
extern const char kMediaCacheSize[];
extern const char kMemoryProfiling[];
extern const char kMessageLoopHistogrammer[];
extern const char kMetricsRecordingOnly[];
extern const char kMultiProfiles[];
extern const char kNaClBrokerProcess[];
extern const char kNaClDebugMask[];
extern const char kNaClGdb[];
extern const char kNaClGdbScript[];
extern const char kNaClLoaderCmdPrefix[];
extern const char kNaClLoaderProcess[];
extern const char kNativeMessagingHosts[];
extern const char kNetLogLevel[];
extern const char kNewProfileManagement[];
extern const char kNoDefaultBrowserCheck[];
extern const char kNoDisplayingInsecureContent[];
extern const char kNoEvents[];
extern const char kNoExperiments[];
extern const char kNoFirstRun[];
extern const char kNoJsRandomness[];
extern const char kNoManaged[];
extern const char kNoNetworkProfileWarning[];
extern const char kNoProxyServer[];
extern const char kNoPings[];
extern const char kNoServiceAutorun[];
extern const char kNoStartupWindow[];
extern const char kNtpAppInstallHint[];
extern const char kNumPacThreads[];
extern const char kOmniboxHistoryQuickProviderReorderForInlining[];
extern const char kOmniboxHistoryQuickProviderReorderForInliningEnabled[];
extern const char kOmniboxHistoryQuickProviderReorderForInliningDisabled[];
extern const char kOmniboxInlineHistoryQuickProvider[];
extern const char kOmniboxInlineHistoryQuickProviderAllowed[];
extern const char kOmniboxInlineHistoryQuickProviderProhibited[];
extern const char kOnlyBlockSettingThirdPartyCookies[];
extern const char kOpenInNewWindow[];
extern const char kOrganicInstall[];
extern const char kOriginPortToForceQuicOn[];
extern const char kPackExtension[];
extern const char kPackExtensionKey[];
extern const char kParentProfile[];
extern const char kPerformanceMonitorGathering[];
extern const char kPlaybackMode[];
extern const char kPnaclDir[];
extern const char kPpapiFlashInProcess[];
extern const char kPpapiFlashPath[];
extern const char kPpapiFlashVersion[];
extern const char kPrerenderFromOmnibox[];
extern const char kPrerenderFromOmniboxSwitchValueAuto[];
extern const char kPrerenderFromOmniboxSwitchValueDisabled[];
extern const char kPrerenderFromOmniboxSwitchValueEnabled[];
extern const char kPrerenderMode[];
extern const char kPrerenderModeSwitchValueAuto[];
extern const char kPrerenderModeSwitchValueDisabled[];
extern const char kPrerenderModeSwitchValueEnabled[];
extern const char kPrerenderModeSwitchValuePrefetchOnly[];
extern const char kProductVersion[];
extern const char kProfileDirectory[];
extern const char kProfilingAtStart[];
extern const char kProfilingFile[];
extern const char kProfilingFlush[];
extern const char kProfilingOutputFile[];
extern const char kPromoServerURL[];
extern const char kPromptForExternalExtensions[];
extern const char kProxyAutoDetect[];
extern const char kProxyBypassList[];
extern const char kProxyPacUrl[];
extern const char kProxyServer[];
extern const char kPurgeMemoryButton[];
extern const char kRecordStats[];
extern const char kRecordMode[];
extern const char kRemoteDebuggingFrontend[];
extern const char kSilentDebuggerExtensionAPI[];
extern const char kRendererPrintPreview[];
extern const char kResetLocalPassphrase[];
extern const char kResetVariationState[];
extern const char kRestoreLastSession[];
extern const char kSavePageAsMHTML[];
extern const char kSbURLPrefix[];
extern const char kSbDisableAutoUpdate[];
extern const char kSbDisableDownloadProtection[];
extern const char kSbDisableExtensionBlacklist[];
extern const char kSbDisableSideEffectFreeWhitelist[];
extern const char kScriptBadges[];
extern const char kScriptBubble[];
extern const char kSearchInOmniboxHint[];
extern const char kServiceProcess[];
extern const char kSilentLaunch[];
extern const char kSetToken[];
extern const char kShowAppList[];
extern const char kShowComponentExtensionOptions[];
extern const char kShowIcons[];
extern const char kShowLauncherAlignmentMenu[];
extern const char kSigninProcess[];
extern const char kSilentDumpOnDCHECK[];
extern const char kSimulateUpgrade[];
extern const char kSimulateCriticalUpdate[];
extern const char kSimulateOutdated[];
extern const char kSpeculativeResourcePrefetching[];
extern const char kSpeculativeResourcePrefetchingDisabled[];
extern const char kSpeculativeResourcePrefetchingLearning[];
extern const char kSpdyProxyAuthOrigin[];
extern const char kSpeculativeResourcePrefetchingEnabled[];
extern const char kSSLVersionMax[];
extern const char kSSLVersionMin[];
extern const char kStartMaximized[];
extern const char kSuggestionNtpFilterWidth[];
extern const char kSuggestionNtpGaussianFilter[];
extern const char kSuggestionNtpLinearFilter[];
extern const char kSyncAllowInsecureXmppConnection[];
extern const char kSyncInvalidateXmppLogin[];
extern const char kSyncKeystoreEncryption[];
extern const char kSyncShortInitialRetryOverride[];
extern const char kSyncNotificationMethod[];
extern const char kSyncNotificationHostPort[];
extern const char kSyncServiceURL[];
extern const char kSyncTabFavicons[];
extern const char kSyncThrowUnrecoverableError[];
extern const char kSyncTrySsltcpFirstForXmpp[];
extern const char kSyncEnableDeferredStartup[];
extern const char kTabBrowserDragging[];
extern const char kTabCapture[];
extern const char kTestNaClSandbox[];
extern const char kTestName[];
extern const char kTestType[];
extern const char kTestingChannelID[];
extern const char kTrackActiveVisitTime[];
extern const char kTranslateScriptURL[];
extern const char kTrustedSpdyProxy[];
extern const char kTryChromeAgain[];
extern const char kUninstall[];
extern const char kUnlimitedStorage[];
extern const char kUseSimpleCacheBackend[];
extern const char kUseSpdy[];
extern const char kUseSpellingSuggestions[];
extern const char kMaxSpdySessionsPerDomain[];
extern const char kMaxSpdyConcurrentStreams[];
extern const char kUserDataDir[];
extern const char kUseClientLoginSigninFlow[];
extern const char kValidateCrx[];
extern const char kVariationsServerURL[];
extern const char kVersion[];
extern const char kViewerConnection[];
extern const char kVisitURLs[];
extern const char kWebIntentsNativeServicesEnabled[];
extern const char kWhitelistedExtensionID[];
extern const char kWindowPosition[];
extern const char kWindowSize[];
extern const char kWinHttpProxyResolver[];


#if defined(ENABLE_PLUGIN_INSTALLATION)
extern const char kPluginsMetadataServerURL[];
#endif

#if defined(OS_ANDROID) || defined(OS_IOS)
extern const char kEnableSpdyProxyAuth[];
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_ANDROID)
extern const char kEnableTranslate[];
extern const char kTabletUI[];
#endif

#if defined(USE_ASH)
extern const char kAshDisableTabScrubbing[];
extern const char kOpenAsh[];
#endif

#if defined(OS_CHROMEOS)
// Keep switches in alphabetical order.
// See also src/chromeos/chromeos_switches.h
extern const char kAshWebUIInit[];
extern const char kDisableAppMode[];
extern const char kDisableBootAnimation[];
extern const char kDisableChromeCaptivePortalDetector[];
extern const char kDisableDrive[];
extern const char kDisableLocalAccounts[];
extern const char kDisableLoginAnimations[];
extern const char kDisableOobeAnimation[];
extern const char kEnableBackgroundLoader[];
extern const char kEnableCarrierSwitching[];
extern const char kEnableChromeCaptivePortalDetector[];
extern const char kEnableTouchpadThreeFingerClick[];
extern const char kEnableTouchpadThreeFingerSwipe[];
extern const char kEnableKioskMode[];
extern const char kEnableRequestTabletSite[];
extern const char kEnableStaticIPConfig[];
extern const char kHasChromeOSKeyboard[];
extern const char kHasChromeOSDiamondKey[];
extern const char kKioskModeScreensaverPath[];
extern const char kNaturalScrollDefault[];
extern const char kNoDiscardTabs[];
extern const char kEchoExtensionPath[];
extern const char kStubCrosSettings[];
extern const char kAuthExtensionPath[];
extern const char kEnterpriseEnrollmentInitialModulus[];
extern const char kEnterpriseEnrollmentModulusLimit[];
extern const char kFileManagerLegacy[];
extern const char kFileManagerLegacyUI[];
#ifndef NDEBUG
extern const char kOobeSkipPostLogin[];
#endif
#endif

#if defined(OS_POSIX)
extern const char kEnableCrashReporter[];
extern const char kEnableCrashReporterForTesting[];
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
extern const char kPasswordStore[];
#endif
#endif

#if defined(OS_MACOSX)
extern const char kEnableExposeForTabs[];
extern const char kKeychainReauthorize[];
extern const char kRelauncherProcess[];
extern const char kUseMockKeychain[];
#endif

#if defined(OS_WIN)
extern const char kEnableAppList[];
extern const char kForceImmersive[];
extern const char kForceDesktop[];
extern const char kOverlappedRead[];
extern const char kPrintRaster[];
extern const char kRelaunchShortcut[];
extern const char kWaitForMutex[];
#endif

#ifndef NDEBUG
extern const char kFileManagerExtensionPath[];
extern const char kImageLoaderExtensionPath[];
extern const char kDumpProfileDependencyGraph[];
#endif

#if defined(GOOGLE_CHROME_BUILD)
extern const char kDisablePrintPreview[];
#else
extern const char kEnablePrintPreview[];
#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H_
