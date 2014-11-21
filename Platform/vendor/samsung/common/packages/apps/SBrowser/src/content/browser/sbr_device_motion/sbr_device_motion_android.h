
#ifndef CONTENT_BROWSER_DEVICE_MOTION_DEVICE_MOTION_ANDROID_H_
#define CONTENT_BROWSER_DEVICE_MOTION_DEVICE_MOTION_ANDROID_H_
#pragma once

#include <jni.h>

#include "base/synchronization/lock.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/sbr_device_motion/sbr_data_fetcher.h"
#include "content/browser/sbr_device_motion/sbr_motion.h"

namespace device_motion{

/**
 * Android implementation of DeviceMotion API.
 *
 * Android's SensorManager has a push API, whereas Chrome wants to pull data.
 * To fit them together, we store incoming sensor events in a 1-element buffer.
 * SensorManager calls SetDeviceMotion() which pushes a new value (discarding the
 * previous value if any). Chrome calls GetDeviceMotion() which reads the most
 * recent value. Repeated calls to GetDeviceMotion() will return the same value.
 *
 * TODO(husky): Hoist the thread out of ProviderImpl and have the Java code own
 * it instead. That way we can have SensorManager post directly to our thread
 * rather than going via the UI thread.
 */
class DeviceMotionAndroid : public DataFetcher {
 public:
  // Must be called at startup, before Create().
  static void Init(JNIEnv* env);

  // Factory function. We'll listen for events for the lifetime of this object.
  // Returns NULL on error.
  static DataFetcher* Create();

  virtual ~DeviceMotionAndroid();

  // Implementation of DataFetcher.
  virtual bool GetDeviceMotion(Devicemotion* motionVal) OVERRIDE;

  void GotDeviceMotion(JNIEnv*, jobject,
                       bool canProvideX, double x, bool canProvideY, double y, bool canProvideZ, double z, double interval);

 private:
  DeviceMotionAndroid();

  // Wrappers for JNI methods.
  bool Start(int rateInMilliseconds);
  void Stop();

  // Value returned by GetOrientation.
  scoped_ptr<Devicemotion> current_motion_;

  // 1-element buffer, written by SetDeviceMotion, read by GetDeviceMotion.
  base::Lock next_motion_lock_;
  scoped_ptr<Devicemotion> next_devicemotion_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionAndroid);
};

} // namespace device_motion

#endif  // CONTENT_BROWSER_DEVICE_MOTION_DEVICE_MOTION_ANDROID_H_
