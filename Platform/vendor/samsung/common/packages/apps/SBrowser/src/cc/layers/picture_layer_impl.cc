// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include <algorithm>
#include <limits>

#include "base/time.h"
#include "cc/base/math_util.h"
#include "cc/base/util.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

#if defined(SBROWSER_NATIVE_TEXTURE_UPLOAD)
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#endif

namespace {
const float kMaxScaleRatioDuringPinch = 16.0f;

// When creating a new tiling during pinch, snap to an existing
// tiling's scale if the desired scale is within this ratio.
const float kSnapToExistingTilingRatio = 0.2f;
}

namespace cc {

PictureLayerImpl::PictureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      pile_(PicturePileImpl::Create(true)),
      last_content_scale_(0),
      is_mask_(false),
      ideal_page_scale_(0.f),
      ideal_device_scale_(0.f),
      ideal_source_scale_(0.f),
      ideal_contents_scale_(0.f),
      raster_page_scale_(0.f),
      raster_device_scale_(0.f),
      raster_source_scale_(0.f),
      raster_contents_scale_(0.f),
      low_res_raster_contents_scale_(0.f),
      raster_source_scale_was_animating_(false),
      is_using_lcd_text_(tree_impl->settings().can_use_lcd_text) {
#if defined(SBROWSER_NATIVE_TEXTURE_UPLOAD)
      PictureLayerImpl::use_native_buffer_textures_ = CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableNativeBufferTextures);
#endif
}

#if defined(SBROWSER_NATIVE_TEXTURE_UPLOAD)
bool PictureLayerImpl::use_native_buffer_textures_;
#endif

PictureLayerImpl::~PictureLayerImpl() {
}

const char* PictureLayerImpl::LayerTypeAsString() const {
  return "PictureLayer";
}

scoped_ptr<LayerImpl> PictureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void PictureLayerImpl::CreateTilingSet() {
  DCHECK(layer_tree_impl()->IsPendingTree());
  DCHECK(!tilings_);
  tilings_.reset(new PictureLayerTilingSet(this, bounds()));
}

void PictureLayerImpl::TransferTilingSet(
    scoped_ptr<PictureLayerTilingSet> tilings) {
  DCHECK(layer_tree_impl()->IsActiveTree());
  tilings->SetClient(this);
  tilings_ = tilings.Pass();
}

void PictureLayerImpl::PushPropertiesTo(LayerImpl* base_layer) {
  LayerImpl::PushPropertiesTo(base_layer);

  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  layer_impl->SetIsMask(is_mask_);
  layer_impl->TransferTilingSet(tilings_.Pass());
  layer_impl->pile_ = pile_;
  pile_ = PicturePileImpl::Create(is_using_lcd_text_);

  layer_impl->raster_page_scale_ = raster_page_scale_;
  layer_impl->raster_device_scale_ = raster_device_scale_;
  layer_impl->raster_source_scale_ = raster_source_scale_;
  layer_impl->raster_contents_scale_ = raster_contents_scale_;
  layer_impl->low_res_raster_contents_scale_ = low_res_raster_contents_scale_;
  layer_impl->is_using_lcd_text_ = is_using_lcd_text_;
}

