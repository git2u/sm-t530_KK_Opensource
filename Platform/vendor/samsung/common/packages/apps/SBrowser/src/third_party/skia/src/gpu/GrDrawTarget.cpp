
/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */



#include "GrDrawTarget.h"
#include "GrContext.h"
#include "GrDrawTargetCaps.h"
#include "GrRenderTarget.h"
#include "GrTexture.h"
#include "GrVertexBuffer.h"

#include "SkStrokeRec.h"

SK_DEFINE_INST_COUNT(GrDrawTarget)

////////////////////////////////////////////////////////////////////////////////

GrDrawTarget::DrawInfo& GrDrawTarget::DrawInfo::operator =(const DrawInfo& di) {
    fPrimitiveType  = di.fPrimitiveType;
    fStartVertex    = di.fStartVertex;
    fStartIndex     = di.fStartIndex;
    fVertexCount    = di.fVertexCount;
    fIndexCount     = di.fIndexCount;

    fInstanceCount          = di.fInstanceCount;
    fVerticesPerInstance    = di.fVerticesPerInstance;
    fIndicesPerInstance     = di.fIndicesPerInstance;

    if (NULL != di.fDevBounds) {
        GrAssert(di.fDevBounds == &di.fDevBoundsStorage);
        fDevBoundsStorage = di.fDevBoundsStorage;
        fDevBounds = &fDevBoundsStorage;
    } else {
        fDevBounds = NULL;
    }

    fDstCopy = di.fDstCopy;

    return *this;
}

#if GR_DEBUG
bool GrDrawTarget::DrawInfo::isInstanced() const {
    if (fInstanceCount > 0) {
        GrAssert(0 == fIndexCount % fIndicesPerInstance);
        GrAssert(0 == fVertexCount % fVerticesPerInstance);
        GrAssert(fIndexCount / fIndicesPerInstance == fInstanceCount);
        GrAssert(fVertexCount / fVerticesPerInstance == fInstanceCount);
        // there is no way to specify a non-zero start index to drawIndexedInstances().
        GrAssert(0 == fStartIndex);
        return true;
    } else {
        GrAssert(!fVerticesPerInstance);
        GrAssert(!fIndicesPerInstance);
        return false;
    }
}
#endif

void GrDrawTarget::DrawInfo::adjustInstanceCount(int instanceOffset) {
    GrAssert(this->isInstanced());
    GrAssert(instanceOffset + fInstanceCount >= 0);
    fInstanceCount += instanceOffset;
    fVertexCount = fVerticesPerInstance * fInstanceCount;
    fIndexCount = fIndicesPerInstance * fInstanceCount;
}

void GrDrawTarget::DrawInfo::adjustStartVertex(int vertexOffset) {
    fStartVertex += vertexOffset;
    GrAssert(fStartVertex >= 0);
}

void GrDrawTarget::DrawInfo::adjustStartIndex(int indexOffset) {
    GrAssert(this->isIndexed());
    fStartIndex += indexOffset;
    GrAssert(fStartIndex >= 0);
}

////////////////////////////////////////////////////////////////////////////////

#define DEBUG_INVAL_BUFFER 0xdeadcafe
#define DEBUG_INVAL_START_IDX -1

GrDrawTarget::GrDrawTarget(GrContext* context)
    : fClip(NULL)
    , fContext(context) {
    GrAssert(NULL != context);

    fDrawState = &fDefaultDrawState;
    // We assume that fDrawState always owns a ref to the object it points at.
    fDefaultDrawState.ref();
    GeometrySrcState& geoSrc = fGeoSrcStateStack.push_back();
#if GR_DEBUG
    geoSrc.fVertexCount = DEBUG_INVAL_START_IDX;
    geoSrc.fVertexBuffer = (GrVertexBuffer*)DEBUG_INVAL_BUFFER;
    geoSrc.fIndexCount = DEBUG_INVAL_START_IDX;
    geoSrc.fIndexBuffer = (GrIndexBuffer*)DEBUG_INVAL_BUFFER;
#endif
    geoSrc.fVertexSrc = kNone_GeometrySrcType;
    geoSrc.fIndexSrc  = kNone_GeometrySrcType;
}

GrDrawTarget::~GrDrawTarget() {
    GrAssert(1 == fGeoSrcStateStack.count());
    SkDEBUGCODE(GeometrySrcState& geoSrc = fGeoSrcStateStack.back());
    GrAssert(kNone_GeometrySrcType == geoSrc.fIndexSrc);
    GrAssert(kNone_GeometrySrcType == geoSrc.fVertexSrc);
    fDrawState->unref();
}

