//
// Shareef Abdoul-Raheem
// Tests a minimal use of the engine.
//
#include "bf/bf_ui.hpp"
#include "bf/gfx/bf_draw_2d.hpp"
#include <bf/bifrost.hpp>

#include "bf/JobSystem.hpp"

#include "PxPhysicsAPI.h"

#if BIFROST_PLATFORM_WINDOWS
#include <malloc.h>  // _aligned_malloc, _aligned_free
#else
#include <cstdlib>
#endif

using namespace bf;

static std::int32_t RoundUpToNearest(float value, std::int32_t grid_size)
{
  const float grid_size_f = float(grid_size);

  return std::int32_t((value + (grid_size_f - 1.0f)) / grid_size_f) * grid_size;
}

/// PhysX Learning

namespace bf
{
  class PhysicsAllocator final : public physx::PxAllocatorCallback
  {
   public:
    void* allocate(size_t size, const char* typeName, const char* filename, int line) override
    {
      (void)typeName;
      (void)filename;
      (void)line;
#if BIFROST_PLATFORM_WINDOWS
      return _aligned_malloc(size, 16);
#else
      return std::aligned_alloc(16, size);
#endif
    }

    void deallocate(void* ptr) override
    {
#if BIFROST_PLATFORM_WINDOWS
      _aligned_free(ptr);
#else
      std::free(ptr);
#endif
    }
  };

  class PhysicsErrorCallback final : public physx::PxErrorCallback
  {
    void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override
    {
      constexpr auto serious_error_mask = physx::PxErrorCode::eABORT |
                                          physx::PxErrorCode::eINVALID_PARAMETER |
                                          physx::PxErrorCode::eINVALID_OPERATION |
                                          physx::PxErrorCode::eINTERNAL_ERROR |
                                          physx::PxErrorCode::eOUT_OF_MEMORY;

      bfLogError("[PHYSX](%i): %s (%s:%i)", code, message, file, line);

      if (code & serious_error_mask)
      {
#if BIFROST_PLATFORM_WINDOWS
        __debugbreak();
#endif
        assert(false);
      }
    }
  };

  class PhysicsCPUDispatcher final : public physx::PxCpuDispatcher
  {
   public:
    void submitTask(physx::PxBaseTask& task) override
    {
      job::taskSubmit(
       job::taskMake(
        [physics_task = &task](job::Task* task) {
          physics_task->run();
          physics_task->release();
        },
        nullptr),
       job::QueueType::HIGH);
    }

    std::uint32_t getWorkerCount() const override
    {
      return std::uint32_t(job::numWorkers());
    }
  };

  class PhysicsSimEventCallback : public physx::PxSimulationEventCallback
  {
   public:
    void onConstraintBreak(::physx::PxConstraintInfo* constraints, ::physx::PxU32 count) override
    {
    }

    void onWake(::physx::PxActor** actors, ::physx::PxU32 count) override
    {
    }

    void onSleep(::physx::PxActor** actors, ::physx::PxU32 count) override
    {
    }

    void onContact(const ::physx::PxContactPairHeader& pairHeader, const ::physx::PxContactPair* pairs, ::physx::PxU32 nbPairs) override
    {
    }

    void onTrigger(::physx::PxTriggerPair* pairs, ::physx::PxU32 count) override
    {
      for (physx::PxU32 i = 0u; i < count; ++i)
      {
        const physx::PxTriggerPair& pair = pairs[i];

        if (pair.status == physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
        {
          std::printf("FOUND_TOUCH: %s Was triggered By %s (flags: %u)\n", pair.triggerActor->getName(), pair.otherActor->getName(), uint32_t(pair.flags));
        }
        else if (pair.status == physx::PxPairFlag::eNOTIFY_TOUCH_LOST)
        {
          std::printf("LOST_TOUCH: %s Was triggered By %s (flags: %u)\n", pair.triggerActor->getName(), pair.otherActor->getName(), uint32_t(pair.flags));
        }
      }
    }

    void onAdvance(const ::physx::PxRigidBody* const* bodyBuffer, const ::physx::PxTransform* poseBuffer, const ::physx::PxU32 count) override
    {
    }
  };

