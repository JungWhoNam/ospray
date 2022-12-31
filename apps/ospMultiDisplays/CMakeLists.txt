## Copyright 2018 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

if (NOT OSPRAY_ENABLE_APPS_EXAMPLES)
  return()
endif()

# prefer libGL over libOpenGl for better compatibility with SWR
set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL 2 REQUIRED)
find_package(glfw3 REQUIRED)
find_package(MPI REQUIRED)

add_executable(ospMPIMultiDisplays
  ${OSPRAY_RESOURCE}
  GLFWOSPRayWindow.cpp
  imgui_impl_glfw_gl3.cpp
  ospExample.cpp
  OffAxisProjection.cpp
)

target_link_libraries(ospMPIMultiDisplays
PRIVATE
  arcball_camera
  ospray_testing
  ospray_imgui
  glfw
  ${OPENGL_LIBRARIES}
  ${MPI_C_LIBRARIES}
)

target_include_directories(ospMPIMultiDisplays PRIVATE
  $<BUILD_INTERFACE:${MPI_C_INCLUDE_PATH}>)

ospray_sign_target(ospMPIMultiDisplays)

install(TARGETS ospMPIMultiDisplays
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT apps
)
