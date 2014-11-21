/*
 * Copyright (c) Samsung Electronics, Inc.
 * All rights reserved.
 *
 */


#ifndef TRIANGULATE_H_
#define TRIANGULATE_H_

#include <vector>
#include "SkPoint.h"

typedef SkScalar TrFloat;

typedef SkPoint NativePoint;

typedef float OutFloat;

struct OutPoint
{
	OutFloat x;
	OutFloat y;
};


struct Triangle {
	OutPoint points[3];
};

class TriangulateImpl;

class Triangulate
{
public:
	Triangulate(NativePoint* points, size_t num);
	Triangulate();
	~Triangulate();
	Triangulate(const TriangulateImpl&); //not implemented
	void addPath(NativePoint* points, size_t num);
	bool compute();
	const std::vector<Triangle>& triangles() const;
	void reset();
private:
	TriangulateImpl* mImpl;
};



#endif //TRIANGULATE_G_