  class Physics
  {
   private:
    static physx::PxFilterFlags SampleSubmarineFilterShader(
     physx::PxFilterObjectAttributes attributes0,
     physx::PxFilterData             filterData0,
     physx::PxFilterObjectAttributes attributes1,
     physx::PxFilterData             filterData1,
     physx::PxPairFlags&             pairFlags,
     const void*                     constantBlock,
     physx::PxU32                    constantBlockSize)
    {
      // let triggers through
      if (physx::PxFilterObjectIsTrigger(attributes0) || physx::PxFilterObjectIsTrigger(attributes1))
      {
        pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
        return physx::PxFilterFlag::eDEFAULT;
      }
      // generate contacts for all that were not filtered above
      pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;

      // trigger the contact callback for pairs (A,B) where
      // the filtermask of A contains the ID of B and vice versa.
      if ((filterData0.word0 & filterData1.word1) && (filterData1.word0 & filterData0.word1))
        pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;

      return physx::PxFilterFlag::eDEFAULT;
    }

   private:
    PhysicsAllocator         m_Allocator     = {};
    PhysicsErrorCallback     m_ErrorCallback = {};
    physx::PxFoundation*     m_PxFoundation  = nullptr;
    physx::PxPhysics*        m_PhysX         = nullptr;
    PhysicsCPUDispatcher*    m_CPUDispatcher = nullptr;
    PhysicsSimEventCallback* m_SimCallback   = nullptr;
    physx::PxScene*          m_MainScene     = nullptr;

   public:
    void init()
    {
      m_PxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_ErrorCallback);

      assert(m_PxFoundation && "Failed to initialize PhysX Foundation!");

      physx::PxPvd* pvd_instance;

#if 0
      pvd_instance                     = physx::PxCreatePvd(*m_PxFoundation);
      physx::PxPvdTransport* transport = physx::PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
      pvd_instance->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);
#else
      pvd_instance = nullptr;
#endif

      const bool do_track_allocations = true;

      physx::PxTolerancesScale tolerance_scale = {};

      m_PhysX = PxCreatePhysics(PX_PHYSICS_VERSION, *m_PxFoundation, tolerance_scale, do_track_allocations, pvd_instance);

      assert(m_PhysX && "Failed to initialize PhysX Physics Object!");

#if 0
      mCooking = PxCreateCooking(PX_PHYSICS_VERSION, *mFoundation, PxCookingParams(scale));
if (!mCooking)
    fatalError("PxCreateCooking failed!");

      if (!PxInitExtensions(*m_PhysX, pvd_instance))
      {
        assert(false);
      }
#endif

      m_CPUDispatcher = new PhysicsCPUDispatcher();
      m_SimCallback   = new PhysicsSimEventCallback();

      /*
       The parameter 'true' to createShape() informs the SDK that the shape will not be shared with other actors. You can use shape sharing to reduce the memory costs of your simulation when you have many actors with identical geometry, but shared shapes have a very strong restriction: you cannot update the attributes of a shared shape while it is attached to an actor.
       */

      physx::PxSceneDesc scene_desc = {tolerance_scale};

      // Set `scene_desc` callbacks here

      scene_desc.gravity = {0.0f, -9.8f * 0.5f, 0.0f};

      scene_desc.filterShader            = &SampleSubmarineFilterShader;
      scene_desc.cpuDispatcher           = m_CPUDispatcher;
      scene_desc.simulationEventCallback = m_SimCallback;

      m_MainScene = m_PhysX->createScene(scene_desc);

      // Test Code

      using namespace physx;

      PxRigidStatic* floor_actor = m_PhysX->createRigidStatic(PxTransform(PxMat44(PxIdentity)));
      PxShape* const floor_shape = m_PhysX->createShape(
       PxBoxGeometry(10.0f, 0.2f, 10.0f),
       *m_PhysX->createMaterial(0.5f, 0.5f, 0.1f),
       true,
       PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE);

