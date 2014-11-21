// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_RASTER_WORKER_POOL_H_

#include <string>

#include "cc/base/worker_pool.h"

namespace cc {
class PicturePileImpl;

// A worker thread pool that runs raster tasks.
class RasterWorkerPool : public WorkerPool {
 public:
  typedef base::Callback<void(PicturePileImpl* picture_pile)> RasterCallback;

  virtual ~RasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      WorkerPoolClient* client, size_t num_threads) {
    return make_scoped_ptr(new RasterWorkerPool(client, num_threads));
  }

  void PostRasterTaskAndReply(PicturePileImpl* picture_pile,
                              const RasterCallback& task,
                              const base::Closure& reply);                             
  
#if defined(SBROWSER_ADJUST_WORKER_THREAD_PRIORITY)
  void SetThreadHighPriority(bool flag);
#endif

 private:
  RasterWorkerPool(WorkerPoolClient* client, size_t num_threads);
  
#if defined(SBROWSER_ADJUST_WORKER_THREAD_PRIORITY)
  virtual void SetThreadPriority(int nice_value);
#endif

  DISALLOW_COPY_AND_ASSIGN(RasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_WORKER_POOL_H_
