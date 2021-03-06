/******************************************************************************/
/*!
 * @file   bifrost_scene.cpp
 * @author Shareef Abdoul-Raheem (https://blufedora.github.io/)
 * @brief
 *   This is where Entities live in the engine.
 *   Also contains the storage for the components.
 *
 * @version 0.0.1
 * @date    2019-12-22
 *
 * @copyright Copyright (c) 2019-2021
 */
/******************************************************************************/
#include "bf/asset_io/bifrost_scene.hpp"

#include "bf/asset_io/bf_document.hpp"             /* BaseDocument             */
#include "bf/asset_io/bifrost_file.hpp"            /* File                 */
#include "bf/asset_io/bifrost_json_serializer.hpp" /* JsonSerializerReader */
#include "bf/core/bifrost_engine.hpp"              /* Engine               */
#include "bf/ecs/bf_entity.hpp"                    /* Entity               */
#include "bf/ecs/bifrost_collision_system.hpp"     /* DebugRenderer        */
#include "bf/utility/bifrost_json.hpp"             /* json::Value          */

namespace bf
{
  Scene::Scene(Engine& engine) :
    Base(),
    m_Engine{engine},
    m_Memory{m_Engine.mainMemory()},
    m_RootEntities{&Entity::m_Hierarchy},
    m_ActiveComponents{m_Memory},
    m_InactiveComponents{m_Memory},
    m_ActiveBehaviors{m_Memory},
    m_BVHTree{m_Memory},
    m_Camera{},
    m_DirtyList{nullptr}
  {
    Camera_init(&m_Camera, nullptr, nullptr, 0.0f, 0.0f);

    m_DoDebugDraw = false;
  }

  void Scene::reflect(ISerializer& serializer)
  {
    if (serializer.mode() == SerializerMode::LOADING)
    {
      // Reset Camera
      Camera_init(&m_Camera, nullptr, nullptr, 0.0f, 0.0f);
    }

    // Load Entities

    std::size_t num_entities;

    if (serializer.pushArray("m_Entities", num_entities))
    {
      if (serializer.mode() == SerializerMode::LOADING)
      {
        while (!m_RootEntities.isEmpty())
        {
          m_RootEntities.back().destroy();
        }

        m_RootEntities.clear();

        for (std::size_t i = 0; i < num_entities; ++i)
        {
          addEntity(nullptr);
        }
      }
      // else
      // {
      //   num_entities = m_RootEntities.size();
      // }

      for (Entity& entity : m_RootEntities)
      {
        if (serializer.pushObject(entity.name()))
        {
          entity.reflect(serializer);
          serializer.popObject();
        }
      }

      serializer.popArray();
    }
  }

  EntityRef Scene::addEntity(const StringRange& name)
  {
    return m_Engine.createEntity(*this, name);
  }

  EntityRef Scene::findEntity(const StringRange& name) const
  {
    for (Entity& root_entity : m_RootEntities)
    {
      if (root_entity.name() == name)
      {
        return EntityRef{root_entity};
      }
    }

    return EntityRef{};
  }

  void Scene::removeEntity(Entity* entity)
  {
    m_RootEntities.erase(*entity);
  }

  void Scene::removeAllEntities()
  {
    while (!m_RootEntities.isEmpty())
    {
      m_RootEntities.back().destroy();
      // Entity::destroy detaches from parent.
      // m_RootEntities.pop();
    }
  }

  void Scene::update(LinearAllocator& temp, DebugRenderer& dbg_renderer)
  {
    updateDirtyListTransforms();

    m_BVHTree.endFrame(temp, false);

    if (m_DoDebugDraw)
    {
      m_BVHTree.traverse([this, &dbg_renderer](const BVHNode& node) {
        static constexpr bfColor4u k_DebugColors[] =
         {
          {255, 0, 0, 255},
          {0, 255, 0, 255},
          {0, 0, 255, 255},
          {255, 0, 255, 255},
          {255, 255, 0, 255},
          {0, 255, 255, 255},
          {255, 255, 255, 255},
         };

        const bfColor4u color = k_DebugColors[node.depth % std::size(k_DebugColors)];

        if (bvh_node::isLeaf(node))
        {
          Entity* const entity = static_cast<Entity*>(node.user_data);

          // Don't draw inactive entities.
          if (!entity->isActive())
          {
            return;
          }

          // color = bfColor4u_fromUint32(BIFROST_COLOR_DEEPPINK);
        }
        else
        {
          // color = bfColor4u_fromUint32(BIFROST_COLOR_CYAN);
        }

        dbg_renderer.addAABB(
         (Vector3f(node.bounds.max[0], node.bounds.max[1], node.bounds.max[2]) + Vector3f(node.bounds.min[0], node.bounds.min[1], node.bounds.min[2])) * 0.5f,
         Vector3f(node.bounds.max[0], node.bounds.max[1], node.bounds.max[2]) - Vector3f(node.bounds.min[0], node.bounds.min[1], node.bounds.min[2]),
         color);
      });
    }
  }

