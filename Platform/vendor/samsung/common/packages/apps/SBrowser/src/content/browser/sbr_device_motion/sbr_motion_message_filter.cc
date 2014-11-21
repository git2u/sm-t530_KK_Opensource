
#include "content/browser/sbr_device_motion/sbr_motion_message_filter.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/sbr_device_motion/sbr_motion.h"
#include "content/browser/sbr_device_motion/sbr_provider.h"
#include "content/public/browser/render_view_host.h"
#include "content/common/device_motion_messages.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace device_motion {

MessageFilter::MessageFilter() : provider_(NULL) {
}

MessageFilter::~MessageFilter() {
}

class MessageFilter::ObserverDelegate
    : public base::RefCounted<ObserverDelegate>, public Provider::Observer {
 public:
    // Create ObserverDelegate that observes provider and forwards updates to
    // render_view_id in process_id.
    // Will stop observing provider when destructed.
    ObserverDelegate(Provider* provider,
                     int render_view_id,
                     IPC::Sender* sender);

    // From Provider::Observer.
    virtual void OnMotionUpdate(const Devicemotion& motion);

 private:
  friend class base::RefCounted<ObserverDelegate>;
  virtual ~ObserverDelegate();

  scoped_refptr<Provider> provider_;
  int render_view_id_;
  IPC::Sender* sender_;  // Weak pointer.

  DISALLOW_COPY_AND_ASSIGN(ObserverDelegate);
};

MessageFilter::ObserverDelegate::ObserverDelegate(Provider* provider,
                                                  int render_view_id,
                                                  IPC::Sender* sender)
    : provider_(provider),
      render_view_id_(render_view_id),
      sender_(sender) {
  provider_->AddObserver(this);
}

MessageFilter::ObserverDelegate::~ObserverDelegate() {
  provider_->RemoveObserver(this);
}

void MessageFilter::ObserverDelegate::OnMotionUpdate(
    const Devicemotion& motion) {
  DeviceMotionMsg_Updated_Params params;
  params.can_provide_x = motion.can_provide_x_;
  params.x = motion.x_;
  params.can_provide_y = motion.can_provide_y_;
  params.y = motion.y_;
  params.can_provide_z = motion.can_provide_z_;
  params.z = motion.z_;
  params.interval = motion.interval_;
  sender_->Send(new DeviceMotionMsg_Updated(render_view_id_, params));
}

bool MessageFilter::OnMessageReceived(const IPC::Message& message,
                                      bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(DeviceMotionHostMsg_StartUpdating, OnStartUpdating)
    IPC_MESSAGE_HANDLER(DeviceMotionHostMsg_StopUpdating, OnStopUpdating)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MessageFilter::OnStartUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!provider_)
    provider_ = Provider::GetInstance();
  observers_map_[render_view_id] = new ObserverDelegate(provider_,
                                                        render_view_id,
                                                        this);
}

void MessageFilter::OnStopUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_map_.erase(render_view_id);
}

}  // namespace device_motion
