/// <copyright file="Arrows.ispc" company="Visualisierungsinstitut der Universität Stuttgart">
/// Copyright © 2019 Visualisierungsinstitut der Universität Stuttgart.
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
/// </copyright>
/// <author>Christoph Müller</author>


#include "common/Ray.ih"
#include "common/Model.ih"

#include "geometry/Geometry.ih"

#include "math/vec.ih"
#include "math/box.ih"


/// \brief Cylindrical texture coordinates for Arrows.
struct ArrowTexCoords {
  vec2f v0;
  vec2f v1;
};


/// \brief The descriptor of the cone geometry.
struct Arrows {
  Geometry super; //!< inherited geometry fields

  uint8 *arrow_data;
  uint32 arrow_stride;
  float base_radius;
  uint8 *colour_data;
  uint32 colour_stride;
  bool has_alpha;
  uint32 material_id;
  int32 offset_axis;
  int32 offset_base;
  int32 offset_base_radius;
  int32 offset_colour_id;
  int32 offset_length;
  int32 offset_material_id;
  int32 offset_tip_radius_scale;
  int32 offset_tip_size;
  float scale;
  ArrowTexCoords *texcoord_data;
  float tip_radius_scale;
  float tip_size;
};


/// \brief Retrieves the (unit) axis of the arrow starting at 'arrow' and
/// returns the desired length of the glyph.
inline uniform float Arrow_axis(uniform vec3f *uniform axis,
                                const uniform Arrows *uniform arrows,
                                const uniform uint8 *uniform arrow) {
  *axis = *((vec3f *uniform) (arrow + arrows->offset_axis));
  uniform float retval = length(*axis);
  *axis = *axis / retval;

  if (arrows->offset_length >= 0) {
    // Use per-arrow length if requested.
    retval =*((const uniform float *) (arrow + arrows->offset_length));
  }

  // Apply global scaling factor.
  retval = arrows->scale * retval;

  return retval;
}


/// \brief Determines the radius of a single arrow starting at 'arrow'.
inline uniform float Arrow_radius(const uniform Arrows *uniform arrows,
                                  const uniform uint8 *uniform arrow)
{
  return (arrows->offset_base_radius >= 0)
    ? *((const uniform float *) (arrow + arrows->offset_base_radius))
    : arrows->base_radius;
}


/// \brief Determines the radius of the arrow tip of a single arrow starting
/// at 'arrow'.
///
/// The function determines the radius of the current cylinder and scales it for
/// the current arrow. It is guaranteed that the scaling factor is at least 1.
inline uniform float Arrow_tip_radius(const uniform Arrows *uniform arrows,
                                      const uniform uint8 *uniform arrow)
{
  uniform float scale = (arrows->offset_tip_radius_scale >= 0)
    ? *((const uniform float *) (arrow + arrows->offset_tip_radius_scale))
    : arrows->tip_radius_scale;
  return max(1.0f, scale) * Arrow_radius(arrows, arrow);
}


/// \brief Determines the relative size of the tip a single arrow starting at
/// 'arrow'.
inline uniform float Arrow_tip_size(const uniform Arrows *uniform arrows,
                                    const uniform uint8 *uniform arrow)
{
  uniform float retval = (arrows->offset_tip_size >= 0)
    ? *((const uniform float *) (arrow + arrows->offset_tip_size))
    : arrows->tip_size;
  return min(1.0f, max(0.0f, retval));
}


/// \brief Computes the world-space bounding box of a cone.
///
/// For now, we approximate the arrow by its enclosing cylinder which is defined
/// by the length of the arrow and the radius of the tip.
unmasked void Arrows_bounds(const RTCBoundsFunctionArguments *uniform args)
{
  const uniform Arrows *uniform that = (uniform Arrows *uniform) args->geometryUserPtr;
  const uint8 *uniform arrow = that->arrow_data + that->arrow_stride * args->primID;

  uniform vec3f axis;
  const uniform float length = Arrow_axis(&axis, that, arrow);
  const uniform vec3f base = *((vec3f *uniform) (arrow + that->offset_base));
  const uniform vec3f apex = base + length * axis;
  uniform float radius = Arrow_tip_radius(that, arrow);

  uniform vec3f a2 = sqr(base - apex);
  uniform float d2 = a2.x + a2.y + a2.z;
  uniform vec3f a = make_vec3f(a2.y + a2.z, a2.x + a2.z, a2.x + a2.y);
  uniform vec3f dr = radius * sqrt(a / d2);

  box3fa *uniform out = (box3fa *uniform) args->bounds_o;
  *out = make_box3fa(min(base, apex) - dr, max(base, apex) + dr);
}


