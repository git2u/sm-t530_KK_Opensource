// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include "base/debug/trace_event.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/input/pinch_zoom_scrollbar.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"
#if defined(SBROWSER_HIDE_URLBAR)
#include "base/logging.h"
#endif

namespace cc {

LayerTreeImpl::LayerTreeImpl(LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl),
      source_frame_number_(-1),
      hud_layer_(0),
#if defined(SBROWSER_HIDE_URLBAR)
      urlbar_layer_(0),
      web_root_layer_(0),
#endif
      root_scroll_layer_(NULL),
      currently_scrolling_layer_(NULL),
      root_layer_scroll_offset_delegate_(NULL),
      background_color_(0),
      has_transparent_background_(false),
      pinch_zoom_scrollbar_horizontal_layer_id_(Layer::INVALID_ID),
      pinch_zoom_scrollbar_vertical_layer_id_(Layer::INVALID_ID),
      page_scale_factor_(1),
      page_scale_delta_(1),
      sent_page_scale_delta_(1),
      min_page_scale_factor_(0),
      max_page_scale_factor_(0),
      scrolling_layer_id_from_previous_tree_(0),
      contents_textures_purged_(false),
      viewport_size_invalid_(false),
      needs_update_draw_properties_(true),
      needs_full_tree_sync_(true) {
}

LayerTreeImpl::~LayerTreeImpl() {
  // Need to explicitly clear the tree prior to destroying this so that
  // the LayerTreeImpl pointer is still valid in the LayerImpl dtor.
  root_layer_.reset();
}

static LayerImpl* FindRootScrollLayerRecursive(LayerImpl* layer) {
  if (!layer)
    return NULL;

  if (layer->scrollable())
    return layer;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    LayerImpl* found = FindRootScrollLayerRecursive(layer->children()[i]);
    if (found)
      return found;
  }

  return NULL;
}

void LayerTreeImpl::SetRootLayer(scoped_ptr<LayerImpl> layer) {
  if (root_scroll_layer_)
    root_scroll_layer_->SetScrollOffsetDelegate(NULL);
  root_layer_ = layer.Pass();
  currently_scrolling_layer_ = NULL;
  root_scroll_layer_ = NULL;

  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::FindRootScrollLayer() {
  root_scroll_layer_ = FindRootScrollLayerRecursive(root_layer_.get());

  if (root_scroll_layer_) {
    root_scroll_layer_->SetScrollOffsetDelegate(
        root_layer_scroll_offset_delegate_);
  }

  if (root_layer_ && scrolling_layer_id_from_previous_tree_) {
    currently_scrolling_layer_ = LayerTreeHostCommon::FindLayerInSubtree(
        root_layer_.get(),
        scrolling_layer_id_from_previous_tree_);
  }

  scrolling_layer_id_from_previous_tree_ = 0;
}

scoped_ptr<LayerImpl> LayerTreeImpl::DetachLayerTree() {
  // Clear all data structures that have direct references to the layer tree.
  scrolling_layer_id_from_previous_tree_ =
    currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0;
  if (root_scroll_layer_)
    root_scroll_layer_->SetScrollOffsetDelegate(NULL);
  root_scroll_layer_ = NULL;
  currently_scrolling_layer_ = NULL;

  render_surface_layer_list_.clear();
  set_needs_update_draw_properties();
  return root_layer_.Pass();
}

void LayerTreeImpl::PushPropertiesTo(LayerTreeImpl* target_tree) {
  target_tree->SetLatencyInfo(latency_info_);
  latency_info_.Clear();
  target_tree->SetPageScaleFactorAndLimits(
      page_scale_factor(), min_page_scale_factor(), max_page_scale_factor());
  target_tree->SetPageScaleDelta(
      target_tree->page_scale_delta() / target_tree->sent_page_scale_delta());
  target_tree->set_sent_page_scale_delta(1);

  // This should match the property synchronization in
  // LayerTreeHost::finishCommitOnImplThread().
  target_tree->set_source_frame_number(source_frame_number());
  target_tree->set_background_color(background_color());
  target_tree->set_has_transparent_background(has_transparent_background());

  if (ContentsTexturesPurged())
    target_tree->SetContentsTexturesPurged();
  else
    target_tree->ResetContentsTexturesPurged();

  if (ViewportSizeInvalid())
    target_tree->SetViewportSizeInvalid();
  else
    target_tree->ResetViewportSizeInvalid();

  if (hud_layer())
    target_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(
        LayerTreeHostCommon::FindLayerInSubtree(
            target_tree->root_layer(), hud_layer()->id())));
  else
    target_tree->set_hud_layer(NULL);

#if defined(SBROWSER_HIDE_URLBAR)
  if (settings().enable_hide_urlbar && urlbar_layer()){
    target_tree->set_urlbar_layer(static_cast<LayerImpl*>(
        LayerTreeHostCommon::FindLayerInSubtree(
            target_tree->root_layer(), urlbar_layer()->id())));
  }
  else{
    target_tree->set_urlbar_layer(NULL);
  }
#endif

  target_tree->SetPinchZoomHorizontalLayerId(
      pinch_zoom_scrollbar_horizontal_layer_id_);
  target_tree->SetPinchZoomVerticalLayerId(
      pinch_zoom_scrollbar_vertical_layer_id_);
}

