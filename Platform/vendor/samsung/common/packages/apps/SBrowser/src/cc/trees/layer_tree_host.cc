// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <algorithm>
#include <stack>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/base/thread.h"
#include "cc/debug/overdraw_metrics.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/pinch_zoom_scrollbar.h"
#include "cc/input/pinch_zoom_scrollbar_geometry.h"
#include "cc/input/pinch_zoom_scrollbar_painter.h"
#include "cc/input/top_controls_manager.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/render_surface.h"
#include "cc/layers/scrollbar_layer.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "ui/gfx/size_conversions.h"
#if defined(SBROWSER_HIDE_URLBAR)
#include "base/logging.h"
#endif
#if defined(SBROWSER_SUPPORT_GLOW_EDGE_EFFECT)
#include "content/browser/android/overscroll_glow.h"
#include "ui/gfx/screen.h"
#endif

namespace {
static int s_num_layer_tree_instances;
}

namespace cc {

RendererCapabilities::RendererCapabilities()
    : best_texture_format(0),
      using_partial_swap(false),
      using_accelerated_painting(false),
      using_set_visibility(false),
      using_swap_complete_callback(false),
      using_gpu_memory_manager(false),
      using_egl_image(false),
      allow_partial_texture_updates(false),
      using_offscreen_context3d(false),
      max_texture_size(0),
      avoid_pow2_textures(false) {}

RendererCapabilities::~RendererCapabilities() {}

bool LayerTreeHost::AnyLayerTreeHostInstanceExists() {
  return s_num_layer_tree_instances > 0;
}

scoped_ptr<LayerTreeHost> LayerTreeHost::Create(
    LayerTreeHostClient* client,
    const LayerTreeSettings& settings,
    scoped_ptr<Thread> impl_thread) {
  scoped_ptr<LayerTreeHost> layer_tree_host(new LayerTreeHost(client,
                                                              settings));
  if (!layer_tree_host->Initialize(impl_thread.Pass()))
    return scoped_ptr<LayerTreeHost>();
  return layer_tree_host.Pass();
}

LayerTreeHost::LayerTreeHost(LayerTreeHostClient* client,
                             const LayerTreeSettings& settings)
    : animating_(false),
      needs_full_tree_sync_(true),
      needs_filter_context_(false),
      client_(client),
      commit_number_(0),
      rendering_stats_instrumentation_(RenderingStatsInstrumentation::Create()),
      output_surface_can_be_initialized_(true),
      output_surface_lost_(true),
      num_failed_recreate_attempts_(0),
      settings_(settings),
      debug_state_(settings.initial_debug_state),
      overdraw_bottom_height_(0.f),
      device_scale_factor_(1.f),
      visible_(true),
      page_scale_factor_(1.f),
      min_page_scale_factor_(1.f),
      max_page_scale_factor_(1.f),
      trigger_idle_updates_(true),
      background_color_(SK_ColorWHITE),
      has_transparent_background_(false),
      partial_texture_update_requests_(0),
      in_paint_layer_contents_(false)
{
  if (settings_.accelerated_animation_enabled)
    animation_registrar_ = AnimationRegistrar::Create();
  s_num_layer_tree_instances++;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());
#if defined(SBROWSER_HIDE_URLBAR)
     urlbar_height_ = 0;
     bitmap_sync_pending_ = false;
#endif
}

bool LayerTreeHost::Initialize(scoped_ptr<Thread> impl_thread) {
  if (impl_thread)
    return InitializeProxy(ThreadProxy::Create(this, impl_thread.Pass()));
  else
    return InitializeProxy(SingleThreadProxy::Create(this));
}

bool LayerTreeHost::InitializeForTesting(scoped_ptr<Proxy> proxy_for_testing) {
  return InitializeProxy(proxy_for_testing.Pass());
}

bool LayerTreeHost::InitializeProxy(scoped_ptr<Proxy> proxy) {
  TRACE_EVENT0("cc", "LayerTreeHost::InitializeForReal");

  scoped_ptr<OutputSurface> output_surface(CreateOutputSurface());
  if (!output_surface)
    return false;

  proxy_ = proxy.Pass();
  proxy_->Start(output_surface.Pass());
  return true;
}

LayerTreeHost::~LayerTreeHost() {
  TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");
  if (root_layer_)
    root_layer_->SetLayerTreeHost(NULL);

  if (proxy_) {
    DCHECK(proxy_->IsMainThread());
    proxy_->Stop();
  }

  s_num_layer_tree_instances--;
  RateLimiterMap::iterator it = rate_limiters_.begin();
  if (it != rate_limiters_.end())
    it->second->Stop();

  if (root_layer_) {
    // The layer tree must be destroyed before the layer tree host. We've
    // made a contract with our animation controllers that the registrar
    // will outlive them, and we must make good.
    root_layer_ = NULL;
  }
}

void LayerTreeHost::SetSurfaceReady() {
  proxy_->SetSurfaceReady();
}

LayerTreeHost::CreateResult
LayerTreeHost::OnCreateAndInitializeOutputSurfaceAttempted(bool success) {
  TRACE_EVENT1("cc",
               "LayerTreeHost::OnCreateAndInitializeOutputSurfaceAttempted",
               "success",
               success);

  DCHECK(output_surface_lost_);
  if (success) {
    output_surface_lost_ = false;

    // Update settings_ based on capabilities that we got back from the
    // renderer.
    settings_.accelerate_painting =
        proxy_->GetRendererCapabilities().using_accelerated_painting;

    // Update settings_ based on partial update capability.
    size_t max_partial_texture_updates = 0;
    if (proxy_->GetRendererCapabilities().allow_partial_texture_updates &&
        !settings_.impl_side_painting) {
      max_partial_texture_updates = std::min(
          settings_.max_partial_texture_updates,
          proxy_->MaxPartialTextureUpdates());
    }
    settings_.max_partial_texture_updates = max_partial_texture_updates;

    if (!contents_texture_manager_) {
      contents_texture_manager_ =
          PrioritizedResourceManager::Create(proxy_.get());
      surface_memory_placeholder_ =
          contents_texture_manager_->CreateTexture(gfx::Size(), GL_RGBA);
    }

    client_->DidRecreateOutputSurface(true);
    return CreateSucceeded;
  }

  // Failure path.

  client_->DidFailToInitializeOutputSurface();

  // Tolerate a certain number of recreation failures to work around races
  // in the output-surface-lost machinery.
  ++num_failed_recreate_attempts_;
  if (num_failed_recreate_attempts_ >= 5) {
    // We have tried too many times to recreate the output surface. Tell the
    // host to fall back to software rendering.
    output_surface_can_be_initialized_ = false;
    client_->DidRecreateOutputSurface(false);
    return CreateFailedAndGaveUp;
  }

  return CreateFailedButTryAgain;
}

