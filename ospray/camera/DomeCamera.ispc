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

#include "Camera.ih"
#include "math/sampling.ih"


void DomeCamera_initRay(Camera *uniform self,
                        varying Ray &ray,
                        const varying CameraSample &sample)
{
    const vec2f screen = Camera_subRegion(self, sample.screen);
    
    const vec2f dmPos = make_vec2f(screen.x - 0.5f, screen.y - 0.5f);
    // :O No dot for vec2f?!
    const float dmLambda = sqrt(dmPos.x * dmPos.x + dmPos.y * dmPos.y);

    const float time = Camera_shutterTime(self, sample.time);
    const vec3f org = self->org;

    if (dmLambda <= 0.5f) {
        const float phi = (1.0f - 2.0f * dmLambda) * (0.5f * M_PI);
        const float skyY = dmLambda * tan(phi);
        vec3f dir = make_vec3f(dmPos.x, skyY, dmPos.y);
        //dir = normalize(dir);

        dir = self->frame * dir;
        setRay(ray, org, dir, self->nearClip, inf, time);

    } else {
        // Wild guess level 10: Zero direction vector seems to terminate rays
        // outside the dome's hemisphere.
        vec3f dir = make_vec3f(0.0f, 0.0f, 0.0f);
        dir = self->frame * dir;
        setRay(ray, org, dir, self->nearClip, inf, time);
    }
}


/*! 
    Create a new ispc-side version of a dome camera - with given
    pointer to the c-class version - and return both class pointer and
    pointer to internal data back via the supplied reference
    parameters.
 */
export void *uniform DomeCamera_create(void *uniform cppE)
{
    Camera *uniform cam = uniform new Camera;
    cam->cppEquivalent = cppE;
    cam->initRay = DomeCamera_initRay;
    cam->doesDOF = false;
    return cam;
}
