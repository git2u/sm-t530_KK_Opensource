
#include "config.h"
#include "SbrWebDeviceMotion.h"

#include "modules/device_orientation/DeviceMotionData.h"
#include <wtf/PassRefPtr.h>

namespace WebKit {
/*
WebDeviceMotion::WebDeviceMotion(const PassRefPtr<WebCore::DeviceMotionData>& motion)
{
 
    if (!motion) {
        m_isNull = true;
	m_acceleration = 0;
    	m_accelerationIncludingGravity =0;
   	m_rotationRate = 0;
   	m_canProvideInterval = 0;
    	m_interval = 0;
     
        return;
    }

    m_isNull = false;
    m_acceleration = motion->m_acceleration;
    m_accelerationIncludingGravity = motion->m_accelerationIncludingGravity;
    m_rotationRate = motion->m_rotationRate;
    m_canProvideInterval =motion->m_canProvideInterval;
    m_interval = motion->m_interval;

  m_accelerationIncludingGravity->canProvideX() = motion->m_accelerationIncludingGravity->canProvideX();
   m_accelerationIncludingGravity->canProvideY() = motion->m_accelerationIncludingGravity->canProvideY();
   m_accelerationIncludingGravity->canProvideZ() = motion->m_accelerationIncludingGravity->canProvideZ();
    m_accelerationIncludingGravity = new WebDeviceMotion::Acceleration(motion->m_accelerationIncludingGravity->canProvideX(), motion->m_accelerationIncludingGravity->x(), motion->m_accelerationIncludingGravity->canProvideY(), motion->m_accelerationIncludingGravity->y(),motion->m_accelerationIncludingGravity->canProvideZ(), motion->m_accelerationIncludingGravity->z());

}

WebDeviceMotion& WebDeviceMotion::operator=(const PassRefPtr<WebCore::DeviceMotionData>& motion)
{

    if (!motion) {
        m_isNull = true;
	m_acceleration = 0;
    	m_accelerationIncludingGravity =0;
   	m_rotationRate = 0;
   	m_canProvideInterval = 0;
    	m_interval = 0;
        return *this;
    }

    m_isNull = false;
    m_isNull = false;
    m_acceleration = motion->m_acceleration;
    m_accelerationIncludingGravity = motion->m_accelerationIncludingGravity;
    m_rotationRate = motion->m_rotationRate;
    m_canProvideInterval =motion->m_canProvideInterval;
    m_interval = motion->m_interval;

   m_accelerationIncludingGravity->canProvideX() = motion->m_accelerationIncludingGravity->canProvideX();
   m_accelerationIncludingGravity->canProvideY() = motion->m_accelerationIncludingGravity->canProvideY();
   m_accelerationIncludingGravity->canProvideZ() = motion->m_accelerationIncludingGravity->canProvideZ();
    return *this;
    
}
*/
WebDeviceMotion::operator PassRefPtr<WebCore::DeviceMotionData>() const
{
    if (m_isNull)
        return 0;
	
    RefPtr<WebCore::DeviceMotionData::Acceleration> accelerationIncludingGravity = WebCore::DeviceMotionData::Acceleration::create(m_accelerationIncludingGravity->canProvideX(), m_accelerationIncludingGravity->x(), m_accelerationIncludingGravity->canProvideY(), m_accelerationIncludingGravity->y(), m_accelerationIncludingGravity->canProvideZ(), m_accelerationIncludingGravity->z());
    bool provideInterval = m_accelerationIncludingGravity->canProvideX()|| m_accelerationIncludingGravity->canProvideY()|| m_accelerationIncludingGravity->canProvideZ();
    return WebCore::DeviceMotionData::create(0, accelerationIncludingGravity.get(), 0, provideInterval, m_interval);
    
}

} // namespace WebKit