void LayerTreeHost::DeleteContentsTexturesOnImplThread(
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread());
  if (contents_texture_manager_)
    contents_texture_manager_->ClearAllMemory(resource_provider);
}

void LayerTreeHost::AcquireLayerTextures() {
  DCHECK(proxy_->IsMainThread());
  proxy_->AcquireLayerTextures();
}

void LayerTreeHost::DidBeginFrame() {
  client_->DidBeginFrame();
}

void LayerTreeHost::UpdateAnimations(base::TimeTicks frame_begin_time) {
  animating_ = true;
  client_->Animate((frame_begin_time - base::TimeTicks()).InSecondsF());
  AnimateLayers(frame_begin_time);
  animating_ = false;
#if defined(SBROWSER_SUPPORT_GLOW_EDGE_EFFECT)
  if (IsGlowAnimationActive()) {
        if(overscroll_effect_->Animate(base::TimeTicks::Now())) {
           SetNeedsAnimate();
        } else  {
	     ReleaseEdgeGlow(LayerTreeHost::AXIS_XY);
        }		
  }
#endif
  rendering_stats_instrumentation_->IncrementAnimationFrameCount();
}

void LayerTreeHost::DidStopFlinging() {
  proxy_->MainThreadHasStoppedFlinging();
}

void LayerTreeHost::Layout() {
  client_->Layout();
}

void LayerTreeHost::BeginCommitOnImplThread(LayerTreeHostImpl* host_impl) {
  DCHECK(proxy_->IsImplThread());
  TRACE_EVENT0("cc", "LayerTreeHost::CommitTo");
}

// This function commits the LayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a Layer,
// should be delayed until the LayerTreeHost::CommitComplete, which will run
// after the commit, but on the main thread.
void LayerTreeHost::FinishCommitOnImplThread(LayerTreeHostImpl* host_impl) {
  DCHECK(proxy_->IsImplThread());

  // If there are linked evicted backings, these backings' resources may be put
  // into the impl tree, so we can't draw yet. Determine this before clearing
  // all evicted backings.
  bool new_impl_tree_has_no_evicted_resources =
      !contents_texture_manager_->LinkedEvictedBackingsExist();

  // If the memory limit has been increased since this now-finishing
  // commit began, and the extra now-available memory would have been used,
  // then request another commit.
  if (contents_texture_manager_->MaxMemoryLimitBytes() <
      host_impl->memory_allocation_limit_bytes() &&
      contents_texture_manager_->MaxMemoryLimitBytes() <
      contents_texture_manager_->MaxMemoryNeededBytes()) {
    host_impl->SetNeedsCommit();
  }

  host_impl->set_max_memory_needed_bytes(
      contents_texture_manager_->MaxMemoryNeededBytes());

  contents_texture_manager_->UpdateBackingsInDrawingImplTree();

  // In impl-side painting, synchronize to the pending tree so that it has
  // time to raster before being displayed.  If no pending tree is needed,
  // synchronization can happen directly to the active tree and
  // unlinked contents resources can be reclaimed immediately.
  LayerTreeImpl* sync_tree;
  if (settings_.impl_side_painting) {
    // Commits should not occur while there is already a pending tree.
    DCHECK(!host_impl->pending_tree());
    host_impl->CreatePendingTree();
    sync_tree = host_impl->pending_tree();
  } else {
    contents_texture_manager_->ReduceMemory(host_impl->resource_provider());
    sync_tree = host_impl->active_tree();
  }

  if (needs_full_tree_sync_)
    sync_tree->SetRootLayer(TreeSynchronizer::SynchronizeTrees(
        root_layer(), sync_tree->DetachLayerTree(), sync_tree));
  {
    TRACE_EVENT0("cc", "LayerTreeHost::PushProperties");
    TreeSynchronizer::PushProperties(root_layer(), sync_tree->root_layer());
  }

  sync_tree->set_needs_full_tree_sync(needs_full_tree_sync_);
  needs_full_tree_sync_ = false;

  if (root_layer_ && hud_layer_) {
    LayerImpl* hud_impl = LayerTreeHostCommon::FindLayerInSubtree(
        sync_tree->root_layer(), hud_layer_->id());
    sync_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    sync_tree->set_hud_layer(NULL);
  }
#if defined(SBROWSER_HIDE_URLBAR)
  if (settings_.enable_hide_urlbar && root_layer_ && urlbar_layer_) {
    LayerImpl* urlbar_impl = LayerTreeHostCommon::FindLayerInSubtree(
        sync_tree->root_layer(), urlbar_layer_->id());
    sync_tree->set_urlbar_layer(static_cast<LayerImpl*>(urlbar_impl));
  } else {
    sync_tree->set_urlbar_layer(NULL);
  }
#endif

  sync_tree->set_source_frame_number(commit_number());
  sync_tree->set_background_color(background_color_);
  sync_tree->set_has_transparent_background(has_transparent_background_);

  sync_tree->FindRootScrollLayer();

  float page_scale_delta, sent_page_scale_delta;
  if (settings_.impl_side_painting) {
    // Update the delta from the active tree, which may have
    // adjusted its delta prior to the pending tree being created.
    // This code is equivalent to that in LayerTreeImpl::SetPageScaleDelta.
    DCHECK_EQ(1.f, sync_tree->sent_page_scale_delta());
    page_scale_delta = host_impl->active_tree()->page_scale_delta();
    sent_page_scale_delta = host_impl->active_tree()->sent_page_scale_delta();
  } else {
    page_scale_delta = sync_tree->page_scale_delta();
    sent_page_scale_delta = sync_tree->sent_page_scale_delta();
    sync_tree->set_sent_page_scale_delta(1.f);
  }

  sync_tree->SetPageScaleFactorAndLimits(page_scale_factor_,
                                         min_page_scale_factor_,
                                         max_page_scale_factor_);
  sync_tree->SetPageScaleDelta(page_scale_delta / sent_page_scale_delta);
  sync_tree->SetLatencyInfo(latency_info_);
  latency_info_.Clear();

  host_impl->SetViewportSize(device_viewport_size_);
  host_impl->SetOverdrawBottomHeight(overdraw_bottom_height_);
  host_impl->SetDeviceScaleFactor(device_scale_factor_);
  host_impl->SetDebugState(debug_state_);
  if (pending_page_scale_animation_) {
    host_impl->StartPageScaleAnimation(
        pending_page_scale_animation_->target_offset,
        pending_page_scale_animation_->use_anchor,
        pending_page_scale_animation_->scale,
        base::TimeTicks::Now(),
        pending_page_scale_animation_->duration);
    pending_page_scale_animation_.reset();
  }

  DCHECK(!sync_tree->ViewportSizeInvalid());

  if (new_impl_tree_has_no_evicted_resources) {
    if (sync_tree->ContentsTexturesPurged())
      sync_tree->ResetContentsTexturesPurged();
  }

  sync_tree->SetPinchZoomHorizontalLayerId(
      pinch_zoom_scrollbar_horizontal_ ?
          pinch_zoom_scrollbar_horizontal_->id() : Layer::INVALID_ID);

  sync_tree->SetPinchZoomVerticalLayerId(
      pinch_zoom_scrollbar_vertical_ ?
          pinch_zoom_scrollbar_vertical_->id() : Layer::INVALID_ID);

  if (!settings_.impl_side_painting) {
    // If we're not in impl-side painting, the tree is immediately
    // considered active.
    sync_tree->DidBecomeActive();
  }

  commit_number_++;
}

