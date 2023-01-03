// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ArcballCamera.h"
#include "OffAxisProjection.h"
// glfw
#include "GLFW/glfw3.h"
// ospray
#include "ospray/ospray_cpp.h"
#include "rkcommon/containers/TransactionalBuffer.h"
// std
#include <functional>

using namespace rkcommon::math;
using namespace ospray;

struct WindowState
{
  bool quit;

  bool rigChanged;
  AffineSpace3f rigTransform;

  bool sceneChanged;
  std::string scene;

  WindowState();
};

class GLFWOSPRayWindow
{
 public:
  GLFWOSPRayWindow();

  ~GLFWOSPRayWindow();

  static GLFWOSPRayWindow *getActiveWindow();

  void mainLoop();

 protected:
  void addObjectToCommit(OSPObject obj);

  void reshape(const vec2i &newWindowSize);
  void motion(const vec2f &position);
  void display();
  void startNewOSPRayFrame();
  void waitOnOSPRayFrame();
  void updateTitleBar();
  void buildUI();
  void refreshScene(bool resetCamera = false);

  static GLFWOSPRayWindow *activeWindow;

  // GLFW window instance
  GLFWwindow *glfwWindow = nullptr;

  // Arcball camera instance
  std::unique_ptr<ArcballCamera> arcballCamera;

  // Off-axis instance
  std::unique_ptr<OffAxisProjection> cameraRig;

  // OSPRay objects managed by this class
  cpp::World world;
  cpp::Renderer renderer{"scivis"};
  cpp::FrameBuffer framebuffer;
  cpp::Future currentFrame;

  std::string scene{"boxes_lit"}; 
  vec2i windowSize;
  vec2f previousMouse{-1.f};

  // List of OSPRay handles to commit before the next frame
  rkcommon::containers::TransactionalBuffer<OSPObject> objectsToCommit;

  // OpenGL framebuffer texture
  GLuint framebufferTexture = 0;

  // optional registered display callback, called before every display()
  std::function<void(GLFWOSPRayWindow *)> displayCallback;

  // toggles display of ImGui UI, if an ImGui callback is provided
  bool showUi = true;

  // optional registered ImGui callback, called during every frame to build UI
  std::function<void()> uiCallback;

  // FPS measurement of last frame
  float latestFPS{0.f};

  int mpiRank = -1;
  int mpiWorldSize = -1;

  // The window state to be sent out over MPI to the other rendering processes
  WindowState windowState;
};
