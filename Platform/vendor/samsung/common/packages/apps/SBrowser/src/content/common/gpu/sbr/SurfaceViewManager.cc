//
//
//  
//
//  @ Project : SBrowser
//  @ File Name : SurfaceViewManager.cc
//  @ Date : 05-June-2013
//  @ Author : Sandeep Soni
//
//
#include "content/common/gpu/image_transport_surface.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "ui/gl/gl_surface_egl.h"
#include "content/common/gpu/sbr/SurfaceViewManager.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
#include "base/sbr/msc.h"
#endif
#include "base/command_line.h"
#include "gpu/command_buffer/service/gpu_switches.h"

#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL)

namespace content {

SurfaceViewManager* SurfaceViewManager::_surfaceViewMgr=NULL;

	void SurfaceViewManager::AddObserver(int surface_id,SurfaceViewObserver* observer) {
		gfx::AcceleratedWidget    view = (gfx::AcceleratedWidget)(GpuSurfaceTracker::Get()->GetNativeWidget(surface_id));
		LOG(INFO)<<__FUNCTION__<<"  "<<observer<<" for "<<view;
		surface_idmap.insert(std::make_pair(surface_id,view));
		if(_observer_map.count(view)==0)
			_observer_map[view] = new ObserverList<SurfaceViewObserver>;
		_observer_map[view]->AddObserver(observer);

#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL_MULTI_DRAG)
		multi_observer.insert(std::make_pair(surface_id,observer));
#endif
		
	}
	void SurfaceViewManager::SetNativeWindow(gfx::AcceleratedWidget view,ANativeWindow* window)
	{
	       //MSC_METHOD_NORMAL("CC","GPU",__FUNCTION__,window);
		   LOG(INFO)<<__FUNCTION__<<" "<<window;
		if(window)
		{
			SurfaceConstIterator  itr = SurfaceMap.find(view);
			if(itr == SurfaceMap.end())
			{
				ANativeWindow_acquire(window);
				scoped_refptr<gfx::NativeViewGLSurfaceEGL> surface_ = new gfx::NativeViewGLSurfaceEGL(false, window);
				#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
				MSC_METHOD_NORMAL("GPU","GPU","Created Native Surface for ",view);
				#endif
				LOG(INFO)<<"Single_Compositor:Created Native Surface for "<<view;
				bool initialize_success = surface_->Initialize();
				if (window)
					ANativeWindow_release(window);
				if (!initialize_success)
				{
					LOG(ERROR)<<"Single_Compositor:Window Creation Failure"<<window;
					return ;
				}
				SurfaceMap.insert(std::make_pair(view,surface_));
				ObserverIterator o_itr = _observer_map.find(view);
				if(o_itr != _observer_map.end())
				{
					FOR_EACH_OBSERVER(SurfaceViewObserver,
						*(o_itr->second),
						OnSurfaceViewCreated(surface_.get()));
				}
                CommandLine* command_line = CommandLine::ForCurrentProcess();
                int ensure_pbuffer=command_line->HasSwitch(switches::kEnsureSurfaceWorkaround);
                if(ensure_pbuffer)
                    LOG(INFO)<<"Single_Compositor:OffScreen Rendering Disabled";
				
			}
			else
			{
				//Should not have happen
				ObserverIterator o_itr = _observer_map.find(view);
				if(o_itr != _observer_map.end())
				{
					FOR_EACH_OBSERVER(SurfaceViewObserver,
						*(o_itr->second),
						OnSurfaceViewChanged(itr->second.get()));
				}

			}

		}
		else
		{
			SurfaceConstIterator  itr = SurfaceMap.find(view);
			if(itr != SurfaceMap.end())
			{
				ObserverIterator o_itr = _observer_map.find(view);
				if(o_itr != _observer_map.end())
				{
					FOR_EACH_OBSERVER(SurfaceViewObserver,
						*(o_itr->second),
						OnSurfaceViewDeleted(pbuffer_surface_.get()));
				}
				
				
				   if(num_visible_widgets)
					{
						_event = new base::WaitableEvent(false,false);
						LOG(INFO)<<"Single_Compositor:Delayed Deleted Native Surface";
						#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
						MSC_METHOD_NORMAL("GPU","GPU","Delayed Deleted Native Surface for ",view);
						#endif
						_event->TimedWait(base::TimeDelta::FromMilliseconds(500));		
						delete _event;
					}
				_event=NULL;
				#if defined(SBROWSER_GRAPHICS_MSC_TOOL)
				MSC_METHOD_NORMAL("GPU","GPU","Deleted Native Surface for ",view);
				#endif
				LOG(INFO)<<"Single_Compositor:Deleted Native Surface Completed "<<view;
				gfx::NativeViewGLSurfaceEGL  * surface = itr->second.get();
				if(surface)
					surface->Destroy();
				SurfaceMap.erase(view);
			}
		}

	}
	bool SurfaceViewManager::HasActiveWindow(gfx::AcceleratedWidget window) const {
		SurfaceConstIterator  itr = SurfaceMap.find(window);
        CommandLine* command_line = CommandLine::ForCurrentProcess();
        int ensure_pbuffer = command_line->HasSwitch(switches::kEnsureSurfaceWorkaround);
		if(itr != SurfaceMap.end())
            return true;
        else
        {
            if(ensure_pbuffer)
                return true; //Temp Changes
            return false;
        }
	}
	bool SurfaceViewManager::HasObserver(int surface_id,SurfaceViewObserver* observer) const {
		gfx::AcceleratedWidget    view = GpuSurfaceTracker::Get()->GetNativeWidget(surface_id);
		ObserverConstIterator itr = _observer_map.find(view);
		if(itr != _observer_map.end())
		{
			return itr->second->HasObserver(observer);
		}
		return false;
	}
	#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL_MULTI_DRAG)
	void SurfaceViewManager::MigrateActiveWindow(int surface_id)
	{
		gfx::AcceleratedWidget    new_view = GpuSurfaceTracker::Get()->GetNativeWidget(surface_id);
		SurfaceViewObserver* observer = multi_observer[surface_id];
		if(observer)
		{
			RemoveObserver(surface_id,observer);
			surface_idmap.erase(surface_id);
			surface_idmap.insert(std::make_pair(surface_id,new_view));
			AddObserver(surface_id,observer);
			SurfaceConstIterator  itr = SurfaceMap.find(new_view);
			if(itr != SurfaceMap.end())
			{
				observer->OnSurfaceViewChanged(itr->second.get());
			}
		}
		
	}
	#endif