      floor_actor->attachShape(*floor_shape);
      floor_actor->setGlobalPose(PxTransform({0.0f, -5.0f, 0.0f}));

      m_MainScene->addActor(*floor_actor);

      /// Trigger Floor

      auto* const trigger_floor_actor = m_PhysX->createRigidStatic(PxTransform(PxVec3(0.0f, -10.0f, 0.0f), PxQuat(k_PI * 0.5f, PxVec3(0.0f, 0.0f, 1.0f))));
      auto* const trigger_floor_shape = m_PhysX->createShape(PxPlaneGeometry(), *m_PhysX->createMaterial(0.5f, 0.5f, 0.1f), true, PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eTRIGGER_SHAPE);

      trigger_floor_actor->setName("Floor Hit Something");

      trigger_floor_actor->attachShape(*trigger_floor_shape);

      m_MainScene->addActor(*trigger_floor_actor);
    }

    void addActor(int type, const Vector3f& pos)
    {
      using namespace physx;

      // m_PhysX->createAggregate()

      PxRigidDynamic* actor = m_PhysX->createRigidDynamic(PxTransform(PxMat44(PxIdentity)));

      PxMaterial* material = m_PhysX->createMaterial(0.5f, 0.5f, 0.1f);

      PxShape* shape;

      if (type == 0)
      {
        shape = m_PhysX->createShape(
         // PxSphereGeometry(1.0f),
         PxBoxGeometry(0.5f, 0.5f, 0.5f),
         *material,
         true,
         PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE);
        actor->setName("Box");
      }
      else
      {
        shape = m_PhysX->createShape(
         PxSphereGeometry(float(rand()) / float(RAND_MAX) + 0.3f),
         *material,
         true,
         PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE);
        actor->setName("Sphere");
      }

      actor->attachShape(*shape);

      actor->setGlobalPose(physx::PxTransform(PxVec3{pos.x, pos.y, pos.z}));

      actor->setLinearVelocity({0.2f, 0.0f, 0.0f}, true);

      shape->release();

      m_MainScene->addActor(*actor);
    }

    void beginFrame(float dt)
    {
      m_MainScene->simulate(dt, nullptr, nullptr, 0u, true);
    }

    void endFrame()
    {
      m_MainScene->fetchResults(true, nullptr);
    }

