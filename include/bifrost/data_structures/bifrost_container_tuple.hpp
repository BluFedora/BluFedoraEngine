#ifndef TIDE_CONTAINER_TUPLE_HPP
#define TIDE_CONTAINER_TUPLE_HPP

#include "bifrost/meta/bifrost_meta_utils.hpp" /* for_each_template */

#include <tuple>       /* size_t, tuple     */
#include <type_traits> /* aligned_storage_t */

namespace bifrost
{
  namespace detail
  {
    template<class T, std::size_t N, class... Args>
    struct get_index_of_element_from_tuple_by_type_impl;

    template<class T, std::size_t N, class... Args>
    struct get_index_of_element_from_tuple_by_type_impl<T, N, T, Args...>
    {
      static constexpr auto value = N;
    };

    template<class T, std::size_t N, class U, class... Args>
    struct get_index_of_element_from_tuple_by_type_impl<T, N, U, Args...>
    {
      static constexpr auto value = get_index_of_element_from_tuple_by_type_impl<T, N + 1, Args...>::value;
    };

    template<class... Trest>
    struct unique_types; /* undefined */

    template<class T1, class T2, class... Trest>
    struct unique_types<T1, T2, Trest...> : private unique_types<T1, T2>
      , unique_types<T1, Trest...>
      , unique_types<T2, Trest...>
    {
    };

    template<class T1, class T2>
    struct unique_types<T1, T2>
    {
      static_assert(!std::is_same<T1, T2>::value, "All types must be unique in this arg list.");
    };

    /*!
     *  @brief
     *    One element is guaranteed to be unique.
     */
    template<class T1>
    struct unique_types<T1>
    {
    };
  }  // namespace detail

  /*!
   *  @brief
   *    Thin wrapper around a tuple of 'TContainer' of differing types.
   */
  template<template<typename...> class TContainer, typename... Args>
  class ContainerTuple : private detail::unique_types<Args...>
  {
   public:
    template<typename F>
    static void forEachType(F&& f)
    {
      meta::for_each_template<Args...>(std::forward<F>(f));
    }

   private:
    using ContainerTupleImpl = std::tuple<std::aligned_storage_t<sizeof(Args), alignof(Args)>...>;

   private:
    ContainerTupleImpl m_Impl;

   public:
    template<typename... CtorArgs>
    ContainerTuple(CtorArgs&&... args) noexcept :
      m_Impl{}
    {
      meta::for_each_template<Args...>([this, &args...](auto t) {
        using T = bfForEachTemplateT(t);

        new (&this->get<T>()) TContainer<T>(std::forward<CtorArgs>(args)...);
      });
    }

    ~ContainerTuple()
    {
      meta::for_each_template<Args...>([this](auto t) {
        using T = bfForEachTemplateT(t);

        this->get<T>().~TContainer<T>();
      });
    }

    ContainerTupleImpl& raw() noexcept
    {
      return m_Impl;
    }

    const ContainerTupleImpl& raw() const noexcept
    {
      return m_Impl;
    }

    template<typename T>
    TContainer<T>& get() noexcept
    {
      return getContainer<TContainer<T>, T>();
    }

    template<typename T>
    const TContainer<T>& get() const noexcept
    {
      return getConstContainer<TContainer<T>, T>();
    }

   private:
    template<typename TCont, typename TElem>
    TCont& getContainer()
    {
      return *reinterpret_cast<TCont*>(&std::get<indexOfType<TElem>()>(raw()));
    }

    template<typename TCont, typename TElem>
    const TCont& getConstContainer() const
    {
      return *reinterpret_cast<const TCont*>(&std::get<indexOfType<TElem>()>(raw()));
    }

    template<typename T>
    static constexpr std::size_t indexOfType()
    {
      return detail::get_index_of_element_from_tuple_by_type_impl<T, 0, Args...>::value;
    }
  };
}  // namespace bifrost

#endif /* TIDE_CONTAINER_TUPLE_HPP */