gfx::Size LayerTreeHost::PinchZoomScrollbarSize(
    WebKit::WebScrollbar::Orientation orientation) const {
  gfx::Size viewport_size = gfx::ToCeiledSize(
      gfx::ScaleSize(device_viewport_size(), 1.f / device_scale_factor()));
  gfx::Size size;
  int track_width = PinchZoomScrollbarGeometry::kTrackWidth;
  if (orientation == WebKit::WebScrollbar::Horizontal)
    size = gfx::Size(viewport_size.width() - track_width, track_width);
  else
    size = gfx::Size(track_width, viewport_size.height() - track_width);
  return size;
}

void LayerTreeHost::SetPinchZoomScrollbarsBoundsAndPosition() {
  if (!pinch_zoom_scrollbar_horizontal_ || !pinch_zoom_scrollbar_vertical_)
    return;

  gfx::Size horizontal_size =
      PinchZoomScrollbarSize(WebKit::WebScrollbar::Horizontal);
  gfx::Size vertical_size =
      PinchZoomScrollbarSize(WebKit::WebScrollbar::Vertical);

  pinch_zoom_scrollbar_horizontal_->SetBounds(horizontal_size);
  pinch_zoom_scrollbar_horizontal_->SetPosition(
      gfx::PointF(0, vertical_size.height()));
  pinch_zoom_scrollbar_vertical_->SetBounds(vertical_size);
  pinch_zoom_scrollbar_vertical_->SetPosition(
      gfx::PointF(horizontal_size.width(), 0));
}

static scoped_refptr<ScrollbarLayer> CreatePinchZoomScrollbar(
    WebKit::WebScrollbar::Orientation orientation,
    LayerTreeHost* owner) {
  scoped_refptr<ScrollbarLayer> scrollbar_layer = ScrollbarLayer::Create(
      make_scoped_ptr(new PinchZoomScrollbar(orientation, owner)).
          PassAs<WebKit::WebScrollbar>(),
      scoped_ptr<ScrollbarThemePainter>(new PinchZoomScrollbarPainter).Pass(),
      scoped_ptr<WebKit::WebScrollbarThemeGeometry>(
          new PinchZoomScrollbarGeometry).Pass(),
      Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID);
  scrollbar_layer->SetIsDrawable(true);
  scrollbar_layer->SetOpacity(0.f);
  return scrollbar_layer;
}

void LayerTreeHost::CreateAndAddPinchZoomScrollbars() {
  bool needs_properties_updated = false;

  if (!pinch_zoom_scrollbar_horizontal_ || !pinch_zoom_scrollbar_vertical_) {
    pinch_zoom_scrollbar_horizontal_ =
        CreatePinchZoomScrollbar(WebKit::WebScrollbar::Horizontal, this);
    pinch_zoom_scrollbar_vertical_ =
        CreatePinchZoomScrollbar(WebKit::WebScrollbar::Vertical, this);
    needs_properties_updated = true;
  }

  DCHECK(pinch_zoom_scrollbar_horizontal_ && pinch_zoom_scrollbar_vertical_);

  if (!pinch_zoom_scrollbar_horizontal_->parent())
    root_layer_->AddChild(pinch_zoom_scrollbar_horizontal_);

  if (!pinch_zoom_scrollbar_vertical_->parent())
    root_layer_->AddChild(pinch_zoom_scrollbar_vertical_);

  if (needs_properties_updated)
    SetPinchZoomScrollbarsBoundsAndPosition();
}

void LayerTreeHost::WillCommit() {
  client_->WillCommit();

  if (settings().use_pinch_zoom_scrollbars)
    CreateAndAddPinchZoomScrollbars();
}

void LayerTreeHost::UpdateHudLayer() {
  if (debug_state_.ShowHudInfo()) {
    if (!hud_layer_)
      hud_layer_ = HeadsUpDisplayLayer::Create();

    if (root_layer_ && !hud_layer_->parent())
      root_layer_->AddChild(hud_layer_);
  } else if (hud_layer_) {
    hud_layer_->RemoveFromParent();
    hud_layer_ = NULL;
  }
}

void LayerTreeHost::CommitComplete() {
  client_->DidCommit();
#if defined(SBROWSER_HIDE_URLBAR)
  if(settings_.enable_hide_urlbar && urlbar_layer_ && BitmapSyncPending()){
      client_->HideUrlBarEventNotify(cc::URLBAR_BITMAP_ACK, 0, 0);
      VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBar Bitmap Ack, Notified App";
      SetBitmapSyncPending(false);
  }
#endif
}

scoped_ptr<OutputSurface> LayerTreeHost::CreateOutputSurface() {
  return client_->CreateOutputSurface();
}

scoped_ptr<InputHandlerClient> LayerTreeHost::CreateInputHandlerClient() {
  return client_->CreateInputHandlerClient();
}

scoped_ptr<LayerTreeHostImpl> LayerTreeHost::CreateLayerTreeHostImpl(
    LayerTreeHostImplClient* client) {
  DCHECK(proxy_->IsImplThread());
  scoped_ptr<LayerTreeHostImpl> host_impl =
      LayerTreeHostImpl::Create(settings_,
                                client,
                                proxy_.get(),
                                rendering_stats_instrumentation_.get());
  if (settings_.calculate_top_controls_position &&
      host_impl->top_controls_manager()) {
    top_controls_manager_weak_ptr_ =
        host_impl->top_controls_manager()->AsWeakPtr();
  }
  return host_impl.Pass();
}

