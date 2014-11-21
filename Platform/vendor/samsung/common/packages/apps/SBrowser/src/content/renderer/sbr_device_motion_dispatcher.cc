
#include "content/renderer/sbr_device_motion_dispatcher.h"

#include "content/common/device_motion_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/SbrWebDeviceMotion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/SbrWebDeviceMotionController.h"

DeviceMotionDispatcher::DeviceMotionDispatcher(
   content::RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view),
      controller_(NULL),
      started_(false) {
}

DeviceMotionDispatcher::~DeviceMotionDispatcher() {
  if (started_)
    stopUpdating();
}

bool DeviceMotionDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceMotionDispatcher, msg)
    IPC_MESSAGE_HANDLER(DeviceMotionMsg_Updated,
                        OnDeviceMotionUpdated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceMotionDispatcher::setController(
    WebKit::WebDeviceMotionController* controller) {
  controller_.reset(controller);
}

void DeviceMotionDispatcher::startUpdating() {
  Send(new DeviceMotionHostMsg_StartUpdating(routing_id()));
  started_ = true;
}

void DeviceMotionDispatcher::stopUpdating() {
  Send(new DeviceMotionHostMsg_StopUpdating(routing_id()));
  started_ = false;
}

WebKit::WebDeviceMotion DeviceMotionDispatcher::currentDeviceMotion()
    const {
  if (!last_motion_.get())
    return WebKit::WebDeviceMotion::nullMotion();
  return *last_motion_;
}

namespace {
bool DeviceMotionEqual(const DeviceMotionMsg_Updated_Params& a,
                       WebKit::WebDeviceMotion* motion) {
   const WebKit::WebDeviceMotion::Acceleration *b =motion->accelerationIncludingGravity();
  if (a.can_provide_x != b->canProvideX())
    return false;
  if (a.can_provide_x && a.x != b->x())
    return false;
  if (a.can_provide_y != b->canProvideY())
    return false;
  if (a.can_provide_y && a.y != b->y())
    return false;
  if (a.can_provide_z != b->canProvideZ())
    return false;
  if (a.can_provide_z && a.z != b->z())
    return false;

  return true;
}
}  // namespace

void DeviceMotionDispatcher::OnDeviceMotionUpdated(
    const DeviceMotionMsg_Updated_Params& p) {
  if (last_motion_.get() && DeviceMotionEqual(p, last_motion_.get()))
    return;
  last_motion_.reset(new WebKit::WebDeviceMotion(0,new WebKit::WebDeviceMotion::Acceleration(p.can_provide_x, p.x,  p.can_provide_y,  p.y,  p.can_provide_z, p.z),
                                                           0,true,p.interval ));

  controller_->didChangeDeviceMotion(*last_motion_);
}

