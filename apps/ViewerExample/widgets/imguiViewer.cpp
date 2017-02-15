// ======================================================================== //
// Copyright 2016 SURVICE Engineering Company                               //
// Copyright 2016 Intel Corporation                                         //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "imguiViewer.h"
#include "common/sg/common/FrameBuffer.h"
#include "transferFunction.h"

#include <imgui.h>
#include <sstream>

using std::string;
using namespace ospcommon;

// Static local helper functions //////////////////////////////////////////////

// helper function to write the rendered image as PPM file
static void writePPM(const string &fileName, const int sizeX, const int sizeY,
                     const uint32_t *pixel)
{
  FILE *file = fopen(fileName.c_str(), "wb");
  fprintf(file, "P6\n%i %i\n255\n", sizeX, sizeY);
  unsigned char *out = (unsigned char *)alloca(3*sizeX);
  for (int y = 0; y < sizeY; y++) {
    const unsigned char *in = (const unsigned char *)&pixel[(sizeY-1-y)*sizeX];
    for (int x = 0; x < sizeX; x++) {
      out[3*x + 0] = in[4*x + 0];
      out[3*x + 1] = in[4*x + 1];
      out[3*x + 2] = in[4*x + 2];
    }
    fwrite(out, 3*sizeX, sizeof(char), file);
  }
  fprintf(file, "\n");
  fclose(file);
}

// ImGuiViewer definitions ////////////////////////////////////////////////////

