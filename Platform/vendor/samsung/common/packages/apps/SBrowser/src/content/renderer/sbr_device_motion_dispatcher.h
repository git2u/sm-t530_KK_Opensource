
#ifndef CONTENT_RENDERER_DEVICE_MOTION_DISPATCHER_H_
#define CONTENT_RENDERER_DEVICE_MOTION_DISPATCHER_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/SbrWebDeviceMotionClient.h"

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"

class RenderViewImpl;

namespace WebKit {
class WebDeviceMotion;
}

struct DeviceMotionMsg_Updated_Params;

class DeviceMotionDispatcher : public content::RenderViewObserver,
                                    public WebKit::WebDeviceMotionClient {
 public:
  explicit DeviceMotionDispatcher(content::RenderViewImpl* render_view);
  virtual ~DeviceMotionDispatcher();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // From WebKit::WebDeviceMotionClient.
  virtual void setController(
      WebKit::WebDeviceMotionController* controller);
  virtual void startUpdating();
  virtual void stopUpdating();
  virtual WebKit::WebDeviceMotion currentDeviceMotion() const;

  void OnDeviceMotionUpdated(
      const DeviceMotionMsg_Updated_Params& p);

  scoped_ptr<WebKit::WebDeviceMotionController> controller_;
  scoped_ptr<WebKit::WebDeviceMotion> last_motion_;
  bool started_;
};

#endif  // CONTENT_RENDERER_DEVICE_MOTION_DISPATCHER_H_
