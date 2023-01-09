#include "OffAxisProjection.h"
#include "camera/PerspectiveCamera.h"

using namespace rkcommon::math;

OffAxisProjection::OffAxisProjection(vec3f topLeftLocal,
                                     vec3f botLeftLocal, 
                                     vec3f botRightLocal, 
                                     vec4f mullion)
{
  // currently only support perspective camera
  PerspectiveCamera *perspective = (PerspectiveCamera *)(camera.handle());
  perspective->offAxisMode = true;

  // update three corners of the image plane
  this->topLeftLocal = topLeftLocal;
  this->botLeftLocal = botLeftLocal;
  this->botRightLocal = botRightLocal;

  perspective->topLeft = topLeftLocal;
  perspective->botLeft = botLeftLocal;
  perspective->botRight = botRightLocal;

  // use mullion values to update the three corners
  vec3f tl = topLeftLocal, bl = botLeftLocal, br = botRightLocal;

  float mullionLeft = mullion[0];
  botLeftLocal += normalize(br - bl) * mullionLeft;
  topLeftLocal +=  normalize(br - bl) * mullionLeft;

  float mullionRight = mullion[1];
  botRightLocal += normalize(bl - br) * mullionRight;

  float mullionTop = mullion[2];
  topLeftLocal += normalize(bl - tl) * mullionTop;

  float mullionBottom = mullion[3];
  botLeftLocal += normalize(tl - bl) * mullionBottom;
  botRightLocal += normalize(tl - bl) * mullionBottom;

  // update aspect ratio and fovy
  camera.setParam("aspect", length(botRightLocal - botLeftLocal) / length(topLeftLocal - botLeftLocal));
  float angle = acos(dot(topLeftLocal, botLeftLocal) / (length(topLeftLocal) * length(botLeftLocal))) * 180.f / M_PI;
  camera.setParam("fovy", angle);
  camera.commit();
}

void OffAxisProjection::update(rkcommon::math::AffineSpace3f transform)
{
  // update camera
  camera.setParam("transform", transform);

  // update projection plane (screen)
  PerspectiveCamera *perspective = (PerspectiveCamera *)(camera.handle());
  perspective->topLeft = xfmPoint(transform, topLeftLocal);
  perspective->botLeft = xfmPoint(transform, botLeftLocal);
  perspective->botRight = xfmPoint(transform, botRightLocal);

  camera.commit();
}