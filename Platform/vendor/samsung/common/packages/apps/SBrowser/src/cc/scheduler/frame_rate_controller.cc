// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/frame_rate_controller.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cc/base/thread.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/time_source.h"

namespace cc {

#if defined(SBROWSER_QC_VSYNC_PATCH)
// Before setTimebaseAndInterval() is called, we assume the max FPS is 60
const double DEFAULT_FPS = 60.0;
// When using vsync, we switch to manual tick if the FPS drops below
// 57 (60 * 0.95).
const double DEFAULT_FPS_MANUAL_TICK_LIMIT = 0.95;
// When not using vsync, we switch to vsync if the FPS goes above
// 58 (60 * 0.966)
const double DEFAULT_FPS_TIMESOURCE_THROTTLING_LIMIT = 0.966;

class FPSAverage
{
 public:
	 // Use a long running average of 120 samples to ensure the FPS does not
	 // vary widely across frames
	 static const int NUM_VALUES = 120;
	 FPSAverage()
	 : index_(0),
	 num_set_(0),
	 sum_(0),
	 last_frame_time_set_(false) {
	 deltas_ = new float[NUM_VALUES];
 }

 ~FPSAverage() {
	 if (deltas_)
		 delete[] deltas_;
 }

 void markFrame(base::TimeTicks time) {
	 if (!last_frame_time_set_) {
		 float delta = (time - last_frame_time_).InSecondsF();
		 add(delta);
	 } else
		 last_frame_time_set_ = true;
	 last_frame_time_ = time;
 }

 float getAverageFPS() {
 if (num_set_ < NUM_VALUES)
	 return DEFAULT_FPS;
 CHECK(getAverage() != 0.0);
	 return 1.0 / getAverage();
 }
 private:
	 inline void nextIndex(int& counter) {
	 counter++;
	 if (counter >= NUM_VALUES)
 		counter = 0;
 }

 void add(float delta) {
 sum_ += delta;

 if (num_set_ >= NUM_VALUES)
 sum_ -= deltas_[index_];

 deltas_[index_] = delta;
 nextIndex(index_);
 if (num_set_ < NUM_VALUES)
	 num_set_++;
 }

 float getAverage() {
	 DCHECK(num_set_ != 0);
	 return sum_ / num_set_;
 }

 float* deltas_;
 int index_;
 int num_set_;
 float sum_;
 base::TimeTicks last_frame_time_;
 bool last_frame_time_set_;
};
#endif

class FrameRateControllerTimeSourceAdapter : public TimeSourceClient {
 public:
  static scoped_ptr<FrameRateControllerTimeSourceAdapter> Create(
      FrameRateController* frame_rate_controller) {
    return make_scoped_ptr(
        new FrameRateControllerTimeSourceAdapter(frame_rate_controller));
  }
  virtual ~FrameRateControllerTimeSourceAdapter() {}

  virtual void OnTimerTick() OVERRIDE { frame_rate_controller_->OnTimerTick(); }

 private:
  explicit FrameRateControllerTimeSourceAdapter(
      FrameRateController* frame_rate_controller)
      : frame_rate_controller_(frame_rate_controller) {}