/// \brief Computes the intersection of a ray and a cone.
void Arrows_intersect_kernel(const RTCIntersectFunctionNArguments *uniform args,
                            const uniform bool isOcclusionTest)
{
  if (!args->valid[programIndex]) {
    return;
  }

  const uniform Arrows *uniform that = (uniform Arrows *uniform) args->geometryUserPtr;
  const uint8 *uniform arrow = that->arrow_data + that->arrow_stride * args->primID;

  // Determine the parameters of the current arrow.
  uniform vec3f axis;
  const uniform float length = Arrow_axis(&axis, that, arrow);
  const uniform vec3f base = *((vec3f *uniform) (arrow + that->offset_base));
  const uniform vec3f apex = base + length * axis;
  const uniform float cylinderRadius = Arrow_radius(that, arrow);
  const uniform float coneRadius = Arrow_tip_radius(that, arrow);

  // Determine derived properties like the axis in world space and the height of
  // the cylinder and cone.
  const uniform float coneLength = Arrow_tip_size(that, arrow) * length;
  const uniform float cylinderLength = length - coneLength;
  const uniform vec3f tipBase = base + cylinderLength * axis;

  // This assumes that the args->rayhit is actually a pointer to a varying ray!
  varying Ray *uniform ray = (varying Ray *uniform) args->rayhit;

  // The idea implemented here is the following: we want to perform all
  // intersection tests in a coordinate system where this is easy. This
  // coordinate system is the local one of the glyph where 'apex' is the origin
  // and 'axis' is the vector pointing in y-direction. Note that the choice for
  // 'apex' as the origin is motivated by the cone, because for the cylinder,
  // this is irrelevant. We define the x-axis of the local coordinate system as
  // the one being perpendicular to 'axis' and (0, 1, 0). As the second vector
  // is constant, we can hardcode the result of the cross product. We use the
  // resulting matrix to transform the ray into the local coordinate system of
  // the glyph to perform the intersection test.
  uniform vec3f mx = make_vec3f(-axis.z, 0, axis.x);
  uniform vec3f my = axis;
  uniform vec3f mz = make_vec3f(0.0f, 0.0f, 0.0f);

  vec3f dir = ray->dir;         // The ray's direction in glyph space.
  vec3f org = ray->org - apex;  // The ray's origin in glyph space.
  bool isHit = false;

  // There is one special case we need to consider, which is the aforementioned
  // cross product between 'axis' and (0, 1, 0) being a null vector. In this
  // case, the vectors are parallel, ie we do not need to rotate the ray,
  // because the glyph is aligned with the world.
  uniform bool isAligned = (dot(mx, mx) == 0.0f);
  if (isAligned) {
    mz = normalize(cross(mx, my));

    dir.x = dot(mx, dir);
    dir.y = dot(my, dir);
    dir.z = dot(mz, dir);
  }

  // Now, intersect the cylinder first, which is x² + z² - r² = 0 in the great
  // coordinate system or our choice. Substituting x and z by our ray o + td
  // yields (o_x + td_x)² + (o_z + td_z)² - r² = 0, simplified
  // (d_x² * d_z²)t² + (2 * o_x * o_z + 2 * d_x * d_z)t + o_x² * o_z² - r² = 0,
  // so a = dot(d, d), b = 2 dot(o, d), c = dot(o, o) - r².
  {
    float a = dot(dir, dir);
    if (a > 0.0f) {
      float b = 2.0f * dot(org, dir);
      float c = dot(org, org) - sqr(cylinderRadius);
      float d = sqr(b) - 4.0f * a * c;

      if (d >= 0.0f) {
        // There is at least one intersection, check which one is closest to the
        // ray's origin.
        float t = ray->t;
        d = sqrt(d);

        {
          float tt = (-b + d) / (2.0f * a);
          float i = org.y + tt * dir.y;
          if ((tt >= ray->t0) && (tt < t) && (i <= -coneLength) && (i >= -length)) {
            t = tt;
          }
        }

        {
          float tt = (-b - d) / (2.0f * a);
          float i = org.y + tt * dir.y;
          if ((tt >= ray->t0) && (tt < t) && (i <= -coneLength) && (i >= -length)) {
            t = tt;
          }
        }

        if (ray->t > t) {
          ray->t = t;
          isHit = true;
        }

      } /* end if (d >= 0.0f) */
    } /* end if (a > 0.0f) */
  }

  // The cone forming the tip is next. Its points are defined by x² + z² - y² = 0.

  // Finally, we need to check the cylinder and cone caps.

  if (isHit) {
    cif (isOcclusionTest) {
      ray->t = neg_inf;

    } else {
      ray->instID = args->context->instID[0];
      ray->primID = args->primID;
      ray->geomID = that->super.geomID;
//    ray->u = (ray->t * sd - sf) * rcp(s2);
//    ray->Ng = td * d - fp - ray->u * s;
    }
  }


  #if 0
  const vec3f d = ray->dir;
  const uniform vec3f s = v1 - v0;  // axis of the cone
  const vec3f sxd = cross(s, d);
  const float a = dot(sxd, sxd);    // (s x d)^2
  if (a == 0.0f) {
    return;
	// ray and cylinder parallel
  }

  const vec3f f = v0 - ray->org;
  const vec3f sxf = cross(s, f);
  const float ra = 1.0f / a;
  const float ts = dot(sxd, sxf) * ra; // (s x d)(s x f) / (s x d)^2, in ray-space
  const vec3f fp = f - ts * d; // f' = v0 - closest point to axis

  const uniform float s2 = dot(s, s); // s^2
  const vec3f perp = cross(s, fp); // s x f'
  const float c = sqr(radius) * s2 - dot(perp, perp); //  r^2 s^2 - (s x f')^2
  if (c < 0.0f) {
    return;
  }

  float td = sqrt(c * ra);
  const float t_in = ts - td;
  const float t_out = ts + td;

  // clip to cylinder caps
  const float sf = dot(s, f);
  const float sd = dot(s, d);
  const float rsd = rcp(sd);
  const float tA = sf * rsd;
  const float tB = tA + s2 * rsd;
  // rsd (and thus tA and tB) can be NaN if the ray is perfectly perpendicular
  // to the cylinder; filter NaNs by having ray t as 2nd argument to min/max
  const float tmin = max(min(tA, tB), ray->t0);
  const float tmax = min(max(tA, tB), ray->t);

  bool hit = false;
  // avoid short-circuiting &&
  if (and(tmin < t_in, t_in <= tmax)) {
    hit = true;
    td *= -1.f;
    ray->t = t_in;
  } else if (and(tmin < t_out, t_out <= tmax)) {
    hit = true;
    ray->t = t_out;
  }

  if (hit) {
    cif (isOcclusionTest) {
      ray->t = neg_inf;
    } else {
      ray->instID = args->context->instID[0];
      ray->primID = args->primID;
      ray->geomID = that->super.geomID;
      // cannot easily be moved to postIntersect
      // we need hit in object-space, in postIntersect it is in world-space
      ray->u = (ray->t * sd - sf) * rcp(s2);
      ray->Ng = td * d - fp - ray->u * s;
    }
  }
  #endif
}


