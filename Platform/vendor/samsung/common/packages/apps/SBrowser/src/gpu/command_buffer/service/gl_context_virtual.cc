// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_context_virtual.h"

#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "ui/gl/gl_surface.h"

namespace gpu {

GLContextVirtual::GLContextVirtual(
    gfx::GLShareGroup* share_group,
    gfx::GLContext* shared_context,
    base::WeakPtr<gles2::GLES2Decoder> decoder)
  : GLContext(share_group),
    shared_context_(shared_context),
    display_(NULL),
    state_restorer_(new GLStateRestorerImpl(decoder)),
    decoder_(decoder) {
}

gfx::Display* GLContextVirtual::display() {
  return display_;
}

bool GLContextVirtual::Initialize(
    gfx::GLSurface* compatible_surface, gfx::GpuPreference gpu_preference) {
  display_ = static_cast<gfx::Display*>(compatible_surface->GetDisplay());
#if defined (SBROWSER_GRAPHICS_MAKE_VIR_CURRNT )
  // Virtual contexts obviously can't make a context that is compatible
  // with the surface (the context already exists), but we do need to
  // make a context current for SetupForVirtualization() below.
  if (!IsCurrent(compatible_surface)) 
  {
  	if (!shared_context_->MakeCurrent(compatible_surface)) 
	{ 
	  // This is likely an error. The real context should be made as
      // compatible with all required surfaces when it was created.
  		LOG(ERROR) << "Failed MakeCurrent(compatible_surface)";
    		return false;
   	}
  }
  shared_context_->SetupForVirtualization();
  shared_context_->MakeVirtuallyCurrent(this, compatible_surface);
  
#else

  if (!shared_context_->MakeCurrent(compatible_surface))
    return false;

  shared_context_->SetupForVirtualization();

  shared_context_->ReleaseCurrent(compatible_surface);
#endif  
  return true;
}

void GLContextVirtual::Destroy() {
  shared_context_->OnDestroyVirtualContext(this);
  shared_context_ = NULL;
  display_ = NULL;
}

bool GLContextVirtual::MakeCurrent(gfx::GLSurface* surface) {
#if defined (SBROWSER_GRAPHICS_MAKE_VIR_CURRNT )
  if (decoder_.get())
#else
 if (decoder_.get() && decoder_->initialized())
#endif  
    shared_context_->MakeVirtuallyCurrent(this, surface);
#if defined (SBROWSER_GRAPHICS_MAKE_VIR_CURRNT )	
  else if (!IsCurrent(surface))
#else
  else
#endif  
    shared_context_->MakeCurrent(surface);
  return true;
}

void GLContextVirtual::ReleaseCurrent(gfx::GLSurface* surface) {
  if (IsCurrent(surface))
    shared_context_->ReleaseCurrent(surface);
}

bool GLContextVirtual::IsCurrent(gfx::GLSurface* surface) {
#if defined (SBROWSER_GRAPHICS_MAKE_VIR_CURRNT )
  // If it's a real surface it needs to be current.

  if (surface &&
      !surface->GetBackingFrameBufferObject() &&
      !surface->IsOffscreen())
    return shared_context_->IsCurrent(surface);

  // Otherwise, only insure the context itself is current.
  return shared_context_->IsCurrent(NULL);  
 #else
   bool context_current = shared_context_->IsCurrent(NULL);
  if (!context_current)
    return false;

  if (!surface)
    return true;

  gfx::GLSurface* current_surface = gfx::GLSurface::GetCurrent();
  LOG(INFO)<<__FUNCTION__<<" Single_Compositor  current Surface is "<<current_surface<<" passed Surface is "<<surface;
  
  return surface->GetBackingFrameBufferObject() ||
      surface->IsOffscreen() ||
      (current_surface &&
       current_surface->GetHandle() == surface->GetHandle());
 #endif 
}

void* GLContextVirtual::GetHandle() {
  return shared_context_->GetHandle();
}

gfx::GLStateRestorer* GLContextVirtual::GetGLStateRestorer() {
  return state_restorer_.get();
}

void GLContextVirtual::SetSwapInterval(int interval) {
  shared_context_->SetSwapInterval(interval);
}

std::string GLContextVirtual::GetExtensions() {
  return shared_context_->GetExtensions();
}

bool GLContextVirtual::GetTotalGpuMemory(size_t* bytes) {
  return shared_context_->GetTotalGpuMemory(bytes);
}

void GLContextVirtual::SetSafeToForceGpuSwitch() {
  // TODO(ccameron): This will not work if two contexts that disagree
  // about whether or not forced gpu switching may be done both share
  // the same underlying shared_context_.
  return shared_context_->SetSafeToForceGpuSwitch();
}

bool GLContextVirtual::WasAllocatedUsingRobustnessExtension() {
  return shared_context_->WasAllocatedUsingRobustnessExtension();
}

void GLContextVirtual::SetUnbindFboOnMakeCurrent() {
  shared_context_->SetUnbindFboOnMakeCurrent();
}

GLContextVirtual::~GLContextVirtual() {
  Destroy();
}

}  // namespace gpu
