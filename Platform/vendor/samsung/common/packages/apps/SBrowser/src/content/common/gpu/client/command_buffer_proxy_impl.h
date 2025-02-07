// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_

#include <map>
#include <queue>
#include <string>

#include "gpu/ipc/command_buffer_proxy.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "cc/debug/latency_info.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "ipc/ipc_listener.h"
#include "media/video/video_decode_accelerator.h"

struct GPUCommandBufferConsoleMessage;

namespace base {
class SharedMemory;
}

namespace gpu {
struct Mailbox;
}

namespace content {
class GpuChannelHost;

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class CommandBufferProxyImpl
    : public CommandBufferProxy,
      public IPC::Listener,
      public base::SupportsWeakPtr<CommandBufferProxyImpl> {
 public:
  class DeletionObserver {
   public:
    // Called during the destruction of the CommandBufferProxyImpl.
    virtual void OnWillDeleteImpl() = 0;

   protected:
    virtual ~DeletionObserver() {}
  };

  typedef base::Callback<void(
      const std::string& msg, int id)> GpuConsoleMessageCallback;

  CommandBufferProxyImpl(GpuChannelHost* channel, int route_id);
  virtual ~CommandBufferProxyImpl();

  // Sends an IPC message to create a GpuVideoDecodeAccelerator. Creates and
  // returns it as an owned pointer to a media::VideoDecodeAccelerator.  Returns
  // NULL on failure to create the GpuVideoDecodeAcceleratorHost.
  // Note that the GpuVideoDecodeAccelerator may still fail to be created in
  // the GPU process, even if this returns non-NULL. In this case the client is
  // notified of an error later.
  scoped_ptr<media::VideoDecodeAccelerator> CreateVideoDecoder(
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client);

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // CommandBufferProxy implementation:
  virtual int GetRouteID() const OVERRIDE;
  virtual bool Echo(const base::Closure& callback) OVERRIDE;
  virtual bool SetParent(CommandBufferProxy* parent_command_buffer,
                         uint32 parent_texture_id) OVERRIDE;
  virtual void SetChannelErrorCallback(const base::Closure& callback) OVERRIDE;

  // CommandBuffer implementation:
  virtual bool Initialize() OVERRIDE;
  virtual State GetState() OVERRIDE;
  virtual State GetLastState() OVERRIDE;
  virtual int32 GetLastToken() OVERRIDE;
  virtual void Flush(int32 put_offset) OVERRIDE;
  virtual State FlushSync(int32 put_offset, int32 last_known_get) OVERRIDE;
  virtual void SetGetBuffer(int32 shm_id) OVERRIDE;
  virtual void SetGetOffset(int32 get_offset) OVERRIDE;
  virtual gpu::Buffer CreateTransferBuffer(size_t size,
                                           int32* id) OVERRIDE;
  virtual void DestroyTransferBuffer(int32 id) OVERRIDE;
  virtual gpu::Buffer GetTransferBuffer(int32 id) OVERRIDE;
  virtual void SetToken(int32 token) OVERRIDE;
  virtual void SetParseError(gpu::error::Error error) OVERRIDE;
  virtual void SetContextLostReason(
      gpu::error::ContextLostReason reason) OVERRIDE;
  virtual uint32 InsertSyncPoint() OVERRIDE;
#if defined(SBROWSER_NATIVE_TEXTURE_UPLOAD)
  virtual void SendFileDescriptors(int32 texid, int32 fd_count, int32* fds) OVERRIDE;
#endif
  void SetMemoryAllocationChangedCallback(
      const base::Callback<void(const GpuMemoryAllocationForRenderer&)>&
          callback);

  void AddDeletionObserver(DeletionObserver* observer);
  void RemoveDeletionObserver(DeletionObserver* observer);

  bool DiscardBackbuffer();
  bool EnsureBackbuffer();

  // Makes this command buffer invoke a task when a sync point is reached, or
  // the command buffer that inserted that sync point is destroyed.
  bool SignalSyncPoint(uint32 sync_point,
                       const base::Closure& callback);

  // Generates n unique mailbox names that can be used with
  // GL_texture_mailbox_CHROMIUM. Unlike genMailboxCHROMIUM, this IPC is
  // handled only on the GPU process' IO thread, and so is not effectively
  // a finish.
  bool GenerateMailboxNames(unsigned num, std::vector<gpu::Mailbox>* names);

  // Sends an IPC message with the new state of surface visibility.
  bool SetSurfaceVisible(bool visible);

  void SetOnConsoleMessageCallback(
      const GpuConsoleMessageCallback& callback);

  void SetLatencyInfo(const cc::LatencyInfo& latency_info);

  // TODO(apatrick): this is a temporary optimization while skia is calling
  // ContentGLContext::MakeCurrent prior to every GL call. It saves returning 6
  // ints redundantly when only the error is needed for the
  // CommandBufferProxyImpl implementation.
  virtual gpu::error::Error GetLastError() OVERRIDE;

  void SendManagedMemoryStats(const GpuManagedMemoryStats& stats);

  GpuChannelHost* channel() const { return channel_; }

 private:
  typedef std::map<int32, gpu::Buffer> TransferBufferMap;
  typedef base::hash_map<uint32, base::Closure> SignalTaskMap;

  // Send an IPC message over the GPU channel. This is private to fully
  // encapsulate the channel; all callers of this function must explicitly
  // verify that the context has not been lost.
  bool Send(IPC::Message* msg);

  // Message handlers:
  void OnUpdateState(const gpu::CommandBuffer::State& state);
  void OnDestroyed(gpu::error::ContextLostReason reason);
  void OnEchoAck();
  void OnConsoleMessage(const GPUCommandBufferConsoleMessage& message);
  void OnSetMemoryAllocation(const GpuMemoryAllocationForRenderer& allocation);
  void OnSignalSyncPointAck(uint32 id);
  void OnGenerateMailboxNamesReply(const std::vector<std::string>& names);

  // Try to read an updated copy of the state from shared memory.
  void TryUpdateState();

  // The shared memory area used to update state.
  gpu::CommandBufferSharedState* shared_state() const {
    return reinterpret_cast<gpu::CommandBufferSharedState*>(
        shared_state_shm_->memory());
  }

  // Local cache of id to transfer buffer mapping.
  TransferBufferMap transfer_buffers_;

  // Unowned list of DeletionObservers.
  ObserverList<DeletionObserver> deletion_observers_;

  // The last cached state received from the service.
  State last_state_;

  // The shared memory area used to update state.
  scoped_ptr<base::SharedMemory> shared_state_shm_;

  // |*this| is owned by |*channel_| and so is always outlived by it, so using a
  // raw pointer is ok.
  GpuChannelHost* channel_;
  int route_id_;
  unsigned int flush_count_;
  int32 last_put_offset_;

  // Tasks to be invoked in echo responses.
  std::queue<base::Closure> echo_tasks_;

  base::Closure channel_error_callback_;

  base::Callback<void(const GpuMemoryAllocationForRenderer&)>
      memory_allocation_changed_callback_;

  GpuConsoleMessageCallback console_message_callback_;

  // Tasks to be invoked in SignalSyncPoint responses.
  uint32 next_signal_id_;
  SignalTaskMap signal_tasks_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferProxyImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
