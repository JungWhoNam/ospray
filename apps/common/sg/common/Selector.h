// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
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

namespace ospray {
  namespace sg {

    /*! a light node - the generic light node */
    struct OSPSG_INTERFACE Selector : public sg::Renderable
    {
      //! \brief constructor
      Selector();

//      virtual void preCommit(RenderContext &ctx) override;
//      virtual void postCommit(RenderContext &ctx) override;
      virtual void preTraverse(RenderContext &ctx, const std::string& operation, bool& traverseChildren) override;
      virtual void postTraverse(RenderContext &ctx, const std::string& operation) override;

    };

  } // ::ospray::sg
} // ::ospray
