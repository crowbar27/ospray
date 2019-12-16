/// <copyright file="Arrows.h" company="Visualisierungsinstitut der Universität Stuttgart">
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

#pragma once


#include "Geometry.h"


namespace ospray {

  /*! \defgroup geometry_arrows Arrows ("arrows")

    \ingroup ospray_supported_geometries

    \brief Geometry representing arrow glyphs with an optional per-arrow length,
    radius and tip size.

    Implements a geometry consisting of individual arrow glyphs, each of which
    can have an individual length as well as tip size and cylinder radius.

    Parameters:
    <dl>
    <dt><code>float        radius = 0.01f</code></dt><dd>Base radius common to all cylinders if 'offset_radius' is not used</dd>
    <dt><code>int32        materialID = 0</code></dt><dd>Material ID common to all cylinders if 'offset_materialID' is not used</dd>
    <dt><code>int32        bytes_per_cylinder = 6*sizeof(float)</code></dt><dd>Size (in bytes, default is for v0/v1 positions) of each cylinder in the data array.</dd>
    <dt><code>int32        offset_v0 = 0</code></dt><dd>Offset (in bytes) of each cylinder's 'vec3f v0' value (the start vertex) within each cylinder</dd>
    <dt><code>int32        offset_v1 = 3*sizeof(float)</code></dt><dd>Offset (in bytes) of each cylinder's 'vec3f v1' value (the end vertex) within each cylinder</dd>
    <dt><code>int32        offset_radius = -1</code></dt><dd>Offset (in bytes) of each cylinder's 'float radius' value within each cylinder. Setting this value to -1 means that there is no per-cylinder radius value, and that all cylinders should use the (shared) 'radius' value instead</dd>
    <dt><code>int32        offset_materialID = -1</code></dt><dd>Offset (in bytes) of each cylinder's 'int materialID' value within each cylinder. Setting this value to -1 means that there is no per-cylinder material ID, and that all cylinders share the same per-geometry 'materialID'</dd>
    <dt><code>int32        offset_colorID = -1</code></dt><dd>Byte offset for each cylinder's color index (for the 'color' data). Setting this value to -1 means that there is no per-cylinder color, and that all cylinders share the same per-geometry color.</dd>
    <dt><code>Data<float>  cylinders</code></dt><dd>Array of data elements.</dd>
    <dt><code>Data<float>  color</code></dt><dd>Array of color (RGBA) elements accessed by indexes (per element) in 'cylinders' colorID data.</dd>
    </dl>

    The functionality for this geometry is implemented via the
    \ref ospray::Arrows class.

  */

  /// \brief A geometry for a set of arrow glyphs.
  struct OSPRAY_SDK_INTERFACE Arrows : public Geometry
  {

    /// \brief Initialises a new instance.
    Arrows(void);

    /// \brief Integrates this geometry's primitives into the respective model's
    /// acceleration structure.
    virtual void finalize(Model *model) override;

    /// \brief Answer a debug representation of the geometry.
    virtual std::string toString() const override;

    /// \brief Holds the per-arrow data.
    Ref<Data> arrow_data;

    /// \brief The stride of two arrows in bytes.
    std::size_t arrow_stride;

    /// \brief The global radius of the cylinder that is used if no per-arrow
    /// base radius is sepcified.
    float base_radius;

    /// \brief The number of arrows.
    std::size_t cnt_glyphs;

    /// \brief Holds the colours used by the arrrows.
    Ref<Data> colour_data;

    /// \brief The global material used if no per-arrow material is specified.
    uint32 material_id;

    /// \brief The offset of the arrows's scaled axis vector in bytes.
    int32 offset_axis;

    /// \brief The offset of the centre position of the arrows's base in bytes.
    int32 offset_base;

    /// \brief The offset of the arrows's base radius in bytes or a negative
    /// value to use the global values.
    int32 offset_base_radius;

    /// \brief The offset of the arrows's colour in bytes or a negativ value if
    /// no per-arrow colour is given.
    int32 offset_colour_id;

    /// \brief The offset of a per-arrow length that the axis is to be scaled to.
    int32 offset_length;

    /// \brief The offset of the arrows's material in bytes or a negative value
    /// if no per-arrow material is given.
    int32 offset_material_id;

    /// \brief The offset of the scaling factor from the base radius to the
    /// radius of the tip or a negative value if no per-arrow scale is given.
    int32 offset_tip_radius_scale;

    /// \brief The offset of the relative size of the arrow's tip of a negative
    /// value if no per-arrow size is given.
    int32 offset_tip_size;

    /// \brief A scaling factor for the length of all arrow glyphs.
    float scale;

    /// \brief Holds the texture coordinates used by the arrows.
    Ref<Data> texcoord_data;

    /// \brief The global scaling factor for computing the radius of the tip
    /// from the base radius if no per-arrow value is given.
    ///
    /// This is the radius of the bottom disk forming the base of the cone that
    /// is the arrow's tip. It is computed from the radius of the cylinger and
    /// must be at least one.
    float tip_radius_scale;

    /// \brief The global relative size of the arrow's tip in relation to its
    /// length.
    ///
    /// This property specifies which part of the arrow is the tip. It therefore
    /// must be larger than zero and less than one.
    float tip_size;

  };

} // ::ospray