    void draw(DebugRenderer& dbg_draw)
    {
      physx::PxActor*    actors[256];
      const physx::PxU32 num_actors = m_MainScene->getActors(physx::PxActorTypeFlag::eRIGID_STATIC | physx::PxActorTypeFlag::eRIGID_DYNAMIC, actors, physx::PxU32(bfCArraySize(actors)), 0);

      for (physx::PxU32 i = 0; i < num_actors; ++i)
      {
        physx::PxActor* const actor = actors[i];
        // physx::PxBounds3      bounds = actor->getWorldBounds(1.0f);
        // physx::PxVec3         bounds_center = bounds.getCenter();
        // physx::PxVec3         bounds_dim    = bounds.getDimensions();
        physx::PxTransform global_pose;

        physx::PxShape* shapes[128];
        physx::PxU32    num_shapes;

        bfColor4u line_clr;

        switch (actor->getType())
        {
          case physx::PxActorType::eRIGID_STATIC:
          {
            physx::PxRigidStatic* const rb = actor->is<physx::PxRigidStatic>();

            global_pose = rb->getGlobalPose();
            num_shapes  = rb->getShapes(shapes, physx::PxU32(bfCArraySize(shapes)), 0);

            line_clr = bfColor4u_fromUint32(0xFFF00AFF);
            break;
          }
          case physx::PxActorType::eRIGID_DYNAMIC:
          {
            physx::PxRigidDynamic* const rb = actor->is<physx::PxRigidDynamic>();

            global_pose = rb->getGlobalPose();
            num_shapes  = rb->getShapes(shapes, physx::PxU32(bfCArraySize(shapes)), 0);
            line_clr    = bfColor4u_fromUint32(0xFDF3F00F);
            break;
          }
          case physx::PxActorType::eARTICULATION_LINK:
          default:
            num_shapes = 0u;
            break;
        }

        for (physx::PxU32 j = 0; j < num_shapes; ++j)
        {
          physx::PxShape* const   shape    = shapes[j];
          physx::PxGeometryHolder geometry = shape->getGeometry();

          switch (shape->getGeometryType())
          {
            case physx::PxGeometryType::eSPHERE:
            {
              const physx::PxSphereGeometry& sphere = geometry.sphere();

              const Vector3f center = {global_pose.p.x, global_pose.p.y, global_pose.p.z};

              dbg_draw.addSphere(center, sphere.radius, line_clr, 20, 20);
              break;
            }
            case physx::PxGeometryType::ePLANE:
            {
              // TODO(SR): Make configurable.
              const float plane_grid_width = 20.0f;
              const float plane_normal_len = 0.4f;
              const int   grid_square_size = 2;

              const float half_grid_line_length = plane_grid_width * 0.5f;
              const int   num_grid_segments     = RoundUpToNearest(plane_grid_width, grid_square_size) / grid_square_size;

              //
              // PhysX's Plane has a default orientation in the Y, Z plane.
              //

              const physx::PxVec3  x_axis         = global_pose.q.rotate(physx::PxVec3{1.0f, 0.0f, 0.0f});
              const physx::PxVec3  y_axis         = global_pose.q.rotate(physx::PxVec3{0.0f, 1.0f, 0.0f});
              const physx::PxVec3  z_axis         = global_pose.q.rotate(physx::PxVec3{0.0f, 0.0f, 1.0f});
              const physx::PxVec3  half_y_extents = y_axis * half_grid_line_length;
              const physx::PxVec3  half_z_extents = z_axis * half_grid_line_length;
              const physx::PxVec3& center         = global_pose.p;

              for (int y = 0; y <= num_grid_segments; ++y)
              {
                const physx::PxVec3 grid_y = center + y_axis * float(y * grid_square_size) - half_y_extents;
                const physx::PxVec3 p0     = grid_y - half_z_extents;
                const physx::PxVec3 p1     = grid_y + half_z_extents;

                dbg_draw.addLine({p0.x, p0.y, p0.z}, {p1.x, p1.y, p1.z}, line_clr);
              }

              for (int z = 0; z <= num_grid_segments; ++z)
              {
                const physx::PxVec3 grid_z = center + z_axis * float(z * grid_square_size) - half_z_extents;
                const physx::PxVec3 p0     = grid_z - half_y_extents;
                const physx::PxVec3 p1     = grid_z + half_y_extents;

                dbg_draw.addLine({p0.x, p0.y, p0.z}, {p1.x, p1.y, p1.z}, line_clr);
              }

              // Draw the normal
              const physx::PxVec3 p0 = center;
              const physx::PxVec3 p1 = center + x_axis * plane_normal_len;

              dbg_draw.addLine({p0.x, p0.y, p0.z}, {p1.x, p1.y, p1.z}, bfColor4u_fromUint32(0xFF000ACF));

              break;
            }
            case physx::PxGeometryType::eCAPSULE:
            {
              break;
            }
            case physx::PxGeometryType::eBOX:
            {
              const physx::PxBoxGeometry& box      = geometry.box();
              const physx::PxVec3         half_ext = box.halfExtents;

              physx::PxVec3 min_max_points[] =
               {
                {-half_ext.x, -half_ext.y, -half_ext.z},
                {+half_ext.x, -half_ext.y, -half_ext.z},
                {-half_ext.x, +half_ext.y, -half_ext.z},
                {+half_ext.x, +half_ext.y, -half_ext.z},
                {-half_ext.x, -half_ext.y, +half_ext.z},
                {+half_ext.x, -half_ext.y, +half_ext.z},
                {-half_ext.x, +half_ext.y, +half_ext.z},
                {+half_ext.x, +half_ext.y, +half_ext.z},
               };

              for (auto& point : min_max_points)
              {
                point = global_pose.transform(point);
              }

              const Vector3f points_to_draw[] =
               {
                {min_max_points[0].x, min_max_points[0].y, min_max_points[0].z},
                {min_max_points[1].x, min_max_points[1].y, min_max_points[1].z},
                {min_max_points[2].x, min_max_points[2].y, min_max_points[2].z},
                {min_max_points[3].x, min_max_points[3].y, min_max_points[3].z},
                {min_max_points[4].x, min_max_points[4].y, min_max_points[4].z},
                {min_max_points[5].x, min_max_points[5].y, min_max_points[5].z},
                {min_max_points[6].x, min_max_points[6].y, min_max_points[6].z},
                {min_max_points[7].x, min_max_points[7].y, min_max_points[7].z},
               };

              dbg_draw.addLine(points_to_draw[0], points_to_draw[1], line_clr);
              dbg_draw.addLine(points_to_draw[0], points_to_draw[2], line_clr);
              dbg_draw.addLine(points_to_draw[0], points_to_draw[4], line_clr);
              dbg_draw.addLine(points_to_draw[1], points_to_draw[3], line_clr);
              dbg_draw.addLine(points_to_draw[1], points_to_draw[5], line_clr);
              dbg_draw.addLine(points_to_draw[2], points_to_draw[3], line_clr);
              dbg_draw.addLine(points_to_draw[2], points_to_draw[6], line_clr);
              dbg_draw.addLine(points_to_draw[3], points_to_draw[7], line_clr);
              dbg_draw.addLine(points_to_draw[4], points_to_draw[5], line_clr);
              dbg_draw.addLine(points_to_draw[4], points_to_draw[6], line_clr);
              dbg_draw.addLine(points_to_draw[5], points_to_draw[7], line_clr);
              dbg_draw.addLine(points_to_draw[6], points_to_draw[7], line_clr);
              break;
            }
            case physx::PxGeometryType::eCONVEXMESH:
            {
              break;
            }
            case physx::PxGeometryType::eTRIANGLEMESH:
            {
              break;
            }
            case physx::PxGeometryType::eHEIGHTFIELD:
            {
              break;
            }
            default:
              break;
          }
        }
      }
    }

