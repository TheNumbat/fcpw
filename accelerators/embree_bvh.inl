#include <common/math/vec3.h>

namespace fcpw {

template<size_t DIM>
inline EmbreeBvh<DIM>::EmbreeBvh(const std::vector<std::shared_ptr<Primitive<DIM>>>& primitives_,
								 const std::shared_ptr<PolygonSoup<DIM>>& soup_):
Baseline<DIM>(primitives_),
soup(soup_)
{
	LOG(INFO) << "EmbreeBvh<DIM>(): No embree support for dimension: " << DIM;
}

template<size_t DIM>
inline EmbreeBvh<DIM>::~EmbreeBvh()
{
	// nothing to do
}

template<size_t DIM>
inline BoundingBox<DIM> EmbreeBvh<DIM>::boundingBox() const
{
	return Baseline<DIM>::boundingBox();
}

template<size_t DIM>
inline Vector<DIM> EmbreeBvh<DIM>::centroid() const
{
	return Baseline<DIM>::centroid();
}

template<size_t DIM>
inline float EmbreeBvh<DIM>::surfaceArea() const
{
	return Baseline<DIM>::surfaceArea();
}

template<size_t DIM>
inline float EmbreeBvh<DIM>::signedVolume() const
{
	return Baseline<DIM>::signedVolume();
}

template<size_t DIM>
inline int EmbreeBvh<DIM>::intersectFromNode(Ray<DIM>& r, std::vector<Interaction<DIM>>& is,
											 int nodeStartIndex, int& nodesVisited,
											 bool checkOcclusion, bool countHits) const
{
	return Baseline<DIM>::intersectFromNode(r, is, nodeStartIndex, nodesVisited,
											checkOcclusion, countHits);
}

template<size_t DIM>
inline bool EmbreeBvh<DIM>::findClosestPointFromNode(BoundingSphere<DIM>& s, Interaction<DIM>& i,
													 int nodeStartIndex, const Vector<DIM>& boundaryHint,
													 int& nodesVisited) const
{
	return Baseline<DIM>::findClosestPointFromNode(s, i, nodeStartIndex, boundaryHint, nodesVisited);
}

void errorFunction(void *userPtr, enum RTCError error, const char *str)
{
	if (error == RTC_ERROR_NONE) return;

	std::string code = "";
	switch (error) {
		case RTC_ERROR_UNKNOWN          : code = "RTC_ERROR_UNKNOWN"; break;
		case RTC_ERROR_INVALID_ARGUMENT : code = "RTC_ERROR_INVALID_ARGUMENT"; break;
		case RTC_ERROR_INVALID_OPERATION: code = "RTC_ERROR_INVALID_OPERATION"; break;
		case RTC_ERROR_OUT_OF_MEMORY    : code = "RTC_ERROR_OUT_OF_MEMORY"; break;
		case RTC_ERROR_UNSUPPORTED_CPU  : code = "RTC_ERROR_UNSUPPORTED_CPU"; break;
		case RTC_ERROR_CANCELLED        : code = "RTC_ERROR_CANCELLED"; break;
		default                         : code = "invalid error code"; break;
	}

	LOG(FATAL) << "Embree error code: " << code << " msg: " << str;
}

struct IntersectContext {
	// constructor
	IntersectContext(const std::vector<std::shared_ptr<Primitive<3>>>& primitives_,
					 std::vector<Interaction<3>>& is_): primitives(primitives_), is(is_) {}

	// members
	RTCIntersectContext context;
	const std::vector<std::shared_ptr<Primitive<3>>>& primitives;
	std::vector<Interaction<3>>& is;
};

void triangleIntersectionCallback(const struct RTCFilterFunctionNArguments *args)
{
	// get required information from args
	RTCRay *ray = (RTCRay *)args->ray;
	RTCHit *hit = (RTCHit *)args->hit;
	IntersectContext *context = (IntersectContext *)args->context;
	const std::vector<std::shared_ptr<Primitive<3>>>& primitives = context->primitives;
	std::vector<Interaction<3>>& is = context->is;
	args->valid[0] = 0; // ignore all hits

	// check if interaction has already been added
	for (size_t i = 0; i < is.size(); i++) {
		if (is[i].primitive == primitives[hit->primID].get()) {
			return;
		}
	}

	// add interaction
	auto it = is.emplace(is.end(), Interaction<3>());
	it->d = ray->tfar;
	it->p = Vector3(ray->org_x, ray->org_y, ray->org_z) +
			Vector3(ray->dir_x, ray->dir_y, ray->dir_z)*it->d;
	it->uv[0] = hit->u;
	it->uv[1] = hit->v;
	it->primitive = primitives[hit->primID].get();
}

embree::Vec3fa closestPointTriangle(embree::Vec3fa const& p, embree::Vec3fa const& a,
									embree::Vec3fa const& b, embree::Vec3fa const& c)
{
	const embree::Vec3fa ab = b - a;
	const embree::Vec3fa ac = c - a;
	const embree::Vec3fa ap = p - a;

	const float d1 = dot(ab, ap);
	const float d2 = dot(ac, ap);
	if (d1 <= 0.0f && d2 <= 0.0f) return a;

	const embree::Vec3fa bp = p - b;
	const float d3 = dot(ab, bp);
	const float d4 = dot(ac, bp);
	if (d3 >= 0.0f && d4 <= d3) return b;

	const embree::Vec3fa cp = p - c;
	const float d5 = dot(ab, cp);
	const float d6 = dot(ac, cp);
	if (d6 >= 0.0f && d5 <= d6) return c;

	const float vc = d1*d4 - d3*d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
		const float v = d1/(d1 - d3);
		return a + v*ab;
	}