LayerImpl* LayerTreeImpl::RootScrollLayer() const {
  DCHECK(IsActiveTree());
  return root_scroll_layer_;
}

LayerImpl* LayerTreeImpl::RootClipLayer() const {
  return root_scroll_layer_ ? root_scroll_layer_->parent() : NULL;
}

LayerImpl* LayerTreeImpl::CurrentlyScrollingLayer() const {
  DCHECK(IsActiveTree());
  return currently_scrolling_layer_;
}

void LayerTreeImpl::SetCurrentlyScrollingLayer(LayerImpl* layer) {
  if (currently_scrolling_layer_ == layer)
    return;

  if (currently_scrolling_layer_ &&
      currently_scrolling_layer_->scrollbar_animation_controller())
    currently_scrolling_layer_->scrollbar_animation_controller()->
        DidScrollGestureEnd(CurrentFrameTimeTicks());
  currently_scrolling_layer_ = layer;
  if (layer && layer->scrollbar_animation_controller())
    layer->scrollbar_animation_controller()->DidScrollGestureBegin();
}

void LayerTreeImpl::ClearCurrentlyScrollingLayer() {
  SetCurrentlyScrollingLayer(NULL);
  scrolling_layer_id_from_previous_tree_ = 0;
}

void LayerTreeImpl::SetPageScaleFactorAndLimits(float page_scale_factor,
    float min_page_scale_factor, float max_page_scale_factor) {
  if (!page_scale_factor)
    return;

  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  page_scale_factor_ = page_scale_factor;
}

void LayerTreeImpl::SetPageScaleDelta(float delta) {
  // Clamp to the current min/max limits.
  float total = page_scale_factor_ * delta;
  if (min_page_scale_factor_ && total < min_page_scale_factor_)
    delta = min_page_scale_factor_ / page_scale_factor_;
  else if (max_page_scale_factor_ && total > max_page_scale_factor_)
    delta = max_page_scale_factor_ / page_scale_factor_;

  if (delta == page_scale_delta_)
    return;

  page_scale_delta_ = delta;

  if (IsActiveTree()) {
    LayerTreeImpl* pending_tree = layer_tree_host_impl_->pending_tree();
    if (pending_tree) {
      DCHECK_EQ(1, pending_tree->sent_page_scale_delta());
      pending_tree->SetPageScaleDelta(
          page_scale_delta_ / sent_page_scale_delta_);
    }
  }

  UpdateMaxScrollOffset();
  set_needs_update_draw_properties();
}

gfx::SizeF LayerTreeImpl::ScrollableViewportSize() const {
  return gfx::ScaleSize(layer_tree_host_impl_->VisibleViewportSize(),
                        1.0f / total_page_scale_factor());
}

void LayerTreeImpl::UpdateMaxScrollOffset() {
  if (!root_scroll_layer_ || !root_scroll_layer_->children().size())
    return;

  gfx::Vector2dF max_scroll = gfx::Rect(ScrollableSize()).bottom_right() -
      gfx::RectF(ScrollableViewportSize()).bottom_right();

  // The viewport may be larger than the contents in some cases, such as
  // having a vertical scrollbar but no horizontal overflow.
  max_scroll.ClampToMin(gfx::Vector2dF());

  root_scroll_layer_->SetMaxScrollOffset(gfx::ToFlooredVector2d(max_scroll));
}