void GrDrawTarget::releaseGeometry() {
    int popCnt = fGeoSrcStateStack.count() - 1;
    while (popCnt) {
        this->popGeometrySource();
        --popCnt;
    }
    this->resetVertexSource();
    this->resetIndexSource();
}

void GrDrawTarget::setClip(const GrClipData* clip) {
    clipWillBeSet(clip);
    fClip = clip;
}

const GrClipData* GrDrawTarget::getClip() const {
    return fClip;
}

void GrDrawTarget::setDrawState(GrDrawState*  drawState) {
    GrAssert(NULL != fDrawState);
    if (NULL == drawState) {
        drawState = &fDefaultDrawState;
    }
    if (fDrawState != drawState) {
        fDrawState->unref();
        drawState->ref();
        fDrawState = drawState;
    }
}

bool GrDrawTarget::reserveVertexSpace(size_t vertexSize,
                                      int vertexCount,
                                      void** vertices) {
    GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    bool acquired = false;
    if (vertexCount > 0) {
        GrAssert(NULL != vertices);
        this->releasePreviousVertexSource();
        geoSrc.fVertexSrc = kNone_GeometrySrcType;

        acquired = this->onReserveVertexSpace(vertexSize,
                                              vertexCount,
                                              vertices);
    }
    if (acquired) {
        geoSrc.fVertexSrc = kReserved_GeometrySrcType;
        geoSrc.fVertexCount = vertexCount;
        geoSrc.fVertexSize = vertexSize;
    } else if (NULL != vertices) {
        *vertices = NULL;
    }
    return acquired;
}

bool GrDrawTarget::reserveIndexSpace(int indexCount,
                                     void** indices) {
    GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    bool acquired = false;
    if (indexCount > 0) {
        GrAssert(NULL != indices);
        this->releasePreviousIndexSource();
        geoSrc.fIndexSrc = kNone_GeometrySrcType;

        acquired = this->onReserveIndexSpace(indexCount, indices);
    }
    if (acquired) {
        geoSrc.fIndexSrc = kReserved_GeometrySrcType;
        geoSrc.fIndexCount = indexCount;
    } else if (NULL != indices) {
        *indices = NULL;
    }
    return acquired;

}

bool GrDrawTarget::reserveVertexAndIndexSpace(int vertexCount,
                                              int indexCount,
                                              void** vertices,
                                              void** indices) {
    size_t vertexSize = this->drawState()->getVertexSize();
    this->willReserveVertexAndIndexSpace(vertexCount, indexCount);
    if (vertexCount) {
        if (!this->reserveVertexSpace(vertexSize, vertexCount, vertices)) {
            if (indexCount) {
                this->resetIndexSource();
            }
            return false;
        }
    }
    if (indexCount) {
        if (!this->reserveIndexSpace(indexCount, indices)) {
            if (vertexCount) {
                this->resetVertexSource();
            }
            return false;
        }
    }
    return true;
}

bool GrDrawTarget::geometryHints(int32_t* vertexCount,
                                 int32_t* indexCount) const {
    if (NULL != vertexCount) {
        *vertexCount = -1;
    }
    if (NULL != indexCount) {
        *indexCount = -1;
    }
    return false;
}

void GrDrawTarget::releasePreviousVertexSource() {
    GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    switch (geoSrc.fVertexSrc) {
        case kNone_GeometrySrcType:
            break;
        case kArray_GeometrySrcType:
            this->releaseVertexArray();
            break;
        case kReserved_GeometrySrcType:
            this->releaseReservedVertexSpace();
            break;
        case kBuffer_GeometrySrcType:
            geoSrc.fVertexBuffer->unref();
#if GR_DEBUG
            geoSrc.fVertexBuffer = (GrVertexBuffer*)DEBUG_INVAL_BUFFER;
#endif
            break;
        default:
            GrCrash("Unknown Vertex Source Type.");
            break;
    }
}

void GrDrawTarget::releasePreviousIndexSource() {
    GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    switch (geoSrc.fIndexSrc) {
        case kNone_GeometrySrcType:   // these two don't require
            break;
        case kArray_GeometrySrcType:
            this->releaseIndexArray();
            break;
        case kReserved_GeometrySrcType:
            this->releaseReservedIndexSpace();
            break;
        case kBuffer_GeometrySrcType:
            geoSrc.fIndexBuffer->unref();
#if GR_DEBUG
            geoSrc.fIndexBuffer = (GrIndexBuffer*)DEBUG_INVAL_BUFFER;
#endif
            break;
        default:
            GrCrash("Unknown Index Source Type.");
            break;
    }
}

