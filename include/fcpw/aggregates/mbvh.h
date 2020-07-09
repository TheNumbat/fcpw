#pragma once

#include <fcpw/aggregates/sbvh.h>
#ifdef FCPW_USE_EIGHT_WIDE_BRANCHING
	#define MBVH_BRANCHING_FACTOR 8
	#define MBVH_MAX_DEPTH 154
#else
	#define MBVH_BRANCHING_FACTOR 4
	#define MBVH_MAX_DEPTH 96
#endif

namespace fcpw {

template<size_t DIM>
struct MbvhNode {
	// constructor
	MbvhNode(): boxMin(FloatP<MBVH_BRANCHING_FACTOR>(maxFloat)),
				boxMax(FloatP<MBVH_BRANCHING_FACTOR>(minFloat)),
				child(maxInt) {}

	// members
	VectorP<MBVH_BRANCHING_FACTOR, DIM> boxMin, boxMax;
	IntP<MBVH_BRANCHING_FACTOR> child; // use sign to differentiate between inner and leaf nodes
};

template<size_t WIDTH, size_t DIM, typename PrimitiveType>
struct MbvhLeafNode {
	// members
	VectorP<WIDTH, DIM> positions[1];
	IntP<WIDTH> primitiveIndex;
};

template<size_t WIDTH, size_t DIM>
struct MbvhLeafNode<WIDTH, DIM, LineSegment> {
	// members
	VectorP<WIDTH, DIM> positions[2];
	IntP<WIDTH> primitiveIndex;
};

template<size_t WIDTH, size_t DIM>
struct MbvhLeafNode<WIDTH, DIM, Triangle> {
	// members
	VectorP<WIDTH, DIM> positions[3];
	IntP<WIDTH> primitiveIndex;
};

template<size_t WIDTH, size_t DIM, typename PrimitiveType=Primitive<DIM>>
class Mbvh: public Aggregate<DIM> {
public:
	// constructor
	Mbvh(const Sbvh<DIM, PrimitiveType> *sbvh_, bool printStats_=false);

	// returns bounding box
	BoundingBox<DIM> boundingBox() const;

	// returns centroid
	Vector<DIM> centroid() const;

	// returns surface area
	float surfaceArea() const;

	// returns signed volume
	float signedVolume() const;

	// intersects with ray, starting the traversal at the specified node in an aggregate;
	// use this for spatially/temporally coherent queries
	// NOTE: interactions are invalid when checkForOcclusion is enabled
	int intersectFromNode(Ray<DIM>& r, std::vector<Interaction<DIM>>& is,
						  int nodeStartIndex, int aggregateIndex, int& nodesVisited,
						  bool checkForOcclusion=false, bool recordAllHits=false) const;

	// finds closest point to sphere center, starting the traversal at the specified node in an aggregate;
	// use this for spatially/temporally coherent queries
	bool findClosestPointFromNode(BoundingSphere<DIM>& s, Interaction<DIM>& i,
								  int nodeStartIndex, int aggregateIndex,
								  const Vector<DIM>& boundaryHint, int& nodesVisited) const;

protected:
	// collapses sbvh into a mbvh
	int collapseSbvh(const Sbvh<DIM, PrimitiveType> *sbvh, int sbvhNodeIndex, int parent, int depth);

	// determines whether mbvh node is a leaf node
	bool isLeafNode(const MbvhNode<DIM>& node) const;

	// populates leaf nodes
	void populateLeafNodes();

	// members
	int nNodes, nLeafs, maxDepth;
	const std::vector<PrimitiveType *>& primitives;
	std::vector<MbvhNode<DIM>> flatTree;
	std::vector<MbvhLeafNode<WIDTH, DIM, PrimitiveType>> leafNodes;
	ObjectType vectorizedLeafType;
	bool primitiveTypeIsAggregate;
};

} // namespace fcpw

#include "mbvh.inl"