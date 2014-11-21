
#if 0
#include "cc/sbr/urlbar_layer.h"

#include "base/debug/trace_event.h"
#include "cc/sbr/urlbar_layer_impl.h"
#include "cc/trees/layer_tree_host.h"

//#if defined(SBROWSER_HIDE_URLBAR)

#include "base/logging.h"

namespace cc {

scoped_refptr<UrlBarLayer> UrlBarLayer::Create() {
  return make_scoped_refptr(new UrlBarLayer());
}

UrlBarLayer::UrlBarLayer() : Layer(),
                             texture_sync_pending_(false),
                             texture_(0){
  //SetBounds(gfx::Size(1, 1));
  texture_ = new TransferTexture();
}

UrlBarLayer::~UrlBarLayer() {
    texture_->bitmap_.reset();
    delete texture_;
}

void UrlBarLayer::Update(ResourceUpdateQueue*,
                                 const OcclusionTracker*,
                                 RenderingStats*) {
  gfx::Size layer_bounds = bounds();
  if(layer_tree_host()->urlbar_height()){
      //int width = layer_tree_host()->device_viewport_size().width() /
      //                  layer_tree_host()->device_scale_factor();
      //int height = layer_tree_host()->urlbar_height()/layer_tree_host()->device_scale_factor();
      int width = layer_tree_host()->device_viewport_size().width();
      int height = layer_tree_host()->urlbar_height();
      layer_bounds.SetSize(width, height);
      SetBounds(layer_bounds);
  }
}

bool UrlBarLayer::DrawsContent() const { return true; }

void UrlBarLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  if(texture_sync_pending_){
    UrlBarLayerImpl* urlbar_layer_impl = static_cast<UrlBarLayerImpl*>(layer);
    urlbar_layer_impl->SetTexture(texture_, texture_);
  }
}

void UrlBarLayer::SetTexture(TransferTexture *texture) {
  if(texture){
    texture_->bitmap_.reset();
    texture->bitmap_.copyTo(&(texture_->bitmap_), SkBitmap::kARGB_8888_Config);
    VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBarLayer Texture Set";
  }
}

scoped_ptr<LayerImpl> UrlBarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return UrlBarLayerImpl::Create(tree_impl, layer_id_).
      PassAs<LayerImpl>();
}

}  // namespace cc
#endif