void GrDrawTarget::setVertexSourceToArray(const void* vertexArray,
                                          int vertexCount) {
    this->releasePreviousVertexSource();
    GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    geoSrc.fVertexSrc = kArray_GeometrySrcType;
    geoSrc.fVertexSize = this->drawState()->getVertexSize();
    geoSrc.fVertexCount = vertexCount;
    this->onSetVertexSourceToArray(vertexArray, vertexCount);
}

void GrDrawTarget::setIndexSourceToArray(const void* indexArray,
                                         int indexCount) {
    this->releasePreviousIndexSource();
    GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    geoSrc.fIndexSrc = kArray_GeometrySrcType;
    geoSrc.fIndexCount = indexCount;
    this->onSetIndexSourceToArray(indexArray, indexCount);
}

void GrDrawTarget::setVertexSourceToBuffer(const GrVertexBuffer* buffer) {
    this->releasePreviousVertexSource();
    GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    geoSrc.fVertexSrc    = kBuffer_GeometrySrcType;
    geoSrc.fVertexBuffer = buffer;
    buffer->ref();
    geoSrc.fVertexSize = this->drawState()->getVertexSize();
}

void GrDrawTarget::setIndexSourceToBuffer(const GrIndexBuffer* buffer) {
    this->releasePreviousIndexSource();
    GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    geoSrc.fIndexSrc     = kBuffer_GeometrySrcType;
    geoSrc.fIndexBuffer  = buffer;
    buffer->ref();
}

void GrDrawTarget::resetVertexSource() {
    this->releasePreviousVertexSource();
    GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    geoSrc.fVertexSrc = kNone_GeometrySrcType;
}

void GrDrawTarget::resetIndexSource() {
    this->releasePreviousIndexSource();
    GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    geoSrc.fIndexSrc = kNone_GeometrySrcType;
}

void GrDrawTarget::pushGeometrySource() {
    this->geometrySourceWillPush();
    GeometrySrcState& newState = fGeoSrcStateStack.push_back();
    newState.fIndexSrc = kNone_GeometrySrcType;
    newState.fVertexSrc = kNone_GeometrySrcType;
#if GR_DEBUG
    newState.fVertexCount  = ~0;
    newState.fVertexBuffer = (GrVertexBuffer*)~0;
    newState.fIndexCount   = ~0;
    newState.fIndexBuffer = (GrIndexBuffer*)~0;
#endif
}

void GrDrawTarget::popGeometrySource() {
    // if popping last element then pops are unbalanced with pushes
    GrAssert(fGeoSrcStateStack.count() > 1);

    this->geometrySourceWillPop(fGeoSrcStateStack.fromBack(1));
    this->releasePreviousVertexSource();
    this->releasePreviousIndexSource();
    fGeoSrcStateStack.pop_back();
}

////////////////////////////////////////////////////////////////////////////////

bool GrDrawTarget::checkDraw(GrPrimitiveType type, int startVertex,
                             int startIndex, int vertexCount,
                             int indexCount) const {
    const GrDrawState& drawState = this->getDrawState();
#if GR_DEBUG
    const GeometrySrcState& geoSrc = fGeoSrcStateStack.back();
    int maxVertex = startVertex + vertexCount;
    int maxValidVertex;
    switch (geoSrc.fVertexSrc) {
        case kNone_GeometrySrcType:
            GrCrash("Attempting to draw without vertex src.");
        case kReserved_GeometrySrcType: // fallthrough
        case kArray_GeometrySrcType:
            maxValidVertex = geoSrc.fVertexCount;
            break;
        case kBuffer_GeometrySrcType:
            maxValidVertex = geoSrc.fVertexBuffer->sizeInBytes() / geoSrc.fVertexSize;
            break;
    }
    if (maxVertex > maxValidVertex) {
        GrCrash("Drawing outside valid vertex range.");
    }
    if (indexCount > 0) {
        int maxIndex = startIndex + indexCount;
        int maxValidIndex;
        switch (geoSrc.fIndexSrc) {
            case kNone_GeometrySrcType:
                GrCrash("Attempting to draw indexed geom without index src.");
            case kReserved_GeometrySrcType: // fallthrough
            case kArray_GeometrySrcType:
                maxValidIndex = geoSrc.fIndexCount;
                break;
            case kBuffer_GeometrySrcType:
                maxValidIndex = geoSrc.fIndexBuffer->sizeInBytes() / sizeof(uint16_t);
                break;
        }
        if (maxIndex > maxValidIndex) {
            GrCrash("Index reads outside valid index range.");
        }
    }

    GrAssert(NULL != drawState.getRenderTarget());
    for (int s = 0; s < GrDrawState::kNumStages; ++s) {
        if (drawState.isStageEnabled(s)) {
            const GrEffectRef& effect = *drawState.getStage(s).getEffect();
            int numTextures = effect->numTextures();
            for (int t = 0; t < numTextures; ++t) {
                GrTexture* texture = effect->texture(t);
                GrAssert(texture->asRenderTarget() != drawState.getRenderTarget());
            }
        }
    }

    GrAssert(drawState.validateVertexAttribs());
#endif
    if (NULL == drawState.getRenderTarget()) {
        return false;
    }
    return true;
}

