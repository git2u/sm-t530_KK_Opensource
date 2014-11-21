
#ifndef WebDeviceMotionClient_h
#define WebDeviceMotionClient_h

namespace WebKit {

class WebDeviceMotion;
class WebDeviceMotionController;

class WebDeviceMotionClient {
public:
    virtual ~WebDeviceMotionClient() {}

    virtual void setController(WebDeviceMotionController*) = 0;
    virtual void startUpdating() = 0;
    virtual void stopUpdating() = 0;

    virtual WebDeviceMotion currentDeviceMotion() const = 0;
};

} // namespace WebKit

#endif // WebDeviceMotionClient_h