void LayerTreeImpl::UpdateSolidColorScrollbars() {
  DCHECK(settings().solid_color_scrollbars);

  LayerImpl* root_scroll = RootScrollLayer();
  DCHECK(root_scroll);
  DCHECK(IsActiveTree());

  gfx::RectF scrollable_viewport(
      gfx::PointAtOffsetFromOrigin(root_scroll->TotalScrollOffset()),
      ScrollableViewportSize());
  float vertical_adjust = 0.0f;
#if defined(SBROWSER_HIDE_URLBAR) && defined(SBROWSER_HIDE_URLBAR_WITHOUT_RESIZING)
  if (settings().enable_hide_urlbar && RootClipLayer()){
    vertical_adjust = -(layer_tree_host_impl_->UrlBarLayerY());
  }
#else
  if (RootClipLayer())
    vertical_adjust = layer_tree_host_impl_->VisibleViewportSize().height() -
                      RootClipLayer()->bounds().height();
#endif
  if (ScrollbarLayerImpl* horiz = root_scroll->horizontal_scrollbar_layer()) {
    horiz->set_vertical_adjust(vertical_adjust);
    horiz->SetViewportWithinScrollableArea(scrollable_viewport,
                                           ScrollableSize());
  }
  if (ScrollbarLayerImpl* vertical = root_scroll->vertical_scrollbar_layer()) {
    vertical->set_vertical_adjust(vertical_adjust);
    vertical->SetViewportWithinScrollableArea(scrollable_viewport,
                                              ScrollableSize());
  }
}

void LayerTreeImpl::UpdateDrawProperties() {
  if (IsActiveTree() && RootScrollLayer() && RootClipLayer())
    UpdateRootScrollLayerSizeDelta();

  if (settings().solid_color_scrollbars &&
      IsActiveTree() &&
      RootScrollLayer()) {
    UpdateSolidColorScrollbars();

    // The top controls manager is incompatible with the WebKit-created cliprect
    // because it can bring into view a larger amount of content when it
    // hides. It's safe to deactivate the clip rect if no non-overlay scrollbars
    // are present.
    if (RootClipLayer() && layer_tree_host_impl_->top_controls_manager())
      RootClipLayer()->SetMasksToBounds(false);
  }

  needs_update_draw_properties_ = false;
  render_surface_layer_list_.clear();

  // For max_texture_size.
  if (!layer_tree_host_impl_->renderer())
      return;

  if (!root_layer())
    return;

#if defined(SBROWSER_HIDE_URLBAR)
  if(settings().enable_hide_urlbar) {
    gfx::PointF origin ;
    if(urlbar_layer_) {
      origin.SetPoint(0, layer_tree_host_impl_->UrlBarLayerY());
      urlbar_layer_->SetPosition(origin);
      VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" UrlBarLayer SetPosition: "<<origin.ToString();
    }
    if(web_root_layer_) {
      gfx::PointF origin1;
      origin.SetPoint(0, layer_tree_host_impl_->WebLayerY());
      origin1.SetPoint(0, layer_tree_host_impl_->WebLayerY()/ total_page_scale_factor() - layer_tree_host_impl_->WebLayerY());
      web_root_layer_->SetPosition(origin);
      if(RootClipLayer()){
        RootClipLayer()->SetPosition(origin1);
      }
      VLOG(0)<<"HideUrlBar: "<<__FUNCTION__<<" WebRootLayer SetPosition: "<<origin.ToString();
    }
  }
#endif //SBROWSER_HIDE_URLBAR
  {
    TRACE_EVENT1("cc",
                 "LayerTreeImpl::UpdateDrawProperties",
                 "IsActive",
                 IsActiveTree());
    LayerTreeHostCommon::CalculateDrawProperties(
        root_layer(),
        device_viewport_size(),
        device_scale_factor(),
        total_page_scale_factor(),
        root_scroll_layer_,
        MaxTextureSize(),
        settings().can_use_lcd_text,
        &render_surface_layer_list_);
  }

  {
    TRACE_EVENT2("cc",
                 "LayerTreeImpl::UpdateTilePriorities",
                 "IsActive",
                 IsActiveTree(),
                 "SourceFrameNumber",
                 source_frame_number_);
    // LayerIterator is used here instead of CallFunctionForSubtree to only
    // UpdateTilePriorities on layers that will be visible (and thus have valid
    // draw properties) and not because any ordering is required.
    typedef LayerIterator<LayerImpl,
                          LayerImplList,
                          RenderSurfaceImpl,
                          LayerIteratorActions::FrontToBack> LayerIteratorType;
    LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list_);
    for (LayerIteratorType it =
             LayerIteratorType::Begin(&render_surface_layer_list_);
         it != end;
         ++it) {
      if (!it.represents_itself())
        continue;
      LayerImpl* layer = *it;

      layer->UpdateTilePriorities();
      if (layer->mask_layer())
        layer->mask_layer()->UpdateTilePriorities();
      if (layer->replica_layer() && layer->replica_layer()->mask_layer())
        layer->replica_layer()->mask_layer()->UpdateTilePriorities();
    }
  }

  DCHECK(!needs_update_draw_properties_) <<
      "CalcDrawProperties should not set_needs_update_draw_properties()";
}