bool GrDrawTarget::setupDstReadIfNecessary(DrawInfo* info) {
    if (this->caps()->dstReadInShaderSupport() || !this->getDrawState().willEffectReadDst()) {
        return true;
    }
    GrRenderTarget* rt = this->drawState()->getRenderTarget();

    const GrClipData* clip = this->getClip();
    GrIRect copyRect;
    clip->getConservativeBounds(this->getDrawState().getRenderTarget(), &copyRect);
    SkIRect drawIBounds;
    if (info->getDevIBounds(&drawIBounds)) {
        if (!copyRect.intersect(drawIBounds)) {
#if GR_DEBUG
            GrPrintf("Missed an early reject. Bailing on draw from setupDstReadIfNecessary.\n");
#endif
            return false;
        }
    } else {
#if GR_DEBUG
        //GrPrintf("No dev bounds when dst copy is made.\n");
#endif
    }

    // MSAA consideration: When there is support for reading MSAA samples in the shader we could
    // have per-sample dst values by making the copy multisampled.
    GrTextureDesc desc;
    this->initCopySurfaceDstDesc(rt, &desc);
    desc.fWidth = copyRect.width();
    desc.fHeight = copyRect.height();

    GrAutoScratchTexture ast(fContext, desc, GrContext::kApprox_ScratchTexMatch);

    if (NULL == ast.texture()) {
        GrPrintf("Failed to create temporary copy of destination texture.\n");
        return false;
    }
    SkIPoint dstPoint = {0, 0};
    if (this->copySurface(ast.texture(), rt, copyRect, dstPoint)) {
        info->fDstCopy.setTexture(ast.texture());
        info->fDstCopy.setOffset(copyRect.fLeft, copyRect.fTop);
        return true;
    } else {
        return false;
    }
}

void GrDrawTarget::drawIndexed(GrPrimitiveType type,
                               int startVertex,
                               int startIndex,
                               int vertexCount,
                               int indexCount,
                               const SkRect* devBounds) {
    if (indexCount > 0 && this->checkDraw(type, startVertex, startIndex, vertexCount, indexCount)) {
        DrawInfo info;
        info.fPrimitiveType = type;
        info.fStartVertex   = startVertex;
        info.fStartIndex    = startIndex;
        info.fVertexCount   = vertexCount;
        info.fIndexCount    = indexCount;

        info.fInstanceCount         = 0;
        info.fVerticesPerInstance   = 0;
        info.fIndicesPerInstance    = 0;

        if (NULL != devBounds) {
            info.setDevBounds(*devBounds);
        }
        // TODO: We should continue with incorrect blending.
        if (!this->setupDstReadIfNecessary(&info)) {
            return;
        }
        this->onDraw(info);
    }
}

void GrDrawTarget::drawNonIndexed(GrPrimitiveType type,
                                  int startVertex,
                                  int vertexCount,
                                  const SkRect* devBounds) {
    if (vertexCount > 0 && this->checkDraw(type, startVertex, -1, vertexCount, -1)) {
        DrawInfo info;
        info.fPrimitiveType = type;
        info.fStartVertex   = startVertex;
        info.fStartIndex    = 0;
        info.fVertexCount   = vertexCount;
        info.fIndexCount    = 0;

        info.fInstanceCount         = 0;
        info.fVerticesPerInstance   = 0;
        info.fIndicesPerInstance    = 0;

        if (NULL != devBounds) {
            info.setDevBounds(*devBounds);
        }
        // TODO: We should continue with incorrect blending.
        if (!this->setupDstReadIfNecessary(&info)) {
            return;
        }
        this->onDraw(info);
    }
}

