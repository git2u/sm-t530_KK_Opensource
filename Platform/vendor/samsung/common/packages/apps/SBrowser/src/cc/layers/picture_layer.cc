// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include "cc/debug/devtools_instrumentation.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_refptr<PictureLayer> PictureLayer::Create(ContentLayerClient* client) {
  return make_scoped_refptr(new PictureLayer(client));
}

PictureLayer::PictureLayer(ContentLayerClient* client)
  : client_(client),
    pile_(make_scoped_refptr(new PicturePile())),
    instrumentation_object_tracker_(id()),
    is_mask_(false)
#if  defined(SBROWSER_PAUSE_ANIMATION_ON_SCROLL_ZOOM)
     ,m_gestureActive(false)
     ,m_triggerPause(false)
#endif   
{
}

PictureLayer::~PictureLayer() {
}

bool PictureLayer::DrawsContent() const {
  return Layer::DrawsContent() && client_;
}

scoped_ptr<LayerImpl> PictureLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void PictureLayer::PushPropertiesTo(LayerImpl* base_layer) {
  Layer::PushPropertiesTo(base_layer);

  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);
  layer_impl->SetIsMask(is_mask_);
  layer_impl->CreateTilingSet();
  layer_impl->invalidation_.Clear();
  layer_impl->invalidation_.Swap(&pile_invalidation_);
  layer_impl->pile_ =
      PicturePileImpl::CreateFromOther(pile_, layer_impl->is_using_lcd_text_);
  layer_impl->SyncFromActiveLayer();
}

void PictureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  Layer::SetLayerTreeHost(host);
  if (host) {
    pile_->SetMinContentsScale(host->settings().minimum_contents_scale);
    pile_->SetTileGridSize(host->settings().default_tile_size);
    pile_->set_num_raster_threads(host->settings().num_raster_threads);
    pile_->set_slow_down_raster_scale_factor(
        host->debug_state().slow_down_raster_scale_factor);
    pile_->set_show_debug_picture_borders(
        host->debug_state().show_picture_borders);
  }
}

void PictureLayer::SetNeedsDisplayRect(const gfx::RectF& layer_rect) {

#if  defined(SBROWSER_PAUSE_ANIMATION_ON_SCROLL_ZOOM)
    if(m_gestureActive)    {
          //pause on animation
	  doAnimationRegionPause(layer_rect);
          m_triggerPause = true;
	  return;
    }
#endif	
  gfx::Rect rect = gfx::ToEnclosedRect(layer_rect);
  if (!rect.IsEmpty()) {
    // Clamp invalidation to the layer bounds.
    rect.Intersect(gfx::Rect(bounds()));
    pending_invalidation_.Union(rect);
  }
  Layer::SetNeedsDisplayRect(layer_rect);
}
#if  defined( SBROWSER_PAUSE_ANIMATION_ON_SCROLL_ZOOM)
void PictureLayer::setGestureState(bool gestureActive)
{
    if(m_gestureActive == gestureActive)
        return;
    m_gestureActive = gestureActive;
    //if pause is not triggered for current picturelayer dont allow it to resume
    if(!m_gestureActive && m_triggerPause){
        //resume on animation
        doAnimationRegionResume();
        m_triggerPause = false;
    }
}
/*All invaidated region united during the pause on animations in pending_invalidation_
will be set as needs for display, so that the picture layer will be updated with latest invalidated 
information to proceed further.*/
void PictureLayer::doAnimationRegionResume()
{
    if(!pending_invalidation_.bounds().IsEmpty()){
        Layer::SetNeedsDisplayRect(pending_invalidation_.bounds());
    }
}
/*Here we add the invalidated rects to pending_invalidation_ during the Pause cycle*/
void PictureLayer::doAnimationRegionPause(const gfx::RectF& layer_rect)
{
    gfx::Rect animation_rect = gfx::ToEnclosedRect(layer_rect);
     if (!animation_rect.IsEmpty()) {
        animation_rect.Intersect(gfx::Rect(bounds()));
        pending_invalidation_.Union(animation_rect);
    }
}
#endif
void PictureLayer::Update(ResourceUpdateQueue*,
                          const OcclusionTracker*,
                          RenderingStats* stats) {
#if  defined( SBROWSER_PAUSE_ANIMATION_ON_SCROLL_ZOOM)
   //do not allow the Update to proceed if the pause on gesture is active.
   if(m_gestureActive){
	return;
  }
#endif
  // Do not early-out of this function so that PicturePile::Update has a chance
  // to record pictures due to changing visibility of this layer.

  pile_->Resize(paint_properties().bounds);

  // Calling paint in WebKit can sometimes cause invalidations, so save
  // off the invalidation prior to calling update.
  pile_invalidation_.Swap(&pending_invalidation_);
  pending_invalidation_.Clear();

  gfx::Rect visible_layer_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(visible_content_rect(), 1.f / contents_scale_x()));
  devtools_instrumentation::ScopedPaintLayer paint_layer(id());
  pile_->Update(client_,
                background_color(),
                pile_invalidation_,
                visible_layer_rect,
                stats);
}

void PictureLayer::SetIsMask(bool is_mask) {
  is_mask_ = is_mask;
}

}  // namespace cc
