// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_request_scheduler.h"

namespace chrome_variations {

VariationsRequestScheduler::VariationsRequestScheduler(
    const base::Closure& task) : task_(task) {
}

VariationsRequestScheduler::~VariationsRequestScheduler() {
}

void VariationsRequestScheduler::Start() {
  // Time between regular seed fetches, in hours.
  const int kFetchPeriodHours = 5;
  task_.Run();
  timer_.Start(FROM_HERE, base::TimeDelta::FromHours(kFetchPeriodHours), task_);
}

void VariationsRequestScheduler::Reset() {
  if (timer_.IsRunning())
    timer_.Reset();
  one_shot_timer_.Stop();
}

void VariationsRequestScheduler::ScheduleFetchShortly() {
  // Reset the regular timer to avoid it triggering soon after.
  Reset();
  // The delay before attempting a fetch shortly, in minutes.
  const int kFetchShortlyDelayMinutes = 5;
  one_shot_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMinutes(kFetchShortlyDelayMinutes),
                        task_);
}

base::Closure VariationsRequestScheduler::task() const {
  return task_;
}

#if !defined(OS_ANDROID)
// static
VariationsRequestScheduler* VariationsRequestScheduler::Create(
    const base::Closure& task,
    PrefService* local_state) {
  return new VariationsRequestScheduler(task);
}
#endif  // !defined(OS_ANDROID)

}  // namespace chrome_variations
