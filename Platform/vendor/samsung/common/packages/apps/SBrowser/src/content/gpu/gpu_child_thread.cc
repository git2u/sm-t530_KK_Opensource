// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_child_thread.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/gpu/gpu_info_collector.h"
#include "content/gpu/gpu_watchdog_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ui/gl/gl_implementation.h"

#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
#include <sys/resource.h>
#endif
#if defined(SBROWSER_GRAPHICS_GETBITMAP)
#include "android/native_window.h"
#include "base/android/jni_android.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "base/logging.h"

enum GpuImageFormat {
    FormatAlpha8,
    FormatRGB565,
    FormatARGB4444,
    FormatARGB8888
};
#endif


namespace content {
namespace {

bool GpuProcessLogMessageHandler(int severity,
                                 const char* file, int line,
                                 size_t message_start,
                                 const std::string& str) {
  std::string header = str.substr(0, message_start);
  std::string message = str.substr(message_start);

  // If we are not on main thread in child process, send through
  // the sync_message_filter; otherwise send directly.
  if (base::MessageLoop::current() !=
      ChildProcess::current()->main_thread()->message_loop()) {
    ChildProcess::current()->main_thread()->sync_message_filter()->Send(
        new GpuHostMsg_OnLogMessage(severity, header, message));
  } else {
    ChildThread::current()->Send(new GpuHostMsg_OnLogMessage(severity, header,
        message));
  }

  return false;
}

}  // namespace

GpuChildThread::GpuChildThread(GpuWatchdogThread* watchdog_thread,
                               bool dead_on_arrival,
                               const GPUInfo& gpu_info)
    : dead_on_arrival_(dead_on_arrival),
      gpu_info_(gpu_info),
      in_browser_process_(false)
#if defined(SBROWSER_GRAPHICS_GETBITMAP)
     , weak_ptr_factory_(this)
#endif
{
  watchdog_thread_ = watchdog_thread;
#if defined(OS_WIN)
  target_services_ = NULL;
#endif
}

GpuChildThread::GpuChildThread(const std::string& channel_id)
    : ChildThread(channel_id),
      dead_on_arrival_(false),
      in_browser_process_(true)
#if defined(SBROWSER_GRAPHICS_GETBITMAP)
      , weak_ptr_factory_(this)
#endif 
{
#if defined(OS_WIN)
  target_services_ = NULL;
#endif
  DCHECK(
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kInProcessGPU));
  // For single process and in-process GPU mode, we need to load and
  // initialize the GL implementation and locate the GL entry points here.
  if (!gfx::GLSurface::InitializeOneOff()) {
    VLOG(1) << "gfx::GLSurface::InitializeOneOff()";
  }
}

GpuChildThread::~GpuChildThread() {
}

void GpuChildThread::Shutdown() {
  logging::SetLogMessageHandler(NULL);
}

void GpuChildThread::Init(const base::Time& process_start_time) {
  process_start_time_ = process_start_time;
}

bool GpuChildThread::Send(IPC::Message* msg) {
  // The GPU process must never send a synchronous IPC message to the browser
  // process. This could result in deadlock.
  DCHECK(!msg->is_sync());

  return ChildThread::Send(msg);
}

bool GpuChildThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool msg_is_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuChildThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(GpuMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(GpuMsg_CollectGraphicsInfo, OnCollectGraphicsInfo)
    IPC_MESSAGE_HANDLER(GpuMsg_GetVideoMemoryUsageStats,
                        OnGetVideoMemoryUsageStats)
    IPC_MESSAGE_HANDLER(GpuMsg_Clean, OnClean)
    IPC_MESSAGE_HANDLER(GpuMsg_Crash, OnCrash)
    IPC_MESSAGE_HANDLER(GpuMsg_Hang, OnHang)
    IPC_MESSAGE_HANDLER(GpuMsg_DisableWatchdog, OnDisableWatchdog)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  if (handled)
    return true;

  return gpu_channel_manager_.get() &&
      gpu_channel_manager_->OnMessageReceived(msg);
}

