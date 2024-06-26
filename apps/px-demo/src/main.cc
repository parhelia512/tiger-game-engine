// clang-format off
#include <glad/glad.h>
// clang-format on

#include <GLFW/glfw3.h>
#include <PxPhysicsAPI.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <fmt/core.h>
#include <imgui.h>

#include <iostream>
#include <memory>

#include "controller/sightseeing_controller.h"
#include "model.h"
#include "mouse.h"
#include "multi_draw_indirect.h"
#include "physics/character_controller.h"
#include "physics/collision_model.h"
#include "skybox.h"
#include "tone_mapping/aces.h"

using namespace glm;
using namespace std;

class Controller : public SightseeingController {
 private:
  OITRenderQuad *oit_render_quad_;
  PostProcess *post_processes_;
  CharacterController *character_controller_;

  void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
    SightseeingController::FramebufferSizeCallback(window, width, height);
    oit_render_quad_->Resize(width, height);
    post_processes_->Resize(width, height);
  }

 public:
  Controller(Camera *camera, OITRenderQuad *oit_render_quad,
             PostProcess *post_processes,
             CharacterController *character_controller, uint32_t width,
             uint32_t height, GLFWwindow *window)
      : SightseeingController(camera, width, height, window),
        oit_render_quad_(oit_render_quad),
        post_processes_(post_processes),
        character_controller_(character_controller) {
    glfwSetFramebufferSizeCallback(
        window, [](GLFWwindow *window, int width, int height) {
          Controller *self = (Controller *)glfwGetWindowUserPointer(window);
          self->FramebufferSizeCallback(window, width, height);
        });

    Keyboard::shared.Clear();
    Keyboard::shared.Register(
        [this](Keyboard::KeyboardState state, double delta) {
          if (state[GLFW_KEY_ESCAPE]) {
            glfwSetWindowShouldClose(this->window_, GL_TRUE);
          } else if (this->rotating_camera_mode_) {
            glm::vec3 disp(0);
            glm::vec3 front = glm::cross(camera_->left(), camera_->up());
            float distance = state[GLFW_KEY_LEFT_SHIFT] ? delta * 4 : delta * 2;
            if (state[GLFW_KEY_W]) disp += front * distance;
            if (state[GLFW_KEY_S]) disp += -front * distance;
            if (state[GLFW_KEY_A]) disp += camera_->left() * distance;
            if (state[GLFW_KEY_D]) disp += -camera_->left() * distance;
            static bool space_already_pressed = false;
            if (state[GLFW_KEY_SPACE] && !space_already_pressed) {
              disp *= 100;
              disp.y = 10;
              character_controller_->Jump(disp, delta);
            } else {
              character_controller_->Move(disp, delta);
            }
            space_already_pressed = state[GLFW_KEY_SPACE];
          } else {
            // we must make sure that gravity is applied
            character_controller_->Move(glm::vec3(0), delta);
          }
        });
  }
};

// scene
std::unique_ptr<MultiDrawIndirect> multi_draw_indirect;
std::unique_ptr<OITRenderQuad> oit_render_quad_ptr;
std::unique_ptr<Model> scene_model_ptr;
std::unique_ptr<PostProcesses> post_processes_ptr;
std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<Skybox> skybox_ptr;
std::unique_ptr<Controller> controller_ptr;

GLFWwindow *window;

// physics
physx::PxDefaultAllocator px_allocator;
physx::PxDefaultErrorCallback px_error_callback;
physx::PxFoundation *foundation = nullptr;
physx::PxPvd *pvd = nullptr;
physx::PxPhysics *physics = nullptr;
physx::PxDefaultCpuDispatcher *dispatcher = nullptr;
physx::PxScene *scene = nullptr;
physx::PxMaterial *material = nullptr;
physx::PxControllerManager *manager = nullptr;

std::unique_ptr<CharacterController> character_controller = nullptr;