void PictureLayerImpl::AppendQuads(QuadSink* quad_sink,
                                   AppendQuadsData* append_quads_data) {
  gfx::Rect rect(visible_content_rect());
  gfx::Rect content_rect(content_bounds());

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  bool clipped = false;
  gfx::QuadF target_quad = MathUtil::MapQuad(
      draw_transform(),
      gfx::QuadF(rect),
      &clipped);
  if (ShowDebugBorders()) {
    for (PictureLayerTilingSet::CoverageIterator iter(
        tilings_.get(), contents_scale_x(), rect, ideal_contents_scale_);
         iter;
         ++iter) {
      SkColor color;
      float width;
      if (*iter && iter->drawing_info().IsReadyToDraw()) {
        ManagedTileState::DrawingInfo::Mode mode = iter->drawing_info().mode();
        if (mode == ManagedTileState::DrawingInfo::SOLID_COLOR_MODE) {
          color = DebugColors::SolidColorTileBorderColor();
          width = DebugColors::SolidColorTileBorderWidth(layer_tree_impl());
        } else if (mode == ManagedTileState::DrawingInfo::PICTURE_PILE_MODE) {
          color = DebugColors::PictureTileBorderColor();
          width = DebugColors::PictureTileBorderWidth(layer_tree_impl());
        } else if (iter->priority(ACTIVE_TREE).resolution == HIGH_RESOLUTION) {
          color = DebugColors::HighResTileBorderColor();
          width = DebugColors::HighResTileBorderWidth(layer_tree_impl());
        } else if (iter->priority(ACTIVE_TREE).resolution == LOW_RESOLUTION) {
          color = DebugColors::LowResTileBorderColor();
          width = DebugColors::LowResTileBorderWidth(layer_tree_impl());
        } else if (iter->contents_scale() > contents_scale_x()) {
          color = DebugColors::ExtraHighResTileBorderColor();
          width = DebugColors::ExtraHighResTileBorderWidth(layer_tree_impl());
        } else {
          color = DebugColors::ExtraLowResTileBorderColor();
          width = DebugColors::ExtraLowResTileBorderWidth(layer_tree_impl());
        }
      } else {
        color = DebugColors::MissingTileBorderColor();
        width = DebugColors::MissingTileBorderWidth(layer_tree_impl());
      }

      scoped_ptr<DebugBorderDrawQuad> debug_border_quad =
          DebugBorderDrawQuad::Create();
      gfx::Rect geometry_rect = iter.geometry_rect();
      debug_border_quad->SetNew(shared_quad_state, geometry_rect, color, width);
      quad_sink->Append(debug_border_quad.PassAs<DrawQuad>(),
                        append_quads_data);
    }
  }

  // Keep track of the tilings that were used so that tilings that are
  // unused can be considered for removal.
  std::vector<PictureLayerTiling*> seen_tilings;

  for (PictureLayerTilingSet::CoverageIterator iter(
      tilings_.get(), contents_scale_x(), rect, ideal_contents_scale_);
       iter;
       ++iter) {
    gfx::Rect geometry_rect = iter.geometry_rect();
    if (!*iter || !iter->drawing_info().IsReadyToDraw()) {
      if (DrawCheckerboardForMissingTiles()) {
        // TODO(enne): Figure out how to show debug "invalidated checker" color
        scoped_ptr<CheckerboardDrawQuad> quad = CheckerboardDrawQuad::Create();
        SkColor color = DebugColors::DefaultCheckerboardColor();
        quad->SetNew(shared_quad_state, geometry_rect, color);
        if (quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data))
          append_quads_data->num_missing_tiles++;
      } else {
        SkColor color = background_color();
        // TODO(wangxianzhu): Change the next |if| condition once we support
        // finer-grain opaqueness. Ensure with the following DCHECK.
        DCHECK(contents_opaque() || VisibleContentOpaqueRegion().IsEmpty());
        if (SkColorGetA(color) != 255 && contents_opaque()) {
          // If content is opaque, the occlusion tracker expects this layer to
          // cover the background, so needs an opaque color.
          for (const LayerImpl* layer = parent(); layer;
               layer = layer->parent()) {
            color = layer->background_color();
            if (SkColorGetA(color) == 255)
              break;
          }
          if (SkColorGetA(color) != 255)
            color = layer_tree_impl()->background_color();
          DCHECK_EQ(SkColorGetA(color), 255u);
        }
        scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
        quad->SetNew(shared_quad_state, geometry_rect, color, false);
        if (quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data))
          append_quads_data->num_missing_tiles++;
      }

      append_quads_data->had_incomplete_tile = true;
      continue;
    }

    const ManagedTileState::DrawingInfo& drawing_info = iter->drawing_info();
    switch (drawing_info.mode()) {
      case ManagedTileState::DrawingInfo::RESOURCE_MODE: {
        gfx::RectF texture_rect = iter.texture_rect();
        gfx::Rect opaque_rect = iter->opaque_rect();
        opaque_rect.Intersect(content_rect);

        if (iter->contents_scale() != ideal_contents_scale_)
          append_quads_data->had_incomplete_tile = true;

        scoped_ptr<TileDrawQuad> quad = TileDrawQuad::Create();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     opaque_rect,
                     drawing_info.get_resource_id(),
                     texture_rect,
                     iter.texture_size(),
                     drawing_info.contents_swizzled());
        quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
        break;
      }
      case ManagedTileState::DrawingInfo::PICTURE_PILE_MODE: {
        gfx::RectF texture_rect = iter.texture_rect();
        gfx::Rect opaque_rect = iter->opaque_rect();
        opaque_rect.Intersect(content_rect);

        scoped_ptr<PictureDrawQuad> quad = PictureDrawQuad::Create();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     opaque_rect,
                     texture_rect,
                     iter.texture_size(),
                     drawing_info.contents_swizzled(),
                     iter->content_rect(),
                     iter->contents_scale(),
                     pile_);
        quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
        break;
      }
      case ManagedTileState::DrawingInfo::SOLID_COLOR_MODE: {
        scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
        quad->SetNew(shared_quad_state,
                     geometry_rect,
                     drawing_info.get_solid_color(),
                     false);
        quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
        break;
      }
      default:
        NOTREACHED();
    }

    if (!seen_tilings.size() || seen_tilings.back() != iter.CurrentTiling())
      seen_tilings.push_back(iter.CurrentTiling());
  }

  // Aggressively remove any tilings that are not seen to save memory. Note
  // that this is at the expense of doing cause more frequent re-painting. A
  // better scheme would be to maintain a tighter visible_content_rect for the
  // finer tilings.
  CleanUpTilingsOnActiveLayer(seen_tilings);
}

