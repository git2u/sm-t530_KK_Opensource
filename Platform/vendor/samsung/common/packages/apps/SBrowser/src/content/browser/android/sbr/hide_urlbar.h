
#ifndef CONTENT_BROWSER_ANDROID_SBR_HIDE_URLBAR_H_
#define CONTENT_BROWSER_ANDROID_SBR_HIDE_URLBAR_H_

#if defined(SBROWSER_HIDE_URLBAR)
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class HideUrlBarCmd {
 public:
    HideUrlBarCmd();
    ~HideUrlBarCmd();
    int cmd;
    bool urlbar_active;
    bool override_allowed;
    SkBitmap urlbar_bitmap;
};

}  // namespace content
#endif

#endif  // CONTENT_BROWSER_ANDROID_SBR_HIDE_URLBAR_H_
