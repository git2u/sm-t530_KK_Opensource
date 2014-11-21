/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/canvas/CanvasPathMethods.h"

#ifdef SBROWSER_BATCHING
#include "core/platform/graphics/transforms/AffineTransform.h"
#include "third_party/skia/include/core/SkPath.h"
#endif
#include "core/dom/ExceptionCode.h"
#include "core/platform/graphics/FloatRect.h"
#include <wtf/MathExtras.h>

namespace WebCore {

void CanvasPathMethods::closePath()
{
    if (m_path.isEmpty())
        return;

    FloatRect boundRect = m_path.boundingRect();
    if (boundRect.width() || boundRect.height())
        m_path.closeSubpath();
}

void CanvasPathMethods::moveTo(float x, float y)
{
#ifdef SBROWSER_BATCHING
	if (x_disableBatching == false)
	{
		Path xPath;

		//Copy co-ordinates to xPath.	
		if (!std::isfinite(x) || !std::isfinite(y))
			return;
		if (!isTransformInvertible())
			return;
		xPath.moveTo(FloatPoint(x, y));


		//Translate, Scale and rotate if required.
		if (x_rotateRequested)
		{
			xPath.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
		}

		if (x_scaleRequested)
		{
			xPath.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
		}

		if (x_translateRequested)
		{
			xPath.transform(AffineTransform().translate(x_tx, x_ty));
		}


		//Copy manipulated xPath co-ordinates to m_path
		const SkPath& modPath = xPath.skPath();
		SkPoint pointToCopy;

		pointToCopy = modPath.getPoint(modPath.countPoints() - 1);
		m_path.moveTo(FloatPoint(pointToCopy.x(), pointToCopy.y()));
   	 }
	else
	{
#endif	
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    if (!isTransformInvertible())
        return;
    m_path.moveTo(FloatPoint(x, y));
#ifdef SBROWSER_BATCHING		
	}
#endif
}

void CanvasPathMethods::lineTo(float x, float y)
{
#ifdef SBROWSER_BATCHING
	if (x_disableBatching == false)
	{
	    Path xPath;

	    //Copy co-ordinates to xPath.	
	    if (!std::isfinite(x) || !std::isfinite(y))
	        return;
	    if (!isTransformInvertible())
	        return;
		
	    FloatPoint p1 = FloatPoint(x, y);
	    xPath.addLineTo(p1);


   	    //Translate, Scale and rotate if required.
	    if (x_rotateRequested)
	    {
		xPath.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
	    }
		
	    if (x_scaleRequested)
	    {
		xPath.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
	    }
		
	    if (x_translateRequested)
	    {
		xPath.transform(AffineTransform().translate(x_tx, x_ty));
	    }


	    //Copy manipulated xPath co-ordinates to m_path
	    const SkPath& modPath = xPath.skPath();
	    SkPoint pointToCopy;

	    pointToCopy = modPath.getPoint(modPath.countPoints() - 1);

	    FloatPoint p2 = FloatPoint(pointToCopy.x(), pointToCopy.y());

	    if (!m_path.hasCurrentPoint())
	    {
	       m_path.moveTo(p2);
	    }
	    else if (p2 != m_path.currentPoint())
	    {
		m_path.addLineTo(p2);
	    }
	}
	else
	{
#endif	
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    if (!isTransformInvertible())
        return;

    FloatPoint p1 = FloatPoint(x, y);
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(p1);
    else if (p1 != m_path.currentPoint())
        m_path.addLineTo(p1);
#ifdef SBROWSER_BATCHING			
	}
#endif
}

void CanvasPathMethods::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
#ifdef SBROWSER_BATCHING
	if (x_disableBatching == false) {

    //Batching does not support this method correctly as yet.
    x_doNotBatch = true;

	if (!std::isfinite(cpx) || !std::isfinite(cpy) || !std::isfinite(x) || !std::isfinite(y))
			return;
	if (!isTransformInvertible())
			return;


	Path xCP;
	xCP.moveTo(FloatPoint(cpx, cpy));

	//Translate, Scale and rotate if required.
	if (x_rotateRequested)
	{
		xCP.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
	}

	if (x_scaleRequested)
	{
		xCP.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
	}

	if (x_translateRequested)
	{
		xCP.transform(AffineTransform().translate(x_tx, x_ty));
	}



	Path xPath;
	xPath.moveTo(FloatPoint(x, y));

	if (x_rotateRequested)
	{
		xPath.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
	}

	if (x_scaleRequested)
	{
		xPath.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
	}

	if (x_translateRequested)
	{
		xPath.transform(AffineTransform().translate(x_tx, x_ty));
	}

	const SkPath& modXCP = xCP.skPath();
	SkPoint xCPPoint;
	xCPPoint = modXCP.getPoint(0);
		
	const SkPath& modXPath = xPath.skPath();
	SkPoint xPathPoint;	
	xPathPoint = modXPath.getPoint(0);


	if (!m_path.hasCurrentPoint())
			m_path.moveTo(FloatPoint(xCPPoint.x(), xCPPoint.y()));

	FloatPoint p1 = FloatPoint(xPathPoint.x(), xPathPoint.y());
	FloatPoint cp = FloatPoint(xCPPoint.x(), xCPPoint.y());
	if (p1 != m_path.currentPoint() || p1 != cp)
			m_path.addQuadCurveTo(cp, p1);

	}else  {
#endif
    if (!std::isfinite(cpx) || !std::isfinite(cpy) || !std::isfinite(x) || !std::isfinite(y))
        return;
    if (!isTransformInvertible())
        return;
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(FloatPoint(cpx, cpy));

    FloatPoint p1 = FloatPoint(x, y);
    FloatPoint cp = FloatPoint(cpx, cpy);
    if (p1 != m_path.currentPoint() || p1 != cp)
        m_path.addQuadCurveTo(cp, p1);
#ifdef SBROWSER_BATCHING			
	}
#endif		
}

