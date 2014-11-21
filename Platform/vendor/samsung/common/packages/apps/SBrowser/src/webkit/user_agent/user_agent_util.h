// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_USER_AGENT_USER_AGENT_UTIL_H_
#define WEBKIT_USER_AGENT_USER_AGENT_UTIL_H_

#if defined(SBROWSER_UA_STRING)
#include <algorithm>
#endif
#include <string>

#include "base/basictypes.h"
#if defined(SBROWSER_UA_STRING)
#include "chrome/browser/browser_process.h"
#include "ui/base/l10n/l10n_util_android.h"
#endif
#include "webkit/user_agent/webkit_user_agent_export.h"

namespace webkit_glue {

// Builds a User-agent compatible string that describes the OS and CPU type.
WEBKIT_USER_AGENT_EXPORT std::string BuildOSCpuInfo();

// Returns the WebKit version, in the form "major.minor (branch@revision)".
WEBKIT_USER_AGENT_EXPORT std::string GetWebKitVersion();

// The following 2 functions return the major and minor webkit versions.
WEBKIT_USER_AGENT_EXPORT int GetWebKitMajorVersion();
WEBKIT_USER_AGENT_EXPORT int GetWebKitMinorVersion();
 
 /**
    * Convert obsolete language codes, including Hebrew/Indonesian/Yiddish,
    * to new standard.
    */
#if defined(SBROWSER_UA_STRING)
WEBKIT_USER_AGENT_EXPORT std::string ConvertObsoleteLanguageCodeToNew(const std::string& locale);
WEBKIT_USER_AGENT_EXPORT std::string GetApplicationLocale();
WEBKIT_USER_AGENT_EXPORT std::string GetApplicationVersion(const std::string& os_info);
#endif
// Helper function to generate a full user agent string from a short
// product name.
WEBKIT_USER_AGENT_EXPORT std::string BuildUserAgentFromProduct(
    const std::string& product);

// Builds a full user agent string given a string describing the OS and a
// product name.
WEBKIT_USER_AGENT_EXPORT std::string BuildUserAgentFromOSAndProduct(
    const std::string& os_info,
    const std::string& product);

}  // namespace webkit_glue

#endif  // WEBKIT_USER_AGENT_USER_AGENT_UTIL_H_