void GrDrawTarget::stencilPath(const GrPath* path, const SkStrokeRec& stroke, SkPath::FillType fill) {
    // TODO: extract portions of checkDraw that are relevant to path stenciling.
    GrAssert(NULL != path);
    GrAssert(this->caps()->pathStencilingSupport());
    GrAssert(!stroke.isHairlineStyle());
    GrAssert(!SkPath::IsInverseFillType(fill));
    this->onStencilPath(path, stroke, fill);
}

////////////////////////////////////////////////////////////////////////////////

bool GrDrawTarget::willUseHWAALines() const {
    // There is a conflict between using smooth lines and our use of premultiplied alpha. Smooth
    // lines tweak the incoming alpha value but not in a premul-alpha way. So we only use them when
    // our alpha is 0xff and tweaking the color for partial coverage is OK
    if (!this->caps()->hwAALineSupport() ||
        !this->getDrawState().isHWAntialiasState()) {
        return false;
    }
    GrDrawState::BlendOptFlags opts = this->getDrawState().getBlendOpts();
    return (GrDrawState::kDisableBlend_BlendOptFlag & opts) &&
           (GrDrawState::kCoverageAsAlpha_BlendOptFlag & opts);
}

bool GrDrawTarget::canApplyCoverage() const {
    // we can correctly apply coverage if a) we have dual source blending
    // or b) one of our blend optimizations applies.
    return this->caps()->dualSourceBlendingSupport() ||
           GrDrawState::kNone_BlendOpt != this->getDrawState().getBlendOpts(true);
}

////////////////////////////////////////////////////////////////////////////////

void GrDrawTarget::drawIndexedInstances(GrPrimitiveType type,
                                        int instanceCount,
                                        int verticesPerInstance,
                                        int indicesPerInstance,
                                        const SkRect* devBounds) {
    if (!verticesPerInstance || !indicesPerInstance) {
        return;
    }

    int maxInstancesPerDraw = this->indexCountInCurrentSource() / indicesPerInstance;
    if (!maxInstancesPerDraw) {
        return;
    }

    DrawInfo info;
    info.fPrimitiveType = type;
    info.fStartIndex = 0;
    info.fStartVertex = 0;
    info.fIndicesPerInstance = indicesPerInstance;
    info.fVerticesPerInstance = verticesPerInstance;

    // Set the same bounds for all the draws.
    if (NULL != devBounds) {
        info.setDevBounds(*devBounds);
    }
    // TODO: We should continue with incorrect blending.
    if (!this->setupDstReadIfNecessary(&info)) {
        return;
    }

    while (instanceCount) {
        info.fInstanceCount = GrMin(instanceCount, maxInstancesPerDraw);
        info.fVertexCount = info.fInstanceCount * verticesPerInstance;
        info.fIndexCount = info.fInstanceCount * indicesPerInstance;

        if (this->checkDraw(type,
                            info.fStartVertex,
                            info.fStartIndex,
                            info.fVertexCount,
                            info.fIndexCount)) {
            this->onDraw(info);
        }
        info.fStartVertex += info.fVertexCount;
        instanceCount -= info.fInstanceCount;
    }
}

////////////////////////////////////////////////////////////////////////////////

namespace {

// position + (optional) texture coord
extern const GrVertexAttrib gBWRectPosUVAttribs[] = {
    {kVec2f_GrVertexAttribType, 0,               kPosition_GrVertexAttribBinding},
    {kVec2f_GrVertexAttribType, sizeof(GrPoint), kLocalCoord_GrVertexAttribBinding}
};

void set_vertex_attributes(GrDrawState* drawState, bool hasUVs) {
    if (hasUVs) {
        drawState->setVertexAttribs<gBWRectPosUVAttribs>(2);
    } else {
        drawState->setVertexAttribs<gBWRectPosUVAttribs>(1);
    }
}

};