void PictureLayerImpl::DumpLayerProperties(std::string*, int indent) const {
  // TODO(enne): implement me
}

void PictureLayerImpl::UpdateTilePriorities() {
  if (!tilings_->num_tilings())
    return;

  double current_frame_time_in_seconds =
      (layer_tree_impl()->CurrentFrameTimeTicks() -
       base::TimeTicks()).InSecondsF();

  bool tiling_needs_update = false;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    if (tilings_->tiling_at(i)->NeedsUpdateForFrameAtTime(
            current_frame_time_in_seconds)) {
      tiling_needs_update = true;
      break;
    }
  }
  if (!tiling_needs_update)
    return;

  // At this point, tile priorities are going to be modified.
  layer_tree_impl()->WillModifyTilePriorities();

  UpdateLCDTextStatus();

  gfx::Transform current_screen_space_transform = screen_space_transform();

  gfx::Rect viewport_in_content_space;
  gfx::Transform screen_to_layer(gfx::Transform::kSkipInitialization);
  if (screen_space_transform().GetInverse(&screen_to_layer)) {
    gfx::Rect device_viewport(layer_tree_impl()->device_viewport_size());
    viewport_in_content_space = gfx::ToEnclosingRect(
        MathUtil::ProjectClippedRect(screen_to_layer, device_viewport));
  }

  WhichTree tree =
      layer_tree_impl()->IsActiveTree() ? ACTIVE_TREE : PENDING_TREE;
  bool store_screen_space_quads_on_tiles =
      layer_tree_impl()->debug_state().trace_all_rendered_frames;
  size_t max_tiles_for_interest_area =
      layer_tree_impl()->settings().max_tiles_for_interest_area;
  tilings_->UpdateTilePriorities(
      tree,
      layer_tree_impl()->device_viewport_size(),
      viewport_in_content_space,
      visible_content_rect(),
      last_bounds_,
      bounds(),
      last_content_scale_,
      contents_scale_x(),
      last_screen_space_transform_,
      current_screen_space_transform,
      current_frame_time_in_seconds,
      store_screen_space_quads_on_tiles,
      max_tiles_for_interest_area);

  if (layer_tree_impl()->IsPendingTree())
  	MarkVisibleResourcesAsRequired();

  last_screen_space_transform_ = current_screen_space_transform;
  last_bounds_ = bounds();
  last_content_scale_ = contents_scale_x();
}

void PictureLayerImpl::DidBecomeActive() {
  LayerImpl::DidBecomeActive();
  tilings_->DidBecomeActive();
}

void PictureLayerImpl::DidLoseOutputSurface() {
  if (tilings_)
    tilings_->RemoveAllTilings();

  ResetRasterScale();
}

void PictureLayerImpl::CalculateContentsScale(
    float ideal_contents_scale,
    bool animating_transform_to_screen,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* content_bounds) {
  if (!DrawsContent()) {
    DCHECK(!tilings_->num_tilings());
    return;
  }

  float min_contents_scale = MinimumContentsScale();
  float min_page_scale = layer_tree_impl()->min_page_scale_factor();
  float min_device_scale = 1.f;
  float min_source_scale =
      min_contents_scale / min_page_scale / min_device_scale;

  float ideal_page_scale = layer_tree_impl()->total_page_scale_factor();
  float ideal_device_scale = layer_tree_impl()->device_scale_factor();
  float ideal_source_scale =
      ideal_contents_scale / ideal_page_scale / ideal_device_scale;

  ideal_contents_scale_ = std::max(ideal_contents_scale, min_contents_scale);
  ideal_page_scale_ = ideal_page_scale;
  ideal_device_scale_ = ideal_device_scale;
  ideal_source_scale_ = std::max(ideal_source_scale, min_source_scale);

  ManageTilings(animating_transform_to_screen);

  // The content scale and bounds for a PictureLayerImpl is somewhat fictitious.
  // There are (usually) several tilings at different scales.  However, the
  // content bounds is the (integer!) space in which quads are generated.
  // In order to guarantee that we can fill this integer space with any set of
  // tilings (and then map back to floating point texture coordinates), the
  // contents scale must be at least as large as the largest of the tilings.
  float max_contents_scale = min_contents_scale;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings_->tiling_at(i);
    max_contents_scale = std::max(max_contents_scale, tiling->contents_scale());
  }

  *contents_scale_x = max_contents_scale;
  *contents_scale_y = max_contents_scale;
  *content_bounds = gfx::ToCeiledSize(
      gfx::ScaleSize(bounds(), max_contents_scale, max_contents_scale));
}

skia::RefPtr<SkPicture> PictureLayerImpl::GetPicture() {
  return pile_->GetFlattenedPicture();
}