    void shutdown()
    {
      m_MainScene->release();
      delete m_SimCallback;
      delete m_CPUDispatcher;
      m_PhysX->release();
      m_PxFoundation->release();
    }
  };
}  // namespace bf

/// PhysX Learning

// Physics Concepts
//
// RigidBody
//   Type: Dynamic   (Normal)
//         Kinematic (Moved Manually by forces, but can hit things, not affected by gravity)
//         Static    (Cannot be moved)
//
//   Shapes[]: Lists of shapes
//
//
// Shape:
//   Flags: IsTrigger (IsSolid must be false)
//          IsSolid   (IsTrigger must be false)
//
//   Geometry: The shape to use.
//
//   Relative pose.
//
//
// Geometry: Collision shape centered at origin.
//
//   Plane: Only can be used with static actors.
//   TriangleMesh, HeightField and Plane geometries are not supported for simulation shapes that are attached to dynamic actors, unless the dynamic actors are configured to be kinematic.
//
// eSIMULATION_SHAPE mut-exclusive to eTRIGGER_SHAPE
//   Triangle meshes and heightfields can not be triggers.
//

using namespace bf;

struct RuntimeGameState final : public IGameStateLayer
{
  bf::RenderView* m_MainCamera        = nullptr;
  Physics         m_PhysicTest        = {};
  float           m_DeltaTime         = 0.0f;
  float           m_TimeSinceDTUpdate = 0.0f;

  void onCreate(Engine& engine) override
  {
    m_PhysicTest.init();
    m_MainCamera = engine.borrowCamera(CameraRenderCreateParams{1280, 720});
  }