void CanvasPathMethods::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
#ifdef SBROWSER_BATCHING
	if (x_disableBatching == false) {

	//Batching does not support this method correctly as yet.
	x_doNotBatch = true;

	if (!std::isfinite(cp1x) || !std::isfinite(cp1y) || !std::isfinite(cp2x) || !std::isfinite(cp2y) || !std::isfinite(x) || !std::isfinite(y))
			return;
	if (!isTransformInvertible())
			return;


	Path xCP1;
	xCP1.moveTo(FloatPoint(cp1x, cp1y));

	//Translate, Scale and rotate if required.
	if (x_rotateRequested)
	{
		xCP1.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
	}

	if (x_scaleRequested)
	{
		xCP1.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
	}

	if (x_translateRequested)
	{
		xCP1.transform(AffineTransform().translate(x_tx, x_ty));
	}


	Path xCP2;
	xCP2.moveTo(FloatPoint(cp2x, cp2y));

	//Translate, Scale and rotate if required.
	if (x_rotateRequested)
	{
		xCP2.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
	}

	if (x_scaleRequested)
	{
		xCP2.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
	}

	if (x_translateRequested)
	{
		xCP2.transform(AffineTransform().translate(x_tx, x_ty));
	}
	

	Path xPath;
	xPath.moveTo(FloatPoint(x, y));

	//Translate, Scale and rotate if required.
	if (x_rotateRequested)
	{
		xPath.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
	}

	if (x_scaleRequested)
	{
		xPath.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
	}

	if (x_translateRequested)
	{
		xPath.transform(AffineTransform().translate(x_tx, x_ty));
	}



	const SkPath& modXCP1 = xCP1.skPath();
	SkPoint xCP1Point;
	xCP1Point = modXCP1.getPoint(0);
		
	const SkPath& modXCP2 = xCP2.skPath();
	SkPoint xCP2Point;
	xCP2Point = modXCP2.getPoint(0);

	const SkPath& modXPath = xPath.skPath();
	SkPoint xPathPoint;	
	xPathPoint = modXPath.getPoint(0);


	if (!m_path.hasCurrentPoint())
			m_path.moveTo(FloatPoint(cp1x, cp1y));

	FloatPoint p1 = FloatPoint(xPathPoint.x(), xPathPoint.y());
	FloatPoint cp1 = FloatPoint(xCP1Point.x(), xCP1Point.y());
	FloatPoint cp2 = FloatPoint(xCP2Point.x(), xCP2Point.y());
	if (p1 != m_path.currentPoint() || p1 != cp1 ||  p1 != cp2)
			m_path.addBezierCurveTo(cp1, cp2, p1);

	}else {
#endif
    if (!std::isfinite(cp1x) || !std::isfinite(cp1y) || !std::isfinite(cp2x) || !std::isfinite(cp2y) || !std::isfinite(x) || !std::isfinite(y))
        return;
    if (!isTransformInvertible())
        return;
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(FloatPoint(cp1x, cp1y));

    FloatPoint p1 = FloatPoint(x, y);
    FloatPoint cp1 = FloatPoint(cp1x, cp1y);
    FloatPoint cp2 = FloatPoint(cp2x, cp2y);
    if (p1 != m_path.currentPoint() || p1 != cp1 ||  p1 != cp2)
        m_path.addBezierCurveTo(cp1, cp2, p1);
#ifdef SBROWSER_BATCHING
	}
