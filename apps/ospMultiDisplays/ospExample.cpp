// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <mpi.h>
#include "GLFWOSPRayWindow.h"
#include "example_common.h"

using namespace ospray;
using rkcommon::make_unique;

int main(int argc, char **argv)
{
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
    auto glfwOSPRayWindow =
        make_unique<GLFWOSPRayWindow>(vec2i(1024, 768));
    glfwOSPRayWindow->mainLoop();
    glfwOSPRayWindow.reset();
  }
  ospShutdown();

  MPI_Finalize();

  return 0;
}