  void onUpdate(Engine& engine, float delta_time) override
  {
    if (m_TimeSinceDTUpdate <= 0.0f)
    {
      m_DeltaTime         = delta_time;
      m_TimeSinceDTUpdate = 1.0f;
    }
    else
    {
      m_TimeSinceDTUpdate -= delta_time;
    }

    m_PhysicTest.draw(engine.debugDraw());

    const auto camera_move_speed = 2.2f * delta_time;

    const std::tuple<int, void (*)(::BifrostCamera*, float), float> camera_controls[] =
     {
      {BIFROST_KEY_W, &Camera_moveForward, camera_move_speed},
      {BIFROST_KEY_A, &Camera_moveLeft, camera_move_speed},
      {BIFROST_KEY_S, &Camera_moveBackward, camera_move_speed},
      {BIFROST_KEY_D, &Camera_moveRight, camera_move_speed},
      {BIFROST_KEY_Q, &Camera_moveUp, camera_move_speed},
      {BIFROST_KEY_E, &Camera_moveDown, camera_move_speed},
      {BIFROST_KEY_R, &Camera_addPitch, -0.01f},
      {BIFROST_KEY_F, &Camera_addPitch, +0.01f},
      {BIFROST_KEY_H, &Camera_addYaw, +0.01f},
      {BIFROST_KEY_G, &Camera_addYaw, -0.01f},
     };

    for (const auto& control : camera_controls)
    {
      if (engine.input().KEY_STATE[std::get<0>(control)])
      {
        std::get<1>(control)(&m_MainCamera->cpu_camera, std::get<2>(control));
      }
    }

    static WindowState s_PhysicsWindowState = {true, {5, 5}, {250.0f, 200.0f}};

    const auto actor_placement = Vector3f{m_MainCamera->cpu_camera.position} + Vector3f{m_MainCamera->cpu_camera.forward} * 5.0f;

    if (UI::BeginWindow("Physics Sandbox", s_PhysicsWindowState))
    {
      if (UI::Button("Add Box"))
      {
        m_PhysicTest.addActor(0, actor_placement);
      }

      if (UI::Button("Add Circle"))
      {
        m_PhysicTest.addActor(1, actor_placement);
      }

      UI::EndWindow();
    }

    engine.debugDraw().addSphere(actor_placement, 0.1f, {255, 255, 255, 255}, 5, 8);
  }

  void onFixedUpdate(Engine& engine, float delta_time) override
  {
    //
    m_PhysicTest.beginFrame(delta_time);
    {
      // Physics is Simulating
    }
    m_PhysicTest.endFrame();
    //
  }

  void onRenderBackbuffer(Engine& engine, float alpha) override
  {
    const float        framebuffer_width  = float(bfTexture_width(engine.renderer().m_MainSurface));
    const float        framebuffer_height = float(bfTexture_height(engine.renderer().m_MainSurface));
    auto&              gfx                = engine.gfx2DScreen();
    const Brush* const screen_brush       = gfx.makeBrush(m_MainCamera->gpu_camera.composite_buffer);

    engine.resizeCamera(m_MainCamera, int(framebuffer_width), int(framebuffer_height));

    gfx.fillRect(screen_brush, AxisQuad::make({0.0f, 0.0f, framebuffer_width, framebuffer_height}));

    char fps_string[64];

    string_utils::fmtBuffer(fps_string, sizeof(fps_string), nullptr, "FPS: %i", int(1.0f / m_DeltaTime));

    gfx.text(gfx.makeBrush(UI::xxx_Font(), {1.0f, 1.0f, 1.0f, 1.0f}), {10.0f, framebuffer_height - 10.0f}, fps_string);
  }

  void onDestroy(Engine& engine) override
  {
    engine.returnCamera(m_MainCamera);
    m_PhysicTest.shutdown();
  }

  const char* name() override { return "Physics Sandbox"; }

  const float k_InvalidMousePos = -1.0f;
  Vector2f    m_OldMousePos     = {k_InvalidMousePos, k_InvalidMousePos};
  bool        m_IsDraggingMouse = false;
  float       m_MouseLookSpeed  = 0.01f;

