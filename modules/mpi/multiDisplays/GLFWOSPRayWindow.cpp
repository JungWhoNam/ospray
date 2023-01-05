// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "GLFWOSPRayWindow.h"
#include "imgui_impl_glfw_gl3.h"
#include "json.hpp"
// ospray_testing
#include "ospray_testing.h"
#include "rkcommon/utility/random.h"
// imgui
#include "imgui.h"
// std
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
// mpi
#include <mpi.h>

// on Windows often only GL 1.1 headers are present
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif
#ifndef GL_FRAMEBUFFER_SRGB
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif
#ifndef GL_RGB32F
#define GL_RGB32F 0x8815
#endif

static bool g_quitNextFrame = false;

static const std::vector<std::string> g_scenes = {"boxes_lit",
    "boxes",
    "cornell_box",
    "curves",
    "gravity_spheres_volume",
    "gravity_spheres_amr",
    "gravity_spheres_isosurface",
    "perlin_noise_volumes",
    "random_spheres",
    "streamlines",
    "subdivision_cube",
    "unstructured_volume",
    "unstructured_volume_isosurface",
    "planes",
    "clip_with_spheres",
    "clip_with_planes",
    "clip_gravity_spheres_volume",
    "clip_perlin_noise_volumes",
    "clip_particle_volume",
    "particle_volume",
    "particle_volume_isosurface",
    "vdb_volume",
    "vdb_volume_packed",
    "instancing"};

bool sceneUI_callback(void *, int index, const char **out_text)
{
  *out_text = g_scenes[index].c_str();
  return true;
}

// GLFWOSPRayWindow definitions ///////////////////////////////////////////////

void error_callback(int error, const char *desc)
{
  std::cerr << "error " << error << ": " << desc << std::endl;
}

GLFWOSPRayWindow *GLFWOSPRayWindow::activeWindow = nullptr;

WindowState::WindowState() 
    : quit(false), rigChanged(false), sceneChanged(false)
{}

GLFWOSPRayWindow::GLFWOSPRayWindow()
{
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);

  if (activeWindow != nullptr) {
    throw std::runtime_error("Cannot create more than one GLFWOSPRayWindow!");
  }

  activeWindow = this;

  glfwSetErrorCallback(error_callback);

  // initialize GLFW
  if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW!");
  }

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

  windowSize.x = config[mpiRank]["screenWidth"];
  windowSize.y = config[mpiRank]["screenHeight"];

  // create GLFW window 
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_DECORATED, mpiRank == 0 ? GLFW_TRUE : GLFW_FALSE);
  glfwWindow = glfwCreateWindow(
      windowSize.x, windowSize.y, "OSPRay Tutorial", nullptr, nullptr);
  
  if (!glfwWindow) {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window!");
  }

  // make the window's context current
  glfwMakeContextCurrent(glfwWindow);

  ImGui_ImplGlfwGL3_Init(glfwWindow, true);

  // set initial OpenGL state
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);

  // create OpenGL frame buffer texture
  glGenTextures(1, &framebufferTexture);
  glBindTexture(GL_TEXTURE_2D, framebufferTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // further configure GLFW window based on rank
  if (mpiRank == 0) {
    glfwSetWindowAspectRatio(glfwWindow, windowSize.x, windowSize.y);
    // set GLFW callbacks (only apply to rank 0)
    glfwSetFramebufferSizeCallback(glfwWindow, [](GLFWwindow *, int newWidth, int newHeight) {
      activeWindow->reshape(vec2i{newWidth, newHeight});
    });
    glfwSetCursorPosCallback(glfwWindow, [](GLFWwindow *, double x, double y) {
      ImGuiIO &io = ImGui::GetIO();
      if (!activeWindow->showUi || !io.WantCaptureMouse) {
        activeWindow->motion(vec2f{float(x), float(y)});
      }
    });
    glfwSetKeyCallback(glfwWindow, [](GLFWwindow *, int key, int, int action, int) {
      if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_G:
          activeWindow->showUi = !(activeWindow->showUi);
          break;
        case GLFW_KEY_Q:
          g_quitNextFrame = true;
          break;
        }
      }
    });
  }
  else {
    int x = config[mpiRank]["screenX"];
    int y = config[mpiRank]["screenY"];
    glfwSetWindowPos(glfwWindow, x, y);
  }

  // OSPRay setup //
  refreshScene(true);

  // set the initial state
  windowState.rigChanged = true;
  windowState.rigTransform = arcballCamera->transform();
  windowState.sceneChanged = false;
  windowState.scene = scene;
  showUi = mpiRank == 0;
  
  // trigger window reshape events with current window size
  glfwGetFramebufferSize(glfwWindow, &this->windowSize.x, &this->windowSize.y);
  reshape(this->windowSize);
}

