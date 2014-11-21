
#ifndef CONTENT_BROWSER_DEVICE_MOTION_MOTION_H_
#define CONTENT_BROWSER_DEVICE_MOTION_MOTION_H_

namespace device_motion {
class Devicemotion {
 public:


  Devicemotion(bool can_provide_x, double x,
              bool can_provide_y, double y,
              bool can_provide_z, double z,
              double interval)
      : x_(x),
        y_(y),
        z_(z),
        interval_(interval),
        can_provide_x_(can_provide_x),
        can_provide_y_(can_provide_y),
        can_provide_z_(can_provide_z) {
  }

  Devicemotion()
      : x_(0),
        y_(0),
        z_(0),
        interval_(0),
        can_provide_x_(false),
        can_provide_y_(false),
        can_provide_z_(false) {
  }

  static Devicemotion Empty() { return Devicemotion(); }

  bool IsEmpty() const {
    return !can_provide_x_ && !can_provide_y_ && !can_provide_z_;
  }

  double x_;
  double y_;
  double z_;
  double interval_;
  bool can_provide_x_;
  bool can_provide_y_;
  bool can_provide_z_;
};

}  // namespace device_motion

#endif  // CONTENT_BROWSER_DEVICE_MOTION_MOTION_H_