scoped_refptr<Tile> PictureLayerImpl::CreateTile(PictureLayerTiling* tiling,
                                                 gfx::Rect content_rect) {
  if (!pile_->CanRaster(tiling->contents_scale(), content_rect))
    return scoped_refptr<Tile>();

  return make_scoped_refptr(new Tile(
      layer_tree_impl()->tile_manager(),
      pile_.get(),
      content_rect.size(),
      content_rect,
      contents_opaque() ? content_rect : gfx::Rect(),
      tiling->contents_scale(),
      id()));
}

void PictureLayerImpl::UpdatePile(Tile* tile) {
  tile->set_picture_pile(pile_);
}

const Region* PictureLayerImpl::GetInvalidation() {
  return &invalidation_;
}

const PictureLayerTiling* PictureLayerImpl::GetTwinTiling(
    const PictureLayerTiling* tiling) {

  const PictureLayerImpl* other_layer = layer_tree_impl()->IsActiveTree() ?
      PendingTwin() : ActiveTwin();
  if (!other_layer)
    return NULL;
  for (size_t i = 0; i < other_layer->tilings_->num_tilings(); ++i)
    if (other_layer->tilings_->tiling_at(i)->contents_scale() ==
        tiling->contents_scale())
      return other_layer->tilings_->tiling_at(i);
  return NULL;
}

gfx::Size PictureLayerImpl::CalculateTileSize(
    gfx::Size content_bounds) {
  if (is_mask_) {
    int max_size = layer_tree_impl()->MaxTextureSize();
    return gfx::Size(
        std::min(max_size, content_bounds.width()),
        std::min(max_size, content_bounds.height()));
  }

  int max_texture_size =
      layer_tree_impl()->resource_provider()->max_texture_size();

  gfx::Size default_tile_size = layer_tree_impl()->settings().default_tile_size;
  default_tile_size.ClampToMax(gfx::Size(max_texture_size, max_texture_size));

  gfx::Size max_untiled_content_size =
      layer_tree_impl()->settings().max_untiled_layer_size;
  max_untiled_content_size.ClampToMax(
      gfx::Size(max_texture_size, max_texture_size));

  bool any_dimension_too_large =
      content_bounds.width() > max_untiled_content_size.width() ||
      content_bounds.height() > max_untiled_content_size.height();

  bool any_dimension_one_tile =
      content_bounds.width() <= default_tile_size.width() ||
      content_bounds.height() <= default_tile_size.height();

  // If long and skinny, tile at the max untiled content size, and clamp
  // the smaller dimension to the content size, e.g. 1000x12 layer with
  // 500x500 max untiled size would get 500x12 tiles.  Also do this
  // if the layer is small.
  if (any_dimension_one_tile || !any_dimension_too_large) {
    int width =
        std::min(max_untiled_content_size.width(), content_bounds.width());
    int height =
        std::min(max_untiled_content_size.height(), content_bounds.height());
    // Round width and height up to the closest multiple of 64, or 56 if
    // we should avoid power-of-two textures. This helps reduce the number
    // of different textures sizes to help recycling, and also keeps all
    // textures multiple-of-eight, which is preferred on some drivers (IMG).
    bool avoid_pow2 =
        layer_tree_impl()->GetRendererCapabilities().avoid_pow2_textures;
    int round_up_to = avoid_pow2 ? 56 : 64;
#if defined(SBROWSER_NATIVE_TEXTURE_UPLOAD)
    if (!PictureLayerImpl::use_native_buffer_textures_)  {
    	width = RoundUp(width, round_up_to);
    	height = RoundUp(height, round_up_to);
    } else {
    	// Round DOWN to match calculated tile size to default tile size
    	// and maximize tile reuse. Previously code rounded up resulting
    	// tiles of 560x560 while for IMG HW default tile for Android was
    	// 504x504.
    	width = std::max(RoundUp(width-round_up_to, round_up_to), round_up_to);
    	height = std::max(RoundUp(height-round_up_to, round_up_to), round_up_to);
    }
#else
    width = RoundUp(width, round_up_to);
    height = RoundUp(height, round_up_to);
#endif
    return gfx::Size(width, height);
  }

  return default_tile_size;
}

void PictureLayerImpl::SyncFromActiveLayer() {
  DCHECK(layer_tree_impl()->IsPendingTree());

  if (PictureLayerImpl* active_twin = ActiveTwin())
    SyncFromActiveLayer(active_twin);
}

