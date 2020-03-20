/******************************************************************************/
/*!
* @file   bifrost_editor_serializer.hpp
* @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
* @brief
*
* @version 0.0.1
* @date    2020-03-16
*
* @copyright Copyright (c) 2020
*/
/******************************************************************************/
#ifndef BIFROST_EDITOR_SERIALIZER_HPP
#define BIFROST_EDITOR_SERIALIZER_HPP

#include "bifrost/asset_io/bifrost_asset_handle.hpp"  // ISerializer

#include <imgui/imgui.h>

namespace bifrost::editor
{
  typedef struct Vec2f_t Vec2f;
  typedef struct Vec3f_t Vec3f;

  class ImGuiSerializer final : public ISerializer
  {
    struct ObjectStackInfo
    {
      bool is_array;
      int  array_index;
    };

   private:
    Array<ObjectStackInfo> m_IsOpenStack;
    char                   m_NameBuffer[256];
    Assets*                m_Assets;

   public:
    explicit ImGuiSerializer(IMemoryManager& memory);

    void setAssets(Assets* assets) { m_Assets = assets; }

    bool beginDocument(bool is_array) override;
    bool pushObject(StringRange key) override;
    bool pushArray(StringRange key) override;
    void serialize(StringRange key, std::int8_t& value) override;
    void serialize(StringRange key, std::uint8_t& value) override;
    void serialize(StringRange key, std::int16_t& value) override;
    void serialize(StringRange key, std::uint16_t& value) override;
    void serialize(StringRange key, std::int32_t& value) override;
    void serialize(StringRange key, std::uint32_t& value) override;
    void serialize(StringRange key, std::int64_t& value) override;
    void serialize(StringRange key, std::uint64_t& value) override;
    void serialize(StringRange key, float& value) override;
    void serialize(StringRange key, double& value) override;
    void serialize(StringRange key, long double& value) override;
    void serialize(StringRange key, Vec2f& value) override;
    void serialize(StringRange key, Vec3f& value) override;
    void serialize(StringRange key, String& value) override;
    void serialize(StringRange key, BifrostUUID& value) override;
    void serialize(StringRange key, BaseAssetHandle& value) override;
    using ISerializer::serialize;
    void popObject() override;
    void popArray() override;
    void endDocument() override;

   private:
    ObjectStackInfo& top() { return m_IsOpenStack.back(); }
    void             setNameBuffer(StringRange key);
  };

  namespace imgui_ext
  {
    bool inspect(const char* label, String& string, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool inspect(const char* label, std::string& string, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool inspect(const char* label, Any& object, meta::BaseClassMetaInfo* info);
  }  // namespace imgui_ext
}  // namespace bifrost::editor

#include "bifrost/math/bifrost_transform.h"

namespace bifrost::meta
{
  template<>
  inline const auto& Meta::registerMembers<BifrostTransform>()
  {
    static auto member_ptrs = members(
      class_info<BifrostTransform>("Transform"),
      ctor<>(),
      field("origin", &BifrostTransform::origin),
      field("local_position", &BifrostTransform::local_position),
      field("local_rotation", &BifrostTransform::local_rotation),
      field("local_scale", &BifrostTransform::local_scale),
      field("world_position", &BifrostTransform::world_position),
      field("world_rotation", &BifrostTransform::world_rotation),
      field("world_scale", &BifrostTransform::world_scale),
      field("local_transform", &BifrostTransform::local_transform),
      field("world_transform", &BifrostTransform::world_transform));

    return member_ptrs;
  }
}  // namespace bifrost::meta

#endif /* BIFROST_EDITOR_SERIALIZER_HPP */