void GrDrawTarget::onDrawRect(const GrRect& rect,
                              const SkMatrix* matrix,
                              const GrRect* localRect,
                              const SkMatrix* localMatrix) {

    GrDrawState::AutoViewMatrixRestore avmr;
    if (NULL != matrix) {
        avmr.set(this->drawState(), *matrix);
    }

    set_vertex_attributes(this->drawState(), NULL != localRect);

    AutoReleaseGeometry geo(this, 4, 0);
    if (!geo.succeeded()) {
        GrPrintf("Failed to get space for vertices!\n");
        return;
    }

    size_t vsize = this->drawState()->getVertexSize();
    geo.positions()->setRectFan(rect.fLeft, rect.fTop, rect.fRight, rect.fBottom, vsize);
    if (NULL != localRect) {
        GrPoint* coords = GrTCast<GrPoint*>(GrTCast<intptr_t>(geo.vertices()) +
                                            sizeof(GrPoint));
        coords->setRectFan(localRect->fLeft, localRect->fTop,
                           localRect->fRight, localRect->fBottom,
                           vsize);
        if (NULL != localMatrix) {
            localMatrix->mapPointsWithStride(coords, vsize, 4);
        }
    }
    SkTLazy<SkRect> bounds;
    if (this->getDrawState().willEffectReadDst()) {
        bounds.init();
        this->getDrawState().getViewMatrix().mapRect(bounds.get(), rect);
    }

    this->drawNonIndexed(kTriangleFan_GrPrimitiveType, 0, 4, bounds.getMaybeNull());
}

void GrDrawTarget::clipWillBeSet(const GrClipData* clipData) {
}

////////////////////////////////////////////////////////////////////////////////

GrDrawTarget::AutoStateRestore::AutoStateRestore() {
    fDrawTarget = NULL;
}

GrDrawTarget::AutoStateRestore::AutoStateRestore(GrDrawTarget* target,
                                                 ASRInit init) {
    fDrawTarget = NULL;
    this->set(target, init);
}

GrDrawTarget::AutoStateRestore::~AutoStateRestore() {
    if (NULL != fDrawTarget) {
        fDrawTarget->setDrawState(fSavedState);
        fSavedState->unref();
    }
}

void GrDrawTarget::AutoStateRestore::set(GrDrawTarget* target, ASRInit init) {
    GrAssert(NULL == fDrawTarget);
    fDrawTarget = target;
    fSavedState = target->drawState();
    GrAssert(fSavedState);
    fSavedState->ref();
    if (kReset_ASRInit == init) {
        // calls the default cons
        fTempState.init();
    } else {
        GrAssert(kPreserve_ASRInit == init);
        // calls the copy cons
        fTempState.set(*fSavedState);
    }
    target->setDrawState(fTempState.get());
}

////////////////////////////////////////////////////////////////////////////////

GrDrawTarget::AutoReleaseGeometry::AutoReleaseGeometry(
                                         GrDrawTarget*  target,
                                         int vertexCount,
                                         int indexCount) {
    fTarget = NULL;
    this->set(target, vertexCount, indexCount);
}

GrDrawTarget::AutoReleaseGeometry::AutoReleaseGeometry() {
    fTarget = NULL;
}

GrDrawTarget::AutoReleaseGeometry::~AutoReleaseGeometry() {
    this->reset();
}

bool GrDrawTarget::AutoReleaseGeometry::set(GrDrawTarget*  target,
                                            int vertexCount,
                                            int indexCount) {
    this->reset();
    fTarget = target;
    bool success = true;
    if (NULL != fTarget) {
        fTarget = target;
        success = target->reserveVertexAndIndexSpace(vertexCount,
                                                     indexCount,
                                                     &fVertices,
                                                     &fIndices);
        if (!success) {
            fTarget = NULL;
            this->reset();
        }
    }
    GrAssert(success == (NULL != fTarget));
    return success;
}

void GrDrawTarget::AutoReleaseGeometry::reset() {
    if (NULL != fTarget) {
        if (NULL != fVertices) {
            fTarget->resetVertexSource();
        }
        if (NULL != fIndices) {
            fTarget->resetIndexSource();
        }
        fTarget = NULL;
    }
    fVertices = NULL;
    fIndices = NULL;
}

GrDrawTarget::AutoClipRestore::AutoClipRestore(GrDrawTarget* target, const SkIRect& newClip) {
    fTarget = target;
    fClip = fTarget->getClip();
    fStack.init();
    fStack.get()->clipDevRect(newClip, SkRegion::kReplace_Op);
    fReplacementClip.fClipStack = fStack.get();
    target->setClip(&fReplacementClip);
}

