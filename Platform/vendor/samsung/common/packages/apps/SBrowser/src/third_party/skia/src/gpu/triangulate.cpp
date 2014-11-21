/*
 * Copyright (c) Samsung Electronics, Inc.
 * All rights reserved.
 *
 */

#include <vector>
#include <deque>
#include <list>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>
#include "triangulate.h"
#include "setjmp.h"
#ifdef ANDROID
#include <android/log.h>
#endif

#if 0
//#ifndef NDEBUG
 #ifdef ANDROID
  #define SRUKLOG(...) __android_log_print(ANDROID_LOG_DEBUG, "SRUK", __VA_ARGS__);
 #else
  #include "GrConfig.h"
  #define SRUKLOG(...) GrPrintf(__VA_ARGS__);
 #endif
#else
 #define SRUKLOG(...)
#endif

//Android does not support C++ exceptions
//fortunately in our compute phase we do not allocate memory to any stack pointers
//therefore no destructors ever need to get called on cleanup
//this allows us to use setjmp and longjmp for our exception handling mechanism


static __thread jmp_buf gJmpBuf; //we want this thread safe

#define THROW(x) longjmp(gJmpBuf, x )

#undef assert
#ifdef ANDROID
#define assert(x) \
	if (!(x)) { \
		__android_log_print(ANDROID_LOG_DEBUG, "SRUK", "Error occurred while triangulating path (%s:%d) assert: " #x "\n", __FILE__,__LINE__);\
		THROW( __LINE__); \
	}
#else
#define assert(x) \
	if (!(x)) { \
		fprintf(stderr, "Error occurred while triangulating path (%s:%d) assert: " #x "\n", __FILE__,__LINE__);\
		THROW( __LINE__); \
	}
#endif


class TriangulateImpl
{
public:
	TriangulateImpl(NativePoint* points, size_t num);
	TriangulateImpl();
	TriangulateImpl(const TriangulateImpl&);//not implemented
	~TriangulateImpl();
	void addPath(NativePoint* points, size_t num);
	bool compute();
	const std::vector<Triangle>& triangles() const;
	void reset();

private:
	struct Edge;
	struct Region;
	struct Node;

	class Point
	{
	public:
		enum Type {
			eV, //point at the bottom
			eA, //point at the top
			eC // point on the side think <
		};
		enum RegionPos {
			eLeftRegion = 0,
			eMiddleRegion = 1,
			eRightRegion = 2,
			eMaxAdjacentRegions = 3
		};
		Point (const NativePoint* np);
		TrFloat x() const;
		TrFloat y() const;

		inline bool liesAbove(const Point* point) const;
		inline bool liesToTheLeftOf(const Point* point) const;
		bool liesToTheLeftOf(const Edge* edge) const ;
		void addRegionAbove(Region* region);
		Region* nextRegionAbove(RegionPos pos);
		const Region* nextRegionAbove(RegionPos pos) const;
		void setPointType(const Point* prev, const Point* next);

		Region* regionAbove[eMaxAdjacentRegions];
		Node* node;
		Type type;
		mutable Point* nextTmpPolyPoint;

	private:
		void addAdjacentRegion(Region** regionArr, Region* region);

		float m_x;
		float m_y;

	};

	struct Edge
	{
		Edge(Point* a, Point* b);
		bool liesAbove(const Point* point) const;
		bool liesToTheLeftOf(const Edge* edge) const;
		Point* topPoint;
		Point* bottomPoint;
	};

	struct Region
	{
		Region();
		Point* topPoint;
		Point* bottomPoint;
		Edge* leftEdge;
		Edge* rightEdge;
		Node* node;

		bool isInsidePoly() const;
		bool pointInRegion(const Point* point) const;

		mutable bool triangulated; //The number of triangles that come from this region
	};

	struct Node
	{
		enum NodeType {
			ePoint,
			eEdge,
			eRegion
		};
		Node(Region* r);
		void setNode (Edge* e);
		void setNode (Point* p);
		Node* navigateToNextNode(Point* point);
		static const char* typeToStr(NodeType type);

		NodeType type;
		void* data;
		Node* lhs;
		Node* rhs;
	};

	Node* getNodeForRegion(Region* region);
	Region* newRegion();

	void processEdge(Edge* edge);
	void addEdgeToTree(Edge* edge);
	void maybeAddPointToTree(Point* point);
	void triangulateFromTrapezoids();
	TrFloat pointVectorCross(const Point* a, const Point* pivot, const Point* b);

	Node* mTree;
	typedef std::deque<Point> PointStoreType;
	PointStoreType mPoints;
	typedef std::deque<Edge> EdgeStoreType;
	EdgeStoreType mEdges;
	typedef std::vector<Region> RegionStoreType;
	RegionStoreType mRegions;
	typedef std::vector<Node> NodeStoreType;
	NodeStoreType mNodeStore;

	//Needed for triangulation
	void triangulateUpEdge(const Region* region, const Edge* edge, Point::RegionPos pos);
	void triangulateFromRegion(const Region* region);
	void processMonotonePoly();
	typedef std::vector<Point*> VPointType;
	VPointType mVPoints; //both edges above point
	std::vector<Triangle> mTriangles;

	//tmpPoly
	mutable Point* mFirstTmpPoly;
	mutable Point* mLastTmpPoly;

};

TriangulateImpl::TriangulateImpl() :
	mTree (NULL),
	mFirstTmpPoly(NULL),
	mLastTmpPoly(NULL)
{
	srand(5);
}

TriangulateImpl::TriangulateImpl(NativePoint* points, size_t num) :
	mTree (NULL)
{
	addPath(points, num);
	srand(5);
}
TriangulateImpl::~TriangulateImpl()
{}

void TriangulateImpl::addPath(NativePoint* points, size_t num)
{
	assert(num > 2);
	size_t beginEl = mPoints.size();
	//mPoints.reserve(num);
	mVPoints.reserve(num / 2); //no more than half the points can be vpoints
	NativePoint* p = points;
	NativePoint* pe = points + num;

	SRUKLOG("StartPath:\n");
	while (p < pe) {
		mPoints.push_back(Point(p));
		SRUKLOG("Adding point %p (%5.3f,%5.3f)\n", &mPoints.back(), p->x(), p->y());
		++p;
	}
	SRUKLOG("EndPath:\n\n");

	size_t endEl = mPoints.size();

	size_t currEl = beginEl;
	size_t nextEl = beginEl + 1;
	size_t prevEl = endEl - 1;

	Point* currP;
	Point* nextP;
	Point* prevP;
	while (nextEl != endEl) {
		currP = &mPoints[currEl];
		nextP = &mPoints[nextEl];
		prevP = &mPoints[prevEl];
		mEdges.push_back(Edge(currP, nextP) );
		currP->setPointType(prevP, nextP);
		if (currP->type == Point::eV) {
			mVPoints.push_back(currP);
		}
		prevEl = currEl;
		currEl++;
		nextEl++;
	}
	//Now join the last point to the first
	nextEl = beginEl;
	currP = &mPoints[currEl];
	nextP = &mPoints[nextEl];
	prevP = &mPoints[prevEl];
	mEdges.push_back(Edge(currP, nextP) );
	currP->setPointType(prevP, nextP);
	if (currP->type == Point::eV) {
		mVPoints.push_back(currP);
	}
}

void TriangulateImpl::reset()
{
	mTree = NULL;
	mPoints.clear();
	mEdges.clear();
	mRegions.clear();
	mNodeStore.clear();
	mVPoints.clear(); //both edges above point
	mTriangles.clear();
	mFirstTmpPoly = NULL;
	mLastTmpPoly = NULL;
}

static int myRand(int max)
{
	return rand() % max;
}

bool TriangulateImpl::compute()
{
	mRegions.reserve(4 * mEdges.size());
	mNodeStore.reserve(mEdges.size() * 16);

	int ret = setjmp(gJmpBuf);
	if (ret == 0) { //means that this is our initial statement
		assert(mTree == NULL);
		Region* region = newRegion();
		mTree = getNodeForRegion(region);

		//first shuffle the edges
		std::random_shuffle(mEdges.begin(), mEdges.end(), myRand);
		EdgeStoreType::iterator it;
		for (it = mEdges.begin(); it != mEdges.end(); ++it) {
			processEdge(&(*it));
		}
		triangulateFromTrapezoids();
		return true;
	} else {
		//we have returned from a longjmp so something has gone wrong
		reset();
		return false;
	}
	triangulateFromTrapezoids();
}


const std::vector<Triangle>& TriangulateImpl::triangles() const
{
	return mTriangles;
}

TriangulateImpl::Point::Point (const NativePoint* np) :
	regionAbove (),
	node (NULL),
	nextTmpPolyPoint(NULL),
	m_x (np->x()),
	m_y (np->y())
{}

TrFloat TriangulateImpl::Point::x() const
{
	return m_x;
}

TrFloat TriangulateImpl::Point::y() const
{
	return m_y;
}

inline bool TriangulateImpl::Point::liesAbove(const Point* point) const
{
	//in the algorithm we assume that no points have equal height
	//we ensure that this is always true by comparing x values if the y values are equal
	assert(this != point);
	if ( y() != point->y() ) {
		return y() > point->y();
	} else {
		return this > point;
	}
}

inline bool TriangulateImpl::Point::liesToTheLeftOf(const Point* point) const
{
	//in the algorithm we assume that no points have equal height
	//we ensure that this is always true by comparing x values if the y values are equal
	assert(this != point);
	if ( x() != point->x() ) {
		return x() < point->x();
	} else {
		return this < point;
	}
}

//returns y corresponding to x given (x1,y1) (x1,x2)
static TrFloat interpolate(TrFloat x1, TrFloat y1, TrFloat x2, TrFloat y2, TrFloat x)
{
    /*     y2 - y1
    ret = -------- ( x - x1) + y1
           x2 - x1
    */
//#ifndef DEBUGLOGGING
#if 0
	//check that we are actualy interpolating and not extrapolating
	if (x1 > x2) {
		assert (x >= x2);
		assert (x <= x1);
	} else {
		assert (x >= x2);
		assert (x <= x1);
	}
#endif
	return (y2 - y1) / (x2 - x1) * (x - x1) + y1;
}


bool TriangulateImpl::Point::liesToTheLeftOf(const Edge* edge) const
{
	if (this == edge->topPoint) {
		return liesToTheLeftOf(edge->bottomPoint);
	}
	if (this == edge->bottomPoint) {
		return liesToTheLeftOf(edge->topPoint);
	}
/*
	if (liesAbove(edge->topPoint)) {
		return false;
	}
	if (!liesAbove(edge->bottomPoint)) {
		return true;
	}
*/
	if (liesToTheLeftOf(edge->topPoint) && liesToTheLeftOf(edge->bottomPoint)) {
		return true;
	}
	if (!liesToTheLeftOf(edge->topPoint) && !liesToTheLeftOf(edge->bottomPoint)) {
		return false;
	}
	//assert(!liesAbove(edge->topPoint));
	//assert(liesAbove(edge->bottomPoint));
	const Point* pa = edge->topPoint;
	const Point* pb = edge->bottomPoint;
	TrFloat px = interpolate(  pa->y(), pa->x(), pb->y(), pb-> x(), y() );
	assert(x() != px);
	return x() < px;
}

void TriangulateImpl::Point::addAdjacentRegion(Region** regionArr, Region* region)
{
	if (regionArr[eLeftRegion] == NULL) {
		regionArr[eLeftRegion] = region;
		regionArr[eRightRegion] = region;
	} else if (region->rightEdge != NULL && regionArr[eLeftRegion]->leftEdge == region->rightEdge) {
		if (regionArr[eRightRegion] != regionArr[eLeftRegion]) {
			regionArr[eMiddleRegion] = regionArr[eLeftRegion];
		}
		regionArr[eLeftRegion] = region;
	} else if (region->leftEdge != NULL && regionArr[eRightRegion]->rightEdge == region->leftEdge) {
		if (regionArr[eLeftRegion] != regionArr[eRightRegion]) {
			regionArr[eMiddleRegion] = regionArr[eRightRegion];
		}
		regionArr[eRightRegion] = region;
	} else {
		regionArr[eMiddleRegion] = region;
	}
}

void TriangulateImpl::Point::addRegionAbove(Region* region)
{
 	addAdjacentRegion(regionAbove, region);
}

const TriangulateImpl::Region* TriangulateImpl::Point::nextRegionAbove(RegionPos pos) const
{
	return regionAbove[pos];
}

TriangulateImpl::Region* TriangulateImpl::Point::nextRegionAbove(RegionPos pos)
{
	return regionAbove[pos];
}

void TriangulateImpl::Point::setPointType(const Point* prev, const Point* next)
{
	bool isPPrevAbove = prev->liesAbove(this);
	bool isPNextAbove = next->liesAbove(this);
	if (isPPrevAbove && isPNextAbove) {
		type = Point::eV;
	} else if (!isPPrevAbove && !isPNextAbove) {
		type = Point::eA;
	} else {
		type = Point::eC;
	}
}


TriangulateImpl::Edge::Edge(Point* a, Point* b)
{
	if (a->liesAbove(b)) {
		topPoint = a;
		bottomPoint = b;
	} else {
		topPoint  = b;
		bottomPoint = a;
	}
}

bool TriangulateImpl::Edge::liesAbove(const Point* point) const
{
	if (topPoint == point) {
		return false;
	}
	if (bottomPoint == point) {
		return true;
	}
	if (bottomPoint->liesAbove(point)) {
		return true;
	}
	if (!topPoint->liesAbove(point)) {
		return false;
	}
	assert(!bottomPoint->liesAbove(point));
	assert(topPoint->liesAbove(point));
	TrFloat py = interpolate(topPoint->x(), topPoint->y(), bottomPoint->x(), bottomPoint->y(), point->x());
	assert(py != point->y());
	return py > point->y();
}

bool TriangulateImpl::Edge::liesToTheLeftOf(const Edge* rhs) const
{
	//we must have overlap for this to work
	assert (topPoint->liesAbove(rhs->bottomPoint));
	assert (rhs->topPoint->liesAbove(bottomPoint));

	const Point* point1;
	const Edge* edge1;
	bool edge1First;
	const Point* point2;
	const Edge* edge2;
	bool edge2First;

	bool ret1 = false;
	bool ret2 = false;
	bool topPointEqual = topPoint == rhs->topPoint;
	if (!topPointEqual) {
		if (topPoint->liesAbove(rhs->topPoint)) {
			point1 = rhs->topPoint;
			edge1 = this;
			edge1First = true;
		} else {
			point1 = topPoint;
			edge1 = rhs;
			edge1First = false;
		}
		ret1 = edge1First ^ point1->liesToTheLeftOf(edge1);
#ifndef DEBUGLOGGING
		return ret1;
#else
		ret2 = ret1;
#endif
	}
#ifndef DEBUGLOGGING
	else
#else
	if (bottomPoint != rhs->bottomPoint)
#endif
	{
		if (bottomPoint->liesAbove(rhs->bottomPoint)) {
			point2 = bottomPoint;
			edge2 = rhs;
			edge2First = false;
		} else {
			point2 = rhs->bottomPoint;
			edge2 = this;
			edge2First = true;
		}
		ret2 = edge2First ^ point2->liesToTheLeftOf(edge2);
#ifndef DEBUGLOGGING
		return ret2;
#else
		if (topPointEqual) {
			ret1 = ret2;
		}
#endif
	}
#ifdef DEBUGLOGGING
	assert (ret1 == ret2);
	return ret1;
#endif
}

TriangulateImpl::Region::Region() :
	topPoint (NULL),
	bottomPoint (NULL),
	leftEdge (NULL),
	rightEdge (NULL),
	node (NULL),
	triangulated(false)
{}

bool TriangulateImpl::Region::isInsidePoly() const
{
	int boundaryCount = 0;
	const Region* region = this;
	while (true)
	{
		if ( region->leftEdge == NULL || region->rightEdge == NULL) {
			//not having edges means the current region is outside the poly
			//if we have crossed the boundary an odd number of times however
			//this means the original region lies inside the poly
			return boundaryCount & 1;
		} else {
			assert(region->rightEdge->topPoint->type != Point::eV);
			if (region->rightEdge->topPoint->type == Point::eC) {
				++boundaryCount;
			} else {
				const Edge* edge = region->rightEdge;
				const Point* point = region->topPoint;
				while (point != edge->topPoint) {
					region = point->nextRegionAbove(Point::eRightRegion);
					point = region->topPoint;
				}
				if (region->leftEdge && region->leftEdge->topPoint == point) {
					boundaryCount++;
				}
			}
			region = region->rightEdge->topPoint->nextRegionAbove(Point::eRightRegion);
		}
	}
}

bool  TriangulateImpl::Region::pointInRegion(const Point* point) const
{
	if (topPoint && point->liesAbove(topPoint)) {
		return false;
	}
	if (bottomPoint && !point->liesAbove(bottomPoint) ) {
		return false;
	}
	if (leftEdge && point->liesToTheLeftOf(leftEdge)) {
		return false;
	}
	if (rightEdge && !point->liesToTheLeftOf(rightEdge)) {
		return false;
	}
	return true;
}

TriangulateImpl::Node::Node(Region* r):
	type (eRegion),
	data ( r ),
	lhs ( NULL),
	rhs (NULL)
{
	r->node = this;
}

void TriangulateImpl::Node::setNode(Edge* e)
{
	assert(type == eRegion);
	Region* region = static_cast<Region*>(data);
	region->node = NULL;
	type = eEdge;
	data = e;
}

void TriangulateImpl::Node::setNode (Point* p)
{
	assert(type == eRegion);
	Region* region = static_cast<Region*>(data);
	region->node = NULL;
	type = ePoint;
	data = p;
	p->node = this;
}


TriangulateImpl::Node* TriangulateImpl::Node::navigateToNextNode(Point* point)
{
//	assert(data != NULL);
//	assert(type == Node::ePoint || type  == Node::eEdge);
	if (type == Node::ePoint) {
		//we navidate left if our point lies below (ie is smaller than) the node's point
		Point* nodepoint = static_cast<Point*>(data);
		return point->liesAbove(nodepoint) ? rhs : lhs;
	} else {
		//we navidate left if our point lies to the left of the nodes edge
		Edge* nodeedge = static_cast<Edge*>(data);
		return point->liesToTheLeftOf(nodeedge) ? lhs : rhs;
	}
}

const char* TriangulateImpl::Node::typeToStr(NodeType type)
{
	switch(type) {
	case Node::ePoint:
		return "Point";
	case Node::eEdge:
		return "Edge";
	case Node::eRegion:
		return "Region";
	default:
		return "unknown";
	}
}


TriangulateImpl::Node* TriangulateImpl::getNodeForRegion(Region* region)
{
	if (region->node) {
		return region->node;
	}
#ifdef DEBUGLOGGING
	assert(mNodeStore.size() < mNodeStore.capacity() + 1);
	{
		NodeStoreType::iterator it;
		for (it = mNodeStore.begin(); it != mNodeStore.end(); ++it) {
			assert(it->data != region);
		}
	}
#endif
	mNodeStore.push_back(Node(region));
	Node* ret = &mNodeStore.back();
	region->node = ret;
	return ret;
}



TriangulateImpl::Region* TriangulateImpl::newRegion() {
	assert(mRegions.size() < mRegions.capacity() + 1);
	mRegions.push_back(Region());
	return &mRegions.back();
}



void TriangulateImpl::maybeAddPointToTree(Point* point)
{
	assert(mTree);
	if (point->node) {
		return;
	}
	Node* node = mTree;
#ifdef DEBULOGGING
	Node* nodeList[mNodeStore.size()];
	memset(nodeList,0, sizeof(nodeList));
	Node** prevNode = nodeList;
#endif
	while (node->type != Node::eRegion) {
#ifdef DEBULOGGING
		*prevNode = node;
		++prevNode;
#endif
		node = node->navigateToNextNode(point);
		assert (node != NULL);
	}
	Region* bottomRegion = static_cast<Region*>(node->data);

	//@TODO fix the bug that makes this workaround required
#ifdef DEBULOGGING
	if (!bottomRegion->pointInRegion(point)) {
		SRUKLOG("Error finding region in tree, will try to brute force it node (%p)\n", node);
		--prevNode;
		while (prevNode > nodeList) {
			SRUKLOG("PrevNode -> node (%p) type (%s) data (%p)\n",
					*prevNode, Node::typeToStr((*prevNode)->type) , (*prevNode)->data);
			--prevNode;
		}

		bottomRegion = NULL;

		{
			RegionStoreType::iterator it;
			for (it = mRegions.begin(); it != mRegions.end(); ++it) {
				if (it->pointInRegion(point)) {
					bottomRegion = &(*it);
					break;
				}
			}
		}
		assert(bottomRegion);
		node = bottomRegion->node;
		SRUKLOG("Needed to find node (%p)\n", node);

		SRUKLOG("\n===Dumping Node Tree\n\n");
		{
			NodeStoreType::iterator it;
			for (it = mNodeStore.begin(); it != mNodeStore.end(); ++it) {
				SRUKLOG("Node (%p), type(%-6s), data (%p) lhs(%p), rhs(%p)\n",
						&(*it), Node::typeToStr(it->type), it->data, it->lhs, it->rhs);
			}
		}

		SRUKLOG("Base Node (%p)\n", mTree);
		SRUKLOG("\n===EndDump\n\n");

		SRUKLOG("\n===Dumping Regions\n\n");
		{
			RegionStoreType::iterator it;
			for (it = mRegions.begin(); it != mRegions.end(); ++it) {
				SRUKLOG("Region %p topPoint (%p), bottomPoint(%p), leftEdge (%p) rightEdge(%p)\n",
						&*it, it->topPoint, it->bottomPoint, it->leftEdge, it->rightEdge);
			}
		}

		SRUKLOG("Base Node (%p)\n", mTree);
		SRUKLOG("\n===EndDump\n\n");


	assert(0);
	}
#endif
	assert(bottomRegion->pointInRegion(point));
	Region* topRegion = newRegion();
	topRegion->leftEdge = bottomRegion->leftEdge;
	topRegion->rightEdge = bottomRegion->rightEdge;
	assert(topRegion->topPoint == NULL);
	topRegion->topPoint = bottomRegion->topPoint;
	topRegion->bottomPoint = point;
	bottomRegion->topPoint = point;
	assert(point->regionAbove[0] == NULL);
	point->addRegionAbove(topRegion);
	assert(topRegion->topPoint == NULL ||  topRegion->topPoint->liesAbove(topRegion->bottomPoint));
	assert(bottomRegion->bottomPoint == NULL || bottomRegion->topPoint->liesAbove(bottomRegion->bottomPoint));
	assert(bottomRegion->topPoint == topRegion->bottomPoint);
	node->setNode(point);
	node->lhs = getNodeForRegion(bottomRegion);
	node->rhs = getNodeForRegion(topRegion);
}



void TriangulateImpl::processEdge(Edge* edge)
{
	bool i = rand() & 1;
	SRUKLOG("Adding edge %s %p (%5.2f,%5.2f)[%p] - (%5.2f,%5.2f)[%p]\n",
			i ? "*" : "",
			edge,
			edge->topPoint->x(),
			edge->topPoint->y(),
			edge->topPoint,
			edge->bottomPoint->x(),
			edge->bottomPoint->y(),
			edge->bottomPoint);

	//need to add points to tree randomly
	//we may want to remove randomization for when only a single point is added
	if (i) {
		maybeAddPointToTree(edge->topPoint);
		maybeAddPointToTree(edge->bottomPoint);
	} else {
		maybeAddPointToTree(edge->bottomPoint);
		maybeAddPointToTree(edge->topPoint);
	}
	addEdgeToTree(edge);
}

void TriangulateImpl::addEdgeToTree(Edge* edge)
{
	Region** r = edge->bottomPoint->regionAbove;
	Region* region1;
	bool gotoNextRegion;
	do {
		region1 = *r;
		assert(r - edge->bottomPoint->regionAbove < Point::eMaxAdjacentRegions);
		if (region1 == NULL) {
			gotoNextRegion = true;
		} else if (region1->leftEdge && edge->liesToTheLeftOf(region1->leftEdge)) {
			gotoNextRegion = true;
		} else if (region1->rightEdge && !edge->liesToTheLeftOf(region1->rightEdge)) {
			gotoNextRegion = true;
		} else {
			gotoNextRegion = false;
		}
		++r;
	} while (gotoNextRegion);

	Node* node = region1->node;
	//we split region1 into region1 and region2 with region2 to the right of region1
	Region* region2 = newRegion();
	//region1 retains its left edge
	assert(region1->bottomPoint == edge->bottomPoint);
	region2->bottomPoint = region1->bottomPoint;
	region2->rightEdge = region1->rightEdge;
	region1->rightEdge = edge;
	region2->leftEdge = edge;
	Point* nextPoint = region1->topPoint;
	assert(nextPoint);
	if (edge->topPoint == nextPoint) {
		assert(region2->topPoint == NULL);
		region2->topPoint = nextPoint;
	} else if (nextPoint->liesToTheLeftOf(edge) ) {
		//let region1 retain its top point and region2 remains NULL
	} else {
		assert(region2->topPoint == NULL);
		region2->topPoint = nextPoint;
	}
	edge->bottomPoint->addRegionAbove(region2);
	node->setNode(edge);
	node->lhs = getNodeForRegion(region1);
	node->rhs = getNodeForRegion(region2);

	while( nextPoint != edge->topPoint) {
		bool goRight  = nextPoint->liesToTheLeftOf(edge);
		if (goRight) {
			region1->topPoint = nextPoint;
			//merge right region so can reuse left region
			region1 = nextPoint->nextRegionAbove(goRight ? Point::eRightRegion : Point::eLeftRegion);
			node = region1->node;
			region1->rightEdge = edge;
			nextPoint = region1->topPoint;
		} else {
			region2->topPoint = nextPoint;
			//merge left region1 so can reuse right region
			region2 = nextPoint->nextRegionAbove(goRight ? Point::eRightRegion : Point::eLeftRegion);
			node = region2->node;
			region2->leftEdge = edge;
			nextPoint = region2->topPoint;
		}
		assert(nextPoint);
		if (edge->topPoint == nextPoint) {
			region1->topPoint = nextPoint;
			region2->topPoint = nextPoint;
		}
		node->setNode(edge);
		node->lhs = getNodeForRegion(region1);
		node->rhs = getNodeForRegion(region2);
	}
}

Triangulate::Triangulate(NativePoint* points, size_t num)
{
	mImpl = new TriangulateImpl(points, num);
}

Triangulate::Triangulate()
{
	mImpl = new TriangulateImpl();
}

void Triangulate::addPath(NativePoint* points, size_t num)
{
	mImpl->addPath(points,num);
}

Triangulate::~Triangulate() {
	delete mImpl;
}

bool Triangulate::compute()
{
	return mImpl->compute();
}

const std::vector<Triangle>& Triangulate::triangles() const
{
	return mImpl->triangles();
}


void Triangulate::reset()
{
	mImpl->reset();
}


void TriangulateImpl::triangulateUpEdge(const Region* region, const Edge* edge, Point::RegionPos pos)
{
	if (region->topPoint == edge->topPoint) {
		return;
	}
	mFirstTmpPoly = region->bottomPoint;
	mLastTmpPoly = region->bottomPoint;
	while (region->topPoint != edge->topPoint) {
		assert(region->topPoint);
		region = region->topPoint->nextRegionAbove(pos);
		mLastTmpPoly->nextTmpPolyPoint = region->bottomPoint;
		mLastTmpPoly = region->bottomPoint;
	}
	mLastTmpPoly->nextTmpPolyPoint = region->topPoint;
	mLastTmpPoly = region->topPoint;
	processMonotonePoly();
}

void TriangulateImpl::triangulateFromRegion(const Region* region)
{
	if (region->triangulated) {
		return;
	}
	region->triangulated = true;
	assert(region->leftEdge);
	assert(region->rightEdge);
	if (region->topPoint != region->leftEdge->topPoint ||
			 	region->topPoint != region->rightEdge->topPoint ) {
		if (region->bottomPoint == region->leftEdge->bottomPoint) {
			triangulateUpEdge(region, region->leftEdge, Point::eLeftRegion);
		}
		if (region->bottomPoint == region->rightEdge->bottomPoint) {
			triangulateUpEdge(region, region->rightEdge, Point::eRightRegion);
		}
		Point* point = region->topPoint;
		assert(point);
		if (region->leftEdge->topPoint == region->rightEdge->topPoint) {//we are at the apex of our A -break
			return;
		}
		switch (point->type) {
		case Point::eC:
		{
			Point::RegionPos pos = region->rightEdge->topPoint == point ? Point::eLeftRegion : Point::eRightRegion;
			triangulateFromRegion(point->nextRegionAbove(pos));
			break;
		}
		case Point::eV:
			triangulateFromRegion(point->nextRegionAbove(Point::eLeftRegion));
			triangulateFromRegion(point->nextRegionAbove(Point::eRightRegion));
			break;
		case Point::eA: //we are on the side of the A and there will only be one region above
			triangulateFromRegion(point->nextRegionAbove(Point::eLeftRegion));
			break;
		default: ;
		}
	}
}

void TriangulateImpl::triangulateFromTrapezoids()
{
	for ( VPointType::iterator it =  mVPoints.begin(); it != mVPoints.end(); ++it) {
		Point* point = *it;
		Region* region = point->nextRegionAbove(Point::eMiddleRegion);
		if (!region->isInsidePoly()) {
			continue;
		}
		triangulateFromRegion(region);
	}
}

void TriangulateImpl::processMonotonePoly()
{
	//We use an ear clipping algorithm to clip off all convex ears as triangles until we have no more triangles left

	//we know that the point 0 - n form a monotone mountain with some concave and some convex vertices
	//the base of the poly is point n - 0. point n and zero must be convex meaning we can easily work out
	//whether this is a clockwise or anti-clockwise poly


	//@TODO This a O(n^2) algoritm although it is unlikely to have that many points
	//We can potentially modify to an O(n) algorithm although this does become a bit
	//more complicated

//	assert(mTmpPoly.size() > 2); //we must have at least a triangle inside here

	if (fabs(mFirstTmpPoly->y() - mLastTmpPoly->y()) < 1e-5) {
		return; //we are dealing with flat poly - no need to triangulate
	}
	Point* p =  mFirstTmpPoly->nextTmpPolyPoint;
	bool isACWPoly = pointVectorCross(mLastTmpPoly, mFirstTmpPoly, p) > 0.0;

	Point* pointA = NULL;
	Point*  pivotPoint = mLastTmpPoly;
	Point*  pointB = NULL;
	while (true) {
		if (pivotPoint == mLastTmpPoly) { //we have reached the end
			pointA = mFirstTmpPoly;
			pivotPoint = pointA->nextTmpPolyPoint;
			pointB = pivotPoint->nextTmpPolyPoint;
		}
		if (pointB == NULL) {
			pointA->nextTmpPolyPoint = NULL;
			mLastTmpPoly = NULL;
			mFirstTmpPoly = NULL;
			mLastTmpPoly = NULL;
			return; //we have finished the triangulation
		}
		TrFloat pvc = pointVectorCross(pointA, pivotPoint, pointB);
		if (isACWPoly == (pvc > 0.0) || pvc == 0)  //We have a convex point
		{
			mTriangles.push_back(Triangle());
			Triangle& tri = mTriangles.back();
			tri.points[0].x = pointA->x();
			tri.points[0].y = pointA->y();
			tri.points[1].x = pivotPoint->x();
			tri.points[1].y = pivotPoint->y();
			tri.points[2].x = pointB->x();
			tri.points[2].y = pointB->y();
			//remove Pivot Point
			Point* tmp = pivotPoint;
			pivotPoint = tmp->nextTmpPolyPoint;
			pointA->nextTmpPolyPoint = pivotPoint;
			pointB = pivotPoint->nextTmpPolyPoint;
			tmp->nextTmpPolyPoint = NULL;
		} else {
			//if concave ear - just inrement to the next position
			pointA = pointA->nextTmpPolyPoint;
			pivotPoint = pivotPoint->nextTmpPolyPoint;
			pointB = pointB->nextTmpPolyPoint;
		}
	}
}

TrFloat TriangulateImpl::pointVectorCross(const Point* a, const Point* pivot, const Point* b)
{
	//is the angle between line a and line b with pivot as the pivot point positive or negative
	double ax = a->x();
	double pivotx = pivot->x();
	double bx = b->x();
	double ay = a->y();
	double pivoty = pivot->y();
	double by = b->y();

	double res = (ax - pivotx) * (by - pivoty) - (ay - pivoty) * (bx - pivotx);
	return res;
}


