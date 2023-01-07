#include "OffAxisProjection.h"
#include "camera/PerspectiveCamera.h"
#include "json.hpp"

OffAxisProjection::OffAxisProjection(int mpiRank)
{
  // currently only support perspective camera
  PerspectiveCamera *perspective = (PerspectiveCamera *)(camera.handle());
  perspective->offAxisMode = true;
  
  // read JSON file
  std::ifstream info("display_settings.json");
  if (!info) {
    throw std::runtime_error("Failed to load display_settings.json!");
  }
  nlohmann::ordered_json config;
  try {
    info >> config;
  } catch (nlohmann::json::exception& e) {
    throw std::runtime_error("Failed to parse display_settings.json!");
  }

   // update three corners of the image plane
  {
    std::vector<float> vals = config[mpiRank]["topLeft"];
    topLeftLocal.x = vals[0];
    topLeftLocal.y = vals[1];
    topLeftLocal.z = vals[2];
    perspective->topLeft = topLeftLocal;
  }
  {
    std::vector<float> vals = config[mpiRank]["botLeft"];
    botLeftLocal.x = vals[0];
    botLeftLocal.y = vals[1];
    botLeftLocal.z = vals[2];
    perspective->botLeft = botLeftLocal;
  }
  {
    std::vector<float> vals = config[mpiRank]["botRight"];
    botRightLocal.x = vals[0];
    botRightLocal.y = vals[1];
    botRightLocal.z = vals[2];
    perspective->botRight = botRightLocal;
  }
  // use mullion values to update the three corners
  {
    vec3f tl = topLeftLocal, bl = botLeftLocal, br = botRightLocal;

    float mullionLeft = config[mpiRank]["mullionLeft"];
    botLeftLocal += normalize(br - bl) * mullionLeft;
    topLeftLocal +=  normalize(br - bl) * mullionLeft;

    float mullionRight = config[mpiRank]["mullionRight"];
    botRightLocal += normalize(bl - br) * mullionRight;

    float mullionTop = config[mpiRank]["mullionTop"];
    topLeftLocal += normalize(bl - tl) * mullionTop;

    float mullionBottom = config[mpiRank]["mullionBottom"];
    botLeftLocal += normalize(tl - bl) * mullionBottom;
    botRightLocal += normalize(tl - bl) * mullionBottom;
  }

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