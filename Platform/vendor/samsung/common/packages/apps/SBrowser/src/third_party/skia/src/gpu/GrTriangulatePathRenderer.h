/*
 * Copyright (c) Samsung Electronics, Inc.
 * All rights reserved.
 *
 */

#ifndef GrTriangulatePathRenderer_DEFINED
#define GrTriangulatePathRenderer_DEFINED

#include "GrPathRenderer.h"
#include "SkTemplates.h"
#include "triangulate.h"
#include <vector>


class GR_API GrTriangulatePathRenderer : public GrPathRenderer {
public:
    GrTriangulatePathRenderer();

    virtual bool canDrawPath(const SkPath& path,
                             const SkStrokeRec& stroke,
                             const GrDrawTarget* target,
                             bool antiAlias) const SK_OVERRIDE;

protected:
    virtual bool onDrawPath(const SkPath& path,
                            const SkStrokeRec& stroke,
                            GrDrawTarget* target,
                            bool antiAlias) SK_OVERRIDE;

private:

	bool createGeom(const SkPath& path, SkScalar srcSpaceTol) const;

	typedef std::vector<GrPoint> PointVector;
	mutable Triangulate mTriangulate;
	mutable PointVector mPoints;
};

#endif
