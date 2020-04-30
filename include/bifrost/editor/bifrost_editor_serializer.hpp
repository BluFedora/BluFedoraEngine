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

namespace bifrost
{
  class Entity;
  class Assets;
}

namespace bifrost::editor
{
  class ImGuiSerializer;

  //
  // For adding custom callbacks for certain types.
  //
  namespace InspectorRegistry
  {
    using Callback = void (*)(ImGuiSerializer& serializer, void* object, meta::BaseClassMetaInfo* type_info, void* user_data);

    void overrideInspectorImpl(meta::BaseClassMetaInfo* type_info, Callback callback, void* user_data);

    template<typename T>
    void overrideInspector(Callback callback, void* user_data = nullptr)
    {
      overrideInspectorImpl(meta::TypeInfo<T>::get(), callback, user_data);
    }
  }  // namespace InspectorRegistry

  class ImGuiSerializer final : public ISerializer
  {
    struct ObjectStackInfo
    {
      bool is_array;
      int  array_index;
    };

   private:
    Array<ObjectStackInfo> m_IsOpenStack;
    Array<bool>            m_HasChangedStack;
    char                   m_NameBuffer[256];
    Assets*                m_Assets;
    bool                   m_IsInCustomCallback;

   public:
    explicit ImGuiSerializer(IMemoryManager& memory);

    void setAssets(Assets* assets) { m_Assets = assets; }

    bool beginDocument(bool is_array) override;
    bool pushObject(StringRange key) override;
    bool pushArray(StringRange key, std::size_t& size) override;
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
    void serialize(StringRange key, bfColor4f& value) override;
    void serialize(StringRange key, bfColor4u& value) override;
    void serialize(StringRange key, String& value) override;
    void serialize(StringRange key, BifrostUUID& value) override;
    void serialize(StringRange key, BaseAssetHandle& value) override;
    void serialize(StringRange key, std::uint64_t& enum_value, meta::BaseClassMetaInfo* type_info) override;
    void serialize(Any& value, meta::BaseClassMetaInfo* type_info) override;
    using ISerializer::serialize;
    void popObject() override;
    void popArray() override;
    void endDocument() override;

    void beginChangeCheck();
    bool endChangedCheck();

   private:
    ObjectStackInfo& top() { return m_IsOpenStack.back(); }
    bool&            hasChangedTop() { return m_HasChangedStack.back(); }
    void             setNameBuffer(StringRange key);
  };

  namespace imgui_ext
  {
    bool inspect(const char* label, String& string, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool inspect(const char* label, std::string& string, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool inspect(const char* label, Any& object, meta::BaseClassMetaInfo* info);
    bool inspect(Engine& engine, Entity& entity, ImGuiSerializer& serializer);
  }  // namespace imgui_ext
}  // namespace bifrost::editor

#endif /* BIFROST_EDITOR_SERIALIZER_HPP */