static void ClearRenderSurfacesOnLayerImplRecursive(LayerImpl* current) {
  DCHECK(current);
  for (size_t i = 0; i < current->children().size(); ++i)
    ClearRenderSurfacesOnLayerImplRecursive(current->children()[i]);
  current->ClearRenderSurface();
}

void LayerTreeImpl::ClearRenderSurfaces() {
  ClearRenderSurfacesOnLayerImplRecursive(root_layer());
  render_surface_layer_list_.clear();
  set_needs_update_draw_properties();
}

const LayerImplList& LayerTreeImpl::RenderSurfaceLayerList() const {
  // If this assert triggers, then the list is dirty.
  DCHECK(!needs_update_draw_properties_);
  return render_surface_layer_list_;
}

gfx::Size LayerTreeImpl::ScrollableSize() const {
  if (!root_scroll_layer_ || root_scroll_layer_->children().empty())
    return gfx::Size();
  return root_scroll_layer_->children()[0]->bounds();
}

LayerImpl* LayerTreeImpl::LayerById(int id) {
  LayerIdMap::iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : NULL;
}

void LayerTreeImpl::RegisterLayer(LayerImpl* layer) {
  DCHECK(!LayerById(layer->id()));
  layer_id_map_[layer->id()] = layer;
}

void LayerTreeImpl::UnregisterLayer(LayerImpl* layer) {
  DCHECK(LayerById(layer->id()));
  layer_id_map_.erase(layer->id());
}

void LayerTreeImpl::PushPersistedState(LayerTreeImpl* pending_tree) {
  int id = currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0;
  pending_tree->SetCurrentlyScrollingLayer(
      LayerTreeHostCommon::FindLayerInSubtree(pending_tree->root_layer(), id));
}

static void DidBecomeActiveRecursive(LayerImpl* layer) {
  layer->DidBecomeActive();
  for (size_t i = 0; i < layer->children().size(); ++i)
    DidBecomeActiveRecursive(layer->children()[i]);
}

void LayerTreeImpl::DidBecomeActive() {
  if (root_layer())
    DidBecomeActiveRecursive(root_layer());
  FindRootScrollLayer();
  UpdateMaxScrollOffset();
  // Main thread scrolls do not get handled in LayerTreeHostImpl, so after
  // each commit (and after the root scroll layer has its max scroll offset
  // set), we need to update pinch zoom scrollbars.
  UpdatePinchZoomScrollbars();
}

bool LayerTreeImpl::ContentsTexturesPurged() const {
  return contents_textures_purged_;
}

