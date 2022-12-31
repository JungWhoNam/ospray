#include "OffAxisProjection.h"

#include "camera/PerspectiveCamera.h"

OffAxisProjection::OffAxisProjection(int screenID)
{
  this->screenID = screenID;

  // currently only support perspective camera
  PerspectiveCamera *perspective = (PerspectiveCamera *)(camera.handle());
  perspective->offAxisMode = true;

  // TODO read these information from a JSON file
  // 1.84752 : 1.1547 = 1792 : 1120
  // 1.84752 * 0.5 = 0.92376
  // 1.1547 * 0.5 = 0.57735
  if (screenID == 0) {
    topLeftLocal = vec3f(0.92376f, 0.57735f, 1.0f);
    botLeftLocal = vec3f(0.92376f, -0.57735f, 1.0f);
    botRightLocal = vec3f(-0.92376f, -0.57735f, 1.0f);
    camera.setParam("aspect", 1.84752f / 1.1547f);
  } else if (screenID == 1) {
    topLeftLocal = vec3f(0.92376f, 0.57735f, 1.0f);
    botLeftLocal = vec3f(0.92376f, -0.57735f, 1.0f);
    botRightLocal = vec3f(0, -0.57735f, 1.0f);
    camera.setParam("aspect", 0.92376f / 1.1547f);
  } else if (screenID == 2) {
    topLeftLocal = vec3f(0.0f, 0.57735f, 1.0f);
    botLeftLocal = vec3f(0.0f, -0.57735f, 1.0f);
    botRightLocal = vec3f(-0.92376f, -0.57735f, 1.0f);
    camera.setParam("aspect", 0.92376f / 1.1547f);
  }

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