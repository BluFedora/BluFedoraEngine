/*!
 * @file   bifrost_behavior.hpp
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   The base class for all gameplay code for extending the engine.
 *
 * @version 0.0.1
 * @date    2020-06-13
 *
 * @copyright Copyright (c) 2020
 */
#ifndef BIFROST_BEHAVIOR_HPP
#define BIFROST_BEHAVIOR_HPP

#include "bifrost/core/bifrost_base_object.hpp"       // BaseObject<T>
#include "bifrost/utility/bifrost_non_copy_move.hpp"  // bfNonCopyMoveable<T>
#include "bifrost_base_component.hpp"                 // BaseComponent, Entity

namespace bifrost
{
  class ISerializer;

  using BehaviorEventFlags = std::uint16_t;  // Upgrade to a bigger type once we exceed 16 events.

  // clang-format off
  class bfPureInterface(IBehavior) : public IBaseObject
  // clang-format on
  {
   public:
    virtual void         serialize(ISerializer & serializer) = 0;
    virtual ~IBehavior()                                     = default;
  };

  // clang-format off
  class BaseBehavior : public IBehavior, public meta::Factory<BaseBehavior>, public BaseComponent, private bfNonCopyMoveable<BaseBehavior>
  // clang-format on
  {
    friend class Entity;

   public:
    static constexpr BehaviorEventFlags ON_UPDATE               = static_cast<BehaviorEventFlags>(bfBit(0));
    static constexpr BehaviorEventFlags ON_KEY_DOWN             = static_cast<BehaviorEventFlags>(bfBit(1));
    static constexpr BehaviorEventFlags ON_KEY_HELD             = static_cast<BehaviorEventFlags>(bfBit(2));
    static constexpr BehaviorEventFlags ON_KEY_UP               = static_cast<BehaviorEventFlags>(bfBit(3));
    static constexpr BehaviorEventFlags ON_COLLISION_ENTER      = static_cast<BehaviorEventFlags>(bfBit(4));
    static constexpr BehaviorEventFlags ON_COLLISION_STAY       = static_cast<BehaviorEventFlags>(bfBit(5));
    static constexpr BehaviorEventFlags ON_COLLISION_EXIT       = static_cast<BehaviorEventFlags>(bfBit(6));
    static constexpr BehaviorEventFlags ON_COMBAT_HEALTH_CHANGE = static_cast<BehaviorEventFlags>(bfBit(7));
    static constexpr BehaviorEventFlags ON_TIMELINE_EVENT       = static_cast<BehaviorEventFlags>(bfBit(8));
    // NOTE(Shareef):
    //   This is a special bit set by the editor when editing a Behavior so that
    //   said behavior can do some debug drawing in it's update loop.
    static constexpr BehaviorEventFlags ON_EDITOR_EDIT = static_cast<BehaviorEventFlags>(bfBit(9));
    // NOTE(Shareef):
    //   This is a special bit that indicates you want a set the update frequency manually.
    //   if set then custom update frequency that must NOT be overriden by the BehaviorSystem
    static constexpr BehaviorEventFlags ON_CUSTOM_UPDATE_FREQ = static_cast<BehaviorEventFlags>(bfBit(10));
    // NOTE(Shareef):
    //   This is a special bit that indicates we are loading data from a prefab rather
    //   than per object instance.
    static constexpr BehaviorEventFlags ON_PREFAB_LOAD = static_cast<BehaviorEventFlags>(bfBit(11));
    static constexpr BehaviorEventFlags ON_NOTHING     = 0x0000;
    static constexpr BehaviorEventFlags ON_ANYTHING    = std::numeric_limits<BehaviorEventFlags>::max();

   protected:
    BehaviorEventFlags m_EventFlags;

   protected:
    explicit BaseBehavior(PrivateCtorTag);

    // Event Flags API
    BehaviorEventFlags eventFlags() const { return m_EventFlags; }
    bool               isEventFlagSet(BehaviorEventFlags flags) const { return (m_EventFlags & flags) == flags; }
    void               setEventFlags(BehaviorEventFlags flags) { m_EventFlags |= flags; }
    void               clearEventFlags(BehaviorEventFlags flags) { m_EventFlags &= ~flags; }

   private:
    void setOwner(Entity& owner) { m_Owner = &owner; }

   public:
    void serialize(ISerializer& serializer) override;
  };

  // clang-format off
  template<typename T>
  class Behavior : public BaseBehavior::Base<T>
  // clang-format on
  {
   protected:
    using Base = Behavior<T>;

   public:
    explicit Behavior() :
      BaseBehavior::Base<T>()
    {
    }

    meta::BaseClassMetaInfo* type() override
    {
      return meta::typeInfoGet<T>();
    }

    // Make the protected virtual dtor from IBehavior public.
    ~Behavior() override {}
  };

#define bfRegisterBehavior(T)                                               \
  BIFROST_META_REGISTER(T)                                                  \
  {                                                                         \
    BIFROST_META_BEGIN()                                                    \
      BIFROST_META_MEMBERS(class_info<T, bifrost::IBehavior>(#T), ctor<>()) \
    BIFROST_META_END()                                                      \
  }
}  // namespace bifrost

BIFROST_META_REGISTER(bifrost::IBehavior)
{
  BIFROST_META_BEGIN()
    BIFROST_META_MEMBERS(class_info<bifrost::IBehavior>("bifrost::IBehavior"))
  BIFROST_META_END()
}

#if 1
// Example of how to declare/define and register a C++ gameplay behavior.
namespace game
{
  class ExampleBehavior : public bifrost::Behavior<ExampleBehavior>
  {
   public:
    ExampleBehavior() = default;  // Required to be default constructable for serialization purposes.
  };
}  // namespace game

bfRegisterBehavior(game::ExampleBehavior)
#endif

#endif /* BIFROST_BEHAVIOR_HPP */