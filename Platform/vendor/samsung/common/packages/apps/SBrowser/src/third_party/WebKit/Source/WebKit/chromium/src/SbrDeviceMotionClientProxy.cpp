
#include "config.h"

#include "SbrDeviceMotionClientProxy.h"

#include "modules/device_orientation/DeviceMotionData.h"
#include "SbrWebDeviceMotion.h"
#include "SbrWebDeviceMotionController.h"

namespace WebCore {
class DeviceMotionController;
}


namespace WebKit {

void DeviceMotionClientProxy::deviceMotionControllerDestroyed()
{
    //delete this;
}

void DeviceMotionClientProxy::setController(WebCore::DeviceMotionController* controller)
{
     if (!m_client) 
        return;
     m_client->setController(new WebDeviceMotionController(controller));
}

void DeviceMotionClientProxy::startUpdating()
{
    if (!m_client)
        return;
    m_client->startUpdating();
}

void DeviceMotionClientProxy::stopUpdating()
{
    if (!m_client)
        return;																																	
    m_client->stopUpdating();
}

WebCore::DeviceMotionData* DeviceMotionClientProxy::lastMotion() const
{
	if (!m_client)
	    return 0;
   	m_Current =  m_client->currentDeviceMotion();    
    	return m_Current.get(); 
}

} // namespece WebKit


