#pragma once

#include "ospray/ospray_cpp.h"
#include "rkcommon/math/AffineSpace.h"

using namespace rkcommon::math;
using namespace ospray;

struct OffAxisProjection
{
  OffAxisProjection(int screenID);

  void update(AffineSpace3f transform);

  int screenID;
  
  vec3f topLeftLocal;
  vec3f botLeftLocal;
  vec3f botRightLocal;

  cpp::Camera camera{"perspective"};
};