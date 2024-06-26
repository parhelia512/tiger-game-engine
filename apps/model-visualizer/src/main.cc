// clang-format off
#include <glad/glad.h>
// clang-format on

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <fmt/core.h>
#include <imgui.h>

#include <iostream>
#include <memory>

#include "bloom.h"
#include "controller/sightseeing_controller.h"
#include "deferred_shading_render_quad.h"
#include "equirectangular_map.h"
#include "model.h"
#include "mouse.h"
#include "multi_draw_indirect.h"
#include "smaa.h"
#include "tone_mapping/aces.h"
#include "utils.h"

using namespace glm;
using namespace std;

class Controller : public SightseeingController {
 private:
  DeferredShadingRenderQuad *deferred_shading_render_quad_;
  PostProcesses *post_processes_;

  void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
    SightseeingController::FramebufferSizeCallback(window, width, height);
    deferred_shading_render_quad_->Resize(width, height);
    post_processes_->Resize(width, height);
  }

 public:
  Controller(Camera *camera,
             DeferredShadingRenderQuad *deferred_shading_render_quad,
             PostProcesses *post_processes, uint32_t width, uint32_t height,
             GLFWwindow *window)
      : SightseeingController(camera, width, height, window),
        deferred_shading_render_quad_(deferred_shading_render_quad),
        post_processes_(post_processes) {
    glfwSetFramebufferSizeCallback(
        window, [](GLFWwindow *window, int width, int height) {
          Controller *self = (Controller *)glfwGetWindowUserPointer(window);
          self->FramebufferSizeCallback(window, width, height);
        });
  }
};

std::unique_ptr<MultiDrawIndirect> multi_draw_indirect;
std::unique_ptr<DeferredShadingRenderQuad> deferred_shading_render_quad_ptr;
std::unique_ptr<PostProcesses> post_processes_ptr;
std::unique_ptr<Model> model_ptr;
std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<EquirectangularMap> equirectangular_map_ptr;
std::unique_ptr<Controller> controller_ptr;

int default_shading_choice = 0;
int enable_ssao = 0;
int enable_smaa = 0;
int enable_bloom = 0;
int enable_moving_shadow = 0;

GLFWwindow *window;

void ImGuiInit() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize.x = controller_ptr->width();
  io.DisplaySize.y = controller_ptr->height();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  ImGui::StyleColorsClassic();
  auto font = io.Fonts->AddFontDefault();
  io.Fonts->Build();
}

