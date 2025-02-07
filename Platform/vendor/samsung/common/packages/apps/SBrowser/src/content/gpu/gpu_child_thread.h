// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_CHILD_THREAD_H_
#define CONTENT_GPU_GPU_CHILD_THREAD_H_

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "build/build_config.h"
#include "content/common/child_thread.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_config.h"
#include "content/common/gpu/x_util.h"
#include "content/public/common/gpu_info.h"
#include "ui/gfx/native_widget_types.h"

#if defined(SBROWSER_GRAPHICS_GETBITMAP)
#include "base/callback.h"
#include "content/common/android/sbr/surface_callback.h"
#endif


namespace sandbox {
class TargetServices;
}

namespace content {
class GpuWatchdogThread;

// The main thread of the GPU child process. There will only ever be one of
// these per process. It does process initialization and shutdown. It forwards
// IPC messages to GpuChannelManager, which is responsible for issuing rendering
// commands to the GPU.
class GpuChildThread : public ChildThread {
 public:
  explicit GpuChildThread(GpuWatchdogThread* gpu_watchdog_thread,
                          bool dead_on_arrival,
                          const GPUInfo& gpu_info);

  // For single-process mode.
  explicit GpuChildThread(const std::string& channel_id);

  virtual ~GpuChildThread();

  virtual void Shutdown() OVERRIDE;

  void Init(const base::Time& process_start_time);
  void StopWatchdog();

  // ChildThread overrides.
  virtual bool Send(IPC::Message* msg) OVERRIDE;
  virtual bool OnControlMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  // Message handlers.
  void OnInitialize();
  void OnCollectGraphicsInfo();
  void OnGetVideoMemoryUsageStats();
  void OnSetVideoMemoryWindowCount(uint32 window_count);

  void OnClean();
  void OnCrash();
  void OnHang();
  void OnDisableWatchdog();

#if defined(USE_TCMALLOC)
  void OnGetGpuTcmalloc();
#endif
#if defined(SBROWSER_GRAPHICS_GETBITMAP)
  // this can be called from any thread
 void SetNativeGetBitmap(BitmapParams imageParams, int* ret,
			int32 view_or_surface_id,
                     int32 renderer_id,
			void** buffer,
			base::WaitableEvent* completion);
#endif


  // Set this flag to true if a fatal error occurred before we receive the
  // OnInitialize message, in which case we just declare ourselves DOA.
  bool dead_on_arrival_;
  base::Time process_start_time_;
  scoped_refptr<GpuWatchdogThread> watchdog_thread_;

#if defined(OS_WIN)
  // Windows specific client sandbox interface.
  sandbox::TargetServices* target_services_;
#endif

  scoped_ptr<GpuChannelManager> gpu_channel_manager_;

  // Information about the GPU, such as device and vendor ID.
  GPUInfo gpu_info_;

  // Whether the GPU thread is running in the browser process.
  bool in_browser_process_;
#if defined(SBROWSER_GRAPHICS_GETBITMAP)
  base::WeakPtrFactory<GpuChildThread> weak_ptr_factory_;
#endif
  DISALLOW_COPY_AND_ASSIGN(GpuChildThread);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_CHILD_THREAD_H_
