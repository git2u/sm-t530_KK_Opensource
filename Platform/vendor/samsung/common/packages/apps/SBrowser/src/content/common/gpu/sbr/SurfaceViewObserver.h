//
//
//  
//
//  @ Project : SBrowser
//  @ File Name : SurfaceViewObserver.h
//  @ Date : 05-June-2013
//  @ Author : Sandeep Soni
//
//


#if !defined(_SURFACEVIEWOBSERVER_H)
#define _SURFACEVIEWOBSERVER_H

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/logging.h"

#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
//#include "msc.h"
namespace gfx{
class GLSurface;
}

namespace content {
class SurfaceViewObserver {
public:
	virtual void OnSurfaceViewCreated(const gfx::GLSurface* surface) = 0;
	virtual void OnSurfaceViewChanged(const gfx::GLSurface* surface) = 0;
	virtual void OnSurfaceViewDeleted(const gfx::GLSurface* surface) = 0;
protected:
	virtual ~SurfaceViewObserver() { }
};
}
#endif

#endif  //_SURFACEVIEWOBSERVER_H
