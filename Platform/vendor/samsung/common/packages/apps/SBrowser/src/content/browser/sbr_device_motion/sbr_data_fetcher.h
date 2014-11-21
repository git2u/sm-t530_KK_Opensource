
#ifndef CONTENT_BROWSER_DEVICE_MOTION_DATA_FETCHER_H_
#define CONTENT_BROWSER_DEVICE_MOTION_DATA_FETCHER_H_

namespace device_motion {

class Devicemotion;

class DataFetcher {
 public:
  virtual ~DataFetcher() {}
  virtual bool GetDeviceMotion(Devicemotion*) = 0;
};

}  // namespace device_motion

#endif  // CONTENT_BROWSER_DEVICE_MOTION_DATA_FETCHER_H_