void LayerTreeHost::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "LayerTreeHost::DidLoseOutputSurface");
  DCHECK(proxy_->IsMainThread());

  if (output_surface_lost_)
    return;

  num_failed_recreate_attempts_ = 0;
  output_surface_lost_ = true;
  SetNeedsCommit();
}

bool LayerTreeHost::CompositeAndReadback(void* pixels,
                                         gfx::Rect rect_in_device_viewport) {
  trigger_idle_updates_ = false;
  bool ret = proxy_->CompositeAndReadback(pixels, rect_in_device_viewport);
  trigger_idle_updates_ = true;
  return ret;
}

void LayerTreeHost::FinishAllRendering() {
  proxy_->FinishAllRendering();
}

void LayerTreeHost::SetDeferCommits(bool defer_commits) {
#if defined(SBROWSER_DEFER_COMMITS_ON_GESTURE)
  client_->SetDeferCommitsOnGesture(defer_commits);
#endif
  proxy_->SetDeferCommits(defer_commits);
}

void LayerTreeHost::DidDeferCommit() {}

void LayerTreeHost::SetNeedsDisplayOnAllLayers() {
  std::stack<Layer*> layer_stack;
  layer_stack.push(root_layer());
  while (!layer_stack.empty()) {
    Layer* current_layer = layer_stack.top();
    layer_stack.pop();
    current_layer->SetNeedsDisplay();
    for (unsigned int i = 0; i < current_layer->children().size(); i++) {
      layer_stack.push(current_layer->child_at(i));
    }
  }
}

void LayerTreeHost::CollectRenderingStats(RenderingStats* stats) const {
  CHECK(debug_state_.RecordRenderingStats());
  *stats = rendering_stats_instrumentation_->GetRenderingStats();
}

const RendererCapabilities& LayerTreeHost::GetRendererCapabilities() const {
  return proxy_->GetRendererCapabilities();
}

void LayerTreeHost::SetNeedsAnimate() {
  DCHECK(proxy_->HasImplThread());
  proxy_->SetNeedsAnimate();
}

void LayerTreeHost::SetNeedsCommit() {
  if (!prepaint_callback_.IsCancelled()) {
    TRACE_EVENT_INSTANT0("cc",
                         "LayerTreeHost::SetNeedsCommit::cancel prepaint",
                         TRACE_EVENT_SCOPE_THREAD);
    prepaint_callback_.Cancel();
  }
  proxy_->SetNeedsCommit();
}

void LayerTreeHost::SetNeedsFullTreeSync() {
  needs_full_tree_sync_ = true;
  SetNeedsCommit();
}

void LayerTreeHost::SetNeedsRedraw() {
  SetNeedsRedrawRect(gfx::Rect(device_viewport_size_));
}

void LayerTreeHost::SetNeedsRedrawRect(gfx::Rect damage_rect) {
  proxy_->SetNeedsRedraw(damage_rect);
  if (!proxy_->ImplThread())
    client_->ScheduleComposite();
}

bool LayerTreeHost::CommitRequested() const {
  return proxy_->CommitRequested();
}

void LayerTreeHost::SetAnimationEvents(scoped_ptr<AnimationEventsVector> events,
                                       base::Time wall_clock_time) {
  DCHECK(proxy_->IsMainThread());
  for (size_t event_index = 0; event_index < events->size(); ++event_index) {
    int event_layer_id = (*events)[event_index].layer_id;

    // Use the map of all controllers, not just active ones, since non-active
    // controllers may still receive events for impl-only animations.
    const AnimationRegistrar::AnimationControllerMap& animation_controllers =
        animation_registrar_->all_animation_controllers();
    AnimationRegistrar::AnimationControllerMap::const_iterator iter =
        animation_controllers.find(event_layer_id);
    if (iter != animation_controllers.end()) {
      switch ((*events)[event_index].type) {
        case AnimationEvent::Started:
          (*iter).second->NotifyAnimationStarted((*events)[event_index],
                                                 wall_clock_time.ToDoubleT());
          break;

        case AnimationEvent::Finished:
          (*iter).second->NotifyAnimationFinished((*events)[event_index],
                                                  wall_clock_time.ToDoubleT());
          break;

        case AnimationEvent::PropertyUpdate:
          (*iter).second->NotifyAnimationPropertyUpdate((*events)[event_index]);
          break;

        default:
          NOTREACHED();
      }
    }
  }
}

void LayerTreeHost::SetRootLayer(scoped_refptr<Layer> root_layer) {
#if defined(SBROWSER_HIDE_URLBAR)
if (!settings_.enable_hide_urlbar)
#endif
{
  if (root_layer_ == root_layer)
    return;

  if (root_layer_)
    root_layer_->SetLayerTreeHost(NULL);
  root_layer_ = root_layer;
  if (root_layer_)
    root_layer_->SetLayerTreeHost(this);

  if (hud_layer_)
    hud_layer_->RemoveFromParent();

  if (pinch_zoom_scrollbar_horizontal_)
    pinch_zoom_scrollbar_horizontal_->RemoveFromParent();

  if (pinch_zoom_scrollbar_vertical_)
    pinch_zoom_scrollbar_vertical_->RemoveFromParent();

#if defined(SBROWSER_SUPPORT_GLOW_EDGE_EFFECT)
  if (overscroll_effect_)
    overscroll_effect_->root_layer()->RemoveFromParent();
#endif
  SetNeedsFullTreeSync();
}
#if defined(SBROWSER_HIDE_URLBAR)
else {
  if (web_root_layer_ == root_layer)
    return;

  if (root_layer_)
    root_layer_->SetLayerTreeHost(NULL);

  if (!root_layer_){
    root_layer_ = Layer::Create();
      VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" NewRootLayer Created ";
  }
  if(root_layer_){
    if (web_root_layer_){
      web_root_layer_->RemoveFromParent();
      web_root_layer_ = NULL;
    }
    if(root_layer){
      web_root_layer_ = root_layer;
      root_layer_->AddChild(web_root_layer_);
      root_layer_->SetLayerTreeHost(this);
      web_root_layer_->SetIsDrawable(true);
      web_root_layer_->SetWebRoot(true);
      VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" WebRootLayer added as child of NewRootLayer ";
    }
  }
  if (hud_layer_)
    hud_layer_->RemoveFromParent();

  if (urlbar_layer_){
    urlbar_layer_->RemoveFromParent();
    urlbar_layer_ = NULL;
  }

  if (pinch_zoom_scrollbar_horizontal_)
    pinch_zoom_scrollbar_horizontal_->RemoveFromParent();

  if (pinch_zoom_scrollbar_vertical_)
    pinch_zoom_scrollbar_vertical_->RemoveFromParent();

#if defined(SBROWSER_SUPPORT_GLOW_EDGE_EFFECT)
  if (overscroll_effect_)
    overscroll_effect_->root_layer()->RemoveFromParent();
#endif

  SetNeedsFullTreeSync();
}
#endif
}

