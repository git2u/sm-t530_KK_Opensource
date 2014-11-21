
#ifndef DeviceMotionClientProxy_h
#define DeviceMotionClientProxy_h

#include "modules/device_orientation/DeviceMotionClient.h"
#include "modules/device_orientation/DeviceMotionData.h"

#include "SbrWebDeviceMotionClient.h"



namespace WebCore {
class DeviceMotionController;
}

namespace WebKit {

class DeviceMotionClientProxy : public WebCore::DeviceMotionClient {
public:
    DeviceMotionClientProxy(WebDeviceMotionClient* client)
        : m_client(client)
    {
    }
   
     void setController(WebCore::DeviceMotionController*);
     void startUpdating();
     void stopUpdating();
     WebCore::DeviceMotionData* lastMotion() const;  
     virtual void deviceMotionControllerDestroyed();

private:
    WebDeviceMotionClient* m_client;
    mutable RefPtr<WebCore::DeviceMotionData> m_Current;
};

} // namespece WebKit

#endif // DeviceMotionClientProxy_h
