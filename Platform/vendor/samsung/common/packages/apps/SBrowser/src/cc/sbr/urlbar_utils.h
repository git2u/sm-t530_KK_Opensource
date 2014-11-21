
#ifndef CC_LAYERS_URLBAR_UTILS_H_
#define CC_LAYERS_URLBAR_UTILS_H_

#if defined(SBROWSER_HIDE_URLBAR)
#include <string>

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

enum CmdHideUrlBar {
  SET_URLBAR_BITMAP  = 1,
  SET_URLBAR_ACTIVE   = 2,
};

enum EventHideUrlBar {
  URLBAR_BITMAP_ACK = 1,
  URLBAR_ACTIVE_ACK  = 2,
  SCROLL_BEGIN_NOTIFY = 4,
  SCROLL_END_NOTIFY  = 8,
  CONTENT_OFFSET_CHANGE_NOTIFY = 16,
};

}  // namespace cc
#endif

#endif  // CC_LAYERS_URLBAR_UTILS_H_