/// \brief Computes the actual ray/arrow intersection point.
unmasked void Arrows_intersect(
  const struct RTCIntersectFunctionNArguments *uniform args)
{
  Arrows_intersect_kernel(args, false);
}


/// \brief Performs an occlusion test for the arrow.
unmasked void Arrows_occluded(
  const struct RTCIntersectFunctionNArguments *uniform args)
{
  Arrows_intersect_kernel(args, true);
}


/// \brief Computes the colour and material of the intersection point.
static void Arrows_postIntersect(Geometry *uniform _self,
                                 Model *uniform model,
                                 varying DifferentialGeometry &dg,
                                 const varying Ray &ray,
                                 uniform int64 flags)
{
  uniform Arrows *uniform that = (uniform Arrows *uniform) _self;
  uniform uint8 *arrow = that->arrow_data + that->arrow_stride * ray.primID;

  dg.Ng = dg.Ns = ray.Ng;
  dg.epsilon = 10;//that->base_radius * ulpEpsilon;	// TODO: Check

  if (((flags & DG_COLOR) != 0) && (that->colour_data != NULL)) {
    uint32 colourID = (that->offset_colour_id >= 0)
      ? *((uniform uint32 *varying) (arrow + that->offset_colour_id))
      : ray.primID;

    dg.color = *((vec4f *) (that->colour_data + colourID * that->colour_stride));

    if (!that->has_alpha) {
      dg.color.w = 1.0f;
    }
  }

  if (((flags & DG_TEXCOORD) != 0) && (that->texcoord_data != NULL)) {
    ArrowTexCoords tc = that->texcoord_data[ray.primID];
    dg.st = lerp(ray.u, tc.v0, tc.v1);
  }

  if (((flags & DG_MATERIALID) != 0) && (that->offset_material_id >= 0)) {
    dg.materialID = *((uniform uint32 *varying) (arrow
      + that->offset_material_id));
  }
}