namespace ospray {

ImGuiViewer::ImGuiViewer(const std::deque<box3f> &worldBounds,
                         const std::deque<cpp::Model> &model,
                         cpp::Renderer renderer,
                         cpp::Camera camera,
                         sg::NodeH scenegraph)
  : ImGui3DWidget(ImGui3DWidget::FRAMEBUFFER_NONE),
    sceneModels(model),
    renderer(renderer),
    camera(camera),
    worldBounds(worldBounds),
    lockFirstAnimationFrame(false),
    scenegraph(scenegraph),
    renderEngine(scenegraph)
{
  if (!worldBounds.empty())
    setWorldBounds(worldBounds[0]);

  renderer.set("model",  sceneModels[0]);
  renderer.set("camera", camera);

  // renderEngine.setRenderer(renderer);
  renderEngine.setFbSize({1024, 768});

  renderEngine.scheduleObjectCommit(renderer);
  renderEngine.start();

  frameTimer = ospcommon::getSysTime();
  animationTimer = 0.;
  animationFrameDelta = .03;
  animationFrameId = 0;
  animationPaused = false;
  originalView = viewPort;
  scale = vec3f(1,1,1);

}

ImGuiViewer::~ImGuiViewer()
{
  renderEngine.stop();
}

void ImGuiViewer::setRenderer(OSPRenderer renderer)
{
  this->renderer = renderer;
  // renderEngine.setRenderer(renderer);
}

void ImGuiViewer::reshape(const vec2i &newSize)
{
  ImGui3DWidget::reshape(newSize);
  windowSize = newSize;

  viewPort.modified = true;

  renderEngine.setFbSize(newSize);
  scenegraph["frameBuffer"]["size"]->setValue(newSize);
  pixelBuffer.resize(newSize.x * newSize.y);
}

void ImGuiViewer::keypress(char key)
{
  switch (key) {
  case ' ':
    animationPaused = !animationPaused;
    break;
  case '=':
    animationFrameDelta = max(animationFrameDelta-0.01, 0.0001);
    break;
  case '-':
    animationFrameDelta = min(animationFrameDelta+0.01, 1.0);
    break;
  case 'R':
    toggleRenderingPaused();
    break;
  case '!':
    saveScreenshot("ospimguiviewer");
    break;
  case 'X':
    if (viewPort.up == vec3f(1,0,0) || viewPort.up == vec3f(-1.f,0,0)) {
      viewPort.up = - viewPort.up;
    } else {
      viewPort.up = vec3f(1,0,0);
    }
    viewPort.modified = true;
    break;
  case 'Y':
    if (viewPort.up == vec3f(0,1,0) || viewPort.up == vec3f(0,-1.f,0)) {
      viewPort.up = - viewPort.up;
    } else {
      viewPort.up = vec3f(0,1,0);
    }
    viewPort.modified = true;
    break;
  case 'Z':
    if (viewPort.up == vec3f(0,0,1) || viewPort.up == vec3f(0,0,-1.f)) {
      viewPort.up = - viewPort.up;
    } else {
      viewPort.up = vec3f(0,0,1);
    }
    viewPort.modified = true;
    break;
  case 'c':
    viewPort.modified = true;//Reset accumulation
    break;
  case 'r':
    resetView();
    break;
  case 'p':
    printViewport();
    break;
  case 27 /*ESC*/:
  case 'q':
  case 'Q':
    renderEngine.stop();
    std::exit(0);
    break;
  default:
    ImGui3DWidget::keypress(key);
  }
}

void ImGuiViewer::resetView()
{
  auto oldAspect = viewPort.aspect;
  viewPort = originalView;
  viewPort.aspect = oldAspect;
}

void ImGuiViewer::printViewport()
{
  printf("-vp %f %f %f -vu %f %f %f -vi %f %f %f\n",
         viewPort.from.x, viewPort.from.y, viewPort.from.z,
         viewPort.up.x,   viewPort.up.y,   viewPort.up.z,
         viewPort.at.x,   viewPort.at.y,   viewPort.at.z);
  fflush(stdout);
}

void ImGuiViewer::saveScreenshot(const std::string &basename)
{
  writePPM(basename + ".ppm", windowSize.x, windowSize.y, pixelBuffer.data());
  std::cout << "saved current frame to '" << basename << ".ppm'" << std::endl;
}

void ImGuiViewer::toggleRenderingPaused()
{
  renderingPaused = !renderingPaused;
  renderingPaused ? renderEngine.stop() : renderEngine.start();
}

void ImGuiViewer::setWorldBounds(const box3f &worldBounds) {
  ImGui3DWidget::setWorldBounds(worldBounds);
  aoDistance = (worldBounds.upper.x - worldBounds.lower.x)/4.f;
  renderer.set("aoDistance", aoDistance);
  renderEngine.scheduleObjectCommit(renderer);
}

void ImGuiViewer::display()
{
  updateAnimation(ospcommon::getSysTime()-frameTimer);
  frameTimer = ospcommon::getSysTime();

  auto dir = viewPort.at - viewPort.from;
  dir = normalize(dir);
  scenegraph["camera"]["dir"]->setValue(dir);
  scenegraph["camera"]["pos"]->setValue(viewPort.from);
  scenegraph["camera"]["up"]->setValue(viewPort.up);
  // scenegraph["camera"]["aspect"]->setValue(viewPort.aspect);
  // scenegraph["camera"]["fovy"]->setValue(viewPort.openingAngle);

  if (renderEngine.hasNewFrame()) {
    auto &mappedFB = renderEngine.mapFramebuffer();
    auto nPixels = windowSize.x * windowSize.y;

    if (mappedFB.size() == nPixels) {
      auto *srcPixels = mappedFB.data();
      auto *dstPixels = pixelBuffer.data();
      memcpy(dstPixels, srcPixels, nPixels * sizeof(uint32_t));
      lastFrameFPS = renderEngine.lastFrameFps();
      renderTime = 1.f/lastFrameFPS;
    }

    renderEngine.unmapFramebuffer();
  }

  //Carson: ucharFB would appear to be a global variable used by imgui3D widget.
  ucharFB = pixelBuffer.data();
  frameBufferMode = ImGui3DWidget::FRAMEBUFFER_UCHAR;
  ImGui3DWidget::display();

  lastTotalTime = ImGui3DWidget::totalTime;
  lastGUITime = ImGui3DWidget::guiTime;
  lastDisplayTime = ImGui3DWidget::displayTime;

  // that pointer is no longer valid, so set it to null
  ucharFB = nullptr;
}

void ImGuiViewer::updateAnimation(double deltaSeconds)
{
  if (sceneModels.size() < 2)
    return;
  if (animationPaused)
    return;
  animationTimer += deltaSeconds;
  int framesSize = sceneModels.size();
  const int frameStart = (lockFirstAnimationFrame ? 1 : 0);
  if (lockFirstAnimationFrame)
    framesSize--;

  if (animationTimer > animationFrameDelta)
  {
    animationFrameId++;

    //set animation time to remainder off of delta
    animationTimer -= int(animationTimer/deltaSeconds) * deltaSeconds;

    size_t dataFrameId = animationFrameId%framesSize+frameStart;
    if (lockFirstAnimationFrame)
    {
      ospcommon::affine3f xfm = ospcommon::one;
      xfm *= ospcommon::affine3f::translate(translate)
             * ospcommon::affine3f::scale(scale);
      OSPGeometry dynInst =
              ospNewInstance((OSPModel)sceneModels[dataFrameId].object(),
              (osp::affine3f&)xfm);
      ospray::cpp::Model worldModel = ospNewModel();
      ospcommon::affine3f staticXFM = ospcommon::one;
      OSPGeometry staticInst =
              ospNewInstance((OSPModel)sceneModels[0].object(),
              (osp::affine3f&)staticXFM);
      //Carson: TODO: creating new world model every frame unecessary
      worldModel.addGeometry(staticInst);
      worldModel.addGeometry(dynInst);
      worldModel.commit();
      renderer.set("model",  worldModel);
    }
    else
    {
      renderer.set("model",  sceneModels[dataFrameId]);
    }

    renderEngine.scheduleObjectCommit(renderer);
  }
}

void ImGuiViewer::buildGui()
{
  ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;

  static bool demo_window = false;

  ImGui::SetNextWindowSizeConstraints(ImVec2(500,800), ImVec2(2048,2048));
  ImGui::Begin("Viewer Controls: press 'g' to show/hide", nullptr, flags);
  ImGui::SetWindowFontScale(0.5f*fontScale);
  // ImGui::SetScrollY(1.f);

  if (ImGui::BeginMenuBar())
  {
    if (ImGui::BeginMenu("App"))
    {
#if 1
      ImGui::Checkbox("Show ImGui Demo Window", &demo_window);
#endif

      ImGui::Checkbox("Auto-Rotate", &animating);
      bool paused = renderingPaused;
      if (ImGui::Checkbox("Pause Rendering", &paused)) {
        toggleRenderingPaused();
      }
      if (ImGui::MenuItem("Take Screenshot")) saveScreenshot("ospimguiviewer");
      if (ImGui::MenuItem("Quit")) {
        renderEngine.stop();
        std::exit(0);
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
      bool orbitMode = (manipulator == inspectCenterManipulator);
      bool flyMode   = (manipulator == moveModeManipulator);

      if (ImGui::Checkbox("Orbit Camera Mode", &orbitMode)) {
        manipulator = inspectCenterManipulator;
      }
      if (ImGui::Checkbox("Fly Camera Mode", &flyMode)) {
        manipulator = moveModeManipulator;
      }

      if (ImGui::MenuItem("Reset View")) resetView();
      if (ImGui::MenuItem("Reset Accumulation")) viewPort.modified = true;
      if (ImGui::MenuItem("Print View")) printViewport();

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  if (demo_window) ImGui::ShowTestWindow(&demo_window);

  if (ImGui::CollapsingHeader("FPS Statistics", "FPS Statistics", true, false))
  {
    ImGui::NewLine();
    ImGui::Text("OSPRay render rate: %.1f FPS", lastFrameFPS);
    ImGui::Text("  Total GUI frame rate: %.1f FPS", ImGui::GetIO().Framerate);
    ImGui::Text("  Total 3dwidget time: %.1fms ", lastTotalTime*1000.f);
    ImGui::Text("  GUI time: %.1fms ", lastGUITime*1000.f);
    ImGui::Text("  display pixel time: %.1fms ", lastDisplayTime*1000.f);
    ImGui3DWidget::display();
    ImGui::NewLine();
  }

  bool renderer_changed = false;
  //build SceneGraph GUI
  static float vec4fv[4] = { 0.10f, 0.20f, 0.30f, 0.44f };
  if (ImGui::CollapsingHeader("SceneGraph", "SceneGraph", true, true))
  {
   buildGUINode(scenegraph, renderer_changed, 0);
  }

  //if (ImGui::CollapsingHeader("Renderer Parameters"))
  if (0)
  {
    static int numThreads = -1;
    if (ImGui::InputInt("# threads", &numThreads, 1)) {
      renderEngine.stop();
      renderEngine.start(numThreads);
      renderer_changed = true;
    }

    static int ao = 1;
    if (ImGui::SliderInt("aoSamples", &ao, 0, 32)) {
      renderer.set("aoSamples", ao);
      renderer_changed = true;
    }

    if (ImGui::InputFloat("aoDistance", &aoDistance)) {
      renderer.set("aoDistance", aoDistance);
      renderer_changed = true;
    }

    static bool ao_transparency = false;
    if (ImGui::Checkbox("ao transparency", &ao_transparency)) {
      renderer.set("aoTransparencyEnabled", int(ao_transparency));
      renderer_changed = true;
    }

    static bool shadows = true;
    if (ImGui::Checkbox("shadows", &shadows)) {
      renderer.set("shadowsEnabled", int(shadows));
      renderer_changed = true;
    }

    static bool singleSidedLighting = true;
    if (ImGui::Checkbox("single_sided_lighting", &singleSidedLighting)) {
      renderer.set("oneSidedLighting", int(singleSidedLighting));
      renderer_changed = true;
    }

    static int exponent = -6;
    if (ImGui::SliderInt("ray_epsilon (exponent)", &exponent, -10, 2)) {
      renderer.set("epsilon", ospcommon::pow(10.f, (float)exponent));
      renderer_changed = true;
    }

    static int spp = 1;
    if (ImGui::SliderInt("spp", &spp, -4, 16)) {
      renderer.set("spp", spp);
      renderer_changed = true;
    }

    static ImVec4 bg_color = ImColor(255, 255, 255);
    if (ImGui::ColorEdit3("bg_color", (float*)&bg_color)) {
      renderer.set("bgColor", bg_color.x, bg_color.y, bg_color.z);
      renderer_changed = true;
    }
  }
  if (renderer_changed)
    renderEngine.scheduleObjectCommit(renderer);

  ImGui::End();
}

void ImGuiViewer::buildGUINode(sg::NodeH node, bool &renderer_changed, int indent)
{
  int styles=0;
  if (!node->isValid())
  {
    ImGui::PushStyleColor(ImGuiCol_Text, ImColor(200, 75, 48,255));
    styles++;
  }
  std::string text;
  text += std::string(node->getName()+" : ");
  if (node->getType() == "vec3f")
  {
      ImGui::Text(text.c_str());
      ImGui::SameLine();
      vec3f val = node->getValue().get<vec3f>();
      text = "##"+((std::ostringstream&)(std::ostringstream("") << node.get())).str(); //TODO: use unique uuid for every node
      if ((node->getFlags() & sg::NodeFlags::gui_color))
      {
        if (ImGui::ColorEdit3(text.c_str(), (float*)&val.x))
          node->setValue(val);
      }
      else if ((node->getFlags() & sg::NodeFlags::gui_slider))
      {
        if (ImGui::SliderFloat3(text.c_str(), &val.x, node->getMin().get<vec3f>().x, node->getMax().get<vec3f>().x))
          node->setValue(val);
      }
      else if (ImGui::DragFloat3(text.c_str(), (float*)&val.x, .01f)) {
        node->setValue(val);
      }
  }
  else if (node->getType() == "vec2f")
  {
    ImGui::Text(text.c_str(),"");
    ImGui::SameLine();
    vec2f val = node->getValue().get<vec2f>();
    text = "##"+((std::ostringstream&)(std::ostringstream("") << node.get())).str(); //TODO: use unique uuid for every node
    if (ImGui::DragFloat2(text.c_str(), (float*)&val.x, .01f)) {
      node->setValue(val);
    }
  }
  else if (node->getType() == "vec2i")
  {
    ImGui::Text(text.c_str());
    ImGui::SameLine();
    vec2i val = node->getValue().get<vec2i>();
    text = "##"+((std::ostringstream&)(std::ostringstream("") << node.get())).str(); //TODO: use unique uuid for every node
    if (ImGui::DragInt2(text.c_str(), (int*)&val.x)) {
      node->setValue(val);
    }
  }
  else if (node->getType() == "float")
  {
    ImGui::Text(text.c_str(),"");
    ImGui::SameLine();
    float val = node->getValue().get<float>();
    text = "##"+((std::ostringstream&)(std::ostringstream("") << node.get())).str(); //TODO: use unique uuid for every node
    if ((node->getFlags() & sg::NodeFlags::gui_slider))
    {
      if (ImGui::SliderFloat(text.c_str(), &val, node->getMin().get<float>(), node->getMax().get<float>()))
        node->setValue(val);
    }
    else if (ImGui::DragFloat(text.c_str(), &val, .01f)) {
      node->setValue(val);
    }
  }
  else if (node->getType() == "bool")
  {
    ImGui::Text(text.c_str(),"");
    ImGui::SameLine();
    bool val = node->getValue().get<bool>();
    text = "##"+((std::ostringstream&)(std::ostringstream("") << node.get())).str(); //TODO: use unique uuid for every node
    if (ImGui::Checkbox(text.c_str(), &val)) {
      node->setValue(val);
    }
  }
  else if (node->getType() == "int")
  {
    ImGui::Text(text.c_str(),"");
    ImGui::SameLine();
    int val = node->getValue().get<int>();
    text = "##"+((std::ostringstream&)(std::ostringstream("") << node.get())).str(); //TODO: use unique uuid for every node
    if ((node->getFlags() & sg::NodeFlags::gui_slider))
    {
      if (ImGui::SliderInt(text.c_str(), &val, node->getMin().get<int>(), node->getMax().get<int>()))
      {
        node->setValue(val);
      }
    }
    else if (ImGui::DragInt(text.c_str(), &val)) {
      node->setValue(val);
    }
  }
  else if (node->getType() == "string")
  {
    std::string value = node->getValue<std::string>().c_str();
    char* buf = (char*)malloc(value.size()+1+256);
    strcpy(buf,value.c_str());
    buf[value.size()] = '\0';
    ImGui::Text(text.c_str(),"");
    ImGui::SameLine();
    text = "##"+((std::ostringstream&)(std::ostringstream("") << node.get())).str(); //TODO: use unique uuid for every node
    if (ImGui::InputText(text.c_str(), buf, value.size()+256, ImGuiInputTextFlags_EnterReturnsTrue))
    {
      node->setValue(std::string(buf));
    }
  }
  else if (node->getType() == "TransferFunction") {
    text += "TODO WILL";
    ImGui::Text(text.c_str());
    if (!node->getParam("transferFunctionWidget")) {
      std::shared_ptr<sg::TransferFunction> tfn = std::dynamic_pointer_cast<sg::TransferFunction>(node.get());
      node->setParam("transferFunctionWidget", TransferFunction(tfn));
    }
    auto tfnWidget = dynamic_cast<sg::ParamT<TransferFunction>*>(node->getParam("transferFunctionWidget").get());
    assert(tfnWidget);
    tfnWidget->value.render();
    tfnWidget->value.drawUi();
  }
  else  // generic holder node
  {
    text+=node->getType();
    text += "##"+((std::ostringstream&)(std::ostringstream("") << node.get())).str(); //TODO: use unique uuid for every node
    if (ImGui::TreeNodeEx(text.c_str(), (indent > 0) ? 0 : ImGuiTreeNodeFlags_DefaultOpen))
    {
      // if (node->getName() == "lights")
      {
        std::string popupName = "Add Node: ##"+((std::ostringstream&)(std::ostringstream("") << node.get())).str();
        float value = 1.f;
        static bool addChild = true;
        if (ImGui::BeginPopupContextItem("item context menu"))
        {
            char buf[256];
            buf[0]='\0';
            if (ImGui::InputText("node name: ", buf, 256, ImGuiInputTextFlags_EnterReturnsTrue))
            {
              std::cout << "add node: \"" << buf << "\"\n";
              try
              {
                static int counter = 0;
                std::stringstream ss;
                ss << "userDefinedNode" << counter++;
                node->add(sg::createNode(ss.str(), buf));
              }
              catch (...)
              {
                std::cerr << "invalid node type: " << buf << std::endl;
              }
            }

            ImGui::EndPopup();

        }
        if (addChild)
        {
          if (ImGui::BeginPopup(popupName.c_str()))
          {
            char buf[256];
            buf[0]='\0';
            if (ImGui::InputText("node name: ", buf, 256, ImGuiInputTextFlags_EnterReturnsTrue))
            {
              std::cout << "add node: \"" << buf << "\"\n";
              try
              {
                static int counter = 0;
                std::stringstream ss;
                ss << "userDefinedNode" << counter++;
                node->add(sg::createNode(ss.str(), buf));
              }
              catch (...)
              {
                std::cerr << "invalid node type: " << buf << std::endl;
              }
            }
            ImGui::EndPopup();
          }
          else
            addChild = false;
        }
        // std::string popupName = "Add Node: ##"+((std::ostringstream&)(std::ostringstream("") << node.get())).str();
        // if (ImGui::SmallButton("+"))
        // {
        //   std::cout << "click!\n";
        //   ImGui::OpenPopup(popupName.c_str());
        // }
        // if (ImGui::BeginPopup(popupName.c_str()))
        // {
        //   char buf[256];
        //   buf[0]='\0';
        //   if (ImGui::InputText("node name: ", buf, 256, ImGuiInputTextFlags_EnterReturnsTrue))
        //   {
        //     std::cout << "add node: \"" << buf << "\"\n";
        //     try
        //     {
        //       static int counter = 0;
        //       std::stringstream ss;
        //       ss << "userDefinedNode" << counter++;
        //       node->add(sg::createNode(ss.str(), buf));
        //     }
        //     catch (...)
        //     {
        //       std::cerr << "invalid node type: " << buf << std::endl;
        //     }
        //   }
        //   ImGui::EndPopup();
        // }
      }
      if (!node->isValid())
        ImGui::PopStyleColor(styles--);

      for(auto child : node->getChildren() )
      {
        buildGUINode(child, renderer_changed, ++indent);
      }
      ImGui::TreePop();
    }
  }
  if (!node->isValid())
    ImGui::PopStyleColor(styles--);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(node->getDocumentation().c_str());
}

}// namepace ospray
