// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_MANAGER_H_
#define CC_RESOURCES_TILE_MANAGER_H_

#include <queue>
#include <set>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile_priority.h"

namespace cc {
class ResourceProvider;
class Tile;
class TileVersion;

class CC_EXPORT TileManagerClient {
 public:
  virtual void ScheduleManageTiles() = 0;
  virtual void DidInitializeVisibleTile() = 0;
  virtual bool ShouldForceTileUploadsRequiredForActivationToComplete() const = 0;

 protected:
  virtual ~TileManagerClient() {}
};

// Tile manager classifying tiles into a few basic
// bins:
enum TileManagerBin {
  NOW_BIN = 0,  // Needed ASAP.
  SOON_BIN = 1,  // Impl-side version of prepainting.
  EVENTUALLY_BIN = 2,  // Nice to have, if we've got memory and time.
  NEVER_BIN = 3,  // Dont bother.
  NUM_BINS = 4
  // Be sure to update TileManagerBinAsValue when adding new fields.
};
scoped_ptr<base::Value> TileManagerBinAsValue(
    TileManagerBin bin);

enum TileManagerBinPriority {
  HIGH_PRIORITY_BIN = 0,
  LOW_PRIORITY_BIN = 1,
  NUM_BIN_PRIORITIES = 2
};
scoped_ptr<base::Value> TileManagerBinPriorityAsValue(
    TileManagerBinPriority bin);

// This class manages tiles, deciding which should get rasterized and which
// should no longer have any memory assigned to them. Tile objects are "owned"
// by layers; they automatically register with the manager when they are
// created, and unregister from the manager when they are deleted.
class CC_EXPORT TileManager : public WorkerPoolClient {
 public:
  TileManager(TileManagerClient* client,
              ResourceProvider *resource_provider,
              size_t num_raster_threads,
              bool use_color_estimator,
              bool prediction_benchmarking,
              RenderingStatsInstrumentation* rendering_stats_instrumentation);
  virtual ~TileManager();

  const GlobalStateThatImpactsTilePriority& GlobalState() const {
      return global_state_;
  }
  void SetGlobalState(const GlobalStateThatImpactsTilePriority& state);

  void ManageTiles();
  void CheckForCompletedTileUploads();
  void AbortPendingTileUploads();

  scoped_ptr<base::Value> BasicStateAsValue() const;
  scoped_ptr<base::Value> AllTilesAsValue() const;
  void GetMemoryStats(size_t* memory_required_bytes,
                      size_t* memory_nice_to_have_bytes,
                      size_t* memory_used_bytes) const;

  const MemoryHistory::Entry& memory_stats_from_last_assign() const {
    return memory_stats_from_last_assign_;
  }
#if defined(SBROWSER_NATIVE_TEXTURE_UPLOAD)
  static bool zerocopy_;
#endif
  // Overridden from WorkerPoolClient:
  virtual void DidFinishDispatchingWorkerPoolCompletionCallbacks() OVERRIDE;

  void WillModifyTilePriorities() {
	ScheduleManageTiles();
  }
   
  bool AreTilesRequiredForActivationReady() const {
     return tiles_that_need_to_be_initialized_for_activation_.empty();
  }

#if defined(SBROWSER_ADJUST_WORKER_THREAD_PRIORITY)
  static void SetHighPriorityToRasterWorker(bool flag, TileManager* tile_manager);
#endif

#if defined(SBROWSER_INCREASE_RASTER_THREAD)
  void SetNumRasterThreads(size_t num_threads);
#endif
  
 protected:
  // Methods called by Tile
  friend class Tile;
  void RegisterTile(Tile* tile);
  void UnregisterTile(Tile* tile);

  // Virtual for test
  virtual void DispatchMoreTasks();

 private:
  // Data that is passed to raster tasks.
  struct RasterTaskMetadata {
      bool prediction_benchmarking;
      bool is_tile_in_pending_tree_now_bin;
      TileResolution tile_resolution;
      int layer_id;
  };
#if defined(SBROWSER_NATIVE_TEXTURE_UPLOAD)
  struct BufferData {
    uint8* buffer;
    size_t stride;
  };
#endif

  RasterTaskMetadata GetRasterTaskMetadata(const Tile& tile) const;