void LayerTreeImpl::SetContentsTexturesPurged() {
  contents_textures_purged_ = true;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::ResetContentsTexturesPurged() {
  contents_textures_purged_ = false;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

bool LayerTreeImpl::ViewportSizeInvalid() const {
  return viewport_size_invalid_;
}

void LayerTreeImpl::SetViewportSizeInvalid() {
  viewport_size_invalid_ = true;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::ResetViewportSizeInvalid() {
  viewport_size_invalid_ = false;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

Proxy* LayerTreeImpl::proxy() const {
  return layer_tree_host_impl_->proxy();
}

const LayerTreeSettings& LayerTreeImpl::settings() const {
  return layer_tree_host_impl_->settings();
}

const RendererCapabilities& LayerTreeImpl::GetRendererCapabilities() const {
  return layer_tree_host_impl_->GetRendererCapabilities();
}

OutputSurface* LayerTreeImpl::output_surface() const {
  return layer_tree_host_impl_->output_surface();
}

ResourceProvider* LayerTreeImpl::resource_provider() const {
  return layer_tree_host_impl_->resource_provider();
}

TileManager* LayerTreeImpl::tile_manager() const {
  return layer_tree_host_impl_->tile_manager();
}

FrameRateCounter* LayerTreeImpl::frame_rate_counter() const {
  return layer_tree_host_impl_->fps_counter();
}

PaintTimeCounter* LayerTreeImpl::paint_time_counter() const {
  return layer_tree_host_impl_->paint_time_counter();
}

MemoryHistory* LayerTreeImpl::memory_history() const {
  return layer_tree_host_impl_->memory_history();
}

bool LayerTreeImpl::IsActiveTree() const {
  return layer_tree_host_impl_->active_tree() == this;
}

bool LayerTreeImpl::IsPendingTree() const {
  return layer_tree_host_impl_->pending_tree() == this;
}

bool LayerTreeImpl::IsRecycleTree() const {
  return layer_tree_host_impl_->recycle_tree() == this;
}

LayerImpl* LayerTreeImpl::FindActiveTreeLayerById(int id) {
  LayerTreeImpl* tree = layer_tree_host_impl_->active_tree();
  if (!tree)
    return NULL;
  return tree->LayerById(id);
}

LayerImpl* LayerTreeImpl::FindPendingTreeLayerById(int id) {
  LayerTreeImpl* tree = layer_tree_host_impl_->pending_tree();
  if (!tree)
    return NULL;
  return tree->LayerById(id);
}

int LayerTreeImpl::MaxTextureSize() const {
  return layer_tree_host_impl_->GetRendererCapabilities().max_texture_size;
}

bool LayerTreeImpl::PinchGestureActive() const {
  return layer_tree_host_impl_->pinch_gesture_active();
}

bool LayerTreeImpl::DoubleTapActive() const {
  return layer_tree_host_impl_->page_scale_animation_active();
}

base::TimeTicks LayerTreeImpl::CurrentFrameTimeTicks() const {
  return layer_tree_host_impl_->CurrentFrameTimeTicks();
}

base::Time LayerTreeImpl::CurrentFrameTime() const {
  return layer_tree_host_impl_->CurrentFrameTime();
}

void LayerTreeImpl::SetNeedsCommit() {
  layer_tree_host_impl_->SetNeedsCommit();
}

void LayerTreeImpl::SetNeedsRedraw() {
  layer_tree_host_impl_->SetNeedsRedraw();
}

const LayerTreeDebugState& LayerTreeImpl::debug_state() const {
  return layer_tree_host_impl_->debug_state();
}

float LayerTreeImpl::device_scale_factor() const {
  return layer_tree_host_impl_->device_scale_factor();
}

gfx::Size LayerTreeImpl::device_viewport_size() const {
  return layer_tree_host_impl_->device_viewport_size();
}

std::string LayerTreeImpl::layer_tree_as_text() const {
  return layer_tree_host_impl_->LayerTreeAsText();
}

DebugRectHistory* LayerTreeImpl::debug_rect_history() const {
  return layer_tree_host_impl_->debug_rect_history();
}

AnimationRegistrar* LayerTreeImpl::animationRegistrar() const {
  return layer_tree_host_impl_->animation_registrar();
}

scoped_ptr<base::Value> LayerTreeImpl::AsValue() const {
  scoped_ptr<base::ListValue> state(new base::ListValue());
  typedef LayerIterator<LayerImpl,
                        LayerImplList,
                        RenderSurfaceImpl,
                        LayerIteratorActions::BackToFront> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list_);
  for (LayerIteratorType it = LayerIteratorType::Begin(
           &render_surface_layer_list_); it != end; ++it) {
    if (!it.represents_itself())
      continue;
    state->Append((*it)->AsValue().release());
  }
  return state.PassAs<base::Value>();
}

void LayerTreeImpl::DidBeginScroll() {
  if (HasPinchZoomScrollbars())
    FadeInPinchZoomScrollbars();
}

void LayerTreeImpl::DidUpdateScroll() {
  if (HasPinchZoomScrollbars())
    UpdatePinchZoomScrollbars();
}

void LayerTreeImpl::DidEndScroll() {
  if (HasPinchZoomScrollbars())
    FadeOutPinchZoomScrollbars();
}

void LayerTreeImpl::SetRootLayerScrollOffsetDelegate(
      LayerScrollOffsetDelegate* root_layer_scroll_offset_delegate) {
  root_layer_scroll_offset_delegate_ = root_layer_scroll_offset_delegate;
  if (root_scroll_layer_) {
    root_scroll_layer_->SetScrollOffsetDelegate(
        root_layer_scroll_offset_delegate_);
  }
}

void LayerTreeImpl::SetPinchZoomHorizontalLayerId(int layer_id) {
  pinch_zoom_scrollbar_horizontal_layer_id_ = layer_id;
}

ScrollbarLayerImpl* LayerTreeImpl::PinchZoomScrollbarHorizontal() {
  return static_cast<ScrollbarLayerImpl*>(LayerById(
    pinch_zoom_scrollbar_horizontal_layer_id_));
}

void LayerTreeImpl::SetPinchZoomVerticalLayerId(int layer_id) {
  pinch_zoom_scrollbar_vertical_layer_id_ = layer_id;
}

ScrollbarLayerImpl* LayerTreeImpl::PinchZoomScrollbarVertical() {
  return static_cast<ScrollbarLayerImpl*>(LayerById(
    pinch_zoom_scrollbar_vertical_layer_id_));
}

void LayerTreeImpl::UpdatePinchZoomScrollbars() {
  LayerImpl* root_scroll_layer = RootScrollLayer();
  if (!root_scroll_layer)
    return;

  if (ScrollbarLayerImpl* scrollbar = PinchZoomScrollbarHorizontal()) {
    scrollbar->SetCurrentPos(root_scroll_layer->scroll_offset().x());
    scrollbar->SetTotalSize(root_scroll_layer->bounds().width());
    scrollbar->SetMaximum(root_scroll_layer->max_scroll_offset().x());
  }
  if (ScrollbarLayerImpl* scrollbar = PinchZoomScrollbarVertical()) {
    scrollbar->SetCurrentPos(root_scroll_layer->scroll_offset().y());
    scrollbar->SetTotalSize(root_scroll_layer->bounds().height());
    scrollbar->SetMaximum(root_scroll_layer->max_scroll_offset().y());
  }
}

static scoped_ptr<Animation> MakePinchZoomFadeAnimation(
    float start_opacity, float end_opacity) {
  scoped_ptr<KeyframedFloatAnimationCurve> curve =
    KeyframedFloatAnimationCurve::Create();
  curve->AddKeyframe(FloatKeyframe::Create(
    0, start_opacity, EaseInTimingFunction::Create()));
  curve->AddKeyframe(FloatKeyframe::Create(
    PinchZoomScrollbar::kFadeDurationInSeconds, end_opacity,
    EaseInTimingFunction::Create()));

  scoped_ptr<Animation> animation = Animation::Create(
      curve.PassAs<AnimationCurve>(), AnimationIdProvider::NextAnimationId(),
      0, Animation::Opacity);
  animation->set_is_impl_only(true);

  return animation.Pass();
}

static void StartFadeInAnimation(ScrollbarLayerImpl* layer) {
  DCHECK(layer);
  float start_opacity = layer->opacity();
  LayerAnimationController* controller = layer->layer_animation_controller();
  // TODO(wjmaclean) It shouldn't be necessary to manually remove the old
  // animation.
  if (Animation* animation = controller->GetAnimation(Animation::Opacity))
    controller->RemoveAnimation(animation->id());
  controller->AddAnimation(MakePinchZoomFadeAnimation(start_opacity,
    PinchZoomScrollbar::kDefaultOpacity));
}

void LayerTreeImpl::FadeInPinchZoomScrollbars() {
  if (!HasPinchZoomScrollbars() || page_scale_factor_ == 1)
    return;

  StartFadeInAnimation(PinchZoomScrollbarHorizontal());
  StartFadeInAnimation(PinchZoomScrollbarVertical());
  SetNeedsRedraw();
}

static void StartFadeOutAnimation(LayerImpl* layer) {
  float opacity = layer->opacity();
  if (!opacity)
    return;

  LayerAnimationController* controller = layer->layer_animation_controller();
  // TODO(wjmaclean) It shouldn't be necessary to manually remove the old
  // animation.
  if (Animation* animation = controller->GetAnimation(Animation::Opacity))
    controller->RemoveAnimation(animation->id());
  controller->AddAnimation(MakePinchZoomFadeAnimation(opacity, 0));
}

void LayerTreeImpl::FadeOutPinchZoomScrollbars() {
  if (!HasPinchZoomScrollbars())
    return;

  StartFadeOutAnimation(PinchZoomScrollbarHorizontal());
  StartFadeOutAnimation(PinchZoomScrollbarVertical());
  SetNeedsRedraw();
}

bool LayerTreeImpl::HasPinchZoomScrollbars() const {
  return pinch_zoom_scrollbar_horizontal_layer_id_ != Layer::INVALID_ID &&
         pinch_zoom_scrollbar_vertical_layer_id_ != Layer::INVALID_ID;
}

void LayerTreeImpl::UpdateRootScrollLayerSizeDelta() {
  LayerImpl* root_scroll = RootScrollLayer();
  LayerImpl* root_clip = RootClipLayer();
  DCHECK(root_scroll);
  DCHECK(root_clip);
  DCHECK(IsActiveTree());

#if !defined(SBROWSER_HIDE_URLBAR) || !defined(SBROWSER_HIDE_URLBAR_WITHOUT_RESIZING)
  gfx::Vector2dF scrollable_viewport_size =
      gfx::RectF(ScrollableViewportSize()).bottom_right() - gfx::PointF();
#endif

  gfx::Vector2dF original_viewport_size =
      gfx::RectF(root_clip->bounds()).bottom_right() -
      gfx::PointF();
  original_viewport_size.Scale(1 / page_scale_factor());

#if defined(SBROWSER_HIDE_URLBAR) && defined(SBROWSER_HIDE_URLBAR_WITHOUT_RESIZING)
  if(settings().enable_hide_urlbar){
    gfx::Vector2dF urlbar_layer_y;
    urlbar_layer_y.set_y(-(layer_tree_host_impl_->UrlBarLayerY()));
    root_scroll->SetFixedContainerSizeDelta(urlbar_layer_y);
  }
#else
  root_scroll->SetFixedContainerSizeDelta(
	scrollable_viewport_size - original_viewport_size);
#endif
}

void LayerTreeImpl::SetLatencyInfo(const LatencyInfo& latency_info) {
  latency_info_.MergeWith(latency_info);
}

const LatencyInfo& LayerTreeImpl::GetLatencyInfo() {
  return latency_info_;
}

void LayerTreeImpl::ClearLatencyInfo() {
  latency_info_.Clear();
}

void LayerTreeImpl::WillModifyTilePriorities() {
#if defined(SBROWSER_SKIP_MODIFYING_TILE_PRIORITY)
	if(!layer_tree_host_impl_->SkipModifyingTilePriority())
#endif
	layer_tree_host_impl_->tile_manager()->WillModifyTilePriorities();
}
 
#if defined(SBROWSER_HIDE_URLBAR)
bool LayerTreeImpl::urlbar_active() {
   if(layer_tree_host_impl_){
      return (layer_tree_host_impl_->urlbar_active());
   }
   return false;
}

void LayerTreeImpl::SetUrlBarUpdate(bool needsUpdate)
{
   if(layer_tree_host_impl_)
      layer_tree_host_impl_->SetUrlBarUpdate(needsUpdate);
}

bool LayerTreeImpl::pending_urlbar_update()
{
   if(layer_tree_host_impl_){
      return (layer_tree_host_impl_->pending_urlbar_update());
   }
   return false;
}

void LayerTreeImpl::SendUrlBarActiveAckIfNeeded() {
   if(layer_tree_host_impl_)
      layer_tree_host_impl_->SendUrlBarActiveAckIfNeeded();
}

#endif

}  // namespace cc
