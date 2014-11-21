
#ifndef WebDeviceMotionController_h
#define WebDeviceMotionController_h

#include "../../../Platform/chromium/public/WebCommon.h"

namespace WebCore { class DeviceMotionController; }

namespace WebKit {

class WebDeviceMotion;

class WebDeviceMotionController {
public:
    WebDeviceMotionController(WebCore::DeviceMotionController* c)
        : m_controller(c)
    {
    }

    WEBKIT_EXPORT void didChangeDeviceMotion(const WebDeviceMotion&);

#if WEBKIT_IMPLEMENTATION
    WebCore::DeviceMotionController* controller() const;
#endif

private:
    WebCore::DeviceMotionController* m_controller;
};

} // namespace WebKit

#endif // WebDeviceMotionController_h