#if defined(SBROWSER_SUPPORT_GLOW_EDGE_EFFECT)
void LayerTreeHost::DidOverscroll(gfx::Vector2dF accumulated_root_overscroll_, 
                                gfx::Vector2dF current_fling_velocity_, bool hScroll, bool vScroll, gfx::SizeF size, float WebLayerY) {
  TRACE_EVENT0("cc", "LayerTreeHost::DidOverscroll");
  DCHECK(proxy_->IsMainThread());
  #if defined(SBROWSER_HIDE_URLBAR)
    //LOG(INFO) << __FUNCTION__ << "\t EdgeGlowEffectLog WebLayerY_previous:"<<WebLayerY_previous<<"\tWebLayerY:"<<WebLayerY;
    if(WebLayerY_previous != WebLayerY && settings_.enable_hide_urlbar) {
       if(WebLayerY_previous == 0){
           ReleaseEdgeGlow(LayerTreeHost::AXIS_XY);	
        }
        WebLayerY_previous = WebLayerY;       
    }
  #endif	

   if(!overscroll_effect_ && settings_.enable_edge_gloweffect) {
    overscroll_effect_ = content::OverscrollGlow::Create();
    set_hScrollbar_previous = false;
    set_vScrollbar_previous = false;
    WebLayerY_previous = -1;
    set_size_previous.set_width(0);
    set_size_previous.set_height(0);
    //LOG(INFO) << __FUNCTION__ << "\t EdgeGlowEffectLog overscroll_effect_ Layer is created ID:"<<overscroll_effect_->root_layer()->id();
  }
  if(settings_.enable_edge_gloweffect) {
      if((hScroll != set_hScrollbar_previous) ||
         (vScroll != set_vScrollbar_previous) || 
         (size.width() != set_size_previous.width()) ||
         (size.height() != set_size_previous.height())) {
          // size changed    
         set_hScrollbar_previous = hScroll;
         set_vScrollbar_previous = vScroll;
         set_size_previous.set_width(size.width());
         set_size_previous.set_height(size.height());
         UpdateContentSize(hScroll, vScroll, size);
        }
      if (overscroll_effect_) {
         AddEdgeGlowLayer();
		#if defined(SBROWSER_HIDE_URLBAR)
			if(settings_.enable_hide_urlbar){		 
				if(WebLayerY == 0)
					overscroll_effect_->root_layer()->SetPosition(gfx::PointF(0,0));
				else if(WebLayerY > 0) 
					overscroll_effect_->root_layer()->SetPosition(gfx::PointF(0,settings_.top_controls_height));
			}
		#endif	
        overscroll_effect_->OnOverscrolled(base::TimeTicks::Now(), accumulated_root_overscroll_, current_fling_velocity_);
        //LOG(INFO) << __FUNCTION__ << "\t EdgeGlowEffectLog accumulated_root_overscroll_:"<<accumulated_root_overscroll_.ToString()<<"\tcurrent_fling_velocity_:"<<current_fling_velocity_.ToString()<<"\tWebLayerY"<<WebLayerY;
        SetNeedsAnimate();
       }
   }
}

bool LayerTreeHost::IsGlowAnimationActive() {
    if (settings_.enable_edge_gloweffect && overscroll_effect_ && overscroll_effect_->IsActive())
	return true;
return false;	
}

void LayerTreeHost::ReleaseEdgeGlow(int axis) {
  TRACE_EVENT0("cc", "LayerTreeHost::ReleaseEdgeGlow");
  DCHECK(proxy_->IsMainThread());
  if(!overscroll_effect_)
      return;  
  if(axis == LayerTreeHost::AXIS_X) {

     overscroll_effect_->AxisRelease(LayerTreeHost::AXIS_X);

  }  else if(axis == LayerTreeHost::AXIS_Y) {

     overscroll_effect_->AxisRelease(LayerTreeHost::AXIS_Y);

  } else if(axis == LayerTreeHost::AXIS_XY) {
    if (overscroll_effect_->IsActive())
        overscroll_effect_->Finish();
    if(overscroll_effect_->root_layer()->parent()) {
 	 //LOG(INFO) << __FUNCTION__ << "\t EdgeGlowEffectLog Released EdgeGlow removing the rootlayer from parent ID :"<<overscroll_effect_->root_layer()->id();
	 overscroll_effect_->root_layer()->RemoveFromParent();
     }
  }
}

void LayerTreeHost::UpdateContentSize(bool hScroll, bool vScroll, gfx::SizeF size) {
  TRACE_EVENT0("cc", "LayerTreeHost::UpdateContentSize");
  DCHECK(proxy_->IsMainThread());
  if (overscroll_effect_) {
    overscroll_effect_->set_horizontal_overscroll_enabled(hScroll);
    overscroll_effect_->set_vertical_overscroll_enabled(vScroll);
    float dpi = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().device_scale_factor();
    size.SetSize(size.width()/dpi,size.height()/dpi);
    overscroll_effect_->set_size(size);
    //LOG(INFO) << __FUNCTION__ << "\t EdgeGlowEffectLog UpdateContentSize size:"<<size.ToString()<<"\thScroll:"<<hScroll<<"\tvScroll:"<<vScroll;
  }
}

void LayerTreeHost::AddEdgeGlowLayer() {
  if (settings_.enable_edge_gloweffect) {
    if (overscroll_effect_) {
            scoped_refptr<Layer> overscroll_root_layer_ = overscroll_effect_->root_layer();
            if (overscroll_root_layer_ && !overscroll_root_layer_->parent()) {
                if(root_layer_)
                    root_layer_->AddChild(overscroll_root_layer_);
           }
        }
    } 
}

#endif

void LayerTreeHost::SetDebugState(const LayerTreeDebugState& debug_state) {
  LayerTreeDebugState new_debug_state =
      LayerTreeDebugState::Unite(settings_.initial_debug_state, debug_state);

  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;

  debug_state_ = new_debug_state;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());

  SetNeedsCommit();
}

void LayerTreeHost::SetViewportSize(gfx::Size device_viewport_size) {
  if (device_viewport_size == device_viewport_size_)
    return;

  device_viewport_size_ = device_viewport_size;

  SetPinchZoomScrollbarsBoundsAndPosition();
  SetNeedsCommit();
}

