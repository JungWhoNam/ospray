// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <mpi.h>
#include "GLFWOSPRayWindow.h"
#include "example_common.h"
#include "json.hpp"

using namespace ospray;
using rkcommon::make_unique;

nlohmann::ordered_json readJSON(std::string filePath)
{
  nlohmann::ordered_json config = nullptr;
  
  try {
    std::ifstream configFile(filePath);
    if (!configFile) {
      std::cerr << "The config file does not exist." << std::endl;
      return nullptr;
    }
    configFile >> config;
  } catch (nlohmann::json::exception &e) {
    std::cerr << "Failed to parse the config file: " << e.what() << std::endl;
    return nullptr;
  }

  return config;
}

int main(int argc, char **argv)
{
  // read config JSON files
  nlohmann::ordered_json configDisplay = readJSON(argc > 1 ? argv[1] : "config/display_settings.json");
  if (configDisplay == nullptr)
    return 1;
  nlohmann::ordered_json configTracking = readJSON(argc > 2 ? argv[2] : "config/tracking_settings.json");
  if (configTracking == nullptr)
    return 1;
  nlohmann::ordered_json configScene = readJSON(argc > 3 ? argv[3] : "config/scene_settings.json");
  // if (configScene == nullptr)
  //   return 1;

  int mpiThreadCapability = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &mpiThreadCapability);
  if (mpiThreadCapability != MPI_THREAD_MULTIPLE
      && mpiThreadCapability != MPI_THREAD_SERIALIZED) {
    fprintf(stderr,
        "OSPRay requires the MPI runtime to support thread "
        "multiple or thread serialized.\n");
    return 1;
  }

  int mpiRank = 0;
  int mpiWorldSize = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);
  
  std::cout << "OSPRay rank " << mpiRank << "/" << mpiWorldSize << "\n";

  initializeOSPRay(0, {}, false);
  {
    auto glfwOSPRayWindow = make_unique<GLFWOSPRayWindow>(configDisplay, configTracking, configScene);
    glfwOSPRayWindow->mainLoop();
    glfwOSPRayWindow.reset();
  }
  ospShutdown();

  MPI_Finalize();

  return 0;
}
