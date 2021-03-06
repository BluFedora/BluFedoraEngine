/******************************************************************************/
/*!
 * @file   bf_behavior.hpp
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   The base class for all gameplay code extending the engine.
 *
 * @version 0.0.1
 * @date    2020-06-13
 *
 * @copyright Copyright (c) 2020-2021
 */
/******************************************************************************/
#ifndef BF_BEHAVIOR_HPP
#define BF_BEHAVIOR_HPP

#include "bf/bf_non_copy_move.hpp"          // bfNonCopyMoveable<T>
#include "bf/core/bifrost_base_object.hpp"  // BaseObject<T>
#include "bf/ecs/bf_base_component.hpp"     // BaseComponent, Entity

#include "bf/ecs/bifrost_iecs_system.hpp"
#include "bf/ecs/bifrost_behavior_system.hpp"

namespace bf
{
  class ISerializer;
  class BehaviorEvents;

  using BehaviorEventFlags = std::uint16_t;  // Upgrade to a bigger type once we exceed 16 event flags.

  // clang-format off

  class bfPureInterface(IBehavior) : public IBaseObject
  {
   public:
    static constexpr BehaviorEventFlags ON_UPDATE               = static_cast<BehaviorEventFlags>(bfBit(0));  //!< Set if you implement the `IBehavior::onUpdate` callback.
    static constexpr BehaviorEventFlags ON_KEY_DOWN             = static_cast<BehaviorEventFlags>(bfBit(1));
    static constexpr BehaviorEventFlags ON_KEY_HELD             = static_cast<BehaviorEventFlags>(bfBit(2));
    static constexpr BehaviorEventFlags ON_KEY_UP               = static_cast<BehaviorEventFlags>(bfBit(3));
    static constexpr BehaviorEventFlags ON_COLLISION_ENTER      = static_cast<BehaviorEventFlags>(bfBit(4));
    static constexpr BehaviorEventFlags ON_COLLISION_STAY       = static_cast<BehaviorEventFlags>(bfBit(5));
    static constexpr BehaviorEventFlags ON_COLLISION_EXIT       = static_cast<BehaviorEventFlags>(bfBit(6));
    static constexpr BehaviorEventFlags ON_COMBAT_HEALTH_CHANGE = static_cast<BehaviorEventFlags>(bfBit(7));
    static constexpr BehaviorEventFlags ON_TIMELINE_EVENT       = static_cast<BehaviorEventFlags>(bfBit(8));

    //   This is a special bit set by the editor when editing a Behavior so that
    //   said behavior can do some debug drawing in it's update loop.
    static constexpr BehaviorEventFlags ON_EDITOR_EDIT = static_cast<BehaviorEventFlags>(bfBit(9));

    //   This is a special bit that indicates you want a set the update frequency manually.
    //   if set then custom update frequency that must NOT be overriden by the BehaviorSystem
    static constexpr BehaviorEventFlags ON_CUSTOM_UPDATE_FREQ = static_cast<BehaviorEventFlags>(bfBit(10));

    //   This is a special bit that indicates we are loading data from a prefab rather
    //   than per object instance.
    static constexpr BehaviorEventFlags ON_PREFAB_LOAD = static_cast<BehaviorEventFlags>(bfBit(11));

    // Special but indicating if this behavior is active.
    static constexpr BehaviorEventFlags IS_ACTIVE = static_cast<BehaviorEventFlags>(bfBit(12));

    static constexpr BehaviorEventFlags ON_NOTHING  = 0x0000;
    static constexpr BehaviorEventFlags ON_ANYTHING = ~ON_NOTHING;

  protected:
    friend class BehaviorSystem;

   public:
    virtual void onEnable(BehaviorEvents& events)         {}
    virtual void onUpdate(UpdateTime dt) { (void)dt; }
    virtual void onDisable(BehaviorEvents& events)        {}

    ~IBehavior() override = default;
  };

  // clang-format on

  // clang-format off
  class BaseBehavior : public IBehavior, public meta::AutoRegisterType<BaseBehavior>, public BaseComponent, private NonCopyMoveable<BaseBehavior>
  // clang-format on
  {
    friend class Entity;
    friend class BehaviorEvents;

   protected:
    BehaviorOnUpdateID m_OnUpdateID;

   protected:
    explicit BaseBehavior(PrivateCtorTag);

   public:
    // Meta

    ClassID::Type classID() const override { return ClassID::BASE_BEHAVIOR; }
    void          reflect(ISerializer& serializer) override;

   private:
    void setOwner(Entity& owner) { m_Owner = &owner; }

   public:
    void setActive(bool is_active);
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

    meta::BaseClassMetaInfo* type() const override
    {
      return meta::typeInfoGet<T>();
    }

    // Make the protected virtual dtor from IBehavior public.
    ~Behavior() override {}
  };

#define bfRegisterBehavior(T)                                          \
  BIFROST_META_REGISTER(T)                                             \
  {                                                                    \
    BIFROST_META_BEGIN()                                               \
      BIFROST_META_MEMBERS(class_info<T, bf::IBehavior>(#T), ctor<>()) \
    BIFROST_META_END()                                                 \
  }
}  // namespace bf

BIFROST_META_REGISTER(bf::IBehavior)
{
  BIFROST_META_BEGIN()
    BIFROST_META_MEMBERS(class_info<bf::IBehavior>("bifrost::IBehavior"))
  BIFROST_META_END()
}

#if 1
// Example of how to declare/define and register a C++ gameplay behavior.
namespace game
{
  class ExampleBehavior : public bf::Behavior<ExampleBehavior>
  {
   public:
    float time = 0.0f;

   public:
    // Required to be default constructable for serialization purposes.
    ExampleBehavior() = default;
    void onEnable(bf::BehaviorEvents& events) override;
    void onUpdate(bf::UpdateTime dt) override;
    void onUpdate2(bf::UpdateTime dt);
  };
}  // namespace game

bfRegisterBehavior(game::ExampleBehavior)
#endif

#endif /* BF_BEHAVIOR_HPP */