#if defined(SBROWSER_CSS_ANIMATION_PERFORMANCE_IMPROVEMENT_3)
void LayerTreeHost::UpdateCompositingLayersIfNeeded()
{        
    client_->UpdateCompositingLayersIfNeeded();    
}
#endif

void LayerTreeHost::SetOverdrawBottomHeight(float overdraw_bottom_height) {
  if (overdraw_bottom_height_ == overdraw_bottom_height)
    return;

  overdraw_bottom_height_ = overdraw_bottom_height;
  SetNeedsCommit();
}

void LayerTreeHost::SetPageScaleFactorAndLimits(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  if (page_scale_factor == page_scale_factor_ &&
      min_page_scale_factor == min_page_scale_factor_ &&
      max_page_scale_factor == max_page_scale_factor_)
    return;

  page_scale_factor_ = page_scale_factor;
  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  SetNeedsCommit();
}

void LayerTreeHost::SetVisible(bool visible) {
  TRACE_EVENT1("cc", "LayerTreeHost::SetVisible","visible",visible);
  VLOG(0)<<"LayerTreeHost::SetVisible visible="<<visible;

  if (visible_ == visible)
    return;

  if(visible && pending_page_scale_animation_){
    TRACE_EVENT0("cc", "LayerTreeHost::SetVisible pending_page_scale_animation_ reset anim");
    VLOG(0)<<"LayerTreeHost::SetVisible pending_page_scale_animation_  reset anim";
    pending_page_scale_animation_.reset();
  }

  visible_ = visible;
  if (!visible)
    ReduceMemoryUsage();
  proxy_->SetVisible(visible);
}

void LayerTreeHost::SetLatencyInfo(const LatencyInfo& latency_info) {
  latency_info_.MergeWith(latency_info);
}

void LayerTreeHost::StartPageScaleAnimation(gfx::Vector2d target_offset,
                                            bool use_anchor,
                                            float scale,
                                            base::TimeDelta duration) {
  pending_page_scale_animation_.reset(new PendingPageScaleAnimation);
  pending_page_scale_animation_->target_offset = target_offset;
  pending_page_scale_animation_->use_anchor = use_anchor;
  pending_page_scale_animation_->scale = scale;
  pending_page_scale_animation_->duration = duration;

  SetNeedsCommit();
}

void LayerTreeHost::Composite(base::TimeTicks frame_begin_time) {
  if (!proxy_->HasImplThread())
    static_cast<SingleThreadProxy*>(proxy_.get())->CompositeImmediately(
        frame_begin_time);
  else
    SetNeedsCommit();
}

void LayerTreeHost::ScheduleComposite() {
  client_->ScheduleComposite();
}

bool LayerTreeHost::InitializeOutputSurfaceIfNeeded() {
  if (!output_surface_can_be_initialized_)
    return false;

  if (output_surface_lost_)
    proxy_->CreateAndInitializeOutputSurface();
  return !output_surface_lost_;
}

void LayerTreeHost::UpdateLayers(ResourceUpdateQueue* queue,
                                 size_t memory_allocation_limit_bytes) {
  DCHECK(!output_surface_lost_);

  if (!root_layer())
    return;

  if (device_viewport_size().IsEmpty())
    return;

  if (memory_allocation_limit_bytes) {
    contents_texture_manager_->SetMaxMemoryLimitBytes(
        memory_allocation_limit_bytes);
  }

  UpdateLayers(root_layer(), queue);
}

static Layer* FindFirstScrollableLayer(Layer* layer) {
  if (!layer)
    return NULL;

  if (layer->scrollable())
    return layer;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    Layer* found = FindFirstScrollableLayer(layer->children()[i].get());
    if (found)
      return found;
  }

  return NULL;
}

const Layer* LayerTreeHost::RootScrollLayer() const {
  return FindFirstScrollableLayer(root_layer_.get());
}

void LayerTreeHost::UpdateLayers(Layer* root_layer,
                                 ResourceUpdateQueue* queue) {
  TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers");

  LayerList update_list;
  {
    UpdateHudLayer();
#if defined(SBROWSER_HIDE_URLBAR)
    UpdateUrlBarLayer();
#endif
    Layer* root_scroll = FindFirstScrollableLayer(root_layer);

    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::CalcDrawProps");
    LayerTreeHostCommon::CalculateDrawProperties(
        root_layer,
        device_viewport_size(),
        device_scale_factor_,
        page_scale_factor_,
        root_scroll,
        GetRendererCapabilities().max_texture_size,
        settings_.can_use_lcd_text,
        &update_list);
  }

  // Reset partial texture update requests.
  partial_texture_update_requests_ = 0;

  bool need_more_updates = PaintLayerContents(update_list, queue);
  if (trigger_idle_updates_ && need_more_updates) {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::posting prepaint task");
    prepaint_callback_.Reset(base::Bind(&LayerTreeHost::TriggerPrepaint,
                                        base::Unretained(this)));
    static base::TimeDelta prepaint_delay =
        base::TimeDelta::FromMilliseconds(100);
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, prepaint_callback_.callback(), prepaint_delay);
  }

  for (size_t i = 0; i < update_list.size(); ++i)
    update_list[i]->ClearRenderSurface();
}

void LayerTreeHost::TriggerPrepaint() {
  prepaint_callback_.Cancel();
  TRACE_EVENT0("cc", "LayerTreeHost::TriggerPrepaint");
  SetNeedsCommit();
}

class LayerTreeHostReduceMemoryFunctor {
 public:
  void operator()(Layer* layer) {
    layer->ReduceMemoryUsage();
  }
};

void LayerTreeHost::ReduceMemoryUsage() {
  if (!root_layer())
    return;

  LayerTreeHostCommon::CallFunctionForSubtree<
      LayerTreeHostReduceMemoryFunctor, Layer>(root_layer());
}

void LayerTreeHost::SetPrioritiesForSurfaces(size_t surface_memory_bytes) {
  // Surfaces have a place holder for their memory since they are managed
  // independantly but should still be tracked and reduce other memory usage.
  surface_memory_placeholder_->SetTextureManager(
      contents_texture_manager_.get());
  surface_memory_placeholder_->set_request_priority(
      PriorityCalculator::RenderSurfacePriority());
  surface_memory_placeholder_->SetToSelfManagedMemoryPlaceholder(
      surface_memory_bytes);
}

