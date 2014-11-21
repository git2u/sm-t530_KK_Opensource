// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_ANDROID_SBR_SBR_CHROME_MAIN_DELEGATE_ANDROID_H_
#define CHROME_APP_ANDROID_SBR_SBR_CHROME_MAIN_DELEGATE_ANDROID_H_

#include "chrome/app/android/chrome_main_delegate_android.h"

class SbrChromeMainDelegateAndroid : public ChromeMainDelegateAndroid {
 public:
  SbrChromeMainDelegateAndroid();
  virtual ~SbrChromeMainDelegateAndroid();

  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE;

  virtual bool RegisterApplicationNativeMethods(JNIEnv* env) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SbrChromeMainDelegateAndroid);
};

#endif  // CHROME_APP_ANDROID_SBR_SBR_CHROME_MAIN_DELEGATE_ANDROID_H_