void InitPhysics() {
  foundation =
      PxCreateFoundation(PX_PHYSICS_VERSION, px_allocator, px_error_callback);

  pvd = PxCreatePvd(*foundation);
  physx::PxPvdTransport *transport =
      physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
  pvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);

  physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation,
                            physx::PxTolerancesScale(), true, pvd);

  physx::PxSceneDesc scene_desc(physics->getTolerancesScale());
  scene_desc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
  dispatcher = physx::PxDefaultCpuDispatcherCreate(2);
  scene_desc.cpuDispatcher = dispatcher;
  scene_desc.filterShader = physx::PxDefaultSimulationFilterShader;

  scene = physics->createScene(scene_desc);
  scene->setVisualizationParameter(
      physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
  scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE,
                                   1.0f);

  physx::PxPvdSceneClient *pvd_client = scene->getScenePvdClient();
  if (pvd_client) {
    pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS,
                                true);
    pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS,
                                true);
    pvd_client->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES,
                                true);
  }

  material = physics->createMaterial(0.5f, 0.5f, 0.6f);

  CollisionModel model("resources/sponza_collision.fbx");

  physx::PxRigidStatic *actor = physics->createRigidStatic(
      physx::PxTransform(physx::PxQuat(0, physx::PxVec3(0.0f, 1.0f, 0.0f))));
  actor->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true);
  physx::PxRigidActorExt::createExclusiveShape(
      *actor, physx::PxTriangleMeshGeometry(model.mesh(physics)), *material);
  scene->addActor(*actor);

  manager = PxCreateControllerManager(*scene);
  physx::PxCapsuleControllerDesc capsule_controller_desc;
  capsule_controller_desc.height = 0.5;
  capsule_controller_desc.radius = 0.15;
  capsule_controller_desc.contactOffset = 0.2;
  capsule_controller_desc.climbingMode = physx::PxCapsuleClimbingMode::eEASY;
  capsule_controller_desc.position = {0, 10, 0};
  capsule_controller_desc.upDirection = {0, 1, 0};
  capsule_controller_desc.stepOffset = 0.25;
  capsule_controller_desc.material = material;

  physx::PxCapsuleController *controller =
      static_cast<physx::PxCapsuleController *>(
          manager->createController(capsule_controller_desc));

  character_controller =
      std::make_unique<CharacterController>(scene, controller);
}

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
  // shadow
  camera_ptr->ImGuiWindow();
  light_sources_ptr->ImGuiWindow(camera_ptr.get());
  post_processes_ptr->ImGuiWindow();
}

void Init(uint32_t width, uint32_t height) {
  InitPhysics();

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window = glfwCreateWindow(width, height, "PhysX Demo", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Shader::include_directories = {"./shaders"};

  post_processes_ptr.reset(new PostProcesses());
  post_processes_ptr->Add(std::unique_ptr<tone_mapping::ACES>(
      new tone_mapping::ACES(width, height)));

  oit_render_quad_ptr = make_unique<OITRenderQuad>(width, height);

  scene_model_ptr =
      make_unique<Model>("resources/sponza/Sponza.gltf", false, true);
  multi_draw_indirect.reset(new MultiDrawIndirect());
  scene_model_ptr->SubmitToMultiDrawIndirect(multi_draw_indirect.get(), 1);
  multi_draw_indirect->PrepareForDraw();
  camera_ptr = make_unique<Camera>(
      vec3(7, 9, 0), static_cast<double>(width) / height,
      -glm::pi<double>() / 2, 0, glm::radians(60.f), 0.1, 500);
  camera_ptr->set_front(-camera_ptr->position());

  light_sources_ptr = make_unique<LightSources>();
  light_sources_ptr->AddDirectional(make_unique<DirectionalLight>(
      vec3(0, -1, 0.1), vec3(5), camera_ptr.get()));
  light_sources_ptr->AddAmbient(make_unique<AmbientLight>(vec3(0.1)));

  skybox_ptr = make_unique<Skybox>("resources/skyboxes/cloud");

  controller_ptr = make_unique<Controller>(
      camera_ptr.get(), oit_render_quad_ptr.get(), post_processes_ptr.get(),
      character_controller.get(), width, height, window);

  ImGuiInit();
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
        sprintf(buf, "PhysX Demo | FPS: %d\n", fps);
        glfwSetWindowTitle(window, buf);
        fps = 0;
        last_time_for_fps = current_time;
      }
    }

    scene->simulate((float)delta_time);
    scene->fetchResults(true);
    glfwPollEvents();
    camera_ptr->set_position(character_controller->position());

    // draw depth map first
    light_sources_ptr->DrawDepthForShadow(
        [](int32_t directional_index, int32_t point_index) {
          multi_draw_indirect->DrawDepthForShadow(
              light_sources_ptr.get(), directional_index, point_index,
              {{scene_model_ptr.get(), {{-1, 0, glm::mat4(1), glm::vec4(0)}}}});
        });

    oit_render_quad_ptr->TwoPasses(
        []() {
          glEnable(GL_CULL_FACE);
          glCullFace(GL_BACK);
          glClearColor(0, 0, 0, 1);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          skybox_ptr->Draw(camera_ptr.get());
        },
        []() {
          glEnable(GL_CULL_FACE);
          glCullFace(GL_BACK);
          multi_draw_indirect->Draw(
              camera_ptr.get(), light_sources_ptr.get(),
              oit_render_quad_ptr.get(), false, nullptr, false, true,
              {{scene_model_ptr.get(), {{-1, 0, glm::mat4(1), glm::vec4(0)}}}});
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