namespace {
// returns true if the read/written rect intersects the src/dst and false if not.
bool clip_srcrect_and_dstpoint(const GrSurface* dst,
                               const GrSurface* src,
                               const SkIRect& srcRect,
                               const SkIPoint& dstPoint,
                               SkIRect* clippedSrcRect,
                               SkIPoint* clippedDstPoint) {
    *clippedSrcRect = srcRect;
    *clippedDstPoint = dstPoint;

    // clip the left edge to src and dst bounds, adjusting dstPoint if necessary
    if (clippedSrcRect->fLeft < 0) {
        clippedDstPoint->fX -= clippedSrcRect->fLeft;
        clippedSrcRect->fLeft = 0;
    }
    if (clippedDstPoint->fX < 0) {
        clippedSrcRect->fLeft -= clippedDstPoint->fX;
        clippedDstPoint->fX = 0;
    }

    // clip the top edge to src and dst bounds, adjusting dstPoint if necessary
    if (clippedSrcRect->fTop < 0) {
        clippedDstPoint->fY -= clippedSrcRect->fTop;
        clippedSrcRect->fTop = 0;
    }
    if (clippedDstPoint->fY < 0) {
        clippedSrcRect->fTop -= clippedDstPoint->fY;
        clippedDstPoint->fY = 0;
    }

    // clip the right edge to the src and dst bounds.
    if (clippedSrcRect->fRight > src->width()) {
        clippedSrcRect->fRight = src->width();
    }
    if (clippedDstPoint->fX + clippedSrcRect->width() > dst->width()) {
        clippedSrcRect->fRight = clippedSrcRect->fLeft + dst->width() - clippedDstPoint->fX;
    }

    // clip the bottom edge to the src and dst bounds.
    if (clippedSrcRect->fBottom > src->height()) {
        clippedSrcRect->fBottom = src->height();
    }
    if (clippedDstPoint->fY + clippedSrcRect->height() > dst->height()) {
        clippedSrcRect->fBottom = clippedSrcRect->fTop + dst->height() - clippedDstPoint->fY;
    }

    // The above clipping steps may have inverted the rect if it didn't intersect either the src or
    // dst bounds.
    return !clippedSrcRect->isEmpty();
}
}

bool GrDrawTarget::copySurface(GrSurface* dst,
                               GrSurface* src,
                               const SkIRect& srcRect,
                               const SkIPoint& dstPoint) {
    GrAssert(NULL != dst);
    GrAssert(NULL != src);

    SkIRect clippedSrcRect;
    SkIPoint clippedDstPoint;
    // If the rect is outside the src or dst then we've already succeeded.
    if (!clip_srcrect_and_dstpoint(dst,
                                   src,
                                   srcRect,
                                   dstPoint,
                                   &clippedSrcRect,
                                   &clippedDstPoint)) {
        GrAssert(this->canCopySurface(dst, src, srcRect, dstPoint));
        return true;
    }

    bool result = this->onCopySurface(dst, src, clippedSrcRect, clippedDstPoint);
    GrAssert(result == this->canCopySurface(dst, src, clippedSrcRect, clippedDstPoint));
    return result;
}

bool GrDrawTarget::canCopySurface(GrSurface* dst,
                                  GrSurface* src,
                                  const SkIRect& srcRect,
                                  const SkIPoint& dstPoint) {
    GrAssert(NULL != dst);
    GrAssert(NULL != src);

    SkIRect clippedSrcRect;
    SkIPoint clippedDstPoint;
    // If the rect is outside the src or dst then we're guaranteed success
    if (!clip_srcrect_and_dstpoint(dst,
                                   src,
                                   srcRect,
                                   dstPoint,
                                   &clippedSrcRect,
                                   &clippedDstPoint)) {
        return true;
    }
    return this->onCanCopySurface(dst, src, clippedSrcRect, clippedDstPoint);
}

bool GrDrawTarget::onCanCopySurface(GrSurface* dst,
                                    GrSurface* src,
                                    const SkIRect& srcRect,
                                    const SkIPoint& dstPoint) {
    // Check that the read/write rects are contained within the src/dst bounds.
    GrAssert(!srcRect.isEmpty());
    GrAssert(SkIRect::MakeWH(src->width(), src->height()).contains(srcRect));
    GrAssert(dstPoint.fX >= 0 && dstPoint.fY >= 0);
    GrAssert(dstPoint.fX + srcRect.width() <= dst->width() &&
             dstPoint.fY + srcRect.height() <= dst->height());

    return !dst->isSameAs(src) && NULL != dst->asRenderTarget() && NULL != src->asTexture();
}