void LayerTreeHost::SetPrioritiesForLayers(const LayerList& update_list) {
  // Use BackToFront since it's cheap and this isn't order-dependent.
  typedef LayerIterator<Layer,
                        LayerList,
                        RenderSurface,
                        LayerIteratorActions::BackToFront> LayerIteratorType;

  PriorityCalculator calculator;
  LayerIteratorType end = LayerIteratorType::End(&update_list);
  for (LayerIteratorType it = LayerIteratorType::Begin(&update_list);
       it != end;
       ++it) {
    if (it.represents_itself()) {
      it->SetTexturePriorities(calculator);
    } else if (it.represents_target_render_surface()) {
      if (it->mask_layer())
        it->mask_layer()->SetTexturePriorities(calculator);
      if (it->replica_layer() && it->replica_layer()->mask_layer())
        it->replica_layer()->mask_layer()->SetTexturePriorities(calculator);
    }
  }
}

void LayerTreeHost::PrioritizeTextures(
    const LayerList& render_surface_layer_list, OverdrawMetrics* metrics) {
  contents_texture_manager_->ClearPriorities();

  size_t memory_for_render_surfaces_metric =
      CalculateMemoryForRenderSurfaces(render_surface_layer_list);

  SetPrioritiesForLayers(render_surface_layer_list);
  SetPrioritiesForSurfaces(memory_for_render_surfaces_metric);

  metrics->DidUseContentsTextureMemoryBytes(
      contents_texture_manager_->MemoryAboveCutoffBytes());
  metrics->DidUseRenderSurfaceTextureMemoryBytes(
      memory_for_render_surfaces_metric);

  contents_texture_manager_->PrioritizeTextures();
}

size_t LayerTreeHost::CalculateMemoryForRenderSurfaces(
    const LayerList& update_list) {
  size_t readback_bytes = 0;
  size_t max_background_texture_bytes = 0;
  size_t contents_texture_bytes = 0;

  // Start iteration at 1 to skip the root surface as it does not have a texture
  // cost.
  for (size_t i = 1; i < update_list.size(); ++i) {
    Layer* render_surface_layer = update_list[i].get();
    RenderSurface* render_surface = render_surface_layer->render_surface();

    size_t bytes =
        Resource::MemorySizeBytes(render_surface->content_rect().size(),
                                  GL_RGBA);
    contents_texture_bytes += bytes;

    if (render_surface_layer->background_filters().isEmpty())
      continue;

    if (bytes > max_background_texture_bytes)
      max_background_texture_bytes = bytes;
    if (!readback_bytes) {
      readback_bytes = Resource::MemorySizeBytes(device_viewport_size_,
                                                 GL_RGBA);
    }
  }
  return readback_bytes + max_background_texture_bytes + contents_texture_bytes;
}

bool LayerTreeHost::PaintMasksForRenderSurface(Layer* render_surface_layer,
                                               ResourceUpdateQueue* queue,
                                               RenderingStats* stats) {
  // Note: Masks and replicas only exist for layers that own render surfaces. If
  // we reach this point in code, we already know that at least something will
  // be drawn into this render surface, so the mask and replica should be
  // painted.

  bool need_more_updates = false;
  Layer* mask_layer = render_surface_layer->mask_layer();
  if (mask_layer) {
    mask_layer->Update(queue, NULL, stats);
    need_more_updates |= mask_layer->NeedMoreUpdates();
  }

  Layer* replica_mask_layer =
      render_surface_layer->replica_layer() ?
      render_surface_layer->replica_layer()->mask_layer() : NULL;
  if (replica_mask_layer) {
    replica_mask_layer->Update(queue, NULL, stats);
    need_more_updates |= replica_mask_layer->NeedMoreUpdates();
  }
  return need_more_updates;
}

bool LayerTreeHost::PaintLayerContents(
    const LayerList& render_surface_layer_list, ResourceUpdateQueue* queue) {
  // Use FrontToBack to allow for testing occlusion and performing culling
  // during the tree walk.
  typedef LayerIterator<Layer,
                        LayerList,
                        RenderSurface,
                        LayerIteratorActions::FrontToBack> LayerIteratorType;

  bool need_more_updates = false;
  bool record_metrics_for_frame =
      settings_.show_overdraw_in_tracing &&
      base::debug::TraceLog::GetInstance() &&
      base::debug::TraceLog::GetInstance()->IsEnabled();
  OcclusionTracker occlusion_tracker(
      root_layer_->render_surface()->content_rect(), record_metrics_for_frame);
  occlusion_tracker.set_minimum_tracking_size(
      settings_.minimum_occlusion_tracking_size);

  PrioritizeTextures(render_surface_layer_list,
                     occlusion_tracker.overdraw_metrics());

  // TODO(egraether): Use RenderingStatsInstrumentation in Layer::update()
  RenderingStats stats;
  RenderingStats* stats_ptr =
      debug_state_.RecordRenderingStats() ? &stats : NULL;

  in_paint_layer_contents_ = true;

  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list);
  for (LayerIteratorType it =
           LayerIteratorType::Begin(&render_surface_layer_list);
       it != end;
       ++it) {
    bool prevent_occlusion =
        it.target_render_surface_layer()->HasRequestCopyCallback();
    occlusion_tracker.EnterLayer(it, prevent_occlusion);

    if (it.represents_target_render_surface()) {
      DCHECK(it->render_surface()->draw_opacity() ||
             it->render_surface()->draw_opacity_is_animating());
      need_more_updates |= PaintMasksForRenderSurface(*it, queue, stats_ptr);
    } else if (it.represents_itself()) {
      DCHECK(!it->paint_properties().bounds.IsEmpty());
      it->Update(queue, &occlusion_tracker, stats_ptr);
      need_more_updates |= it->NeedMoreUpdates();
    }

    occlusion_tracker.LeaveLayer(it);
  }

  in_paint_layer_contents_ = false;

  rendering_stats_instrumentation_->AddStats(stats);

  occlusion_tracker.overdraw_metrics()->RecordMetrics(this);

  return need_more_updates;
}

void LayerTreeHost::ApplyScrollAndScale(const ScrollAndScaleSet& info) {
  if (!root_layer_)
    return;

  Layer* root_scroll_layer = FindFirstScrollableLayer(root_layer_.get());
  gfx::Vector2d root_scroll_delta;

  for (size_t i = 0; i < info.scrolls.size(); ++i) {
    Layer* layer =
        LayerTreeHostCommon::FindLayerInSubtree(root_layer_.get(),
                                                info.scrolls[i].layer_id);
    if (!layer)
      continue;
    if (layer == root_scroll_layer)
      root_scroll_delta += info.scrolls[i].scroll_delta;
    else
      layer->SetScrollOffset(layer->scroll_offset() +
                             info.scrolls[i].scroll_delta);
  }
  if (!root_scroll_delta.IsZero() || info.page_scale_delta != 1.f)
    client_->ApplyScrollAndScale(root_scroll_delta, info.page_scale_delta);
}