GLFWOSPRayWindow::~GLFWOSPRayWindow()
{
  ImGui_ImplGlfwGL3_Shutdown();
  // cleanly terminate GLFW
  glfwTerminate();
}

GLFWOSPRayWindow *GLFWOSPRayWindow::getActiveWindow()
{
  return activeWindow;
}

void GLFWOSPRayWindow::mainLoop()
{
  while (true) {
    MPI_Bcast(&windowState, sizeof(WindowState), MPI_BYTE, 0, MPI_COMM_WORLD);
    if (windowState.quit) {
      break;
    }

    startNewOSPRayFrame();
    waitOnOSPRayFrame();

    if (showUi) {
      ImGui_ImplGlfwGL3_NewFrame();
    }

    display();

    // poll and process events
    glfwPollEvents();

    if (mpiRank == 0) {  
      windowState.quit = glfwWindowShouldClose(glfwWindow) || g_quitNextFrame;
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }
}

void GLFWOSPRayWindow::reshape(const vec2i &newWindowSize)
{
  windowSize = newWindowSize;

  // create new frame buffer
  auto buffers = OSP_FB_COLOR;
  framebuffer = cpp::FrameBuffer(windowSize.x, windowSize.y, OSP_FB_RGBA32F, OSP_FB_COLOR);

  // reset OpenGL viewport and orthographic projection
  glViewport(0, 0, windowSize.x, windowSize.y);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, windowSize.x, 0.0, windowSize.y, -1.0, 1.0);

  // update camera
  arcballCamera->updateWindowSize(windowSize);
}

void GLFWOSPRayWindow::motion(const vec2f &position)
{
  const vec2f mouse(position.x, position.y);
  if (previousMouse != vec2f(-1)) {
    const bool leftDown =
        glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool rightDown =
        glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool middleDown =
        glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    const vec2f prev = previousMouse;

    bool cameraChanged = leftDown || rightDown || middleDown;

    if (leftDown) {
      const vec2f mouseFrom(clamp(prev.x * 2.f / windowSize.x - 1.f, -1.f, 1.f),
          clamp(prev.y * 2.f / windowSize.y - 1.f, -1.f, 1.f));
      const vec2f mouseTo(clamp(mouse.x * 2.f / windowSize.x - 1.f, -1.f, 1.f),
          clamp(mouse.y * 2.f / windowSize.y - 1.f, -1.f, 1.f));
      arcballCamera->rotate(mouseFrom, mouseTo);
    } else if (rightDown) {
      arcballCamera->zoom(mouse.y - prev.y);
    } else if (middleDown) {
      arcballCamera->pan(vec2f(mouse.x - prev.x, prev.y - mouse.y));
    }

    if (cameraChanged) {
      windowState.rigChanged = true;
      windowState.rigTransform = arcballCamera->transform();
    }
  }

  previousMouse = mouse;
}

void GLFWOSPRayWindow::display()
{
  if (showUi)
    buildUI();

  if (displayCallback)
    displayCallback(this);

  updateTitleBar();

  glEnable(GL_FRAMEBUFFER_SRGB); // Turn on sRGB conversion for OSPRay frame

  static bool firstFrame = true;
  if (firstFrame || currentFrame.isReady()) {
    waitOnOSPRayFrame();

    latestFPS = 1.f / currentFrame.duration();

    // map OSPRay frame buffer, update OpenGL texture with its contents, then unmap
    auto *fb = framebuffer.map(OSP_FB_COLOR);

    glBindTexture(GL_TEXTURE_2D, framebufferTexture);
    glTexImage2D(GL_TEXTURE_2D,
        0,
        GL_RGBA32F,
        windowSize.x,
        windowSize.y,
        0,
        GL_RGBA,
        GL_FLOAT,
        fb);

    framebuffer.unmap(fb);

    firstFrame = false;
  }

  // clear current OpenGL color buffer
  glClear(GL_COLOR_BUFFER_BIT);

  // render textured quad with OSPRay frame buffer contents
  glBegin(GL_QUADS);

  glTexCoord2f(0.f, 0.f);
  glVertex2f(0.f, 0.f);

  glTexCoord2f(0.f, 1.f);
  glVertex2f(0.f, windowSize.y);

  glTexCoord2f(1.f, 1.f);
  glVertex2f(windowSize.x, windowSize.y);

  glTexCoord2f(1.f, 0.f);
  glVertex2f(windowSize.x, 0.f);

  glEnd();

  if (showUi) {
    glDisable(GL_FRAMEBUFFER_SRGB); // Disable SRGB conversion for UI

    ImGui::Render();
    ImGui_ImplGlfwGL3_Render();
  }

  // wait for other ranks to reach this point before swapping the buffer
  MPI_Barrier(MPI_COMM_WORLD);

  // swap buffers
  glfwSwapBuffers(glfwWindow);
}

