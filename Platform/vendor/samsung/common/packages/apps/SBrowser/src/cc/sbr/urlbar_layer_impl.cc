
#if 0
#include "cc/sbr/urlbar_layer_impl.h"

#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/debug_rect_history.h"
#include "cc/debug/frame_rate_counter.h"
#include "cc/debug/paint_time_counter.h"
#include "cc/layers/quad_sink.h"
#include "cc/output/renderer.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/tile_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"
#include "base/logging.h"

//#if defined(SBROWSER_HIDE_URLBAR)

namespace cc {

UrlBarLayerImpl::UrlBarLayerImpl(LayerTreeImpl* tree_impl,
                                                 int id)
    : LayerImpl(tree_impl, id),
      transfer_texture_(0),
      urlbar_texture_(0),
      urlbar_canvas_(0) {
      texture_changed_ = false;
      transfer_texture_ = new TransferTexture();
      layer_texture_ = 0;
}

UrlBarLayerImpl::~UrlBarLayerImpl() {
  transfer_texture_->bitmap_.reset();
  delete transfer_texture_;
}

scoped_ptr<LayerImpl> UrlBarLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return UrlBarLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

bool UrlBarLayerImpl::active(){
  if(layer_tree_impl()){
    return layer_tree_impl()->urlbar_active();
  }
  return false;
}

void UrlBarLayerImpl::SetUrlBarUpdate(bool needsUpdate)
{
   if(layer_tree_impl())
      layer_tree_impl()->SetUrlBarUpdate(needsUpdate);
}

bool UrlBarLayerImpl::pending_urlbar_update()
{
   if(layer_tree_impl()){
      return (layer_tree_impl()->pending_urlbar_update());
   }
   return false;
}

void UrlBarLayerImpl::WillDraw(ResourceProvider* resource_provider) {
  LayerImpl::WillDraw(resource_provider);

  if (!urlbar_texture_)
    urlbar_texture_ = ScopedResource::create(resource_provider);

  if (urlbar_texture_->size() != bounds() ||
      resource_provider->InUseByConsumer(urlbar_texture_->id())){
      urlbar_texture_->Free();
      VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" Freeing UrlBarLayer Texture due to Bounds mismatch";
  }

  if (!urlbar_texture_->id()) {
    urlbar_texture_->Allocate(
        bounds(), GL_RGBA, ResourceProvider::TextureUsageAny);
    resource_provider->AllocateForTesting(urlbar_texture_->id());
    texture_changed_ = true;
    VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" Allocating UrlBarLayer Texture";
  }
}

ResourceProvider::ResourceId UrlBarLayerImpl::GetResourceId(){
   if(!urlbar_texture_ || !urlbar_texture_->id()){
     return ResourceProvider::ResourceId(0);
   }
   return urlbar_texture_->id();
}

void UrlBarLayerImpl::AppendQuads(QuadSink* quad_sink,
                                          AppendQuadsData* append_quads_data) {
  VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" Appending Quads for UrlBarLayer ";
  if (!urlbar_texture_ || !urlbar_texture_->id() /*|| !active()*/){
    VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBarLayer Texture is InValid, Returning";
    return;
  }

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());

  gfx::Rect quad_rect(bounds().width()/layer_tree_impl()->device_scale_factor(), bounds().height()/layer_tree_impl()->device_scale_factor());
  gfx::Rect opaque_rect(quad_rect);
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(0.f, 0.f);
  gfx::PointF uv_bottom_right(1.f, 1.f);
  const float vertex_opacity[] = { 1.f, 1.f, 1.f, 1.f };
  bool flipped = false;
  scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               quad_rect,
               opaque_rect,
               urlbar_texture_->id(),
               premultiplied_alpha,
               uv_top_left,
               uv_bottom_right,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
}