  void Scene::startup()
  {
    for (Entity& entity : m_RootEntities)
    {
      entity.startup();
    }
  }

  void Scene::shutdown()
  {
    for (Entity& entity : m_RootEntities)
    {
      entity.startup();
    }
  }

  AssetStatus SceneDocument::onLoad()
  {
    File file_in{fullPath(), file::FILE_MODE_READ};

    if (file_in)
    {
      auto&                temp_alloc = assets().engine().tempMemory();
      LinearAllocatorScope mem_scope  = {temp_alloc};
      const BufferRange    buffer     = file_in.readEntireFile(temp_alloc);
      json::Value          json_value = json::parse(buffer.buffer, buffer.length);
      JsonSerializerReader reader     = {assets(), temp_alloc, json_value};

      if (reader.beginDocument())
      {
        m_SceneAsset = addAsset<SceneAsset>(ResourceID{1u}, relativePath(), assets().engine());

        m_SceneAsset->reflect(reader);
        reader.endDocument();

        return AssetStatus::LOADED;
      }
    }

    return AssetStatus::FAILED;
  }

  void SceneDocument::onUnload()
  {
    if (m_SceneAsset)
    {
      m_SceneAsset->removeAllEntities();
      m_SceneAsset = nullptr;
    }
  }

  void SceneDocument::onSaveAsset()
  {
    defaultSave([this](ISerializer& serialzier) {
      m_SceneAsset->reflect(serialzier);
    });
  }

  static AABB calcBounds(const MeshRenderer& mesh_renderer, const bfTransform& transform)
  {
    if (mesh_renderer.m_Model)
    {
      return aabb::transform(mesh_renderer.m_Model->m_ObjectSpaceBounds, transform.world_transform);
    }

    return AABB(Vector3f{0.0f}, Vector3f{0.0f});
  }

  static AABB calcBounds(const SkinnedMeshRenderer& mesh_renderer, const bfTransform& transform)
  {
    if (mesh_renderer.m_Model)
    {
      return aabb::transform(mesh_renderer.m_Model->m_ObjectSpaceBounds, transform.world_transform);
    }

    return AABB(Vector3f{0.0f}, Vector3f{0.0f});
  }

  static AABB calcBounds(const SpriteRenderer& sprite, const bfTransform& transform)
  {
    const float    thickness           = 0.05f;  // Needed since an AABB of zero volume is a bad idea.
    const Vector3f half_extents        = Vector3f{sprite.m_Size.x, sprite.m_Size.y, thickness, 0.0f} * 0.5f;
    const AABB     object_space_bounds = {-half_extents, half_extents};
    const AABB     world_space_bounds  = aabb::transform(object_space_bounds, transform.world_transform);

    return world_space_bounds;
  }

  void Scene::updateDirtyListTransforms()
  {
    if (m_DirtyList)
    {
      bfTransform* transform = m_DirtyList;

      while (transform)
      {
        bfTransform* const next_transform = std::exchange(transform->dirty_list_next, nullptr);
        transform->flags &= ~BF_TRANSFORM_LOCAL_DIRTY;

        Entity* const entity       = Entity::fromTransform(transform);
        auto* const   mesh         = entity->get<MeshRenderer>();
        auto* const   skinned_mesh = entity->get<SkinnedMeshRenderer>();
        auto* const   sprite       = entity->get<SpriteRenderer>();

        if (mesh)
        {
          m_BVHTree.markLeafDirty(mesh->m_BHVNode, calcBounds(*mesh, *transform));
        }

        if (skinned_mesh)
        {
          m_BVHTree.markLeafDirty(skinned_mesh->m_BHVNode, calcBounds(*skinned_mesh, *transform));
        }

        if (sprite)
        {
          m_BVHTree.markLeafDirty(sprite->m_BHVNode, calcBounds(*sprite, *transform));
        }

        transform = next_transform;
      }

      m_DirtyList = nullptr;
    }
  }

  Scene::~Scene()
  {
    removeAllEntities();
  }

  void assetImportScene(AssetImportCtx& ctx)
  {
    ctx.document = ctx.asset_memory->allocateT<SceneDocument>();
  }
}  // namespace bf
