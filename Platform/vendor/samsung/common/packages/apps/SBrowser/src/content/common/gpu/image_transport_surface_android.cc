// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "base/logging.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "ui/gl/gl_surface_egl.h"
#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
#include "content/common/gpu/sbr/SurfaceViewManager.h"
#endif
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
#include "base/sbr/msc.h"
#endif

namespace content {

#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
extern int single_compositor;
#endif

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  DCHECK(GpuSurfaceLookup::GetInstance());
  DCHECK_EQ(handle.transport_type, gfx::NATIVE_DIRECT);
#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
  if(single_compositor)
  {
  	SurfaceViewManager  *mgr=SurfaceViewManager::GetInstance();
       scoped_refptr<gfx::GLSurface> surface;
	 gfx::AcceleratedWidget    q_window=GpuSurfaceLookup::GetInstance()->GetNativeWidget(stub->surface_id());
	 LOG(INFO)<<"Single_Compositor: Checking for "<<stub->surface_id()<<" to "<<q_window;
  	if((mgr)&&(mgr->HasActiveWindow(q_window))&&(mgr->GetActiveWindow(q_window)!=NULL))
  	{
  		surface = new PassThroughImageTransportSurface(
        		manager, stub, mgr->GetActiveWindow(q_window), false);

#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
		MSC_METHOD_NORMAL("GPU","GPU",__FUNCTION__,surface);
#else
		LOG(INFO)<<"Single_Compositor: Created nativeSurface";
#endif
    	return surface;
  	
 	 }
	LOG(ERROR) << "Single_Compositor:SurfaceViewManager Doesn't has active Window";
	return NULL;
  }
  #endif
  ANativeWindow* window =
      GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(
          stub->surface_id());
  scoped_refptr<gfx::GLSurface> surface =
      new gfx::NativeViewGLSurfaceEGL(false, window);
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
MSC_METHOD_NORMAL("GPU","GPU",__FUNCTION__,"");
#endif
  bool initialize_success = surface->Initialize();
  if (window)
    ANativeWindow_release(window);
  if (!initialize_success)
    return scoped_refptr<gfx::GLSurface>();

  return scoped_refptr<gfx::GLSurface>(new PassThroughImageTransportSurface(
      manager, stub, surface.get(), false));
}

}  // namespace content
