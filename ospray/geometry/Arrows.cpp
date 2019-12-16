/// <copyright file="Arrows.cpp" company="Visualisierungsinstitut der Universität Stuttgart">
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
// clang-format off

#undef NDEBUG

#include "common/Data.h"
#include "common/Model.h"

#include "Arrows.h"
#include "Arrows_ispc.h"


ospray::Arrows::Arrows(void)
  : arrow_stride(0),
  base_radius(0.0f),
  cnt_glyphs(0),
  material_id(-1),
  offset_axis(-1),
  offset_base(-1),
  offset_base_radius(-1),
  offset_colour_id(-1),
  offset_length(-1),
  offset_material_id(-1),
  offset_tip_radius_scale(-1),
  offset_tip_size(-1)
{
    this->ispcEquivalent = ispc::Arrows_create(this);
}


void ospray::Arrows::finalize(Model *model)
{
  Geometry::finalize(model);

  this->arrow_data = this->getParamData("arrows");
  this->arrow_stride = this->getParam1i("arrow_stride", 6 * sizeof(float));
  this->base_radius  = this->getParam1f("radius", 0.01f);
  this->colour_data = this->getParamData("colours");
  this->material_id = this->getParam1i("material_id" ,0);
  this->offset_axis = this->getParam1i("offset_axis", 3 * sizeof(float));
  this->offset_base = this->getParam1i("offset_base", 0);
  this->offset_base_radius = this->getParam1i("offset_radius", -1);
  this->offset_colour_id = getParam1i("offset_colour_id", -1);
  this->offset_length = getParam1i("offset_length", -1);
  this->offset_material_id = this->getParam1i("offset_material_id", -1);
  this->offset_tip_radius_scale = this->getParam1i("offset_tip_radius", -1);
  this->offset_tip_size = this->getParam1i("offset_tip_length", -1);
  this->scale = this->getParam1f("scale", 1.0f);
  this->texcoord_data = this->getParamData("texcoords");
  this->tip_radius_scale = getParam1f("tip_radius", 1.1f);
  this->tip_size = getParam1f("tip_length", 0.25f);

  if ((this->arrow_data.ptr == nullptr) || (this->arrow_stride == 0)) {
    throw std::runtime_error("#ospray:geometry/Arrows: no 'arrows' "
      "data specified or the specified stride is invalid.");
  }

  assert(this->arrow_data->numBytes % this->arrow_stride == 0);
  this->cnt_glyphs = this->arrow_data->numBytes / this->arrow_stride;
  postStatusMsg(2) << "#osp: creating 'arrows' geometry, # of glyphs is "
    << this->cnt_glyphs;

  // Compute bounding box of whole set of glyphs.
  auto arrow = static_cast<const char *>(this->arrow_data->data);
  this->bounds = empty;
  for (decltype(this->cnt_glyphs) i = 0; i < this->cnt_glyphs;
      ++i, arrow += this->arrow_stride) {
    auto r = (this->offset_base_radius >= 0)
      ? *reinterpret_cast<const float *>(arrow + this->offset_base_radius)
      : this->base_radius;
    auto s = (this->offset_tip_radius_scale > 0)
      ? *reinterpret_cast<const float *>(arrow + this->offset_tip_radius_scale)
      : this->tip_radius_scale;
    if (s < 1.0f) {
      r *= s;
    }

    auto v0 = *reinterpret_cast<const vec3f *>(arrow + this->offset_base);
    auto dir = *reinterpret_cast<const vec3f *>(arrow + this->offset_axis);
    if (this->offset_length > 0) {
        auto l = *reinterpret_cast<const float *>(arrow + this->offset_length);
        dir = scale * l * normalize(dir);
    } else {
        dir = scale * dir;
    }
    auto v1 = v0 + dir;

    this->bounds.extend(box3f(v0 - r, v0 + r));
    this->bounds.extend(box3f(v1 - r, v1 + r));
  }

  auto colComps
      = (this->colour_data && (this->colour_data->type == OSP_FLOAT3))
      ? 3
      : 4;

  ispc::ArrowsGeometry_set(this->getIE(),
    model->getIE(),
    this->arrow_data->data,
    this->cnt_glyphs,
    this->arrow_stride,
    this->materialList ? ispcMaterialPtrs.data() : nullptr,
    this->texcoord_data ? this->texcoord_data->data : nullptr,
    this->colour_data ? this->colour_data->data : nullptr,
    colComps * sizeof(float),
    this->colour_data && this->colour_data->type == OSP_FLOAT4,
    this->base_radius,
    this->tip_radius_scale,
    this->tip_size,
    this->scale,
    this->material_id,
    this->offset_base,
    this->offset_axis,
    this->offset_base_radius,
    this->offset_tip_radius_scale,
    this->offset_tip_size,
    this->offset_length,
    this->offset_material_id,
    this->offset_colour_id);
}


std::string ospray::Arrows::toString() const
{
  return "ospray::Arrows";
}


namespace ospray { OSP_REGISTER_GEOMETRY(Arrows, arrows); }