void PictureLayerImpl::SyncFromActiveLayer(const PictureLayerImpl* other) {
  // UpdateLCDTextStatus() depends on LCD text status always being synced.
  is_using_lcd_text_ = other->is_using_lcd_text_;

  if (!DrawsContent()) {
    ResetRasterScale();
    return;
  }

  raster_page_scale_ = other->raster_page_scale_;
  raster_device_scale_ = other->raster_device_scale_;
  raster_source_scale_ = other->raster_source_scale_;
  raster_contents_scale_ = other->raster_contents_scale_;
  low_res_raster_contents_scale_ = other->low_res_raster_contents_scale_;

  // Add synthetic invalidations for any recordings that were dropped.  As
  // tiles are updated to point to this new pile, this will force the dropping
  // of tiles that can no longer be rastered.  This is not ideal, but is a
  // trade-off for memory (use the same pile as much as possible, by switching
  // during DidBecomeActive) and for time (don't bother checking every tile
  // during activation to see if the new pile can still raster it).
  //
  // TODO(enne): Clean up this double loop.
  for (int x = 0; x < pile_->num_tiles_x(); ++x) {
    for (int y = 0; y < pile_->num_tiles_y(); ++y) {
      bool previously_had = other->pile_->HasRecordingAt(x, y);
      bool now_has = pile_->HasRecordingAt(x, y);
      if (now_has || !previously_had)
        continue;
      gfx::Rect layer_rect = pile_->tile_bounds(x, y);
      invalidation_.Union(layer_rect);
    }
  }

  // Union in the other newly exposed regions as invalid.
  Region difference_region = Region(gfx::Rect(bounds()));
  difference_region.Subtract(gfx::Rect(other->bounds()));
  invalidation_.Union(difference_region);

  tilings_->RemoveAllTilings();
  tilings_->AddTilingsToMatchScales(*other->tilings_, MinimumContentsScale());
  DCHECK(bounds() == tilings_->layer_bounds());
}

void PictureLayerImpl::SyncTiling(
    const PictureLayerTiling* tiling) {
  if (!DrawsContent() || tiling->contents_scale() < MinimumContentsScale())
    return;
  tilings_->AddTiling(tiling->contents_scale());

  // If this tree needs update draw properties, then the tiling will
  // get updated prior to drawing or activation.  If this tree does not
  // need update draw properties, then its transforms are up to date and
  // we can create tiles for this tiling immediately.
  if (!layer_tree_impl()->needs_update_draw_properties())
    UpdateTilePriorities();
}

void PictureLayerImpl::SetIsMask(bool is_mask) {
  if (is_mask_ == is_mask)
    return;
  is_mask_ = is_mask;
  if (tilings_)
    tilings_->RemoveAllTiles();
}

ResourceProvider::ResourceId PictureLayerImpl::ContentsResourceId() const {
  gfx::Rect content_rect(content_bounds());
  float scale = contents_scale_x();
  for (PictureLayerTilingSet::CoverageIterator
           iter(tilings_.get(), scale, content_rect, ideal_contents_scale_);
       iter;
       ++iter) {
    // Mask resource not ready yet.
    if (!*iter ||
        iter->drawing_info().mode() !=
            ManagedTileState::DrawingInfo::RESOURCE_MODE ||
        !iter->drawing_info().IsReadyToDraw())
      return 0;
    // Masks only supported if they fit on exactly one tile.
    if (iter.geometry_rect() != content_rect)
      return 0;
    return iter->drawing_info().get_resource_id();
  }
  return 0;
}

void PictureLayerImpl::MarkVisibleResourcesAsRequired() const {
  DCHECK(layer_tree_impl()->IsPendingTree());
  DCHECK(!layer_tree_impl()->needs_update_draw_properties());
  DCHECK(ideal_contents_scale_);
  DCHECK_GT(tilings_->num_tilings(), 0u);

  gfx::Rect rect(visible_content_rect());

  float min_acceptable_scale =
      std::min(raster_contents_scale_, ideal_contents_scale_);

  if (PictureLayerImpl* twin = ActiveTwin()) {
    float twin_min_acceptable_scale =
        std::min(twin->ideal_contents_scale_, twin->raster_contents_scale_);
    // Ignore 0 scale in case CalculateContentsScale() has never been
    // called for active twin.
    if (twin_min_acceptable_scale != 0.0f) {
      min_acceptable_scale =
          std::min(min_acceptable_scale, twin_min_acceptable_scale);
    }
  }

  // Mark tiles for activation in two passes.  Ready to draw tiles in acceptable
  // but non-ideal tilings are marked as required for activation, but any
  // non-ready tiles are not marked as required.  From there, any missing holes
  // will need to be filled in from the high res tiling.

  PictureLayerTiling* high_res = NULL;
  Region missing_region = rect;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    DCHECK(tiling->has_ever_been_updated());

    if (tiling->contents_scale() < min_acceptable_scale)
      continue;

	if (tiling->resolution() == HIGH_RESOLUTION) {
		DCHECK(!high_res) << "There can only be one high res tiling";
		high_res = tiling;
		continue;
	}

    for (PictureLayerTiling::CoverageIterator iter(tiling,
                                                   contents_scale_x(),
                                                   rect);
         iter;
         ++iter) {
    	if (!*iter || !iter->drawing_info().IsReadyToDraw())
			continue;
		missing_region.Subtract(iter.geometry_rect());
		iter->mark_required_for_activation();

		DCHECK_EQ(iter->priority(PENDING_TREE).distance_to_visible_in_pixels, 0);
		DCHECK_EQ(iter->priority(PENDING_TREE).time_to_visible_in_seconds, 0);
    }
  }

  DCHECK(high_res) << "There must be one high res tiling";	
  if(!high_res)
  	return;
  for (PictureLayerTiling::CoverageIterator iter(high_res,
		contents_scale_x(), rect);iter; ++iter) {
	// A null tile (i.e. missing recording) can just be skipped.
	// If the missing region doesn't cover it, this tile is fully
	// covered by acceptable tiles at other scales.
	if (!*iter || !missing_region.Intersects(iter.geometry_rect()))
	continue;
	iter->mark_required_for_activation();

	// These must be true for this tile to end up in the NOW_BIN in TileManager.
	DCHECK_EQ(iter->priority(PENDING_TREE).distance_to_visible_in_pixels, 0);
	DCHECK_EQ(iter->priority(PENDING_TREE).time_to_visible_in_seconds, 0);
  }  
}

