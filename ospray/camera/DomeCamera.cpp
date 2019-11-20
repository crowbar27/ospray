/// <copyright file="DomeCamera.cpp" company="Visualisierungsinstitut der Universität Stuttgart">
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

#include "DomeCamera.h"
#include "DomeCamera_ispc.h"


namespace ospray {

  DomeCamera::DomeCamera(void)
  {
    this->ispcEquivalent = ispc::DomeCamera_create(this);
  }

  void DomeCamera::commit(void)
  {
    Camera::commit();

    auto apertureRadius = this->getParamf("apertureRadius", 0.0f);
    auto aspectRatio = this->getParamf("aspect", 1.0f);
    auto focusDistance = this->getParamf("focusDistance", 1.0f);
    //auto fovy = this->getParamf("fovy", 60.f);
    auto fovy = 180.0f; // FOV is always the whole hemisphere.

    auto scaledAperture = 0.0f;
    if (apertureRadius > 0.f) {
      auto height = 2.f * std::tan(deg2rad(0.5f * fovy));
      auto width = height * aspectRatio;
      scaledAperture = apertureRadius / (width * focusDistance);
    }

    ispc::DomeCamera_set(this->getIE(), aspectRatio, scaledAperture);
  }

  std::string DomeCamera::toString(void) const
  {
    return "ospray::DomeCamera";
  }

  OSP_REGISTER_CAMERA(DomeCamera, dome);

} // namespace ospray