void GLFWOSPRayWindow::startNewOSPRayFrame()
{
  bool fbNeedsClear = false;

  if (windowState.sceneChanged) {
    windowState.sceneChanged = false;
    scene = windowState.scene;
    refreshScene(true);
    fbNeedsClear = true;
  }

  auto handles = objectsToCommit.consume();
  if (!handles.empty()) {
    for (auto &h : handles)
      ospCommit(h);
    fbNeedsClear = true;
  }

  if (windowState.rigChanged) {
    windowState.rigChanged = false;
    cameraRig->update(windowState.rigTransform);
    fbNeedsClear = true;
  }

  if (fbNeedsClear)
    framebuffer.resetAccumulation();

  currentFrame = framebuffer.renderFrame(renderer, cameraRig->camera, world);
}

void GLFWOSPRayWindow::waitOnOSPRayFrame()
{
  currentFrame.wait();
}

void GLFWOSPRayWindow::addObjectToCommit(OSPObject obj)
{
  objectsToCommit.push_back(obj);
}

void GLFWOSPRayWindow::updateTitleBar()
{
  std::stringstream windowTitle;
  windowTitle << "OSPRay: " << std::setprecision(3) << latestFPS << " fps";
  if (latestFPS < 2.f) {
    float progress = currentFrame.progress();
    windowTitle << " | ";
    int barWidth = 20;
    std::string progBar;
    progBar.resize(barWidth + 2);
    auto start = progBar.begin() + 1;
    auto end = start + progress * barWidth;
    std::fill(start, end, '=');
    std::fill(end, progBar.end(), '_');
    *end = '>';
    progBar.front() = '[';
    progBar.back() = ']';
    windowTitle << progBar;
  }

  glfwSetWindowTitle(glfwWindow, windowTitle.str().c_str());
}

void GLFWOSPRayWindow::buildUI()
{
  ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
  ImGui::Begin("press 'g' to hide/show UI", nullptr, flags);

  static int whichScene = 0;
  if (ImGui::Combo("scene##whichScene",
          &whichScene,
          sceneUI_callback,
          nullptr,
          g_scenes.size())) {
    scene = g_scenes[whichScene];
    windowState.sceneChanged = true;
    windowState.scene = scene;
  }
  ImGui::Separator();

  // assume rendererType == OSPRayRendererType::SCIVIS
  static bool shadowsEnabled = false;
  if (ImGui::Checkbox("shadows", &shadowsEnabled)) {
    renderer.setParam("shadows", shadowsEnabled);
    addObjectToCommit(renderer.handle());
  }
  static bool visibleLights = false;
  if (ImGui::Checkbox("visibleLights", &visibleLights)) {
    renderer.setParam("visibleLights", visibleLights);
    addObjectToCommit(renderer.handle());
  }
  static int aoSamples = 0;
  if (ImGui::SliderInt("aoSamples", &aoSamples, 0, 64)) {
    renderer.setParam("aoSamples", aoSamples);
    addObjectToCommit(renderer.handle());
  }
  static float samplingRate = 1.f;
  if (ImGui::SliderFloat("volumeSamplingRate", &samplingRate, 0.001f, 2.f)) {
    renderer.setParam("volumeSamplingRate", samplingRate);
    addObjectToCommit(renderer.handle());
  }

  if (uiCallback) {
    ImGui::Separator();
    uiCallback();
  }

  ImGui::End();
}

void GLFWOSPRayWindow::refreshScene(bool resetCamera)
{
  auto builder = testing::newBuilder(scene);
  testing::setParam(builder, "rendererType", "scivis");
  testing::commit(builder);

  world = testing::buildWorld(builder);
  testing::release(builder);
  world.commit();

  if (resetCamera) {
    arcballCamera.reset(
      new ArcballCamera(world.getBounds<box3f>(), windowSize));
    
    cameraRig.reset(new OffAxisProjection(mpiRank));
    cameraRig->update(arcballCamera->transform());
  }
}