PictureLayerTiling* PictureLayerImpl::AddTiling(float contents_scale) {
  DCHECK(contents_scale >= MinimumContentsScale());

  PictureLayerTiling* tiling = tilings_->AddTiling(contents_scale);

  const Region& recorded = pile_->recorded_region();
  DCHECK(!recorded.IsEmpty());

  PictureLayerImpl* twin =
      layer_tree_impl()->IsPendingTree() ? ActiveTwin() : PendingTwin();
  if (twin)
    twin->SyncTiling(tiling);

  return tiling;
}

void PictureLayerImpl::RemoveTiling(float contents_scale) {
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    if (tiling->contents_scale() == contents_scale) {
      tilings_->Remove(tiling);
      break;
    }
  }
}

namespace {

inline float PositiveRatio(float float1, float float2) {
  DCHECK_GT(float1, 0);
  DCHECK_GT(float2, 0);
  return float1 > float2 ? float1 / float2 : float2 / float1;
}

inline bool IsCloserToThan(
    PictureLayerTiling* layer1,
    PictureLayerTiling* layer2,
    float contents_scale) {
  // Absolute value for ratios.
  float ratio1 = PositiveRatio(layer1->contents_scale(), contents_scale);
  float ratio2 = PositiveRatio(layer2->contents_scale(), contents_scale);
  return ratio1 < ratio2;
}

}  // namespace

void PictureLayerImpl::ManageTilings(bool animating_transform_to_screen) {
  DCHECK(ideal_contents_scale_);
  DCHECK(ideal_page_scale_);
  DCHECK(ideal_device_scale_);
  DCHECK(ideal_source_scale_);

  if (pile_->recorded_region().IsEmpty())
    return;

  bool change_target_tiling =
      raster_page_scale_ == 0.f ||
      raster_device_scale_ == 0.f ||
      raster_source_scale_ == 0.f ||
      raster_contents_scale_ == 0.f ||
      low_res_raster_contents_scale_ == 0.f ||
      ShouldAdjustRasterScale(animating_transform_to_screen);

  if (layer_tree_impl()->IsActiveTree()) {
    // Store the value for the next time ShouldAdjustRasterScale is called.
    raster_source_scale_was_animating_ = animating_transform_to_screen;
  }

  if (!change_target_tiling)
    return;

  RecalculateRasterScales(animating_transform_to_screen);

  PictureLayerTiling* high_res = NULL;
  PictureLayerTiling* low_res = NULL;

  PictureLayerTiling* previous_low_res = NULL;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);
    if (tiling->contents_scale() == raster_contents_scale_)
      high_res = tiling;
    if (tiling->contents_scale() == low_res_raster_contents_scale_)
      low_res = tiling;
    if (tiling->resolution() == LOW_RESOLUTION)
      previous_low_res = tiling;

    // Reset all tilings to non-ideal until the end of this function.
    tiling->set_resolution(NON_IDEAL_RESOLUTION);
  }

  if (!high_res) {
    high_res = AddTiling(raster_contents_scale_);
    if (raster_contents_scale_ == low_res_raster_contents_scale_)
      low_res = high_res;
  }

  // Only create new low res tilings when the transform is static.  This
  // prevents wastefully creating a paired low res tiling for every new high res
  // tiling during a pinch or a CSS animation.

  // SAMSUNG CHANGE : Power consumption(reduce tile & fling speed)>>
  //bool is_pinching = layer_tree_impl()->PinchGestureActive();
  //if (!is_pinching && !animating_transform_to_screen && !low_res &&
  //    low_res != high_res)
  //    low_res = AddTiling(low_res_raster_contents_scale_);*/
  // SAMSUNG CHANGE : Power consumption(reduce tile & fling speed)<<

    
  // Set low-res if we have one.
  if (low_res && low_res != high_res)
    low_res->set_resolution(LOW_RESOLUTION);
  else if (!low_res && previous_low_res)
    previous_low_res->set_resolution(LOW_RESOLUTION);

  // Make sure we always have one high-res (even if high == low).
  high_res->set_resolution(HIGH_RESOLUTION);

}