void UrlBarLayerImpl::UpdateUrlBarTexture(
    ResourceProvider* resource_provider) {
  VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" Updating UrlBar Texture ";
  if (!urlbar_texture_ || !urlbar_texture_->id() /*|| !active() || isTextureValid()*/){
    VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBarLayer Texture is InValid, Returning ";
    return;
  }
#if 1
  if(!pending_urlbar_update() && !texture_changed_)
        return;

  SkISize canvas_size;
  if (urlbar_canvas_)
    canvas_size = urlbar_canvas_->getDeviceSize();
  else
    canvas_size.set(0, 0);

  if (canvas_size.fWidth != bounds().width() ||
      canvas_size.fHeight != bounds().height() || !urlbar_canvas_) {
    bool opaque = false;
    urlbar_canvas_ = make_scoped_ptr(
        skia::CreateBitmapCanvas(bounds().width(), bounds().height(), opaque));
  }
  
  urlbar_canvas_->clear(SkColorSetARGB(0, 0, 0, 0));
  DrawUrlBarContents(urlbar_canvas_.get());

  const SkBitmap* bitmap = &urlbar_canvas_->getDevice()->accessBitmap(false);
  SkAutoLockPixels locker(*bitmap);

  gfx::Rect layer_rect(bounds());
  DCHECK(bitmap->config() == SkBitmap::kARGB_8888_Config);
  resource_provider->SetPixels(urlbar_texture_->id(),
                               static_cast<const uint8_t*>(bitmap->getPixels()),
                               layer_rect,
                               layer_rect,
                               gfx::Vector2d());
#else //Prevent Extra Draw to Canvas, keeping old code for reference, to be removed on feature completion
  if(active()){
      SkAutoLockPixels locker(transfer_texture_->bitmap_);
    
      gfx::Rect layer_rect(bounds());
      DCHECK(transfer_texture_->bitmap_.config() == SkBitmap::kARGB_8888_Config);
      resource_provider->SetPixels(urlbar_texture_->id(),
                                   static_cast<const uint8_t*>(transfer_texture_->bitmap_.getPixels()),
                                   layer_rect,
                                   layer_rect,
                                   gfx::Vector2d());
  }
#endif
}

void UrlBarLayerImpl::DidDraw(ResourceProvider* resource_provider) {
  LayerImpl::DidDraw(resource_provider);

  if (!urlbar_texture_ || !urlbar_texture_->id())
    return;

  // FIXME: the following assert will not be true when sending resources to a
  // parent compositor. We will probably need to hold on to urlbar_texture_ for
  // longer, and have several UrlBar textures in the pipeline.
  DCHECK(!resource_provider->InUseByConsumer(urlbar_texture_->id()));
}

void UrlBarLayerImpl::DidLoseOutputSurface() { urlbar_texture_.reset(); }

bool UrlBarLayerImpl::LayerIsAlwaysDamaged() const { return false; }

void UrlBarLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  UrlBarLayerImpl* urlbar_layer_impl = static_cast<UrlBarLayerImpl*>(layer);
  urlbar_layer_impl->SetTexture(transfer_texture_, layer_texture_);
}

void UrlBarLayerImpl::SetTexture(TransferTexture *texture, TransferTexture *layer_texture){

  VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" Enters ";
  if(layer_texture_ == layer_texture) {
     VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBarLayerImpl Texture Set is same";
     return;
  }
  if(texture){
    transfer_texture_->bitmap_.reset();
    texture->bitmap_.copyTo(&(transfer_texture_->bitmap_), SkBitmap::kARGB_8888_Config);
    layer_texture_ = layer_texture;
    texture_changed_ = true;
    VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBarLayerImpl Texture Set";
  }
}

bool UrlBarLayerImpl::isTextureValid(){
  if (!urlbar_texture_ || !urlbar_texture_->id()) {
    return false;
  }
  return true;
}

void UrlBarLayerImpl::SendUrlBarActiveAckIfNeeded() {
     if(layer_tree_impl())
       layer_tree_impl()->SendUrlBarActiveAckIfNeeded();
}

void UrlBarLayerImpl::DrawUrlBarContents(SkCanvas* canvas)  {

  SkPaint paint;

    VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" Enters  ";
    if(!transfer_texture_)
        return;

  if((pending_urlbar_update() || texture_changed_) && active()){
    VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBarLayer Draws Active Bitmap ";
    canvas->drawBitmap(transfer_texture_->bitmap_, SkIntToScalar(0), SkIntToScalar(0), &paint);
    if(pending_urlbar_update()){
      SendUrlBarActiveAckIfNeeded();
    }
    //canvas->drawLine(SkIntToScalar(0), SkIntToScalar(70),SkIntToScalar(1000), SkIntToScalar(70), paint);
    SetUrlBarUpdate(false);
    texture_changed_ = false;
    }
  else {
        VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBarLayer InActive, Draw Skipped  this = " << this;
    }
    
}

const char* UrlBarLayerImpl::LayerTypeAsString() const {
  return "UrlBarLayer";
}

}  // namespace cc
#endif
