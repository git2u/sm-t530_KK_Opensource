// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/async_pixel_transfer_delegate.h"

#include "base/debug/trace_event.h"
#include "ui/gl/async_pixel_transfer_delegate_egl.h"
#include "ui/gl/async_pixel_transfer_delegate_stub.h"
#include "ui/gl/async_pixel_transfer_delegate_sync.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

namespace gfx {
namespace {

bool IsBroadcom() {
  const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  if (vendor)
    return std::string(vendor).find("Broadcom") != std::string::npos;
  return false;
}

bool IsImagination() {
  const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  if (vendor)
    return std::string(vendor).find("Imagination") != std::string::npos;
  return false;
}

}

// We only used threaded uploads when we can:
// - Create EGLImages out of OpenGL textures (EGL_KHR_gl_texture_2D_image)
// - Bind EGLImages to OpenGL textures (GL_OES_EGL_image)
// - Use fences (to test for upload completion).
#if defined(SBROWSER_IPC_SHARED_MEMORY_TRACKING)
AsyncPixelTransferDelegate* AsyncPixelTransferDelegate::Create(
    gfx::GLContext* context, SharedMemoryPoolInterface* gles2Decoder) {
#else
AsyncPixelTransferDelegate* AsyncPixelTransferDelegate::Create(
    gfx::GLContext* context) {
#endif
  TRACE_EVENT0("gpu", "AsyncPixelTransferDelegate::Create");
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
      DCHECK(context);
      if (context->HasExtension("EGL_KHR_fence_sync") &&
          context->HasExtension("EGL_KHR_image") &&
          context->HasExtension("EGL_KHR_image_base") &&
          context->HasExtension("EGL_KHR_gl_texture_2D_image") &&
          context->HasExtension("GL_OES_EGL_image") &&
          !IsBroadcom() &&
          !IsImagination()) {
#if defined(SBROWSER_IPC_SHARED_MEMORY_TRACKING)
          return new AsyncPixelTransferDelegateEGL(gles2Decoder);
#else
          return new AsyncPixelTransferDelegateEGL;
#endif
      }
      LOG(INFO) << "Async pixel transfers not supported";
      return new AsyncPixelTransferDelegateSync;
    case kGLImplementationMockGL:
      return new AsyncPixelTransferDelegateStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace gfx