bool GrDrawTarget::onCopySurface(GrSurface* dst,
                                 GrSurface* src,
                                 const SkIRect& srcRect,
                                 const SkIPoint& dstPoint) {
    if (!GrDrawTarget::onCanCopySurface(dst, src, srcRect, dstPoint)) {
        return false;
    }

    GrRenderTarget* rt = dst->asRenderTarget();
    GrTexture* tex = src->asTexture();

    GrDrawTarget::AutoStateRestore asr(this, kReset_ASRInit);
    this->drawState()->setRenderTarget(rt);
    SkMatrix matrix;
    matrix.setTranslate(SkIntToScalar(srcRect.fLeft - dstPoint.fX),
                        SkIntToScalar(srcRect.fTop - dstPoint.fY));
    matrix.postIDiv(tex->width(), tex->height());
    this->drawState()->createTextureEffect(0, tex, matrix);
    SkIRect dstRect = SkIRect::MakeXYWH(dstPoint.fX,
                                        dstPoint.fY,
                                        srcRect.width(),
                                        srcRect.height());
    this->drawSimpleRect(dstRect);
    return true;
}

void GrDrawTarget::initCopySurfaceDstDesc(const GrSurface* src, GrTextureDesc* desc) {
    // Make the dst of the copy be a render target because the default copySurface draws to the dst.
    desc->fOrigin = kDefault_GrSurfaceOrigin;
    desc->fFlags = kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
    desc->fConfig = src->config();
}

///////////////////////////////////////////////////////////////////////////////

SK_DEFINE_INST_COUNT(GrDrawTargetCaps)

void GrDrawTargetCaps::reset() {
    f8BitPaletteSupport = false;
    fNPOTTextureTileSupport = false;
    fTwoSidedStencilSupport = false;
    fStencilWrapOpsSupport = false;
    fHWAALineSupport = false;
    fShaderDerivativeSupport = false;
    fGeometryShaderSupport = false;
    fDualSourceBlendingSupport = false;
    fBufferLockSupport = false;
    fPathStencilingSupport = false;

    fMaxRenderTargetSize = 0;
    fMaxTextureSize = 0;
    fMaxSampleCount = 0;
}

GrDrawTargetCaps& GrDrawTargetCaps::operator=(const GrDrawTargetCaps& other) {
    f8BitPaletteSupport = other.f8BitPaletteSupport;
    fNPOTTextureTileSupport = other.fNPOTTextureTileSupport;
    fTwoSidedStencilSupport = other.fTwoSidedStencilSupport;
    fStencilWrapOpsSupport = other.fStencilWrapOpsSupport;
    fHWAALineSupport = other.fHWAALineSupport;
    fShaderDerivativeSupport = other.fShaderDerivativeSupport;
    fGeometryShaderSupport = other.fGeometryShaderSupport;
    fDualSourceBlendingSupport = other.fDualSourceBlendingSupport;
    fBufferLockSupport = other.fBufferLockSupport;
    fPathStencilingSupport = other.fPathStencilingSupport;

    fMaxRenderTargetSize = other.fMaxRenderTargetSize;
    fMaxTextureSize = other.fMaxTextureSize;
    fMaxSampleCount = other.fMaxSampleCount;

    return *this;
}

void GrDrawTargetCaps::print() const {
    static const char* gNY[] = {"NO", "YES"};
    GrPrintf("8 Bit Palette Support       : %s\n", gNY[f8BitPaletteSupport]);
    GrPrintf("NPOT Texture Tile Support   : %s\n", gNY[fNPOTTextureTileSupport]);
    GrPrintf("Two Sided Stencil Support   : %s\n", gNY[fTwoSidedStencilSupport]);
    GrPrintf("Stencil Wrap Ops  Support   : %s\n", gNY[fStencilWrapOpsSupport]);
    GrPrintf("HW AA Lines Support         : %s\n", gNY[fHWAALineSupport]);
    GrPrintf("Shader Derivative Support   : %s\n", gNY[fShaderDerivativeSupport]);
    GrPrintf("Geometry Shader Support     : %s\n", gNY[fGeometryShaderSupport]);
    GrPrintf("Dual Source Blending Support: %s\n", gNY[fDualSourceBlendingSupport]);
    GrPrintf("Buffer Lock Support         : %s\n", gNY[fBufferLockSupport]);
    GrPrintf("Path Stenciling Support     : %s\n", gNY[fPathStencilingSupport]);
    GrPrintf("Dst Read In Shader Support  : %s\n", gNY[fDstReadInShaderSupport]);
    GrPrintf("Max Texture Size            : %d\n", fMaxTextureSize);
    GrPrintf("Max Render Target Size      : %d\n", fMaxRenderTargetSize);
    GrPrintf("Max Sample Count            : %d\n", fMaxSampleCount);
}
