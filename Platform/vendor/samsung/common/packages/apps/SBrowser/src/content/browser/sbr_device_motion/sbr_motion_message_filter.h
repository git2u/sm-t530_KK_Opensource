
#ifndef CONTENT_BROWSER_DEVICE_MOTION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_DEVICE_MOTION_MESSAGE_FILTER_H_

#include <map>

#include "content/browser/sbr_device_motion/sbr_provider.h"
#include "content/public/browser/browser_message_filter.h"

namespace device_motion {

class MessageFilter : public content::BrowserMessageFilter {
 public:
  MessageFilter();

  // content::BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  virtual ~MessageFilter();

  void OnStartUpdating(int render_view_id);
  void OnStopUpdating(int render_view_id);

  // Helper class that observes a Provider and forwards updates to a RenderView.
  class ObserverDelegate;

  // map from render_view_id to ObserverDelegate.
  std::map<int, scoped_refptr<ObserverDelegate> > observers_map_;

  scoped_refptr<Provider> provider_;

  DISALLOW_COPY_AND_ASSIGN(MessageFilter);
};

}  // namespace device_motion

#endif  // CONTENT_BROWSER_DEVICE_MOTION_MESSAGE_FILTER_H_