void LayerTreeHost::StartRateLimiter(WebKit::WebGraphicsContext3D* context3d) {
  if (animating_)
    return;

  DCHECK(context3d);
  RateLimiterMap::iterator it = rate_limiters_.find(context3d);
  if (it != rate_limiters_.end()) {
    it->second->Start();
  } else {
    scoped_refptr<RateLimiter> rate_limiter =
        RateLimiter::Create(context3d, this, proxy_->MainThread());
    rate_limiters_[context3d] = rate_limiter;
    rate_limiter->Start();
  }
}

void LayerTreeHost::StopRateLimiter(WebKit::WebGraphicsContext3D* context3d) {
  RateLimiterMap::iterator it = rate_limiters_.find(context3d);
  if (it != rate_limiters_.end()) {
    it->second->Stop();
    rate_limiters_.erase(it);
  }
}

void LayerTreeHost::RateLimit() {
  // Force a no-op command on the compositor context, so that any ratelimiting
  // commands will wait for the compositing context, and therefore for the
  // SwapBuffers.
  proxy_->ForceSerializeOnSwapBuffers();
}

bool LayerTreeHost::RequestPartialTextureUpdate() {
  if (partial_texture_update_requests_ >= settings_.max_partial_texture_updates)
    return false;

  partial_texture_update_requests_++;
  return true;
}

void LayerTreeHost::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor ==  device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  SetNeedsCommit();
}

void LayerTreeHost::UpdateTopControlsState(bool enable_hiding,
                                           bool enable_showing,
                                           bool animate) {
  if (!settings_.calculate_top_controls_position)
    return;

  // Top controls are only used in threaded mode.
  proxy_->ImplThread()->PostTask(
      base::Bind(&TopControlsManager::UpdateTopControlsState,
                 top_controls_manager_weak_ptr_,
                 enable_hiding,
                 enable_showing,
                 animate));
}

bool LayerTreeHost::BlocksPendingCommit() const {
  if (!root_layer_)
    return false;
  return root_layer_->BlocksPendingCommitRecursive();
}

scoped_ptr<base::Value> LayerTreeHost::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->Set("proxy", proxy_->AsValue().release());
  return state.PassAs<base::Value>();
}
#if   defined(SBROWSER_PAUSE_ANIMATION_ON_SCROLL_ZOOM)
void LayerTreeHost::setGestureState(bool gestureActive)
{
    setGestureStateRecursive(root_layer_.get(), gestureActive);
}

void LayerTreeHost::setGestureStateRecursive(Layer* current, bool gestureActive)
{
    if(!current)
        return;

    current->setGestureState(gestureActive);

    for (size_t i = 0; i < current->children().size(); ++i) {
            setGestureStateRecursive(current->children()[i].get(), gestureActive);
    }
}
#endif

void LayerTreeHost::AnimateLayers(base::TimeTicks time) {
  if (!settings_.accelerated_animation_enabled ||
      animation_registrar_->active_animation_controllers().empty())
    return;

  TRACE_EVENT0("cc", "LayerTreeHostImpl::AnimateLayers");

  double monotonic_time = (time - base::TimeTicks()).InSecondsF();

  AnimationRegistrar::AnimationControllerMap copy =
      animation_registrar_->active_animation_controllers();
  for (AnimationRegistrar::AnimationControllerMap::iterator iter = copy.begin();
       iter != copy.end();
       ++iter) {
    (*iter).second->Animate(monotonic_time);
    bool start_ready_animations = true;
    (*iter).second->UpdateState(start_ready_animations, NULL);
  }
}

skia::RefPtr<SkPicture> LayerTreeHost::CapturePicture() {
  return proxy_->CapturePicture();
}
#if defined(SBROWSER_COMMIT_BLOCK_ONTOUCH)
void LayerTreeHost::setCommitBlock(bool blockState)
{
    DCHECK(proxy_->HasImplThread());
    proxy_->setCommitBlock(blockState);
}
#endif

#if defined(SBROWSER_FLING_END_NOTIFICATION_CANE)
void LayerTreeHost::FlingAnimationCompleted(bool page_end){
    client_->FlingAnimationCompleted(page_end);
}
#endif

#if defined(SBROWSER_HIDE_URLBAR)
void LayerTreeHost::UpdateUrlBarLayer() {
  if (settings_.enable_hide_urlbar) {
      if (root_layer_ && !urlbar_layer_){
          urlbar_layer_ = ImageLayer::Create();
          root_layer_->AddChild(urlbar_layer_);
          VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBarLayer added as child of NewRootLayer ";
    }
  }
}

void LayerTreeHost::SetUrlBarBitmap(SkBitmap* bitmap) {
  if(settings_.enable_hide_urlbar && bitmap){
    VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBarBitmap Set by App";
    if(urlbar_layer_){
      SkBitmap urlbar_bitmap;
      bitmap->copyTo(&(urlbar_bitmap), SkBitmap::kARGB_8888_Config);
      urlbar_layer_->SetBitmap(urlbar_bitmap);
      gfx::Size size(urlbar_bitmap.width()/device_scale_factor(), urlbar_bitmap.height()/device_scale_factor());
      urlbar_layer_->SetBounds(size);
      urlbar_layer_->SetIsDrawable(true);
      SetBitmapSyncPending(true);
      SetNeedsCommit();
    }
  }
}

void LayerTreeHost::SetUrlBarActive(bool active, bool override_allowed) {
    if(settings_.enable_hide_urlbar){
        urlbar_height_ = settings_.top_controls_height*device_scale_factor();
        VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" active: "<<active<<" override_allowed: "<<override_allowed;
      	proxy_->HideUrlBarCmdReq(active, override_allowed);
    }
}

void LayerTreeHost::DidHideUrlBarEvent(int event, int event_data1, int event_data2) {
   if(settings_.enable_hide_urlbar){
      client_->HideUrlBarEventNotify(event, event_data1, event_data2);
   }
}
#endif
#if defined(SBROWSER_ADJUST_WORKER_THREAD_PRIORITY)
void LayerTreeHost::SetHighPriorityToRasterWorker(bool flag) {     
    proxy_->SetHighPriorityToRasterWorker(flag); 
    
#if defined(SBROWSER_INCREASE_RASTER_THREAD) 
    if (flag == true){
        settings_.num_raster_threads = 4;
    } else if (settings_.num_raster_threads == 4) {
        settings_.num_raster_threads = 1;	
    }    
#endif // defined(SBROWSER_INCREASE_RASTER_THREAD) 
}
#endif
}  // namespace cc
