
#ifndef CC_LAYERS_URLBAR_LAYER_H_
#define CC_LAYERS_URLBAR_LAYER_H_

#if 0
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/sbr/urlbar_utils.h"

//#if defined(SBROWSER_HIDE_URLBAR)
namespace cc {

class CC_EXPORT UrlBarLayer : public Layer {
 public:
  static scoped_refptr<UrlBarLayer> Create();

  virtual void Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* tracker,
                      RenderingStats* stats) OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  void SetTextureSyncPending(bool sync) {texture_sync_pending_ = sync;}
  bool TextureSyncPending() {return texture_sync_pending_;}
  void SetTexture(TransferTexture *texture);

 protected:
  UrlBarLayer();

 private:
  virtual ~UrlBarLayer();
  bool texture_sync_pending_;
  TransferTexture *texture_;
};

}  // namespace cc
#endif

#endif  // CC_LAYERS_URLBAR_LAYER_H_
