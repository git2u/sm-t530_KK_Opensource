/*
 * Copyright (c) Samsung Electronics, Inc.
 * All rights reserved.
 *
 */

//#define SRUK_DEBUG

#ifdef SRUK_DEBUG
 #ifdef ANDROID
#include <android/log.h>
  #define SRUKLOG(...) __android_log_print(ANDROID_LOG_DEBUG, "SRUK", __VA_ARGS__);
 #else
  #include "GrConfig.h"
  #define SRUKLOG(...) GrPrintf(__VA_ARGS__);
 #endif
#else
 #define SRUKLOG(...)
#endif


#include "GrTriangulatePathRenderer.h"

#include "GrContext.h"
#include "GrDrawState.h"
#include "GrDrawTarget.h"
#include "GrPathUtils.h"
#include "SkString.h"
#include "SkStrokeRec.h"
#include "SkTrace.h"

#include "SkTemplates.h"

#include <limits.h>
#include "triangulate.h"
#include <vector>


//tests whether the first and last
static bool hasCloseCrossover(NativePoint* points, size_t num)
{
	if (num < 4) {
		return false;
	}
	NativePoint* point1 = points;
	NativePoint* point2 = points + 1;
	NativePoint* point3 = points + num - 1;
	NativePoint* point4 = points + num - 2;

	TrFloat a1 = (point2->y() - point1->y()) / (point2->x() - point1->x());
	TrFloat b1 = point2->y() - a1 * point2->x();
	TrFloat a2 = (point4->y() - point3->y()) / (point4->x() - point3->x());
	TrFloat b2 = point4->y() - a2 * point4->x();
	if (a1 == a2) {
		return false;
	}
	TrFloat intX = - (b2 - b1) / (a2 - a1);
	SRUKLOG("Intx %f", intX);

	bool liesOnLine1 = point1->x() <= point2->x() && point1->x() <= intX && intX <= point2->x();
	liesOnLine1 = liesOnLine1 || point1->x() >= point2->x() && point1->x() >= intX && intX >= point2->x();
	bool liesOnLine2 = point3->x() <= point4->x() && point3->x() <= intX && intX <= point4->x();
	liesOnLine2 = liesOnLine2 || point3->x() >= point4->x() && point3->x() >= intX && intX >= point4->x();
	return liesOnLine1 && liesOnLine2;
}


bool GrTriangulatePathRenderer::createGeom(const SkPath& path, SkScalar srcSpaceTol) const
{
	const size_t kRedundantPointNumber = 0;

#ifdef SRUK_DEBUG
	path.dump(false, __func__);
#endif
	mPoints.clear();
    SkScalar srcSpaceTolSqd = SkScalarMul(srcSpaceTol, srcSpaceTol);
#if 1
    int contourCnt;
    int maxPts = GrPathUtils::worstCasePointCount(path, &contourCnt,
                                                  srcSpaceTol);

    if (maxPts <= 0) {
        return false;
    }
    if (maxPts > ((int)SK_MaxU16 + 1)) {
        GrPrintf("Path not rendered, too many verts (%d)\n", maxPts);
        return false;
    }

	mPoints.reserve(maxPts + 16); //the 16 is just some extra
#endif
	size_t subpathStart = mPoints.size();

    GrPoint pts[4];

    bool first = true;

    SkPath::Iter iter(path, false);

    for (;;) {
        GrPathCmd cmd = (GrPathCmd)iter.next(pts);
        switch (cmd) {
            case kMove_PathCmd:
                if (mPoints.size() - subpathStart >= 3) {
#ifdef SRUK_DEBUG
					SRUKLOG("path is:");
					for (int i = subpathStart; i < mPoints.size() - kRedundantPointNumber ; ++i) {
						SRUKLOG("\t(%6.3f, %6.3f)", mPoints[i].x(), mPoints[i].y() );
					}
					SRUKLOG("end points\n");
#endif
					int pathSize =  mPoints.size() - subpathStart - kRedundantPointNumber;
					if (hasCloseCrossover(&mPoints[subpathStart],pathSize) ) {
						--pathSize;
					}
					mTriangulate.addPath(&mPoints[subpathStart], pathSize);
					SRUKLOG("First point is (%6.3f, %6.3f)", mPoints[0].x(), mPoints[0].y() );
				}
				subpathStart = mPoints.size();
				mPoints.push_back(pts[0]);
                break;
            case kLine_PathCmd:
				mPoints.push_back(pts[1]);
                break;
            case kQuadratic_PathCmd: {
                uint16_t numPts =  GrPathUtils::quadraticPointCount(pts, srcSpaceTol);
				GrPoint* end = &mPoints[mPoints.size()];
				mPoints.resize(numPts + mPoints.size() );
                    GrPathUtils::generateQuadraticPoints(
                            pts[0], pts[1], pts[2],
                            srcSpaceTolSqd, &end, numPts);
				mPoints.resize(end - &mPoints[0]);
                break;
            }
            case kCubic_PathCmd: {
                uint16_t numPts = GrPathUtils::cubicPointCount(pts, srcSpaceTol);
				GrPoint* tmp = &mPoints[mPoints.size()];
				mPoints.resize(numPts + mPoints.size() );
                GrPathUtils::generateCubicPoints(
                                pts[0], pts[1], pts[2], pts[3],
                                srcSpaceTolSqd, &tmp, numPts);
				mPoints.resize(tmp - &mPoints[0]);
                break;
            }
            case kClose_PathCmd:
                break;
            case kEnd_PathCmd:
             // uint16_t currIdx = (uint16_t) (vert - base);
                goto FINISHED;
        }
        first = false;
    }
FINISHED:
#ifdef SRUK_DEBUG
		SRUKLOG("path is:");
		for (int i = subpathStart; i < mPoints.size() - kRedundantPointNumber; ++i) {
			SRUKLOG("\t(%6.3f, %6.3f)", mPoints[i].x(), mPoints[i].y() );
		}
		SRUKLOG("end points\n");
#endif
		{
			int pathSize =  mPoints.size() - subpathStart - kRedundantPointNumber;
			if (hasCloseCrossover(&mPoints[subpathStart],pathSize) ) {
					--pathSize;
			}
			mTriangulate.addPath(&mPoints[subpathStart], pathSize);
		}
    return true;
}

GrTriangulatePathRenderer::GrTriangulatePathRenderer() {}

bool GrTriangulatePathRenderer::onDrawPath(const SkPath& path,
                                        const SkStrokeRec& stroke,
                                        GrDrawTarget* target,
                                        bool antiAlias) {

	target->drawState()->setDefaultVertexAttribs();

	const std::vector<Triangle>& tris = mTriangulate.triangles();
	size_t sz = tris.size();
   	target->setVertexSourceToArray(&tris[0], sz * 3);
   	target->drawNonIndexed(kTriangles_GrPrimitiveType, 0, sz * 3);
    return true;
}

bool GrTriangulatePathRenderer::canDrawPath(const SkPath& path,
                                         const SkStrokeRec& stroke,
                                         const GrDrawTarget* target,
                                         bool antiAlias) const {
	if (!antiAlias) {
		return false;
	}
	if (!stroke.isFillStyle()) {
		return false;
	}

	SkMatrix viewM = target->getDrawState().getViewMatrix();
    SkScalar tol = SK_Scalar1;
    tol = GrPathUtils::scaleToleranceToSrc(tol, viewM, path.getBounds());

	mTriangulate.reset();
    if (!createGeom(path, tol)) {
        return false;
    }

	return mTriangulate.compute();
}


