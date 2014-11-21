
#include "content/browser/sbr_device_motion/sbr_device_motion_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "content/browser/sbr_device_motion/sbr_motion.h"
#include "jni/SbrDeviceMotion_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::GetClass;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace device_motion {

namespace {

// This should match ProviderImpl::kDesiredSamplingIntervalMs.
// TODO(husky): Make that constant public so we can use it directly.
const int kPeriodInMilliseconds = 100;

base::LazyInstance<ScopedJavaGlobalRef<jobject> >
     g_jni_obj = LAZY_INSTANCE_INITIALIZER;

}  // namespace

DeviceMotionAndroid::DeviceMotionAndroid() {
}

void DeviceMotionAndroid::Init(JNIEnv* env) { 
 bool result = RegisterNativesImpl(env);
  DCHECK(result);
  g_jni_obj.Get().Reset(Java_SbrDeviceMotion_create(env));
}

DataFetcher* DeviceMotionAndroid::Create() {
  scoped_ptr<DeviceMotionAndroid> fetcher(new DeviceMotionAndroid);
  if (fetcher->Start(kPeriodInMilliseconds))
    return fetcher.release();

  return NULL;
}

DeviceMotionAndroid::~DeviceMotionAndroid() {
  Stop();
}

bool DeviceMotionAndroid:: GetDeviceMotion(Devicemotion* motionVal)  {
  // Do we have a new motion value? (It's safe to do this outside the lock
  // because we only skip the lock if the value is null. We always enter the
  // lock if we're going to make use of the new value.)
  if (next_devicemotion_.get()) {
    base::AutoLock autolock(next_motion_lock_);
    next_devicemotion_.swap(current_motion_);
  }
  if (current_motion_.get()) {
    *motionVal = *current_motion_;
  }
  return true;
}

void DeviceMotionAndroid::GotDeviceMotion(JNIEnv*, jobject,
                       bool canProvideX, double x, bool canProvideY, double y, bool canProvideZ, double z, double interval) {
  base::AutoLock autolock(next_motion_lock_);
  next_devicemotion_.reset(
      new Devicemotion(canProvideX,  x,  canProvideY,  y,  canProvideZ,  z,  interval));
}

bool DeviceMotionAndroid::Start(int rateInMilliseconds) {
  DCHECK(!g_jni_obj.Get().is_null());
  return Java_SbrDeviceMotion_start(AttachCurrentThread(),
                                      g_jni_obj.Get().obj(),
                                      reinterpret_cast<jint>(this),
                                      rateInMilliseconds);
}

void DeviceMotionAndroid::Stop() {
  DCHECK(!g_jni_obj.Get().is_null());
  Java_SbrDeviceMotion_stop(AttachCurrentThread(), g_jni_obj.Get().obj());
}

}  // namespace device_motion