/// \brief Determines the material of the given arrow.
int32 Arrows_getMaterialID(const Geometry *uniform const _self,
                           const int32 primID)
{
  const Arrows *const uniform that = (const Arrows *uniform) _self;

  if (that->offset_material_id >= 0) {
    uniform uint8 *arrow = that->arrow_data + that->arrow_stride * primID;
    return *((uniform uint32 *varying) (arrow + that->offset_material_id));
  } else {
    return -1;
  }
}


SampleAreaRes Arrows_sampleArea(const Geometry *uniform const _self,
                               const int32 primID,
                               const uniform affine3f &xfm,
                               const uniform affine3f &rcp_xfm,
                               const vec2f& s)
{
  const Arrows *const uniform that = (const Arrows *uniform) _self;
  const uniform uint8 *arrow = that->arrow_data + that->arrow_stride * primID;
#if 0
  const float radius = (that->offset_radius >= 0)
    ? *((float *)(cone + that->offset_radius))
    : that->radius;
  vec3f v0 = *((vec3f *) (cone + that->offset_base));
  vec3f v1 = *((vec3f *) (cone + that->offset_apex));

  linear3f frame = frame(v1 - v0);
  frame.vy = normalize(frame.vy);
  const float phi = two_pi * s.x;
  float sinPhi, cosPhi;
  sincos(phi, &sinPhi, &cosPhi);
  const vec3f localNormal = (cosPhi*radius)*frame.vx + (sinPhi*radius)*frame.vy;
  const vec3f localPos = v0 + localNormal + s.y*frame.vz;

  SampleAreaRes res;
  res.pos = xfmPoint(xfm, localPos);
  res.normal = normalize(xfmVector(transposed(rcp_xfm.l), localNormal));

  return res;
#endif
  SampleAreaRes retval;
  return retval;
}


void Arrows_getAreas(const Geometry *const uniform _self,
                    const int32 *const uniform primIDs,
                    const uniform int32 numPrims,
                    const uniform affine3f &xfm,
                    float *const uniform areas)
{
  const Arrows *const uniform that = (const Arrows *uniform) _self;

#if 0
  // detect (non-uniform) scaling; get length of transformed unit-vectors
  const uniform vec3f scaling3 =
    make_vec3f(length(xfm.l.vx), length(xfm.l.vy), length(xfm.l.vz));
  const uniform float min_scaling = reduce_min(scaling3);
  const uniform float max_scaling = reduce_max(scaling3);
  if ((max_scaling - min_scaling) > 1e-4f*min_scaling)
    print("#osp:Arrows_getAreas: Non-uniform scaling in instance "
          "transformation detected! Importance sampling for emissive "
          "materials and thus resulting image may be wrong.\n");
  // use average as approximation to closest uniform scaling
  const uniform float scaling = reduce_add(scaling3)/3.f;

  uniform float radius = that->radius;
  const uniform int64 stride = that->arrow_stride;
  // TODO vectorize this loop, with foreach or ProgramCount & ProgramIndex
  for (uniform int32 i = 0; i < numPrims; i++) {
    uint8 *uniform cylinderPtr = that->arrow_data + primIDs[i] * stride;
    if (that->offset_radius >= 0)
      radius = *((float *uniform)(cylinderPtr + that->offset_radius));
    const uniform float circ = two_pi * radius * scaling;
    uniform vec3f v0 = *((vec3f *uniform)(cylinderPtr + that->offset_base));
    uniform vec3f v1 = *((vec3f *uniform)(cylinderPtr + that->offset_apex));
    areas[i] = circ * length(v0 - v1) * scaling;
  }
#endif
}


