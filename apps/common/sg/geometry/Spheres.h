// ======================================================================== //
// Copyright 2009-2018 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "sg/common/Node.h"
#include "sg/geometry/Geometry.h"

namespace ospray {
  namespace sg {

    // simple spheres, with all of the key info (position and radius)
    struct OSPSG_INTERFACE Spheres : public sg::Geometry
    {
      Spheres();

      // return bounding box of all primitives
      box3f bounds() const override;

      OSPGeometry ospGeometry {nullptr};
    };

  } // ::ospray::sg
} // ::ospray
