
#include "config.h"
#include "SbrWebDeviceMotionController.h"

#include "modules/device_orientation/DeviceMotionData.h"
#include "modules/device_orientation/DeviceMotionController.h"
#include "SbrWebDeviceMotion.h"
#include <wtf/PassRefPtr.h>


namespace WebKit {

void WebDeviceMotionController::didChangeDeviceMotion(const WebDeviceMotion& webMotion)
{
    PassRefPtr<WebCore::DeviceMotionData> deviceMotionData(webMotion);
    m_controller->didChangeDeviceMotion(deviceMotionData.get());
}

WebCore::DeviceMotionController* WebDeviceMotionController::controller() const
{
    return m_controller;
}

} // namespace WebKit
