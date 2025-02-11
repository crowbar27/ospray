/// <copyright file="DomeCamera.h" company="Visualisierungsinstitut der Universität Stuttgart">
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

#include "camera/Camera.h"


namespace ospray {

  /*! \defgroup dome_camera The Dome Master Camera ("dome")

      \brief todo

      \ingroup ospray_supported_cameras

      A camera that produces a Dome Master image on a quadratic frame buffer.
      This camera type is loaded by passing the type string "dome" to
      \ref ospNewCamera.

      The functionality for a dome camera is implemented via the
      \ref ospray::DomeCamera class.
    */

  /// Implements a DomeMaster camera.
  struct OSPRAY_SDK_INTERFACE DomeCamera : public Camera
  {
    DomeCamera(void);

    virtual ~DomeCamera(void) override = default;

    virtual void commit(void) override;

    virtual std::string toString(void) const override;

  };

} // ::ospray
