
#ifndef CONTENT_BROWSER_DEVICE_MOTION_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_DEVICE_MOTION_PROVIDER_IMPL_H_

#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "content/browser/sbr_device_motion/sbr_data_fetcher.h"
#include "content/browser/sbr_device_motion/sbr_motion.h"
#include "content/browser/sbr_device_motion/sbr_provider.h"
#include "content/common/content_export.h"

class MessageLoop;

namespace base {
class Thread;
}

namespace device_motion {

class ProviderImpl : public Provider {
 public:
  typedef DataFetcher* (*DataFetcherFactory)();

  // Create a ProviderImpl that uses the NULL-terminated factories array to find
  // a DataFetcher that can provide motion data.
  CONTENT_EXPORT ProviderImpl(const DataFetcherFactory factories[]);

  // From Provider.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;

 private:
  virtual ~ProviderImpl();

  // Starts or Stops the provider. Called from creator_loop_.
  void Start();
  void Stop();

  // Method for finding a suitable DataFetcher and starting the polling.
  // Runs on the polling_thread_.
  void DoInitializePollingThread(
      const std::vector<DataFetcherFactory>& factories);
  void ScheduleInitializePollingThread();

  // Method for polling a DataFetcher. Runs on the polling_thread_.
  void DoPoll();
  void ScheduleDoPoll();

  // Method for notifying observers of an motion update.
  // Runs on the creator_thread_.
  void DoNotify(const Devicemotion& motion);
  void ScheduleDoNotify(const Devicemotion& motion);

  static bool SignificantlyDifferent(const Devicemotion& motion1,
                                     const Devicemotion& motion2);

  enum { kDesiredSamplingIntervalMs = 100 };
  base::TimeDelta SamplingInterval() const;

  // The Message Loop on which this object was created.
  // Typically the I/O loop, but may be something else during testing.
  MessageLoop* creator_loop_;

  // Members below are only to be used from the creator_loop_.
  std::vector<DataFetcherFactory> factories_;
  std::set<Observer*> observers_;
  Devicemotion last_notification_;

  // When polling_thread_ is running, members below are only to be used
  // from that thread.
  scoped_ptr<DataFetcher> data_fetcher_;
  Devicemotion last_motion_;
  base::WeakPtrFactory<ProviderImpl> weak_factory_;

  // Polling is done on this background thread.
  scoped_ptr<base::Thread> polling_thread_;
};

}  // namespace device_motion

#endif  // CONTENT_BROWSER_DEVICE_MOTION_PROVIDER_IMPL_H_
