
#include <cmath>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/sbr_device_motion/sbr_motion.h"
#include "content/browser/sbr_device_motion/sbr_provider_impl.h"

namespace device_motion {

ProviderImpl::ProviderImpl(const DataFetcherFactory factories[])
    : creator_loop_(MessageLoop::current()),
      weak_factory_(this) {
  for (const DataFetcherFactory* fp = factories; *fp; ++fp)
    factories_.push_back(*fp);
}

ProviderImpl::~ProviderImpl() {
}

void ProviderImpl::AddObserver(Observer* observer) {
  DCHECK(MessageLoop::current() == creator_loop_);

  observers_.insert(observer);
  if (observers_.size() == 1)
    Start();
  else
    observer->OnMotionUpdate(last_notification_);
}

void ProviderImpl::RemoveObserver(Observer* observer) {
  DCHECK(MessageLoop::current() == creator_loop_);

  observers_.erase(observer);
  if (observers_.empty())
    Stop();
}

void ProviderImpl::Start() {
  DCHECK(MessageLoop::current() == creator_loop_);
  DCHECK(!polling_thread_.get());
  polling_thread_.reset(new base::Thread("Device motion polling thread"));
  if (!polling_thread_->Start()) {
    LOG(ERROR) << "Failed to start device motion polling thread";
    polling_thread_.reset();
    return;
  }
  ScheduleInitializePollingThread();
}

void ProviderImpl::Stop() {
  DCHECK(MessageLoop::current() == creator_loop_);
  // TODO(hans): Don't join the thread. See crbug.com/72286.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  polling_thread_.reset();
  data_fetcher_.reset();
}

void ProviderImpl::DoInitializePollingThread(
    const std::vector<DataFetcherFactory>& factories) {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());

  typedef std::vector<DataFetcherFactory>::const_iterator Iterator;
  for (Iterator i = factories_.begin(), e = factories_.end(); i != e; ++i) {
    DataFetcherFactory factory = *i;
    scoped_ptr<DataFetcher> fetcher(factory());
    Devicemotion motion;

    if (fetcher.get() && fetcher->GetDeviceMotion(&motion)) {
      // Pass ownership of fetcher to provider_.
      data_fetcher_.swap(fetcher);
      last_motion_= motion;

#if defined(OS_ANDROID)
      // TODO(husky): Android's SensorManager has a push rather than a pull API,
      // so we won't have any data yet. Make this neater with an upstream tweak.
#else
      // Notify observers.
      ScheduleDoNotify(motion);
#endif
      // Start polling.
      if(polling_thread_.get())
          ScheduleDoPoll();
      return;
    }
  }

  // When no  data can be provided.
  ScheduleDoNotify(Devicemotion::Empty());
}

void ProviderImpl::ScheduleInitializePollingThread() {
  DCHECK(MessageLoop::current() == creator_loop_);

  MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostTask(FROM_HERE,
                         base::Bind(&ProviderImpl::DoInitializePollingThread,
                                    this,
                                    factories_));
}

void ProviderImpl::DoNotify(const Devicemotion& motion) {
  DCHECK(MessageLoop::current() == creator_loop_);

  last_notification_ = motion;

  typedef std::set<Observer*>::const_iterator Iterator;
  for (Iterator i = observers_.begin(), e = observers_.end(); i != e; ++i)
    (*i)->OnMotionUpdate(motion);

  if (motion.IsEmpty()) {
    // Notify observers about failure to provide data exactly once.
    observers_.clear();
    Stop();
  }
}

void ProviderImpl::ScheduleDoNotify(const Devicemotion& motion) {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());

  creator_loop_->PostTask(
      FROM_HERE, base::Bind(&ProviderImpl::DoNotify, this, motion));
}

void ProviderImpl::DoPoll() {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());

  Devicemotion motion;
  if (!data_fetcher_->GetDeviceMotion(&motion)) {
    LOG(ERROR) << "Failed to poll device motion data fetcher.";

    ScheduleDoNotify(Devicemotion::Empty());
    return;
  }

  if (SignificantlyDifferent(motion, last_motion_)) {
    last_motion_ = motion;
#if defined(OS_ANDROID)
    // TODO(husky): Android's SensorManager has a push rather than a pull API.
    // Allow the DataFetcher to return Empty() if the data is still pending.
    // I am going to clean up DataFetcher's contract with an upstream change.
    if (!motion.IsEmpty()) {
      ScheduleDoNotify(motion);
    }
#else
    ScheduleDoNotify(motion);
#endif
  }

  if(polling_thread_.get())
  	ScheduleDoPoll();
}

void ProviderImpl::ScheduleDoPoll() {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());

  MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ProviderImpl::DoPoll, weak_factory_.GetWeakPtr()),
      SamplingInterval());
}

namespace {

bool IsElementSignificantlyDifferent(bool can_provide_element1,
                                     bool can_provide_element2,
                                     double element1,
                                     double element2) {
  const double kThreshold = 0.1;

  if (can_provide_element1 != can_provide_element2)
    return true;
  if (can_provide_element1 &&
      std::fabs(element1 - element2) >= kThreshold)
    return true;
  return false;
}
}  // namespace

// Returns true if two values are considered different enough that
// observers should be notified of the new value.
bool ProviderImpl::SignificantlyDifferent(const Devicemotion& m1,
                                          const Devicemotion& m2) {
  return IsElementSignificantlyDifferent(m1.can_provide_x_,
                                        m2.can_provide_x_,
                                         m1.x_,
                                         m2.x_) ||
      IsElementSignificantlyDifferent(m1.can_provide_y_,
                                        m2.can_provide_y_,
                                         m1.y_,
                                         m2.y_) ||
      IsElementSignificantlyDifferent(m1.can_provide_z_,
                                        m2.can_provide_z_,
                                         m1.z_,
                                         m2.z_);
}

base::TimeDelta ProviderImpl::SamplingInterval() const {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());
  DCHECK(data_fetcher_.get());

  // TODO(erg): There used to be unused code here, that called a default
  // implementation on the DataFetcherInterface that was never defined. I'm
  // removing unused methods from headers.
  return base::TimeDelta::FromMilliseconds(kDesiredSamplingIntervalMs);
}

}  // namespace device_motion
