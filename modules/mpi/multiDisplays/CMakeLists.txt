find_package(MPI REQUIRED)
set(OpenGL_GL_PREFERENCE "LEGACY") # prefer libGL over libOpenGl for better compatibility with SWR
find_package(OpenGL 2 REQUIRED)
find_package(glfw3 REQUIRED)
if (TARGET glfw)
  get_property(glfw3_INCLUDE_DIRS TARGET glfw
    PROPERTY INTERFACE_INCLUDE_DIRECTORIES
  )
endif()

# MPI multiple display utilities library
add_library(mpi_multi_displays_common STATIC
  GLFWOSPRayWindow.cpp
  imgui_impl_glfw_gl3.cpp
  OffAxisProjection.cpp
)
target_link_libraries(mpi_multi_displays_common PUBLIC
  rkcommon::rkcommon
  ospray
  ospray_module_cpu
  ospray_testing
  ospray_imgui
  glfw
  arcball_camera
  ${OPENGL_LIBRARIES}
  MPI::MPI_CXX
)
target_compile_definitions(mpi_multi_displays_common PUBLIC 
 -DOSPRAY_CPP_RKCOMMON_TYPES
)
# Add external libraries for this MPI multiple display utilities library
add_subdirectory(external)
target_link_libraries(mpi_multi_displays_common PUBLIC json)


# build ospMPIMultiDisplays for demonstration
add_executable(ospMPIMultiDisplays
 ${OSPRAY_RESOURCE}
 ospExample.cpp
)
target_link_libraries(ospMPIMultiDisplays PRIVATE mpi_multi_displays_common)
install(TARGETS ospMPIMultiDisplays
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT mpi
)
ospray_sign_target(ospMPIMultiDisplays)

# target_link_libraries(ospMPIMultiDisplays PUBLIC
#   ospray_testing
# )

