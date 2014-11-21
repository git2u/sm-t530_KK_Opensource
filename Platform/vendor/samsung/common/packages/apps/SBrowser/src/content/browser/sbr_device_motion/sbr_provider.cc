
#include "content/browser/sbr_device_motion/sbr_provider.h"

#include "base/logging.h"
#include "content/browser/sbr_device_motion/sbr_data_fetcher.h"
#include "content/browser/sbr_device_motion/sbr_provider_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/browser/sbr_device_motion/sbr_device_motion_android.h"

using content::BrowserThread;

namespace device_motion {

Provider* Provider::GetInstance() {
  if (!instance_) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const ProviderImpl::DataFetcherFactory default_factories[] = {
#if defined(OS_ANDROID)
      DeviceMotionAndroid::Create,
#endif
      NULL
    };

    instance_ = new ProviderImpl(default_factories);
  }
  return instance_;
}

void Provider::SetInstanceForTests(Provider* provider) {
  DCHECK(!instance_);
  instance_ = provider;
}

Provider* Provider::GetInstanceForTests() {
  return instance_;
}

Provider::Provider() {
}

Provider::~Provider() {
  DCHECK(instance_ == this);
  instance_ = NULL;
}

Provider* Provider::instance_ = NULL;

} //  namespace device_motion
