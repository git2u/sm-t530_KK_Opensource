
#ifndef CC_LAYERS_URLBAR_LAYER_IMPL_H_
#define CC_LAYERS_URLBAR_LAYER_IMPL_H_

#if 0
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/resources/scoped_resource.h"
#include "cc/sbr/urlbar_utils.h"

//#if defined(SBROWSER_HIDE_URLBAR)
class SkCanvas;
class SkPaint;
class SkTypeface;
struct SkRect;

namespace cc {

class CC_EXPORT UrlBarLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<UrlBarLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                    int id) {
    return make_scoped_ptr(new UrlBarLayerImpl(tree_impl, id));
  }
  virtual ~UrlBarLayerImpl();

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  virtual void WillDraw(ResourceProvider* resource_provider) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;
  void UpdateUrlBarTexture(ResourceProvider* resource_provider);
  virtual void DidDraw(ResourceProvider* resource_provider) OVERRIDE;

  virtual void DidLoseOutputSurface() OVERRIDE;

  virtual bool LayerIsAlwaysDamaged() const OVERRIDE;

  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  ResourceProvider::ResourceId GetResourceId();
  bool active();

  void SetTexture(TransferTexture *texture, TransferTexture *layer_texture);
  bool isTextureValid();
  void SendUrlBarActiveAckIfNeeded();
 
 private:

  UrlBarLayerImpl(LayerTreeImpl* tree_impl, int id);

  virtual const char* LayerTypeAsString() const OVERRIDE;

  void DrawUrlBarContents(SkCanvas* canvas) ;

  void SetUrlBarUpdate(bool needsUpdate);
  bool pending_urlbar_update();

  TransferTexture *transfer_texture_;
  TransferTexture *layer_texture_;
  scoped_ptr<ScopedResource> urlbar_texture_;
  scoped_ptr<SkCanvas> urlbar_canvas_;
  bool active_ack_pending_;
  bool texture_changed_;
  DISALLOW_COPY_AND_ASSIGN(UrlBarLayerImpl);
};

}  // namespace cc
#endif

#endif  // CC_LAYERS_URLBAR_LAYER_IMPL_H_