void ImGuiWindow() {
  // model
  static char buf[1 << 10] = {0};
  const char *choices[] = {"off", "on"};

  ImGui::Begin("Panel");
  if (ImGui::InputText("Model path", buf, sizeof(buf),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    model_ptr.reset(new Model(buf, true, true));
    multi_draw_indirect.reset(new MultiDrawIndirect());
    model_ptr->SubmitToMultiDrawIndirect(multi_draw_indirect.get(), 1);
    multi_draw_indirect->PrepareForDraw();
  }
  ImGui::ListBox("Default shading", &default_shading_choice, choices,
                 IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable SSAO", &enable_ssao, choices, IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable SMAA", &enable_smaa, choices, IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable Bloom", &enable_bloom, choices, IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable Moving Shadow", &enable_moving_shadow, choices,
                 IM_ARRAYSIZE(choices));
  ImGui::End();

  camera_ptr->ImGuiWindow();
  light_sources_ptr->ImGuiWindow(camera_ptr.get());
  post_processes_ptr->ImGuiWindow();

  post_processes_ptr->Enable(0, enable_bloom);
  post_processes_ptr->Enable(2, enable_smaa);
}

void Init(uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window =
      glfwCreateWindow(width, height, "Model Visualizer", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Shader::include_directories = {"./shaders"};

  deferred_shading_render_quad_ptr.reset(
      new DeferredShadingRenderQuad(width, height));

  post_processes_ptr.reset(new PostProcesses());
  post_processes_ptr->Add(std::unique_ptr<Bloom>(new Bloom(width, height, 6)));
  post_processes_ptr->Add(std::unique_ptr<tone_mapping::ACES>(
      new tone_mapping::ACES(width, height)));
  post_processes_ptr->Add(
      std::unique_ptr<SMAA>(new SMAA("./third_party/smaa", width, height)));

  camera_ptr = make_unique<Camera>(
      vec3(7, 9, 0), static_cast<double>(width) / height,
      -glm::pi<double>() / 2, 0, glm::radians(60.f), 0.1, 500);
  camera_ptr->set_front(-camera_ptr->position());

  light_sources_ptr = make_unique<LightSources>();
  light_sources_ptr->AddDirectional(make_unique<DirectionalLight>(
      vec3(0, -1, 0.5), vec3(10), camera_ptr.get()));
  light_sources_ptr->AddAmbient(make_unique<AmbientLight>(vec3(0.1)));

  model_ptr.reset(
      new Model("resources/Bistro_v5_2/BistroExterior.fbx", true, true));
  multi_draw_indirect.reset(new MultiDrawIndirect());
  model_ptr->SubmitToMultiDrawIndirect(multi_draw_indirect.get(), 1);
  multi_draw_indirect->PrepareForDraw();

  equirectangular_map_ptr.reset(new EquirectangularMap(
      "resources/kloofendal_48d_partly_cloudy_puresky_4k.hdr", 2048));

  controller_ptr = make_unique<Controller>(
      camera_ptr.get(), deferred_shading_render_quad_ptr.get(),
      post_processes_ptr.get(), width, height, window);

  ImGuiInit();
}

void MovingShadow(double time) {
  if (enable_moving_shadow && light_sources_ptr->SizeDirectional() >= 1) {
    auto light = light_sources_ptr->GetDirectional(0);
    if (light->shadow() != nullptr) {
      float input = time * 1e-2;
      float x = sin(input);
      float y = -abs(cos(input));
      light->set_dir(glm::vec3(x, y, 0));
    }
  }
}

int main(int argc, char *argv[]) {
  try {
    Init(1920, 1080);
  } catch (const std::exception &e) {
    fmt::print(stderr, "[error] {}\n", e.what());
    exit(1);
  }

  while (!glfwWindowShouldClose(window)) {
    static uint32_t fps = 0;
    static double last_time_for_fps = glfwGetTime();
    static double last_time = glfwGetTime();
    double current_time = glfwGetTime();
    double delta_time = current_time - last_time;
    last_time = current_time;

    Keyboard::shared.Elapse(delta_time);

    {
      fps += 1;
      if (current_time - last_time_for_fps >= 1.0) {
        char buf[1 << 10];
        sprintf(buf, "Model Visualizer | FPS: %d\n", fps);
        glfwSetWindowTitle(window, buf);
        fps = 0;
        last_time_for_fps = current_time;
      }
    }

    glfwPollEvents();

    MovingShadow(current_time);

    // draw depth map first
    light_sources_ptr->DrawDepthForShadow(
        [](int32_t directional_index, int32_t point_index) {
          multi_draw_indirect->DrawDepthForShadow(
              light_sources_ptr.get(), directional_index, point_index,
              {{model_ptr.get(), {{-1, 0, glm::mat4(1), glm::vec4(0)}}}});
        });

    deferred_shading_render_quad_ptr->TwoPasses(
        camera_ptr.get(), light_sources_ptr.get(), enable_ssao, nullptr,
        []() {
          glEnable(GL_CULL_FACE);
          glCullFace(GL_BACK);
          glClearColor(0, 0, 0, 1);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          equirectangular_map_ptr->skybox()->Draw(camera_ptr.get());
        },
        []() {
          glDisable(GL_CULL_FACE);
          multi_draw_indirect->Draw(
              camera_ptr.get(), nullptr, nullptr, true, nullptr,
              default_shading_choice, true,
              {{model_ptr.get(), {{-1, 0, glm::mat4(1), glm::vec4(0)}}}});
        },
        post_processes_ptr->fbo());

    post_processes_ptr->Draw(nullptr);

    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    ImGuiWindow();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }
  return 0;
}