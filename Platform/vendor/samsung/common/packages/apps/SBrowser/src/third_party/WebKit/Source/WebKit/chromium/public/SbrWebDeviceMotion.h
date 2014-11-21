
#ifndef WebDeviceMotion_h
#define WebDeviceMotion_h
#include "../../../Platform/chromium/public/WebPrivatePtr.h"
#if WEBKIT_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
namespace WebCore { class DeviceMotionData; }
#endif


namespace WebKit {

class WebDeviceMotion{
public:
    class  Acceleration {
    public:
       
        bool canProvideX() const { return m_canProvideX; }
        bool canProvideY() const { return m_canProvideY; }
        bool canProvideZ() const { return m_canProvideZ; }

        double x() const { return m_x; }
        double y() const { return m_y; }
        double z() const { return m_z; }

    
        Acceleration(bool canProvideX, double x, bool canProvideY, double y, bool canProvideZ, double z){
		m_canProvideX = canProvideX;
		m_x = x;
		m_canProvideY = canProvideY;
		m_y = y;
		m_canProvideZ = canProvideZ;
		m_z = z;
		}
private:
        double m_x;
        double m_y;
        double m_z;

        bool m_canProvideX;
        bool m_canProvideY;
        bool m_canProvideZ;
    };

    class RotationRate {
    public:
 
        bool canProvideAlpha() const { return m_canProvideAlpha; }
        bool canProvideBeta() const { return m_canProvideBeta; }
        bool canProvideGamma() const { return m_canProvideGamma; }

        double alpha() const { return m_alpha; }
        double beta() const { return m_beta; }
        double gamma() const { return m_gamma; }

    
        RotationRate(bool canProvideAlpha, double alpha, bool canProvideBeta,  double beta, bool canProvideGamma, double gamma);
private:
        double m_alpha;
        double m_beta;
        double m_gamma;

        bool m_canProvideAlpha;
        bool m_canProvideBeta;
        bool m_canProvideGamma;
    };

    
    const Acceleration* acceleration() const { return m_acceleration; }
    const Acceleration* accelerationIncludingGravity() const { return m_accelerationIncludingGravity; }
    const RotationRate* rotationRate() const { return m_rotationRate; }
    double interval() const { return m_interval; }
    bool canProvideInterval() const { return m_canProvideInterval; }

    static WebDeviceMotion nullMotion() { return WebDeviceMotion(); }
    bool isNull() { return m_isNull; }
	 
    WebDeviceMotion(Acceleration *acceleration, Acceleration *accelerationIncludingGravity, RotationRate * rotationRate, bool canProvideInterval, double interval){
	m_isNull = false;
	m_acceleration = acceleration;
	m_accelerationIncludingGravity = accelerationIncludingGravity;
	m_rotationRate = rotationRate;
	m_canProvideInterval = canProvideInterval;
	m_interval = interval;
   	}

#if WEBKIT_IMPLEMENTATION
  //  WebDeviceMotion(const WTF::PassRefPtr<WebCore::DeviceMotionData>&);
  //  WebDeviceMotion& operator=(const WTF::PassRefPtr<WebCore::DeviceMotionData>&);
    operator WTF::PassRefPtr<WebCore::DeviceMotionData>() const;
#endif
private:
    WebDeviceMotion():
		m_isNull(true),
		m_acceleration(0),
		m_accelerationIncludingGravity(0),
		m_rotationRate(0),
		m_canProvideInterval(0),
		m_interval(0)
	{
	}

   
    bool m_isNull;
    Acceleration *m_acceleration;
    Acceleration *m_accelerationIncludingGravity;
    RotationRate *m_rotationRate;
    bool m_canProvideInterval;
    double m_interval;
    
};

} // namespace WebKit

#endif // WebDeviceMotion_h