	void SurfaceViewManager::RemoveObserver(int surface_id,SurfaceViewObserver* observer) {
		gfx::AcceleratedWidget    view = surface_idmap[surface_id];
		LOG(INFO)<<__FUNCTION__<<"  "<<observer<<" for "<<view;
		if(_observer_map.count(view)==1)
		{
			_observer_map[view]->RemoveObserver(observer);
			if(_observer_map[view]->size()==0)
			{
				delete _observer_map[view];
				_observer_map.erase(view);
			}
			surface_idmap.erase(surface_id);
			
			#if defined(SBROWSER_GRAPHICS_UI_COMPOSITOR_REMOVAL_MULTI_DRAG)
			multi_observer.erase(surface_id);
			#endif
		}
		
	}

	SurfaceViewManager* SurfaceViewManager::GetInstance() {
		if(_surfaceViewMgr == NULL)
		{
			_surfaceViewMgr= new SurfaceViewManager();
		}
		return _surfaceViewMgr;
	}

	SurfaceViewManager::~SurfaceViewManager() {
		_surfaceViewMgr = NULL;
		pbuffer_surface_ = NULL;
	}
	SurfaceViewManager::SurfaceViewManager() {
		 pbuffer_surface_ = new gfx::PbufferGLSurfaceEGL(false, gfx::Size(1,1));
 		 pbuffer_surface_->Initialize();
		 num_visible_widgets=0;
		 _event = NULL;
	}
	gfx::GLSurface* SurfaceViewManager::GetActiveWindow(gfx::AcceleratedWidget window) const{
		SurfaceConstIterator  itr = SurfaceMap.find(window);
        CommandLine* command_line = CommandLine::ForCurrentProcess();
        int ensure_pbuffer = command_line->HasSwitch(switches::kEnsureSurfaceWorkaround);
		if(itr != SurfaceMap.end())
			return itr->second.get();
        else
        {
            if(ensure_pbuffer)
            {
                LOG(INFO)<<"Single_Compositor:OffScreen Rendering Active";
                return pbuffer_surface_.get();
            }
            else
                return NULL;
        }
	}

    void SurfaceViewManager::SetSurfaceVisibility(bool visiblility)
	{
		LOG(INFO)<<"Single_Compositor: "<<num_visible_widgets<<" "<<visiblility;
		if(visiblility)
			num_visible_widgets++;
		else
		{
			if(num_visible_widgets)
				num_visible_widgets--;
			if((num_visible_widgets == 0)&&(_event != NULL))
			{
				_event->Signal();
			}
		}
		
	}

}
#endif