  void onEvent(Engine& engine, bfEvent& event) override
  {
    auto& mouse_evt = event.mouse;

    if (event.type == BIFROST_EVT_ON_MOUSE_DOWN || event.type == BIFROST_EVT_ON_MOUSE_UP)
    {
      m_OldMousePos = {k_InvalidMousePos, k_InvalidMousePos};

      if (event.type == BIFROST_EVT_ON_MOUSE_DOWN)
      {
        if (mouse_evt.target_button == BIFROST_BUTTON_MIDDLE)
        {
          m_IsDraggingMouse = true;
        }
      }
      else
      {
        m_IsDraggingMouse = false;
        event.accept();
      }
    }
    else if (event.type == BIFROST_EVT_ON_MOUSE_MOVE)
    {
      if (m_IsDraggingMouse && mouse_evt.button_state & BIFROST_BUTTON_MIDDLE)
      {
        const float newx = float(mouse_evt.x);
        const float newy = float(mouse_evt.y);

        if (m_OldMousePos.x == k_InvalidMousePos) { m_OldMousePos.x = newx; }
        if (m_OldMousePos.y == k_InvalidMousePos) { m_OldMousePos.y = newy; }

        if (m_MainCamera)
        {
          Camera_mouse(
           &m_MainCamera->cpu_camera,
           (newx - m_OldMousePos.x) * m_MouseLookSpeed,
           (newy - m_OldMousePos.y) * -m_MouseLookSpeed);
        }

        m_OldMousePos.x = newx;
        m_OldMousePos.y = newy;
      }
    }

    UI::PumpEvents(&event);
  }
};

enum class ReturnCode : int
{
  SUCCESS,
  FAILED_TO_INITIALIZE_PLATFORM,
  FAILED_TO_CREATE_MAIN_WINDOW,
  FAILED_TO_ALLOCATE_ENGINE_MEMORY,
};

#define MainQuit(code, label) \
  err_code = (code);          \
  goto label

int main(int argc, char* argv[])
{
  // [https://www.deadalnix.me/2017/01/08/variable-size-integer-encoding/]
  //  7 bits: 0XXXXXXX
  // 14 bits: 10XXXXXX XXXXXXXX
  // 21 bits: 110XXXXX XXXXXXXX XXXXXXXX
  // 28 bits: 1110XXXX XXXXXXXX XXXXXXXX XXXXXXXX
  // 35 bits: 11110XXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX
  // 42 bits: 111110XX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX
  // 49 bits: 1111110X XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX
  // 56 bits: 11111110 XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX
  // 64 bits: 11111111 XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX

  ReturnCode err_code = ReturnCode::SUCCESS;

  if (bfPlatformInit({argc, argv, nullptr, nullptr}))
  {
    bfWindow* const main_window = bfPlatformCreateWindow("Runtime Standalone Test", 1800, 1000, k_bfWindowFlagsDefault & ~k_bfWindowFlagIsMaximizedOnShow);

    if (main_window)
    {
      try
      {
        const std::size_t             engine_memory_size = bfMegabytes(100);
        const std::unique_ptr<char[]> engine_memory      = std::make_unique<char[]>(engine_memory_size);
        std::unique_ptr<Engine>       engine             = std::make_unique<Engine>(engine_memory.get(), engine_memory_size, argc, argv);
        const EngineCreateParams      params             = {{argv[0], 0}, 60};

        engine->init(params, main_window);
        engine->stateMachine().push<RuntimeGameState>();

        main_window->user_data = engine.get();
        main_window->event_fn  = [](bfWindow* window, bfEvent* event) { static_cast<Engine*>(window->user_data)->onEvent(window, *event); };
        main_window->frame_fn  = [](bfWindow* window) { static_cast<Engine*>(window->user_data)->tick(); };

        bfPlatformDoMainLoop(main_window);
        engine->deinit();
      }
      catch (std::bad_alloc&)
      {
        MainQuit(ReturnCode::FAILED_TO_ALLOCATE_ENGINE_MEMORY, quit_window);
      }

    quit_window:
      bfPlatformDestroyWindow(main_window);
    }
    else
    {
      MainQuit(ReturnCode::FAILED_TO_CREATE_MAIN_WINDOW, quit_platform);
    }
  }
  else
  {
    MainQuit(ReturnCode::FAILED_TO_INITIALIZE_PLATFORM, quit_main);
  }

quit_platform:
  bfPlatformQuit();

quit_main:
  return int(err_code);
}

#undef MainQuit