	const float vb = d5*d2 - d1*d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
		const float v = d2/(d2 - d6);
		return a + v*ac;
	}

	const float va = d3*d6 - d5*d4;
	if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
		const float v = (d4 - d3)/((d4 - d3) + (d5 - d6));
		return b + v*(c - b);
	}

	const float denom = 1.0f/(va + vb + vc);
	const float v = vb*denom;
	const float w = vc*denom;
	return a + v*ab + w*ac;
}

struct ClosestPointResult {
	// constructor
	ClosestPointResult(): primID(RTC_INVALID_GEOMETRY_ID), geomID(RTC_INVALID_GEOMETRY_ID) {}

	// members
	embree::Vec3fa p;
	unsigned int primID;
	unsigned int geomID;
};

// NOTE: this global variable is created with great sadness; alternatives
// such as declaring closestPointTriangleCallback as a member function of Embree Bvh
// and using that function as the callback, or using std::bind to wrap the soup that
// needs to be passed to closestPointTriangleCallback don't work since their function
// signatures don't match those of the callback.
static std::shared_ptr<PolygonSoup<3>> callbackSoup = nullptr;

bool closestPointTriangleCallback(RTCPointQueryFunctionArguments *args)
{
	// get required information from args
	const unsigned int primID = args->primID;
	const unsigned int geomID = args->geomID;
	embree::Vec3fa q(args->query->x, args->query->y, args->query->z);

	// determine distance to closest point on triangle
	const Vector3& pa = callbackSoup->positions[callbackSoup->indices[3*primID]];
	const Vector3& pb = callbackSoup->positions[callbackSoup->indices[3*primID + 1]];
	const Vector3& pc = callbackSoup->positions[callbackSoup->indices[3*primID + 2]];
	embree::Vec3fa v1(pa[0], pa[1], pa[2]);
	embree::Vec3fa v2(pb[0], pb[1], pb[2]);
	embree::Vec3fa v3(pc[0], pc[1], pc[2]);

	const embree::Vec3fa p = closestPointTriangle(q, v1, v2, v3);
	float d = distance(q, p);

	// store result in userPtr and update the query radius if we found a point
	// closer to the query position
	if (d < args->query->radius) {
		args->query->radius = d;
		ClosestPointResult *result = (ClosestPointResult *)args->userPtr;
		result->p = p;
		result->primID = primID;
		result->geomID = geomID;

		return true;
	}

	return false;
}

template<>
inline EmbreeBvh<3>::EmbreeBvh(const std::vector<std::shared_ptr<Primitive<3>>>& primitives_,
							   const std::shared_ptr<PolygonSoup<3>>& soup_):
Baseline<3>(primitives_),
soup(soup_)
{
	using namespace std::chrono;
	high_resolution_clock::time_point t1 = high_resolution_clock::now();

	// initialize device
	device = rtcNewDevice(NULL); // specify flags e.g. threads, isa, verbose, tri_accel=bvh4.triangle4v if required
	if (!device) LOG(FATAL) << "EmbreeBvh<3>(): Unable to create device: " << rtcGetDeviceError(NULL);
	this->setNormals = false;

	// register error callback
	rtcSetDeviceErrorFunction(device, errorFunction, NULL);

	// initialize scene
	scene = rtcNewScene(device);
	rtcSetSceneFlags(scene, RTC_SCENE_FLAG_ROBUST | RTC_SCENE_FLAG_CONTEXT_FILTER_FUNCTION);
	rtcSetSceneBuildQuality(scene, RTC_BUILD_QUALITY_HIGH);

	// initialize geometry; NOTE: specialized to triangle meshes for now
	RTCGeometry geometry = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
	rtcSetGeometryBuildQuality(geometry, RTC_BUILD_QUALITY_HIGH);

	// register closest point callback
	callbackSoup = soup;
	rtcSetGeometryPointQueryFunction(geometry, closestPointTriangleCallback);

	// load geometry
	float *vertices = (float *)rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_VERTEX, 0,
													   RTC_FORMAT_FLOAT3, 3*sizeof(float),
													   soup->positions.size());
	unsigned int *indices = (unsigned int *)rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_INDEX, 0,
																	RTC_FORMAT_UINT3, 3*sizeof(unsigned int),
																	soup->indices.size()/3);

	if (vertices && indices) {
		for (size_t i = 0; i < soup->positions.size(); i++) {
			for (size_t j = 0; j < 3; j++) {
				vertices[3*i + j] = soup->positions[i][j];
			}
		}

		for (size_t i = 0; i < soup->indices.size(); i++) {
			indices[i] = soup->indices[i];
		}
	}

	// commit, attach and release geometry
	rtcCommitGeometry(geometry);
	rtcAttachGeometry(scene, geometry);
	rtcReleaseGeometry(geometry);

	// commit scene
	rtcCommitScene(scene);

	// print bvh stats
	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	duration<double> timeSpan = duration_cast<duration<double>>(t2 - t1);
	std::cout << "Built Embree Bvh with "
			  << primitives.size() << " primitives in "
			  << timeSpan.count() << " seconds" << std::endl;
}