  FrameRateController* frame_rate_controller_;
};

#if defined(SBROWSER_QC_VSYNC_PATCH)
FrameRateController::FrameRateController(scoped_refptr<TimeSource> timer, Thread* thread)
#else
FrameRateController::FrameRateController(scoped_refptr<TimeSource> timer)
#endif
    : client_(NULL),
      num_frames_pending_(0),
      max_frames_pending_(0),
      time_source_(timer),
      active_(false),
      swap_buffers_complete_supported_(true),
      is_time_source_throttling_(true),
      weak_factory_(this),
#if defined(SBROWSER_QC_VSYNC_PATCH)
      force_manual_tick_(false),
      max_fps_(DEFAULT_FPS),
      fps_average_(new FPSAverage()),
      thread_(thread) {
#else
      thread_(NULL) {
#endif
  time_source_client_adapter_ =
      FrameRateControllerTimeSourceAdapter::Create(this);
  time_source_->SetClient(time_source_client_adapter_.get());
}

FrameRateController::FrameRateController(Thread* thread)
    : client_(NULL),
      num_frames_pending_(0),
      max_frames_pending_(0),
      active_(false),
      swap_buffers_complete_supported_(true),
      is_time_source_throttling_(false),
      weak_factory_(this),
#if defined(SBROWSER_QC_VSYNC_PATCH)
      force_manual_tick_(false),
      max_fps_(DEFAULT_FPS),
      fps_average_(new FPSAverage()),
#endif     
      thread_(thread) {}

FrameRateController::~FrameRateController() {
  if (is_time_source_throttling_)
    time_source_->SetActive(false);
}

void FrameRateController::SetActive(bool active) {
  if (active_ == active)
    return;
  TRACE_EVENT1("cc", "FrameRateController::SetActive", "active", active);
  active_ = active;

#if defined(SBROWSER_QC_VSYNC_PATCH)
  if (is_time_source_throttling_ && !force_manual_tick_) {
#else
  if (is_time_source_throttling_) {
#endif      
    time_source_->SetActive(active);
  } else {
    if (active)
      PostManualTick();
    else
      weak_factory_.InvalidateWeakPtrs();
  }
}

#if defined(SBROWSER_QC_VSYNC_PATCH)
void FrameRateController::chooseManualOrTimeSourceThrottling()
{
    if (!is_time_source_throttling_)
        return;

    double fps = fps_average_->getAverageFPS();
    bool set;
    if (!force_manual_tick_)
        set = fps < (max_fps_ * DEFAULT_FPS_MANUAL_TICK_LIMIT);
    else
        set = fps < (max_fps_ * DEFAULT_FPS_TIMESOURCE_THROTTLING_LIMIT);

    if (set == force_manual_tick_)
        return;

    if (active_) {
        if (!set) {
            //switch to timer
            time_source_->SetActive(true);
            weak_factory_.InvalidateWeakPtrs();
        } else {
            //switch to manual
            time_source_->SetActive(false);
        }
    }
    force_manual_tick_ = set;
}
#endif

void FrameRateController::SetMaxFramesPending(int max_frames_pending) {
  DCHECK_GE(max_frames_pending, 0);
  max_frames_pending_ = max_frames_pending;
}

void FrameRateController::SetTimebaseAndInterval(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  if (is_time_source_throttling_)
    time_source_->SetTimebaseAndInterval(timebase, interval);

#if defined(SBROWSER_QC_VSYNC_PATCH)
  double delta = interval.InSecondsF();
  if (delta > 0.0)
  max_fps_ = 1.0 / delta;
  else
  max_fps_ = DEFAULT_FPS;  
#endif  
}

void FrameRateController::SetSwapBuffersCompleteSupported(bool supported) {
  swap_buffers_complete_supported_ = supported;
}

void FrameRateController::OnTimerTick() {
  DCHECK(active_);

  // Check if we have too many frames in flight.
  bool throttled =
      max_frames_pending_ && num_frames_pending_ >= max_frames_pending_;
  TRACE_COUNTER_ID1("cc", "ThrottledVSyncInterval", thread_, throttled);

  if (client_)
    client_->VSyncTick(throttled);

#if defined(SBROWSER_QC_VSYNC_PATCH)
  // only check for throttling if a thread is available for posting manual ticks
  if (thread_)
    chooseManualOrTimeSourceThrottling();

  if (swap_buffers_complete_supported_ &&
      (!is_time_source_throttling_ || force_manual_tick_) && !throttled)
#else
  if (swap_buffers_complete_supported_ && !is_time_source_throttling_ &&
      !throttled)
#endif      
    PostManualTick();
}

void FrameRateController::PostManualTick() {
#if defined(SBROWSER_QC_VSYNC_PATCH)
  if (active_ && thread_) {
#else   
  if (active_) {
#endif
    thread_->PostTask(base::Bind(&FrameRateController::ManualTick,
                                 weak_factory_.GetWeakPtr()));
  }
}

void FrameRateController::ManualTick() { OnTimerTick(); }

void FrameRateController::DidBeginFrame() {
#if defined(SBROWSER_QC_VSYNC_PATCH)
  fps_average_->markFrame(base::TimeTicks::Now());
#endif  
  if (swap_buffers_complete_supported_)
    num_frames_pending_++;
#if defined(SBROWSER_QC_VSYNC_PATCH)   
  else if (!is_time_source_throttling_ || force_manual_tick_)
#else 
  else if (!is_time_source_throttling_)
#endif
    PostManualTick();
}

void FrameRateController::DidFinishFrame() {
  DCHECK(swap_buffers_complete_supported_);

  num_frames_pending_--;
#if defined(SBROWSER_QC_VSYNC_PATCH)   
  if (!is_time_source_throttling_ || force_manual_tick_)
#else 
  if (!is_time_source_throttling_)
#endif
    PostManualTick();
}

void FrameRateController::DidAbortAllPendingFrames() {
  num_frames_pending_ = 0;
}

base::TimeTicks FrameRateController::NextTickTime() {
#if defined(SBROWSER_QC_VSYNC_PATCH)   
  if (is_time_source_throttling_ && !force_manual_tick_)
#else 
  if (is_time_source_throttling_)
#endif
   return time_source_->NextTickTime();

  return base::TimeTicks();
}

base::TimeTicks FrameRateController::LastTickTime() {
#if defined(SBROWSER_QC_VSYNC_PATCH)   
  if (is_time_source_throttling_ && !force_manual_tick_)
#else 
  if (is_time_source_throttling_)
#endif
   return time_source_->LastTickTime();

  return base::TimeTicks::Now();
}

}  // namespace cc