bool PictureLayerImpl::ShouldAdjustRasterScale(
    bool animating_transform_to_screen) const {
  // TODO(danakj): Adjust raster source scale closer to ideal source scale at
  // a throttled rate. Possibly make use of invalidation_.IsEmpty() on pending
  // tree. This will allow CSS scale changes to get re-rastered at an
  // appropriate rate.

  bool is_active_layer = layer_tree_impl()->IsActiveTree();
  if (is_active_layer && raster_source_scale_was_animating_ &&
      !animating_transform_to_screen)
    return true;

  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  bool is_doubletap = layer_tree_impl()->DoubleTapActive();
  if (is_active_layer && (is_pinching||is_doubletap) && raster_page_scale_) {
    // We change our raster scale when it is:
    // - Higher than ideal (need a lower-res tiling available)
    // - Too far from ideal (need a higher-res tiling available)
    float ratio = ideal_page_scale_ / raster_page_scale_;
    if (raster_page_scale_ > ideal_page_scale_ ||
        ratio > kMaxScaleRatioDuringPinch)
      return true;
  }

  if (!(is_pinching||is_doubletap)) {
    // When not pinching, match the ideal page scale factor.
    if (raster_page_scale_ != ideal_page_scale_)
      return true;
  }

  // Always match the ideal device scale factor.
  if (raster_device_scale_ != ideal_device_scale_)
    return true;

  return false;
}

float PictureLayerImpl::SnappedContentsScale(float scale) {
  // If a tiling exists within the max snapping ratio, snap to its scale.
  float snapped_contents_scale = scale;
  float snapped_ratio = kSnapToExistingTilingRatio;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    float tiling_contents_scale = tilings_->tiling_at(i)->contents_scale();
    float ratio = PositiveRatio(tiling_contents_scale, scale);
    if (ratio < snapped_ratio) {
      snapped_contents_scale = tiling_contents_scale;
      snapped_ratio = ratio;
    }
  }
  return snapped_contents_scale;
}

void PictureLayerImpl::RecalculateRasterScales(
  bool animating_transform_to_screen) {
  raster_device_scale_ = ideal_device_scale_;
  raster_source_scale_ = ideal_source_scale_;

  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  bool is_doubletap = layer_tree_impl()->DoubleTapActive();
  if (!(is_pinching||is_doubletap)) {
    // When not pinching, we use ideal scale:
    raster_page_scale_ = ideal_page_scale_;
    raster_contents_scale_ = ideal_contents_scale_;
  } else {
    // See ShouldAdjustRasterScale:
    // - When zooming out, preemptively create new tiling at lower resolution.
    // - When zooming in, approximate ideal using multiple of kMaxScaleRatio.
    bool zooming_out = raster_page_scale_ > ideal_page_scale_;
    float desired_contents_scale =
    zooming_out ? raster_contents_scale_ / kMaxScaleRatioDuringPinch
    : raster_contents_scale_ * kMaxScaleRatioDuringPinch;
    raster_contents_scale_ = SnappedContentsScale(desired_contents_scale);
    raster_page_scale_ = raster_contents_scale_ / raster_device_scale_;
  }

  // Don't allow animating CSS scales to drop below 1.
  if (animating_transform_to_screen) {
    raster_contents_scale_ = std::max(
    raster_contents_scale_, 1.f * ideal_page_scale_ * ideal_device_scale_);
  }

  float low_res_factor =
      layer_tree_impl()->settings().low_res_contents_scale_factor;
  low_res_raster_contents_scale_ = std::max(
      raster_contents_scale_ * low_res_factor,
      MinimumContentsScale());
}

