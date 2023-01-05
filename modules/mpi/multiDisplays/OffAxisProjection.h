#pragma once

#include "ospray/ospray_cpp.h"
#include "rkcommon/math/AffineSpace.h"

using namespace rkcommon::math;
using namespace ospray;

struct OffAxisProjection
{
  OffAxisProjection(int mpiRank);

  void update(AffineSpace3f transform);

  vec3f topLeftLocal;
  vec3f botLeftLocal;
  vec3f botRightLocal;

  cpp::Camera camera{"perspective"};
};