/******************************************************************************/
/*!
 * @file   bifrost_base_object.hpp
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *  All reflectable / serializable engine objects will inherit from this class.
 *
 * @version 0.0.1
 * @date 2019-12-22
 *
 * @copyright Copyright (c) 2019
 */
/******************************************************************************/
#ifndef BIFROST_BASE_OBJECT_HPP
#define BIFROST_BASE_OBJECT_HPP

#include "bifrost/meta/bifrost_meta_factory.hpp"     /* Factory<T>           */
#include "bifrost/utility/bifrost_non_copy_move.hpp" /* bfNonCopyMoveable<T> */

class BifrostEngine;

namespace bifrost
{
  // NOTE(Shareef): Use this class as the base type.

  // clang-format off
  class BaseObjectT : /*private bfNonCopyMoveable<BaseObjectT>,*/ public meta::Factory<BaseObjectT>
  // clang-format on
  {
   protected:
    explicit BaseObjectT(PrivateCtorTag) {}

   public:
    virtual meta::BaseClassMetaInfo* type() = 0;
    virtual ~BaseObjectT()                  = default;
  };

  // NOTE(Shareef): Inherit from this
  // template<typename... T>
  // using BaseObject = BaseObjectT::Base<T...>;

  namespace detail
  {
    template<typename... Ts>
    struct FirstT;

    template<typename T0, typename... Ts>
    struct FirstT<T0, Ts...>
    {
      using Type = T0;
    };
  }  // namespace detail

  template<typename... T>
  class BaseObject : public BaseObjectT::Base<T...>
  {
   public:
    static meta::BaseClassMetaInfo* staticType()
    {
      return meta::TypeInfo<typename detail::FirstT<T...>::Type>::get();
    }

    meta::BaseClassMetaInfo* type() override
    {
      return staticType();
    }
  };
}  // namespace bifrost

#endif /* BIFROST_BASE_OBJECT_HPP */
