#ifndef BIFROST_ENGINE_HPP
#define BIFROST_ENGINE_HPP

#include "bifrost/asset_io/bifrost_assets.hpp"
#include "bifrost/asset_io/bifrost_scene.hpp"
#include "bifrost/ecs/bifrost_iecs_system.hpp"
#include "bifrost/graphics/bifrost_debug_renderer.hpp"
#include "bifrost/graphics/bifrost_standard_renderer.hpp"
#include "bifrost/memory/bifrost_linear_allocator.hpp"
#include "bifrost/memory/bifrost_pool_allocator.hpp"
#include "bifrost/script/bifrost_vm.hpp"
#include "bifrost_game_state_machine.hpp"
#include "bifrost_igame_state_layer.hpp"

// ReSharper disable once CppUnusedIncludeDirective
#include "bifrost/memory/bifrost_c_allocator.hpp"
// ReSharper disable once CppUnusedIncludeDirective
#include "bifrost/memory/bifrost_freelist_allocator.hpp"

#include <utility>

using BifrostEngineCreateParams = bfGfxContextCreateParams;

static void userErrorFn(struct BifrostVM_t* vm, BifrostVMError err, int line_no, const char* message)
{
  (void)vm;
  (void)line_no;
  if (err == BIFROST_VM_ERROR_STACK_TRACE_BEGIN || err == BIFROST_VM_ERROR_STACK_TRACE_END)
  {
    printf("### ------------ ERROR ------------ ###\n");
  }
  else
  {
    std::printf("%s", message);
  }
}

namespace bifrost::detail
{
  class CoreEngineGameStateLayer final : public IGameStateLayer
  {
   protected:
    void onEvent(Engine& engine, Event& event) override;

   public:
    const char* name() override { return "__CoreEngineLayer__"; }
  };
}  // namespace bifrost::detail

namespace bifrost
{
  static constexpr int k_MaxNumCamera = 16;

  struct CameraRenderCreateParams final
  {
    int width;
    int height;
  };

  struct CameraRender final
  {
    friend class ::Engine;

    bfGfxDeviceHandle device;
    BifrostCamera     cpu_camera;
    CameraGPUData     gpu_camera;
    int               old_width;
    int               old_height;
    int               new_width;
    int               new_height;
    CameraRender*     prev;
    CameraRender*     next;
    CameraRender*     resize_list_next;

    CameraRender(CameraRender*& head, bfGfxDeviceHandle device, bfGfxFrameInfo frame_info, const CameraRenderCreateParams& params) :
      device{device},
      cpu_camera{},
      gpu_camera{},
      old_width{params.width},
      old_height{params.height},
      new_width{params.width},
      new_height{params.height},
      prev{nullptr},
      next{head},
      resize_list_next{nullptr}
    {
      if (head)
      {
        head->prev = this;
        // head->next = /* same */;
      }

      head = this;

      const Vec3f cam_pos = {0.0f, 0.0f, 4.0f, 1.0f};
      Camera_init(&cpu_camera, &cam_pos, nullptr, 0.0f, 0.0f);
      gpu_camera.init(device, frame_info, params.width, params.height);
    }

    ~CameraRender()
    {
      gpu_camera.deinit(device);
    }

   private:
    void resize()
    {
      if (old_width != new_width || old_height != new_height)
      {
        Camera_onResize(&cpu_camera, unsigned(new_width), unsigned(new_height));
        gpu_camera.resize(device, new_width, new_height);

        old_width  = new_width;
        old_height = new_height;
      }
    }
  };

  enum class EngineState : std::uint8_t
  {
    RUNTIME_PLAYING,
    EDITOR_PLAYING,
    PAUSED,
  };
}  // namespace bifrost

using namespace bifrost;

#define USE_CRT_HEAP 0

#if USE_CRT_HEAP
using MainHeap = CAllocator;
#else
using MainHeap = FreeListAllocator;
#endif

class Engine : private bfNonCopyMoveable<Engine>
{
 private:
  using CommandLineArgs    = std::pair<int, char**>;
  using CameraRenderMemory = PoolAllocator<CameraRender, k_MaxNumCamera>;

 private:
  CommandLineArgs         m_CmdlineArgs;
  MainHeap                m_MainMemory;
  LinearAllocator         m_TempMemory;
  NoFreeAllocator         m_TempAdapter;
  GameStateMachine        m_StateMachine;
  VM                      m_Scripting;
  StandardRenderer        m_Renderer;
  DebugRenderer           m_DebugRenderer;
  Array<AssetSceneHandle> m_SceneStack;
  Assets                  m_Assets;
  Array<IECSSystem*>      m_Systems;
  CameraRenderMemory      m_CameraMemory;
  CameraRender*           m_CameraList;
  CameraRender*           m_CameraResizeList;
  CameraRender*           m_CameraDeleteList;
  EngineState             m_State;

 public:
  explicit Engine(char* main_memory, std::size_t main_memory_size, int argc, char* argv[]);

  // Subsystem Accessors

  MainHeap&         mainMemory() { return m_MainMemory; }
  LinearAllocator&  tempMemory() { return m_TempMemory; }
  IMemoryManager&   tempMemoryNoFree() { return m_TempAdapter; }
  GameStateMachine& stateMachine() { return m_StateMachine; }
  VM&               scripting() { return m_Scripting; }
  StandardRenderer& renderer() { return m_Renderer; }
  DebugRenderer&    debugDraw() { return m_DebugRenderer; }
  Assets&           assets() { return m_Assets; }
  AssetSceneHandle  currentScene() const;
  EngineState       state() const { return m_State; }
  void              setState(EngineState value) { m_State = value; }

  // Low Level Camera API

  CameraRender* borrowCamera(const CameraRenderCreateParams& params);
  void          resizeCamera(CameraRender* camera, int width, int height);
  void          returnCamera(CameraRender* camera);

  template<typename F>
  void forEachCamera(F&& callback)
  {
    CameraRender* camera = m_CameraList;

    while (camera)
    {
      callback(camera);
      camera = camera->next;
    }
  }

  // Scene Management API

  void      openScene(const AssetSceneHandle& scene);
  EntityRef createEntity(Scene& scene, const StringRange& name = "New Entity");

  // "System" Functions to be called by the Application

  template<typename T>
  void addECSSystem()
  {
    m_Systems.push(m_MainMemory.allocateT<T>());
  }

  void               init(const BifrostEngineCreateParams& params, BifrostWindow* main_window);
  [[nodiscard]] bool beginFrame();
  void               onEvent(Event& evt);
  void               fixedUpdate(float delta_time);
  void               update(float delta_time);
  void               drawBegin(float render_alpha);
  void               drawEnd() const;
  void               endFrame();
  void               deinit();

 private:
  void resizeCameras();
  void deleteCameras();
};

#endif /* BIFROST_ENGINE_HPP */