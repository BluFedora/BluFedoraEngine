/******************************************************************************/
/*!
* @file   bifrost_iecs_system.hpp
* @author Shareef Abdoul-Raheem (https://blufedora.github.io/)
* @brief
*
* @version 0.0.1
* @date    2020-XX-XX
*
* @copyright Copyright (c) 2020-2021
*/
/******************************************************************************/
#ifndef BF_IECS_SYSTEM_HPP
#define BF_IECS_SYSTEM_HPP

namespace bf
{
  struct UpdateTime
  {
    float dt;
    float fixed_dt;
  };

  struct RenderView;
  class Engine;

  class IECSSystem
  {
    friend class Engine;

   private:
    bool m_IsEnabled;

   public:
    bool isEnabled() const { return m_IsEnabled; }

   protected:
    IECSSystem();

   public:  // TODO: This isn't right...
            // clang-format off
    virtual void onInit(Engine& engine) { (void)engine; }
    virtual void onFrameBegin(Engine& engine, float dt)   { (void)engine; (void)dt;    }
    virtual void onFrameUpdate(Engine& engine, float dt)  { (void)engine; (void)dt;    }
    virtual void onFrameEnd(Engine& engine, float dt)     { (void)engine; (void)dt;    }
    virtual void onFrameDraw(Engine& engine, RenderView& camera, float alpha) { (void)engine; (void)camera; (void)alpha; }
    virtual void onDeinit(Engine& engine) { (void)engine; }
            // clang-format on

   public:
    virtual ~IECSSystem() = default;
  };
}  // namespace bf

#endif /* BF_IECS_SYSTEM_HPP */
