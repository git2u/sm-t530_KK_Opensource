// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_GL_RENDERER_H_
#define CC_OUTPUT_GL_RENDERER_H_

#include "base/cancelable_callback.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/gl_renderer_draw_cache.h"
#include "cc/output/renderer.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsMemoryAllocation.h"
#include "ui/gfx/quad_f.h"

class SkBitmap;

namespace cc {

class GLRendererShaderTest;
class OutputSurface;
class PictureDrawQuad;
class ScopedResource;
class StreamVideoDrawQuad;
class TextureDrawQuad;
class GeometryBinding;
class ScopedEnsureFramebufferAllocation;

// Class that handles drawing of composited render layers using GL.
class CC_EXPORT GLRenderer
  : public DirectRenderer,
    public NON_EXPORTED_BASE(
        WebKit::WebGraphicsContext3D::
            WebGraphicsMemoryAllocationChangedCallbackCHROMIUM) {
 public:
  static scoped_ptr<GLRenderer> Create(RendererClient* client,
                                       OutputSurface* output_surface,
                                       ResourceProvider* resource_provider,
                                       int highp_threshold_min);

  virtual ~GLRenderer();

  virtual const RendererCapabilities& Capabilities() const OVERRIDE;

  WebKit::WebGraphicsContext3D* Context();

  virtual void ViewportChanged() OVERRIDE;
#if defined(SBROWSER_BLOCK_SENDING_FRAME_MESSAGE)
  virtual void SetGestureInProgress(bool gesture) OVERRIDE;
#endif
  // Waits for rendering to finish.
  virtual void Finish() OVERRIDE;

  virtual void DoNoOp() OVERRIDE;
  // Puts backbuffer onscreen.
  virtual void SwapBuffers(const LatencyInfo& latency_info) OVERRIDE;

  virtual void GetFramebufferPixels(void* pixels, gfx::Rect rect) OVERRIDE;

  virtual bool IsContextLost() OVERRIDE;

  virtual void SetVisible(bool visible) OVERRIDE;

  virtual void SendManagedMemoryStats(size_t bytes_visible,
                                      size_t bytes_visible_and_nearby,
                                      size_t bytes_allocated) OVERRIDE;

  static void DebugGLCall(WebKit::WebGraphicsContext3D* context,
                          const char* command,
                          const char* file,
                          int line);

 protected:
  GLRenderer(RendererClient* client,
             OutputSurface* output_surface,
             ResourceProvider* resource_provider,
             int highp_threshold_min);

  bool IsBackbufferDiscarded() const { return is_backbuffer_discarded_; }
  bool Initialize();

  const gfx::QuadF& SharedGeometryQuad() const { return shared_geometry_quad_; }
  const GeometryBinding* SharedGeometry() const {
    return shared_geometry_.get();
  }

  void GetFramebufferPixelsAsync(gfx::Rect rect,
                                 CopyRenderPassCallback callback);
  bool GetFramebufferTexture(ScopedResource* resource, gfx::Rect device_rect);
  void ReleaseRenderPassTextures();

  virtual void BindFramebufferToOutputSurface(DrawingFrame* frame) OVERRIDE;
  virtual bool BindFramebufferToTexture(DrawingFrame* frame,
                                        const ScopedResource* resource,
                                        gfx::Rect framebuffer_rect) OVERRIDE;
  virtual void SetDrawViewportSize(gfx::Size viewport_size) OVERRIDE;
  virtual void SetScissorTestRect(gfx::Rect scissor_rect) OVERRIDE;
  virtual void ClearFramebuffer(DrawingFrame* frame) OVERRIDE;
  virtual void DoDrawQuad(DrawingFrame* frame, const class DrawQuad*) OVERRIDE;
  virtual void BeginDrawingFrame(DrawingFrame* frame) OVERRIDE;
  virtual void FinishDrawingFrame(DrawingFrame* frame) OVERRIDE;
  virtual bool FlippedFramebuffer() const OVERRIDE;
  virtual void EnsureScissorTestEnabled() OVERRIDE;
  virtual void EnsureScissorTestDisabled() OVERRIDE;
  virtual void CopyCurrentRenderPassToBitmap(
      DrawingFrame* frame,
      const CopyRenderPassCallback& callback) OVERRIDE;
  virtual void FinishDrawingQuadList() OVERRIDE;

 private:
  friend class GLRendererShaderPixelTest;
  friend class GLRendererShaderTest;

  static void ToGLMatrix(float* gl_matrix, const gfx::Transform& transform);
  static ManagedMemoryPolicy::PriorityCutoff PriorityCutoff(
      WebKit::WebGraphicsMemoryAllocation::PriorityCutoff priority_cutoff);

  void DrawCheckerboardQuad(const DrawingFrame* frame,
                            const CheckerboardDrawQuad* quad);
  void DrawDebugBorderQuad(const DrawingFrame* frame,
                           const DebugBorderDrawQuad* quad);
  scoped_ptr<ScopedResource> DrawBackgroundFilters(
      DrawingFrame* frame,
      const RenderPassDrawQuad* quad,
      const gfx::Transform& contents_device_transform,
      const gfx::Transform& contents_device_transformInverse);
  void DrawRenderPassQuad(DrawingFrame* frame, const RenderPassDrawQuad* quad);
  void DrawSolidColorQuad(const DrawingFrame* frame,
                          const SolidColorDrawQuad* quad);
  void DrawStreamVideoQuad(const DrawingFrame* frame,
                           const StreamVideoDrawQuad* quad);
  void DrawTextureQuad(const DrawingFrame* frame, const TextureDrawQuad* quad);
  void EnqueueTextureQuad(const DrawingFrame* frame,
                          const TextureDrawQuad* quad);
  void FlushTextureQuadCache();
  void DrawIOSurfaceQuad(const DrawingFrame* frame,
                         const IOSurfaceDrawQuad* quad);
  void DrawTileQuad(const DrawingFrame* frame, const TileDrawQuad* quad);
  void DrawContentQuad(const DrawingFrame* frame,
                       const ContentDrawQuadBase* quad,
                       ResourceProvider::ResourceId resource_id);
  void DrawYUVVideoQuad(const DrawingFrame* frame,
                        const YUVVideoDrawQuad* quad);
  void DrawPictureQuad(const DrawingFrame* frame,
                       const PictureDrawQuad* quad);

  void SetShaderOpacity(float opacity, int alpha_location);
  void SetShaderQuadF(const gfx::QuadF& quad, int quad_location);
  void DrawQuadGeometry(const DrawingFrame* frame,
                        const gfx::Transform& draw_transform,
                        const gfx::RectF& quad_rect,
                        int matrix_location);
  void SetBlendEnabled(bool enabled);
  bool blend_enabled() const { return blend_shadow_; }
  void SetUseProgram(unsigned program);

  void CopyTextureToFramebuffer(const DrawingFrame* frame,
                                int texture_id,
                                gfx::Rect rect,
                                const gfx::Transform& draw_matrix);

  // Check if quad needs antialiasing and if so, inflate the quad and
  // fill edge array for fragment shader.  local_quad is set to
  // inflated quad if antialiasing is required, otherwise it is left
  // unchanged.  edge array is filled with inflated quad's edge data
  // if antialiasing is required, otherwise it is left unchanged.
  // Returns true if quad requires antialiasing and false otherwise.
  bool SetupQuadForAntialiasing(const gfx::Transform& device_transform,
                                const DrawQuad* quad,
                                gfx::QuadF* local_quad,
                                float edge[24]) const;

  bool UseScopedTexture(DrawingFrame* frame,
                        const ScopedResource* resource,
                        gfx::Rect viewport_rect);

  bool MakeContextCurrent();

  bool InitializeSharedObjects();
  void CleanupSharedObjects();

  typedef base::Callback<void(bool success)>
      AsyncGetFramebufferPixelsCleanupCallback;
  void DoGetFramebufferPixels(
      uint8* pixels,
      gfx::Rect rect,
      const AsyncGetFramebufferPixelsCleanupCallback& cleanup_callback);
  void FinishedReadback(
      const AsyncGetFramebufferPixelsCleanupCallback& cleanup_callback,
      unsigned source_buffer,
      uint8_t* dest_pixels,
      gfx::Size size);
  void PassOnSkBitmap(
      scoped_ptr<SkBitmap> bitmap,
      scoped_ptr<SkAutoLockPixels> lock,
      const CopyRenderPassCallback& callback,
      bool success);

  // WebKit::
  // WebGraphicsContext3D::WebGraphicsMemoryAllocationChangedCallbackCHROMIUM
  // implementation.
  virtual void onMemoryAllocationChanged(
      WebKit::WebGraphicsMemoryAllocation allocation) OVERRIDE;
  void DiscardBackbuffer();
  void EnsureBackbuffer();
  void EnforceMemoryPolicy();

  RendererCapabilities capabilities_;

  unsigned offscreen_framebuffer_id_;

  scoped_ptr<GeometryBinding> shared_geometry_;
  gfx::QuadF shared_geometry_quad_;

  // This block of bindings defines all of the programs used by the compositor
  // itself.  Add any new programs here to GLRendererShaderTest.

  // Tiled layer shaders.
  typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexAlpha>
      TileProgram;
  typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexClampAlphaAA>
      TileProgramAA;
  typedef ProgramBinding<VertexShaderTile,
                         FragmentShaderRGBATexClampSwizzleAlphaAA>
      TileProgramSwizzleAA;
  typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexOpaque>
      TileProgramOpaque;
  typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexSwizzleAlpha>
      TileProgramSwizzle;
  typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexSwizzleOpaque>
      TileProgramSwizzleOpaque;
  typedef ProgramBinding<VertexShaderPosTex, FragmentShaderCheckerboard>
      TileCheckerboardProgram;

  // Texture shaders.
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexVaryingAlpha> TextureProgram;
  typedef ProgramBinding<VertexShaderPosTexTransformFlip,
                         FragmentShaderRGBATexVaryingAlpha> TextureProgramFlip;
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexRectVaryingAlpha>
      TextureIOSurfaceProgram;

  // Render surface shaders.
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexAlpha> RenderPassProgram;
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexAlphaMask> RenderPassMaskProgram;
  typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaAA>
      RenderPassProgramAA;
  typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaMaskAA>
      RenderPassMaskProgramAA;
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexColorMatrixAlpha>
      RenderPassColorMatrixProgram;
  typedef ProgramBinding<VertexShaderQuad,
                         FragmentShaderRGBATexAlphaMaskColorMatrixAA>
      RenderPassMaskColorMatrixProgramAA;
  typedef ProgramBinding<VertexShaderQuad,
                         FragmentShaderRGBATexAlphaColorMatrixAA>
      RenderPassColorMatrixProgramAA;
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexAlphaMaskColorMatrix>
      RenderPassMaskColorMatrixProgram;

  // Video shaders.
  typedef ProgramBinding<VertexShaderVideoTransform,
                         FragmentShaderOESImageExternal>
      VideoStreamTextureProgram;
  typedef ProgramBinding<VertexShaderPosTexYUVStretch, FragmentShaderYUVVideo>
      VideoYUVProgram;

  // Special purpose / effects shaders.
  typedef ProgramBinding<VertexShaderPos, FragmentShaderColor>
      DebugBorderProgram;
  typedef ProgramBinding<VertexShaderQuad, FragmentShaderColor>
      SolidColorProgram;
  typedef ProgramBinding<VertexShaderQuad, FragmentShaderColorAA>
      SolidColorProgramAA;

  const TileProgram* GetTileProgram(TexCoordPrecision precision);
  const TileProgramOpaque* GetTileProgramOpaque(TexCoordPrecision precision);
  const TileProgramAA* GetTileProgramAA(TexCoordPrecision precision);
  const TileProgramSwizzle* GetTileProgramSwizzle(TexCoordPrecision precision);
  const TileProgramSwizzleOpaque* GetTileProgramSwizzleOpaque(
      TexCoordPrecision precision);
  const TileProgramSwizzleAA* GetTileProgramSwizzleAA(
      TexCoordPrecision precision);
  const TileCheckerboardProgram* GetTileCheckerboardProgram();

  const RenderPassProgram* GetRenderPassProgram(
      TexCoordPrecision precision);
  const RenderPassProgramAA* GetRenderPassProgramAA(
      TexCoordPrecision precision);
  const RenderPassMaskProgram* GetRenderPassMaskProgram(
      TexCoordPrecision precision);
  const RenderPassMaskProgramAA* GetRenderPassMaskProgramAA(
      TexCoordPrecision precision);
  const RenderPassColorMatrixProgram* GetRenderPassColorMatrixProgram(
      TexCoordPrecision precision);
  const RenderPassColorMatrixProgramAA* GetRenderPassColorMatrixProgramAA(
      TexCoordPrecision precision);
  const RenderPassMaskColorMatrixProgram* GetRenderPassMaskColorMatrixProgram(
      TexCoordPrecision precision);
  const RenderPassMaskColorMatrixProgramAA*
      GetRenderPassMaskColorMatrixProgramAA(TexCoordPrecision precision);

  const TextureProgram* GetTextureProgram(
      TexCoordPrecision precision);
  const TextureProgramFlip* GetTextureProgramFlip(
      TexCoordPrecision precision);
  const TextureIOSurfaceProgram* GetTextureIOSurfaceProgram(
      TexCoordPrecision precision);

  const VideoYUVProgram* GetVideoYUVProgram(
      TexCoordPrecision precision);
  const VideoStreamTextureProgram* GetVideoStreamTextureProgram(
      TexCoordPrecision precision);

  const DebugBorderProgram* GetDebugBorderProgram();
  const SolidColorProgram* GetSolidColorProgram();
  const SolidColorProgramAA* GetSolidColorProgramAA();

  scoped_ptr<TileProgram> tile_program_;
  scoped_ptr<TileProgramOpaque> tile_program_opaque_;
  scoped_ptr<TileProgramAA> tile_program_aa_;
  scoped_ptr<TileProgramSwizzle> tile_program_swizzle_;
  scoped_ptr<TileProgramSwizzleOpaque> tile_program_swizzle_opaque_;
  scoped_ptr<TileProgramSwizzleAA> tile_program_swizzle_aa_;
  scoped_ptr<TileCheckerboardProgram> tile_checkerboard_program_;

  scoped_ptr<TileProgram> tile_program_highp_;
  scoped_ptr<TileProgramOpaque> tile_program_opaque_highp_;
  scoped_ptr<TileProgramAA> tile_program_aa_highp_;
  scoped_ptr<TileProgramSwizzle> tile_program_swizzle_highp_;
  scoped_ptr<TileProgramSwizzleOpaque> tile_program_swizzle_opaque_highp_;
  scoped_ptr<TileProgramSwizzleAA> tile_program_swizzle_aa_highp_;

  scoped_ptr<TextureProgram> texture_program_;
  scoped_ptr<TextureProgramFlip> texture_program_flip_;
  scoped_ptr<TextureIOSurfaceProgram> texture_io_surface_program_;

  scoped_ptr<TextureProgram> texture_program_highp_;
  scoped_ptr<TextureProgramFlip> texture_program_flip_highp_;
  scoped_ptr<TextureIOSurfaceProgram> texture_io_surface_program_highp_;

  scoped_ptr<RenderPassProgram> render_pass_program_;
  scoped_ptr<RenderPassProgramAA> render_pass_program_aa_;
  scoped_ptr<RenderPassMaskProgram> render_pass_mask_program_;
  scoped_ptr<RenderPassMaskProgramAA> render_pass_mask_program_aa_;
  scoped_ptr<RenderPassColorMatrixProgram> render_pass_color_matrix_program_;
  scoped_ptr<RenderPassColorMatrixProgramAA>
      render_pass_color_matrix_program_aa_;
  scoped_ptr<RenderPassMaskColorMatrixProgram>
      render_pass_mask_color_matrix_program_;
  scoped_ptr<RenderPassMaskColorMatrixProgramAA>
      render_pass_mask_color_matrix_program_aa_;

  scoped_ptr<RenderPassProgram> render_pass_program_highp_;
  scoped_ptr<RenderPassProgramAA> render_pass_program_aa_highp_;
  scoped_ptr<RenderPassMaskProgram> render_pass_mask_program_highp_;
  scoped_ptr<RenderPassMaskProgramAA> render_pass_mask_program_aa_highp_;
  scoped_ptr<RenderPassColorMatrixProgram>
      render_pass_color_matrix_program_highp_;
  scoped_ptr<RenderPassColorMatrixProgramAA>
      render_pass_color_matrix_program_aa_highp_;
  scoped_ptr<RenderPassMaskColorMatrixProgram>
      render_pass_mask_color_matrix_program_highp_;
  scoped_ptr<RenderPassMaskColorMatrixProgramAA>
      render_pass_mask_color_matrix_program_aa_highp_;

  scoped_ptr<VideoYUVProgram> video_yuv_program_;
  scoped_ptr<VideoStreamTextureProgram> video_stream_texture_program_;

  scoped_ptr<VideoYUVProgram> video_yuv_program_highp_;
  scoped_ptr<VideoStreamTextureProgram> video_stream_texture_program_highp_;

  scoped_ptr<DebugBorderProgram> debug_border_program_;
  scoped_ptr<SolidColorProgram> solid_color_program_;
  scoped_ptr<SolidColorProgramAA> solid_color_program_aa_;

  OutputSurface* output_surface_;
  WebKit::WebGraphicsContext3D* context_;

  gfx::Rect swap_buffer_rect_;
  gfx::Rect scissor_rect_;
  bool is_viewport_changed_;
#if defined(SBROWSER_BLOCK_SENDING_FRAME_MESSAGE)
  bool gesture_in_progress_;
  bool skiped_sending_frame_info_;
#endif
  bool is_backbuffer_discarded_;
  bool discard_backbuffer_when_not_visible_;
  bool is_using_bind_uniform_;
  bool visible_;
  bool is_scissor_enabled_;
  bool blend_shadow_;
  unsigned program_shadow_;
  TexturedQuadDrawCache draw_cache_;
  int highp_threshold_min_;

  struct PendingAsyncReadPixels;
  ScopedPtrVector<PendingAsyncReadPixels> pending_async_read_pixels_;

  scoped_ptr<ResourceProvider::ScopedWriteLockGL> current_framebuffer_lock_;

  scoped_refptr<ResourceProvider::Fence> last_swap_fence_;

  SkBitmap on_demand_tile_raster_bitmap_;
  ResourceProvider::ResourceId on_demand_tile_raster_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(GLRenderer);
};

// Setting DEBUG_GL_CALLS to 1 will call glGetError() after almost every GL
// call made by the compositor. Useful for debugging rendering issues but
// will significantly degrade performance.
#define DEBUG_GL_CALLS 0

#if DEBUG_GL_CALLS && !defined(NDEBUG)
#define GLC(context, x)                                                        \
  (x, GLRenderer::DebugGLCall(&* context, #x, __FILE__, __LINE__))
#else
#define GLC(context, x) (x)
#endif

}  // namespace cc

#endif  // CC_OUTPUT_GL_RENDERER_H_