template<>
inline EmbreeBvh<3>::~EmbreeBvh()
{
	rtcReleaseScene(scene);
	rtcReleaseDevice(device);
}

template<>
inline BoundingBox<3> EmbreeBvh<3>::boundingBox() const
{
	return Baseline<3>::boundingBox();
}

template<>
inline Vector3 EmbreeBvh<3>::centroid() const
{
	return Baseline<3>::centroid();
}

template<>
inline float EmbreeBvh<3>::surfaceArea() const
{
	return Baseline<3>::surfaceArea();
}

template<>
inline float EmbreeBvh<3>::signedVolume() const
{
	return Baseline<3>::signedVolume();
}

template<>
inline int EmbreeBvh<3>::intersectFromNode(Ray<3>& r, std::vector<Interaction<3>>& is,
										   int nodeStartIndex, int& nodesVisited,
										   bool checkOcclusion, bool countHits) const
{
#ifdef PROFILE
	PROFILE_SCOPED();
#endif

	// initialize intersect context (RTC_INTERSECT_CONTEXT_FLAG_INCOHERENT is enabled by default)
	IntersectContext context(this->primitives, is);
	rtcInitIntersectContext(&context.context);
	nodesVisited++;

	// initialize rayhit structure
	RTCRayHit rayhit;
	rayhit.ray.org_x = r.o[0];
	rayhit.ray.org_y = r.o[1];
	rayhit.ray.org_z = r.o[2];
	rayhit.ray.dir_x = r.d[0];
	rayhit.ray.dir_y = r.d[1];
	rayhit.ray.dir_z = r.d[2];
	rayhit.ray.tnear = 0.0f;
	rayhit.ray.tfar = r.tMax;
	rayhit.ray.mask = 0;
	rayhit.ray.flags = 0;
	rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
	rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
	is.clear();

	if (checkOcclusion) {
		// test for occlusion
		rtcOccluded1(scene, &context.context, &rayhit.ray);
		return rayhit.ray.tfar >= 0.0f ? 0 : 1;
	}

	// set filter function to collect all hits if requested
	if (countHits) context.context.filter = triangleIntersectionCallback;

	// intersect single ray with the scene
	rtcIntersect1(scene, &context.context, &rayhit);
	int hits = 0;

	if (is.size() > 0) {
		// sort interactions
		std::sort(is.begin(), is.end(), compareInteractions<3>);
		is = removeDuplicates<3>(is);
		hits = is.size();
		r.tMax = is[0].d;

	} else {
		if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
			// record closest interaction
			auto it = is.emplace(is.end(), Interaction<3>());
			it->d = rayhit.ray.tfar;
			it->p = r(it->d);
			it->uv[0] = rayhit.hit.u;
			it->uv[1] = rayhit.hit.v;
			it->primitive = this->primitives[rayhit.hit.primID].get();
			r.tMax = it->d;
			hits++;
		}
	}

	// set normals
	if (this->setNormals) {
		for (size_t i = 0; i < is.size(); i++) {
			is[i].n = is[i].primitive->normal(is[i].uv);
		}
	}

	return hits;
}

template<>
inline bool EmbreeBvh<3>::findClosestPointFromNode(BoundingSphere<3>& s, Interaction<3>& i,
												   int nodeStartIndex, const Vector3& boundaryHint,
												   int& nodesVisited) const
{
#ifdef PROFILE
	PROFILE_SCOPED();
#endif

	// initialize closest point context
	RTCPointQueryContext context;
	rtcInitPointQueryContext(&context);
	nodesVisited++;

	// initialize point query
	RTCPointQuery query;
	query.x = s.c[0];
	query.y = s.c[1];
	query.z = s.c[2];
	query.radius = std::sqrt(s.r2);
	query.time = 0.0f;

	// perform query
	ClosestPointResult result;
	rtcPointQuery(scene, &query, &context, nullptr, (void*)&result);

	if (result.geomID != RTC_INVALID_GEOMETRY_ID) {
		// record result
		i.p[0] = result.p.x;
		i.p[1] = result.p.y;
		i.p[2] = result.p.z;
		i.d = norm<3>(i.p - s.c);
		i.primitive = this->primitives[result.primID].get();
		i.uv = static_cast<const Triangle *>(i.primitive)->barycentricCoordinates(i.p);
		if (this->setNormals) i.n = static_cast<const Triangle *>(i.primitive)->normal(i.uv);
		s.r2 = i.d*i.d;

		return true;
	}

	return false;
}

} // namespace fcpw
