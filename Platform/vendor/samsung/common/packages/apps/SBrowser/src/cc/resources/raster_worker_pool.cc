// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include "cc/resources/picture_pile_impl.h"

#include "base/debug/trace_event.h"

namespace cc {

namespace {

class RasterWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  RasterWorkerPoolTaskImpl(PicturePileImpl* picture_pile,
                           const RasterWorkerPool::RasterCallback& task,
                           const base::Closure& reply)
      : internal::WorkerPoolTask(reply),
        picture_pile_(picture_pile),
        task_(task) {
    DCHECK(picture_pile_);
  }

  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
#if defined(SBROWSER_INCREASE_RASTER_THREAD) 	
    if(thread_index > 0){ //Ensure that pic pile has num of clones = 4	
    	if (picture_pile_->getNumofClones() == 4){
    	    task_.Run(picture_pile_->GetCloneForDrawingOnThread(thread_index));
    	}
    } else {		
         task_.Run(picture_pile_->GetCloneForDrawingOnThread(thread_index));	
    }	
#else
    task_.Run(picture_pile_->GetCloneForDrawingOnThread(thread_index));	
#endif //defined(SBROWSER_INCREASE_RASTER_THREAD) 	
  }

 private:
  scoped_refptr<PicturePileImpl> picture_pile_;
  RasterWorkerPool::RasterCallback task_;
};

const char* kWorkerThreadNamePrefix = "CompositorRaster";

const int kCheckForCompletedTasksDelayMs = 6;

}  // namespace

RasterWorkerPool::RasterWorkerPool(
    WorkerPoolClient* client, size_t num_threads) : WorkerPool(
        client,
        num_threads,
        base::TimeDelta::FromMilliseconds(kCheckForCompletedTasksDelayMs),
        kWorkerThreadNamePrefix) {
}

RasterWorkerPool::~RasterWorkerPool() {
}

void RasterWorkerPool::PostRasterTaskAndReply(PicturePileImpl* picture_pile,
                                              const RasterCallback& task,
                                              const base::Closure& reply) {
  PostTask(make_scoped_ptr(new RasterWorkerPoolTaskImpl(
                               picture_pile,
                               task,
                               reply)).PassAs<internal::WorkerPoolTask>());
}

#if defined(SBROWSER_ADJUST_WORKER_THREAD_PRIORITY)
void RasterWorkerPool::SetThreadHighPriority(bool flag) { 
    TRACE_EVENT1("cc","RasterWorkerPool::SetThreadHighPriority", "flag", (flag ? "true" : "false"));
    
    const int normal_thread_priority = 10;
    const int high_thread_priority = -6;
    
    /*
    *   Notice : CC has 114 and Main has 120 priority. 
    *   Raster has 130 as normal. but we change this to 114 with this flag.
    */
    if(flag) 
        SetThreadPriority(high_thread_priority); 
    else 
        SetThreadPriority(normal_thread_priority); 
}

void RasterWorkerPool::SetThreadPriority(int nice_value) {
    const std::string thread_prefix_name = kWorkerThreadNamePrefix;
    WorkerPool::SetThreadPriority(thread_prefix_name, nice_value);
}
#endif

}  // namespace cc