#endif		
}

void CanvasPathMethods::arcTo(float x1, float y1, float x2, float y2, float r, ExceptionCode& ec)
{
#ifdef SBROWSER_BATCHING
	if (x_disableBatching == false) {

	//Batching does not support this method correctly as yet.
	x_doNotBatch = true;

	float radiusX=r;
	float radiusY=r;
	
	ec = 0;
	if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2) || !std::isfinite(r))
			return;

	if (r < 0) {
			ec = INDEX_SIZE_ERR;
			return;
	}

	if (!isTransformInvertible())
			return;


	Path xPath1;
	xPath1.moveTo(FloatPoint(x1, y1));

	//Translate, Scale and rotate if required.
	if (x_rotateRequested)
	{
		xPath1.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
	}

	if (x_scaleRequested)
	{
		xPath1.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
	}

	if (x_translateRequested)
	{
		xPath1.transform(AffineTransform().translate(x_tx, x_ty));
	}



	Path xPath2;
	xPath2.moveTo(FloatPoint(x2, y2));

	if (x_rotateRequested)
	{
		xPath2.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
	}

	if (x_scaleRequested)
	{
		xPath2.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
	}

	if (x_translateRequested)
	{
		xPath2.transform(AffineTransform().translate(x_tx, x_ty));
	}


	if (x_scaleRequested)
	{
		radiusX *= x_sx;
		radiusY *= x_sy;
	}

	const SkPath& modXPath1 = xPath1.skPath();
	SkPoint startPoint;
	startPoint = modXPath1.getPoint(0);
	FloatPoint xP1 = FloatPoint(startPoint.x(), startPoint.y());
		
	const SkPath& modXPath2 = xPath2.skPath();
	SkPoint endPoint;	
	endPoint = modXPath2.getPoint(0);
	FloatPoint xP2 = FloatPoint(endPoint.x(), endPoint.y());

	if (!m_path.hasCurrentPoint())
			m_path.moveTo(xP1);
	else if (xP1 == m_path.currentPoint() || xP1 == xP2 || !r)
			lineTo(x1, y1);
	else
			m_path.addArcTo(xP1, xP2, radiusX, radiusY);

	}else {
#endif
    ec = 0;
    if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2) || !std::isfinite(r))
        return;

    if (r < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!isTransformInvertible())
        return;

    FloatPoint p1 = FloatPoint(x1, y1);
    FloatPoint p2 = FloatPoint(x2, y2);

    if (!m_path.hasCurrentPoint())
        m_path.moveTo(p1);
    else if (p1 == m_path.currentPoint() || p1 == p2 || !r)
        lineTo(x1, y1);
    else
        m_path.addArcTo(p1, p2, r);
#ifdef SBROWSER_BATCHING
	}
#endif		
}

