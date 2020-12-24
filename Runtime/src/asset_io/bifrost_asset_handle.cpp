/******************************************************************************/
/*!
* @file   bifrost_asset_handle.cpp
* @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
* @brief
*  Asset handle definitions.
*
*  Types of Assets:
*    > Shader Module
*    > Shader Program
*    > Texture
*    > Material
*    > Spritesheet Animations
*    > Audio Source
*    > Scene
*    > Font
*    > Script
*    > Models (Meshes)
*
* @version 0.0.1
* @date    2019-12-26
*
* @copyright Copyright (c) 2019
*/
/******************************************************************************/

#include "bf/asset_io/bifrost_json_serializer.hpp"
#include "bf/core/bifrost_engine.hpp" /* Engine */
#include "bf/utility/bifrost_json.hpp"

namespace bf
{
  void IBaseObject::reflect(ISerializer& serializer)
  {
    serializer.serialize(*this);
  }

  static constexpr int         k_MaxDigitsUInt64 = 20;
  static constexpr StringRange k_EnumValueKey    = "__EnumValue__";

  bool ISerializer::hasKey(StringRange key)
  {
    (void)key;
    return false;
  }

  void ISerializer::serialize(StringRange key, Vec2f& value)
  {
    if (pushObject(key))
    {
      serialize("x", value.x);
      serialize("y", value.y);

      popObject();
    }
  }

  void ISerializer::serialize(StringRange key, Vec3f& value)
  {
    if (pushObject(key))
    {
      serialize("x", value.x);
      serialize("y", value.y);
      serialize("z", value.z);
      serialize("w", value.w);

      popObject();
    }
  }

  void ISerializer::serialize(StringRange key, Quaternionf& value)
  {
    if (pushObject(key))
    {
      serialize("x", value.x);
      serialize("y", value.y);
      serialize("z", value.z);
      serialize("w", value.w);

      popObject();
    }
  }

  void ISerializer::serialize(StringRange key, bfColor4f& value)
  {
    if (pushObject(key))
    {
      serialize("r", value.r);
      serialize("g", value.g);
      serialize("b", value.b);
      serialize("a", value.a);

      popObject();
    }
  }

  void ISerializer::serialize(StringRange key, bfColor4u& value)
  {
    if (pushObject(key))
    {
      serialize("r", value.r);
      serialize("g", value.g);
      serialize("b", value.b);
      serialize("a", value.a);

      popObject();
    }
  }

  void ISerializer::serialize(StringRange key, Rect2f& value)
  {
    if (pushObject(key))
    {
      float x = value.left();
      float y = value.top();
      float w = value.width();
      float h = value.height();

      serialize("x", x);
      serialize("y", y);
      serialize("width", w);
      serialize("height", h);

      value.setX(x);
      value.setY(y);
      value.setWidth(w);
      value.setHeight(h);

      popObject();
    }
  }

  void ISerializer::serialize(StringRange key, bfUUIDNumber& value)
  {
    char as_string_chars[k_bfUUIDStringCapacity];
    bfUUID_numberToString(value.data, as_string_chars);

    String as_string = {as_string_chars, k_bfUUIDStringLength};

    serialize(key, as_string);

    if (mode() == SerializerMode::LOADING)
    {
      if (as_string.length() != k_bfUUIDStringLength || !hasKey(key))
      {
        std::memset(&value, 0x0, sizeof(value));
      }
      else
      {
        const bfUUID uuid = bfUUID_fromString(as_string.c_str());

        value = uuid.as_number;
      }
    }
  }

  void ISerializer::serialize(StringRange key, bfUUID& value)
  {
    serialize(key, value.as_number);

    if (mode() == SerializerMode::LOADING)
    {
      bfUUID_numberToString(value.as_number.data, value.as_string.data);
    }
  }

  void ISerializer::serialize(StringRange key, IBaseObject& value)
  {
    if (pushObject(key))
    {
      serialize(value);
      popObject();
    }
  }

  void ISerializer::serialize(IBaseObject& value)
  {
    meta::MetaObject meta_obj;  // NOLINT(cppcoreguidelines-pro-type-member-init)

    meta_obj.type_info  = value.type();
    meta_obj.object_ref = &value;

    serialize(meta_obj);
  }

  void ISerializer::serialize(StringRange key, meta::MetaObject& value)
  {
    if (pushObject(key))
    {
      serialize(value);
      popObject();
    }
  }

  void ISerializer::serialize(meta::MetaObject& value)
  {
    meta::BaseClassMetaInfo* const type_info = value.type_info;

    if (type_info->isEnum())
    {
      serialize(k_EnumValueKey, value.enum_value);
    }
    else
    {
      const meta::MetaVariant as_variant = {value};

      for (auto& prop : type_info->properties())
      {
        const StringRange field_name  = StringRange(prop->name().data(), prop->name().size());
        auto              field_value = prop->get(as_variant);

        serialize(field_name, field_value);

        prop->set(as_variant, field_value);
      }

      if (type_info->isArray())
      {
        std::size_t array_size;

        if (pushArray("Elements", array_size))
        {
          const std::size_t size = type_info->numElements(as_variant);
          char              label_buffer[k_MaxDigitsUInt64 + 1];
          std::size_t       idx_label_length;

          for (std::size_t i = 0; i < size; ++i)
          {
            if (string_utils::fmtBuffer(label_buffer, sizeof(label_buffer), &idx_label_length, "%zu", i))
            {
              auto element = type_info->elementAt(as_variant, i);
              serialize(StringRange(label_buffer, idx_label_length), element);

              (void)type_info->setElementAt(as_variant, i, element);
            }
            else
            {
              // TODO(Shareef): This should _never_ happen, and what do you do if it does?
            }
          }

          popArray();
        }
      }
    }
  }

  void ISerializer::serialize(StringRange key, meta::MetaVariant& value)
  {
    visit_all(
     meta::overloaded{
      [this, &key](auto& primitive_value) -> void { serialize(key, primitive_value); },
      [this, &key](IBaseObject* base_obj) -> void { serialize(key, *base_obj); },
      [this, &key](IARCHandle* base_handle) -> void { serialize(key, *base_handle); },
     },
     value);
  }

  void ISerializer::serialize(meta::MetaVariant& value)
  {
    visit_all(
     meta::overloaded{
      [this](auto& primitive_value) -> void {
        meta::MetaObject meta_obj;  // NOLINT(cppcoreguidelines-pro-type-member-init)

        meta_obj.type_info  = meta::TypeInfo<decltype(primitive_value)>::get();
        meta_obj.object_ref = &primitive_value;

        serialize(meta_obj);
      },
      [this](IBaseObject* base_obj) -> void { serialize(*base_obj); },
      [this](meta::MetaObject& meta_obj) -> void { serialize(meta_obj); },
     },
     value);
  }

  void ISerializer::serialize(StringRange key, Vector2f& value)
  {
    serialize(key, static_cast<Vec2f&>(value));
  }

  void ISerializer::serialize(StringRange key, Vector3f& value)
  {
    serialize(key, static_cast<Vec3f&>(value));
  }

  LinearAllocator& ENGINE_TEMP_MEM(Engine& engine)
  {
    return engine.tempMemory();
  }

  IMemoryManager&  ENGINE_TEMP_MEM_NO_FREE(Engine& engine)
  {
    return engine.tempMemoryNoFree();
  }
}  // namespace bf