void GpuChildThread::OnInitialize() {
  Send(new GpuHostMsg_Initialized(!dead_on_arrival_));

  if (dead_on_arrival_) {
    VLOG(1) << "Exiting GPU process due to errors during initialization";
    base::MessageLoop::current()->Quit();
    return;
  }

#if defined(OS_ANDROID)
  // TODO(epenner): Move thread priorities to base. (crbug.com/170549)
  int nice_value = -6; // High priority
  setpriority(PRIO_PROCESS, base::PlatformThread::CurrentId(), nice_value);
#endif

  // We don't need to pipe log messages if we are running the GPU thread in
  // the browser process.
  if (!in_browser_process_)
    logging::SetLogMessageHandler(GpuProcessLogMessageHandler);

  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  gpu_info_.initialization_time = base::Time::Now() - process_start_time_;

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_.reset(new GpuChannelManager(
      this,
      watchdog_thread_,
      ChildProcess::current()->io_message_loop_proxy(),
      ChildProcess::current()->GetShutDownEvent()));
  
#if defined(SBROWSER_GRAPHICS_GETBITMAP)
  NativeGetBitmapCallback gb = base::Bind(&GpuChildThread::SetNativeGetBitmap,
                                       weak_ptr_factory_.GetWeakPtr());
  RegisterNativeGetBitmapCallback(base::MessageLoopProxy::current(), gb);
#endif

  // Ensure the browser process receives the GPU info before a reply to any
  // subsequent IPC it might send.
  if (!in_browser_process_)
    Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));
}

void GpuChildThread::StopWatchdog() {
  if (watchdog_thread_) {
    watchdog_thread_->Stop();
  }
}

void GpuChildThread::OnCollectGraphicsInfo() {
#if defined(OS_WIN)
  // GPU full info collection should only happen on un-sandboxed GPU process
  // or single process/in-process gpu mode on Windows.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  DCHECK(command_line->HasSwitch(switches::kDisableGpuSandbox) ||
         in_browser_process_);
#endif  // OS_WIN

  if (!gpu_info_collector::CollectContextGraphicsInfo(&gpu_info_))
    VLOG(1) << "gpu_info_collector::CollectGraphicsInfo failed";
  GetContentClient()->SetGpuInfo(gpu_info_);

#if defined(OS_WIN)
  // This is slow, but it's the only thing the unsandboxed GPU process does,
  // and GpuDataManager prevents us from sending multiple collecting requests,
  // so it's OK to be blocking.
  gpu_info_collector::GetDxDiagnostics(&gpu_info_.dx_diagnostics);
  gpu_info_.finalized = true;
#endif  // OS_WIN

  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));

#if defined(OS_WIN)
  if (!in_browser_process_) {
    // The unsandboxed GPU process fulfilled its duty.  Rest in peace.
    base::MessageLoop::current()->Quit();
  }
#endif  // OS_WIN
}

void GpuChildThread::OnGetVideoMemoryUsageStats() {
  GPUVideoMemoryUsageStats video_memory_usage_stats;
  if (gpu_channel_manager_)
    gpu_channel_manager_->gpu_memory_manager()->GetVideoMemoryUsageStats(
        &video_memory_usage_stats);
  Send(new GpuHostMsg_VideoMemoryUsageStats(video_memory_usage_stats));
}

void GpuChildThread::OnClean() {
  VLOG(1) << "GPU: Removing all contexts";
  if (gpu_channel_manager_)
    gpu_channel_manager_->LoseAllContexts();
}

void GpuChildThread::OnCrash() {
  VLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuChildThread::OnHang() {
  VLOG(1) << "GPU: Simulating GPU hang";
  for (;;) {
    // Do not sleep here. The GPU watchdog timer tracks the amount of user
    // time this thread is using and it doesn't use much while calling Sleep.
  }
}

void GpuChildThread::OnDisableWatchdog() {
  VLOG(1) << "GPU: Disabling watchdog thread";
  if (watchdog_thread_) {
    // Disarm the watchdog before shutting down the message loop. This prevents
    // the future posting of tasks to the message loop.
    if (watchdog_thread_->message_loop())
      watchdog_thread_->PostAcknowledge();
    // Prevent rearming.
    watchdog_thread_->Stop();
  }
}

#if defined(SBROWSER_GRAPHICS_GETBITMAP)
void GpuChildThread::SetNativeGetBitmap(BitmapParams imageParams, int* ret,
                                    int32 view_or_surface_id,
                                     int32 renderer_id,
				     void** buffer,
				     base::WaitableEvent* completion)
{
// Must be called on the gpu thread.
  DCHECK(message_loop() == MessageLoop::current());
  DCHECK(gpu_channel_manager_.get());
  VLOG(0)<<"GetBitmap"<<__FUNCTION__;
  *ret = gpu_channel_manager_->GetNativeBitmap(imageParams, view_or_surface_id, renderer_id,buffer);
  if (completion)
    completion->Signal();
  VLOG(0)<<"GetBitmap"<<__FUNCTION__<<"Signal Release";
}
#endif

}  // namespace content