void CanvasPathMethods::arc(float x, float y, float r, float sa, float ea, bool anticlockwise, ExceptionCode& ec)
{
#ifdef SBROWSER_BATCHING
	if (x_disableBatching == false) {
	//Batching does not support this method correctly as yet.
	x_doNotBatch = true;

	float rx = r;
	float ry = r;

	Path xPath1;
	xPath1.moveTo(FloatPoint(x, y));

	//Translate, Scale and rotate if required.
	if (x_rotateRequested)
	{
		xPath1.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
	}

	if (x_scaleRequested)
	{
		xPath1.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));

		rx *= x_sx;
		ry *= x_sy;
	}

	if (x_translateRequested)
	{
		xPath1.transform(AffineTransform().translate(x_tx, x_ty));
	}

	const SkPath& modXPath1 = xPath1.skPath();
	SkPoint startPoint;
	startPoint = modXPath1.getPoint(0);

	ec = 0;
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(r) || !std::isfinite(sa) || !std::isfinite(ea))
			return;

	if (r < 0) {
			ec = INDEX_SIZE_ERR;
			return;
	}

	if (!r || sa == ea) {
			// The arc is empty but we still need to draw the connecting line.
			lineTo(x + r * cosf(sa), y + r * sinf(sa));
			return;
	}

	if (!isTransformInvertible())
			return;

	// If 'sa' and 'ea' differ by more than 2Pi, just add a circle starting/ending at 'sa'.
	if (anticlockwise && sa - ea >= 2 * piFloat) {
			m_path.addArc(FloatPoint(startPoint.x(), startPoint.y()), rx, ry, sa, sa - 2 * piFloat, anticlockwise);
			return;
	}
	if (!anticlockwise && ea - sa >= 2 * piFloat) {
			m_path.addArc(FloatPoint(startPoint.x(), startPoint.y()), rx, ry, sa, sa + 2 * piFloat, anticlockwise);
			return;
	}

	m_path.addArc(FloatPoint(startPoint.x(), startPoint.y()), rx, ry, sa, ea, anticlockwise);

	}else {
#endif
    ec = 0;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(r) || !std::isfinite(sa) || !std::isfinite(ea))
        return;

    if (r < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!r || sa == ea) {
        // The arc is empty but we still need to draw the connecting line.
        lineTo(x + r * cosf(sa), y + r * sinf(sa));
        return;
    }

    if (!isTransformInvertible())
        return;

    // If 'sa' and 'ea' differ by more than 2Pi, just add a circle starting/ending at 'sa'.
    if (anticlockwise && sa - ea >= 2 * piFloat) {
        m_path.addArc(FloatPoint(x, y), r, sa, sa - 2 * piFloat, anticlockwise);
        return;
    }
    if (!anticlockwise && ea - sa >= 2 * piFloat) {
        m_path.addArc(FloatPoint(x, y), r, sa, sa + 2 * piFloat, anticlockwise);
        return;
    }

    m_path.addArc(FloatPoint(x, y), r, sa, ea, anticlockwise);
#ifdef SBROWSER_BATCHING		
	}
#endif		
}

void CanvasPathMethods::rect(float x, float y, float width, float height)
{
#ifdef SBROWSER_BATCHING
	if (x_disableBatching == false) {
	//Batching does not support this method correctly as yet.
	x_doNotBatch = true;

	if (!isTransformInvertible())
			return;

	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height))
			return;


	Path xPath;
	
	//Copy co-ordinates to xPath. 
	if (!std::isfinite(x) || !std::isfinite(y))
		return;
	if (!isTransformInvertible())
		return;

	if (!width && !height) {

			xPath.moveTo(FloatPoint(x, y));

			//Translate, Scale and rotate if required.
			if (x_rotateRequested)
			{
				xPath.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
			}
			
			if (x_scaleRequested)
			{
				xPath.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
			}
			
			if (x_translateRequested)
			{
				xPath.transform(AffineTransform().translate(x_tx, x_ty));
			}

			const SkPath& modXPath = xPath.skPath();
			SkPoint point;
				
			//Starting Point
			point = modXPath.getPoint(0);
			m_path.moveTo(FloatPoint(point.x(),point.y()));
			return;
	}

	xPath.addRect(FloatRect(x, y, width, height));
	

	//Translate, Scale and rotate if required.
	if (x_rotateRequested)
	{
		xPath.transform(AffineTransform().rotate(x_rotateAngle / piDouble * 180.0));
	}

	if (x_scaleRequested)
	{
		xPath.transform(AffineTransform().scaleNonUniform(x_sx, x_sy));
	}

	if (x_translateRequested)
	{
		xPath.transform(AffineTransform().translate(x_tx, x_ty));
	}

	const SkPath& modXPath = xPath.skPath();
	SkPoint point;
		
	//Starting Point
	point = modXPath.getPoint(0);
	m_path.moveTo(FloatPoint(point.x(),point.y()));
	
	point = modXPath.getPoint(1);
	m_path.addLineTo(FloatPoint(point.x(),point.y()));

	point = modXPath.getPoint(2);
	m_path.addLineTo(FloatPoint(point.x(),point.y()));

	point = modXPath.getPoint(3);
	m_path.addLineTo(FloatPoint(point.x(),point.y()));

	m_path.closeSubpath();

	

	}else {
#endif
    if (!isTransformInvertible())
        return;

    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height))
        return;

    if (!width && !height) {
        m_path.moveTo(FloatPoint(x, y));
        return;
    }

    m_path.addRect(FloatRect(x, y, width, height));
#ifdef SBROWSER_BATCHING		
	}
#endif		
}
}
