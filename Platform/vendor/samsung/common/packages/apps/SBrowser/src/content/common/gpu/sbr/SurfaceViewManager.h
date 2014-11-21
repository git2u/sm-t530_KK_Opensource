//
//
//  
//
//  @ Project : SBrowser
//  @ File Name : SurfaceViewManager.h
//  @ Date : 05-June-2013
//  @ Author : Sandeep Soni
//
//


#if !defined(_SURFACEVIEWMANAGER_H)
#define _SURFACEVIEWMANAGER_H


#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_surface.h"
#include "ui/surface/transport_dib.h"
#include "content/common/gpu/sbr/SurfaceViewObserver.h"
#include "base/synchronization/waitable_event.h"


#define SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL_MULTI_DRAG

#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)
namespace gfx{
class NativeViewGLSurfaceEGL;
}

namespace content {

class SurfaceViewManager {
public:
	void AddObserver(int,SurfaceViewObserver* observer);
    void SetSurfaceVisibility(bool visiblility);
	void RemoveObserver(int,SurfaceViewObserver* observer);
	bool HasObserver(int,SurfaceViewObserver* observer) const;

	void SetNativeWindow(gfx::AcceleratedWidget,ANativeWindow* window);
	bool HasActiveWindow(gfx::AcceleratedWidget) const;
       gfx::GLSurface* GetActiveWindow(gfx::AcceleratedWidget) const;
#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL_MULTI_DRAG)
	void MigrateActiveWindow(int surface_id);
#endif

	static SurfaceViewManager* GetInstance();
	~SurfaceViewManager();
private:
	typedef std::map<gfx::AcceleratedWidget, scoped_refptr<gfx::NativeViewGLSurfaceEGL> >    NativeSurfaceMap;
	typedef typename NativeSurfaceMap::const_iterator SurfaceConstIterator;
	typedef  ObserverList<SurfaceViewObserver> SurfaceObserverList;
	typedef std::map<gfx::AcceleratedWidget, SurfaceObserverList * >    ObserverMap;
      typedef typename ObserverMap::iterator ObserverIterator;
	  typedef typename ObserverMap::const_iterator ObserverConstIterator;
	//scoped_refptr<gfx::NativeViewGLSurfaceEGL> surface_;
	NativeSurfaceMap SurfaceMap;
	ObserverMap        _observer_map;
	scoped_refptr<gfx::GLSurface> pbuffer_surface_;
#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL_MULTI_DRAG)
	std::map<int,SurfaceViewObserver*> multi_observer;
#endif
	static SurfaceViewManager* _surfaceViewMgr;
	std::map<int,gfx::AcceleratedWidget > surface_idmap;
	base::WaitableEvent* _event;
	int num_visible_widgets;
	SurfaceViewManager();
};
}
#endif

#endif  //_SURFACEVIEWMANAGER_H
