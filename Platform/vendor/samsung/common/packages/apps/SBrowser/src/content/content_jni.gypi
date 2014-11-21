# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # TODO(jrg): Update this action and other jni generators to only
  # require specifying the java directory and generate the rest.
  # TODO(jrg): when doing the above, make sure we support multiple
  # output directories (e.g. browser/jni and common/jni if needed).
  'conditions': [
  ['enable_sbrowser_apk==1', {
  'sources': [
    '../../SBrowser/src/org/chromium/content/app/ChildProcessService.java',
    '../../SBrowser/src/org/chromium/content/app/ContentMain.java',
    '../../SBrowser/src/org/chromium/content/app/LibraryLoader.java',
    '../../SBrowser/src/org/chromium/content/browser/AndroidBrowserProcess.java',
    '../../SBrowser/src/org/chromium/content/browser/ChildProcessLauncher.java',
    '../../SBrowser/src/org/chromium/content/browser/ContentSettings.java',
    '../../SBrowser/src/org/chromium/content/browser/ContentVideoView.java',
    '../../SBrowser/src/org/chromium/content/browser/ContentViewCore.java',
    '../../SBrowser/src/org/chromium/content/browser/ContentViewRenderView.java',
    '../../SBrowser/src/org/chromium/content/browser/ContentViewStatics.java',
    '../../SBrowser/src/org/chromium/content/browser/DeviceMotionAndOrientation.java',
    '../../SBrowser/src/org/chromium/content/browser/DownloadController.java',
    '../../SBrowser/src/org/chromium/content/browser/input/ImeAdapter.java',
    '../../SBrowser/src/org/chromium/content/browser/input/DateTimeChooserAndroid.java',
    '../../SBrowser/src/org/chromium/content/browser/InterstitialPageDelegateAndroid.java',
    '../../SBrowser/src/org/chromium/content/browser/LoadUrlParams.java',
    '../../SBrowser/src/org/chromium/content/browser/LocationProvider.java',
    '../../SBrowser/src/org/chromium/content/browser/MediaResourceGetter.java',
    '../../SBrowser/src/org/chromium/content/browser/TouchPoint.java',
    '../../SBrowser/src/org/chromium/content/browser/TracingIntentHandler.java',
    '../../SBrowser/src/org/chromium/content/browser/WebContentsObserverAndroid.java',
    '../../SBrowser/src/org/chromium/content/common/CommandLine.java',
    '../../SBrowser/src/org/chromium/content/common/DeviceTelephonyInfo.java',
    '../../SBrowser/src/org/chromium/content/common/TraceEvent.java',
   ],
   },
   {
   'sources': [
    'public/android/java/src/org/chromium/content/app/ChildProcessService.java',
    'public/android/java/src/org/chromium/content/app/ContentMain.java',
    'public/android/java/src/org/chromium/content/app/LibraryLoader.java',
    'public/android/java/src/org/chromium/content/browser/AndroidBrowserProcess.java',
    'public/android/java/src/org/chromium/content/browser/ChildProcessLauncher.java',
    'public/android/java/src/org/chromium/content/browser/ContentSettings.java',
    'public/android/java/src/org/chromium/content/browser/ContentVideoView.java',
    'public/android/java/src/org/chromium/content/browser/ContentViewCore.java',
    'public/android/java/src/org/chromium/content/browser/ContentViewRenderView.java',
    'public/android/java/src/org/chromium/content/browser/ContentViewStatics.java',
    'public/android/java/src/org/chromium/content/browser/DeviceMotionAndOrientation.java',
    'public/android/java/src/org/chromium/content/browser/DownloadController.java',
    'public/android/java/src/org/chromium/content/browser/input/ImeAdapter.java',
    'public/android/java/src/org/chromium/content/browser/input/DateTimeChooserAndroid.java',
    'public/android/java/src/org/chromium/content/browser/InterstitialPageDelegateAndroid.java',
    'public/android/java/src/org/chromium/content/browser/LoadUrlParams.java',
    'public/android/java/src/org/chromium/content/browser/LocationProvider.java',
    'public/android/java/src/org/chromium/content/browser/MediaResourceGetter.java',
    'public/android/java/src/org/chromium/content/browser/TouchPoint.java',
    'public/android/java/src/org/chromium/content/browser/TracingIntentHandler.java',
    'public/android/java/src/org/chromium/content/browser/WebContentsObserverAndroid.java',
    'public/android/java/src/org/chromium/content/common/CommandLine.java',
    'public/android/java/src/org/chromium/content/common/DeviceTelephonyInfo.java',
    'public/android/java/src/org/chromium/content/common/TraceEvent.java',
    ],
   }],
   ],
  'variables': {
    'jni_gen_package': 'content'
  },
  'includes': [ '../build/jni_generator.gypi' ],
}