void PictureLayerImpl::CleanUpTilingsOnActiveLayer(
    std::vector<PictureLayerTiling*> used_tilings) {
  DCHECK(layer_tree_impl()->IsActiveTree());

  float min_acceptable_high_res_scale = std::min(
      raster_contents_scale_, ideal_contents_scale_);
  float max_acceptable_high_res_scale = std::max(
      raster_contents_scale_, ideal_contents_scale_);

  PictureLayerImpl* twin = PendingTwin();
  if (twin) {
    min_acceptable_high_res_scale = std::min(
        min_acceptable_high_res_scale,
        std::min(twin->raster_contents_scale_, twin->ideal_contents_scale_));
    max_acceptable_high_res_scale = std::max(
        max_acceptable_high_res_scale,
        std::max(twin->raster_contents_scale_, twin->ideal_contents_scale_));
  }

  float low_res_factor =
      layer_tree_impl()->settings().low_res_contents_scale_factor;

  float min_acceptable_low_res_scale =
      low_res_factor * min_acceptable_high_res_scale;
  float max_acceptable_low_res_scale =
      low_res_factor * max_acceptable_high_res_scale;

  std::vector<PictureLayerTiling*> to_remove;
  for (size_t i = 0; i < tilings_->num_tilings(); ++i) {
    PictureLayerTiling* tiling = tilings_->tiling_at(i);

    if (tiling->contents_scale() >= min_acceptable_high_res_scale &&
        tiling->contents_scale() <= max_acceptable_high_res_scale)
      continue;

    if (tiling->contents_scale() >= min_acceptable_low_res_scale &&
        tiling->contents_scale() <= max_acceptable_low_res_scale)
      continue;

    // Don't remove tilings that are being used and expected to stay around.
    if (std::find(used_tilings.begin(), used_tilings.end(), tiling) !=
        used_tilings.end())
      continue;

    to_remove.push_back(tiling);
  }

  for (size_t i = 0; i < to_remove.size(); ++i) {
    if (twin)
      twin->RemoveTiling(to_remove[i]->contents_scale());
    tilings_->Remove(to_remove[i]);
  }
}

PictureLayerImpl* PictureLayerImpl::PendingTwin() const {
  DCHECK(layer_tree_impl()->IsActiveTree());

  PictureLayerImpl* twin = static_cast<PictureLayerImpl*>(
      layer_tree_impl()->FindPendingTreeLayerById(id()));
  if (twin)
    DCHECK_EQ(id(), twin->id());
  return twin;
}

PictureLayerImpl* PictureLayerImpl::ActiveTwin() const {
  DCHECK(layer_tree_impl()->IsPendingTree());

  PictureLayerImpl* twin = static_cast<PictureLayerImpl*>(
      layer_tree_impl()->FindActiveTreeLayerById(id()));
  if (twin)
    DCHECK_EQ(id(), twin->id());
  return twin;
}

float PictureLayerImpl::MinimumContentsScale() const {
  float setting_min = layer_tree_impl()->settings().minimum_contents_scale;

  // If the contents scale is less than 1 / width (also for height),
  // then it will end up having less than one pixel of content in that
  // dimension.  Bump the minimum contents scale up in this case to prevent
  // this from happening.
  int min_dimension = std::min(bounds().width(), bounds().height());
  if (!min_dimension)
    return setting_min;

  return std::max(1.f / min_dimension, setting_min);
}

void PictureLayerImpl::UpdateLCDTextStatus() {
  // Once this layer is not using lcd text, don't switch back.
  if (!is_using_lcd_text_)
    return;

  if (is_using_lcd_text_ == can_use_lcd_text())
    return;

  is_using_lcd_text_ = can_use_lcd_text();

  // As a trade-off between jank and drawing with the incorrect resources,
  // don't ever update the active tree's resources in place.  Instead,
  // update lcd text on the pending tree.  If this is the active tree and
  // there is no pending twin, then call set needs commit to create one.
  if (layer_tree_impl()->IsActiveTree() && !PendingTwin()) {
    // TODO(enne): Handle this by updating these resources in-place instead.
    layer_tree_impl()->SetNeedsCommit();
    return;
  }

  // The heuristic of never switching back to lcd text enabled implies that
  // this property needs to be synchronized to the pending tree right now.
  PictureLayerImpl* pending_layer =
      layer_tree_impl()->IsActiveTree() ? PendingTwin() : this;
  if (layer_tree_impl()->IsActiveTree() &&
      pending_layer->is_using_lcd_text_ == is_using_lcd_text_)
    return;

  // Further tiles created due to new tilings should be considered invalidated.
  pending_layer->invalidation_.Union(gfx::Rect(bounds()));
  pending_layer->is_using_lcd_text_ = is_using_lcd_text_;
  pending_layer->pile_ = PicturePileImpl::CreateFromOther(pending_layer->pile_,
                                                          is_using_lcd_text_);
  pending_layer->tilings_->DestroyAndRecreateTilesWithText();
}

void PictureLayerImpl::ResetRasterScale() {
  raster_page_scale_ = 0.f;
  raster_device_scale_ = 0.f;
  raster_source_scale_ = 0.f;
  raster_contents_scale_ = 0.f;
  low_res_raster_contents_scale_ = 0.f;
}

void PictureLayerImpl::GetDebugBorderProperties(
    SkColor* color,
    float* width) const {
  *color = DebugColors::TiledContentLayerBorderColor();
  *width = DebugColors::TiledContentLayerBorderWidth(layer_tree_impl());
}

scoped_ptr<base::Value> PictureLayerImpl::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  LayerImpl::AsValueInto(state.get());

  state->SetDouble("ideal_contents_scale", ideal_contents_scale_);
  state->Set("tilings", tilings_->AsValue().release());
  return state.PassAs<base::Value>();
}

}  // namespace cc