  void AssignBinsToTiles();
  void SortTiles();
  void AssignGpuMemoryToTiles();
  void FreeResourcesForTile(Tile* tile);
  void ForceTileUploadToComplete(Tile* tile);
  void ScheduleManageTiles() {
    if (manage_tiles_pending_)
      return;
    client_->ScheduleManageTiles();
    manage_tiles_pending_ = true;
  }
  bool DispatchImageDecodeTasksForTile(Tile* tile);
  void DispatchOneImageDecodeTask(
      scoped_refptr<Tile> tile, skia::LazyPixelRef* pixel_ref);
  void OnImageDecodeTaskCompleted(
      scoped_refptr<Tile> tile,
      uint32_t pixel_ref_id);
  bool CanDispatchRasterTask(Tile* tile) const;
  scoped_ptr<ResourcePool::Resource> PrepareTileForRaster(Tile* tile);
  void DispatchOneRasterTask(scoped_refptr<Tile> tile);
  void OnRasterTaskCompleted(
      scoped_refptr<Tile> tile,
      scoped_ptr<ResourcePool::Resource> resource,
      PicturePileImpl::Analysis* analysis,
      int manage_tiles_call_count_when_dispatched);
  void DidFinishTileInitialization(Tile* tile);
  void DidTileTreeBinChange(Tile* tile,
                            TileManagerBin new_tree_bin,
                            WhichTree tree);
  scoped_ptr<Value> GetMemoryRequirementsAsValue() const;
  void AddRequiredTileForActivation(Tile* tile);

  static void RunAnalyzeAndRasterTask(
      const RasterWorkerPool::RasterCallback& analyze_task,
      const RasterWorkerPool::RasterCallback& raster_task,
      PicturePileImpl* picture_pile);
  static void RunAnalyzeTask(
      PicturePileImpl::Analysis* analysis,
      gfx::Rect rect,
      float contents_scale,
      bool use_color_estimator,
      const RasterTaskMetadata& metadata,
      RenderingStatsInstrumentation* stats_instrumentation,
      PicturePileImpl* picture_pile);
#if defined(SBROWSER_NATIVE_TEXTURE_UPLOAD)
  static void RunRasterTask(
      BufferData raster_data,
      PicturePileImpl::Analysis* analysis,
      gfx::Rect rect,
      float contents_scale,
      const RasterTaskMetadata& metadata,
      RenderingStatsInstrumentation* stats_instrumentation,
      PicturePileImpl* picture_pile);
#else
  static void RunRasterTask(
      uint8* buffer,
      PicturePileImpl::Analysis* analysis,
      gfx::Rect rect,
      float contents_scale,
      const RasterTaskMetadata& metadata,
      RenderingStatsInstrumentation* stats_instrumentation,
      PicturePileImpl* picture_pile);
#endif
  static void RunImageDecodeTask(
      skia::LazyPixelRef* pixel_ref /*,
      RenderingStatsInstrumentation* stats_instrumentation*/);

  static void RecordSolidColorPredictorResults(const SkPMColor* actual_colors,
                                               size_t color_count,
                                               bool is_predicted_solid,
                                               SkPMColor predicted_color);

  TileManagerClient* client_;
  scoped_ptr<ResourcePool> resource_pool_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
  bool manage_tiles_pending_;
  int manage_tiles_call_count_;

  GlobalStateThatImpactsTilePriority global_state_;

  typedef std::vector<Tile*> TileVector;
  TileVector tiles_;
  TileVector tiles_that_need_to_be_rasterized_;
  typedef std::set<Tile*> TileSet;
  TileSet tiles_that_need_to_be_initialized_for_activation_;

  typedef base::hash_set<uint32_t> PixelRefSet;
  PixelRefSet pending_decode_tasks_;

  typedef std::queue<scoped_refptr<Tile> > TileQueue;
  TileQueue tiles_with_pending_upload_;

  size_t memory_required_bytes_;
  size_t memory_nice_to_have_bytes_;
  size_t memory_used_bytes_;
  
  size_t bytes_pending_upload_;
  bool has_performed_uploads_since_last_flush_;
  bool ever_exceeded_memory_budget_;
  MemoryHistory::Entry memory_stats_from_last_assign_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;

  bool use_color_estimator_;
  bool prediction_benchmarking_;
  bool did_initialize_visible_tile_;

  size_t pending_tasks_;
  size_t max_pending_tasks_;

  DISALLOW_COPY_AND_ASSIGN(TileManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_MANAGER_H_