/// \brief Creates an Embree context for the given C++ equivalent.
export void *uniform Arrows_create(void *uniform cppEquivalent)
{
  uniform Arrows *uniform retval = uniform new uniform Arrows;
  Geometry_Constructor(&retval->super, cppEquivalent, Arrows_postIntersect,
                       NULL, NULL, 0, NULL);
  return retval;
}


/// \brief Creates new Embree geometry for the given Arrows object.
export void ArrowsGeometry_set(void *uniform _self,
  void *uniform _model,
  void *uniform data,
  uniform int cnt_data,
  uniform int data_stride,
  void *uniform materials,
  void *uniform texcoords,
  void *uniform colours,
  uniform int colour_stride,
  uniform bool has_alpha,
  uniform float base_radius,
  uniform float tip_radius_scale,
  uniform float tip_size,
  uniform float scale,
  uniform int materialID,
  uniform uint32 offset_base,
  uniform uint32 offset_axis,
  uniform uint32 offset_base_radius,
  uniform uint32 offset_tip_radius_scale,
  uniform uint32 offset_tip_size,
  uniform uint32 offset_length,
  uniform uint32 offset_material_id,
  uniform uint32 offset_colour_id)
{
  Arrows *uniform that = (Arrows *uniform) _self;
  Model *uniform model = (Model *uniform) _model;

  RTCGeometry geom = rtcNewGeometry(ispc_embreeDevice(),
    RTC_GEOMETRY_TYPE_USER);
  uniform uint32 geomID = rtcAttachGeometry(model->embreeSceneHandle, geom);

  that->super.model = model;
  that->super.geomID = geomID;
  that->super.numPrimitives = cnt_data;
  that->super.getMaterialID = Arrows_getMaterialID;
  //that->super.getAreas = Arrows_getAreas;
  that->super.getAreas = NULL;
  //that->super.sampleArea = Arrows_sampleArea;
  that->super.sampleArea = NULL;
  that->super.materialList = (Material **) materials;

  that->arrow_data = (uint8 *uniform) data;
  that->arrow_stride = data_stride;
  that->colour_data = (uint8 *uniform) colours;
  that->colour_stride = colour_stride;
  that->has_alpha = has_alpha;
  that->material_id = materialID;
  that->offset_axis = offset_axis;
  that->offset_base = offset_base;
  that->offset_base_radius = offset_base_radius;
  that->offset_colour_id = offset_colour_id;
  that->offset_length = offset_length;
  that->offset_material_id = offset_material_id;
  that->offset_tip_radius_scale = offset_tip_radius_scale;
  that->offset_tip_size = offset_tip_size;
  that->scale = scale;
  that->texcoord_data = (ArrowTexCoords *uniform) texcoords;
  that->tip_radius_scale = tip_radius_scale;
  that->tip_size = tip_size;

  rtcSetGeometryUserData(geom, that);
  rtcSetGeometryUserPrimitiveCount(geom, that->super.numPrimitives);
  rtcSetGeometryBoundsFunction(geom,
    (uniform RTCBoundsFunction) &Arrows_bounds, that);
  rtcSetGeometryIntersectFunction(geom,
    (uniform RTCIntersectFunctionN) &Arrows_intersect);
  rtcSetGeometryOccludedFunction(geom,
    (uniform RTCOccludedFunctionN) &Arrows_occluded);
  rtcCommitGeometry(geom);
  rtcReleaseGeometry(geom);
}
