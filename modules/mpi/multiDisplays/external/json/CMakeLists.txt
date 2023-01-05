add_library(json INTERFACE)
target_include_directories(json
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>)