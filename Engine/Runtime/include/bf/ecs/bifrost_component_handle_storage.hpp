/*!
 * @file   bifrost_component_handle_storage.hpp
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *
 * @version 0.0.1
 * @date    2020-06-11
 *
 * @copyright Copyright (c) 2020
 */
#ifndef BIFROST_COMPONENT_HANDLE_STORAGE_HPP
#define BIFROST_COMPONENT_HANDLE_STORAGE_HPP

#include "bf/data_structures/bifrost_container_tuple.hpp"  /* ContainerTuple<T> */
#include "bf/data_structures/bifrost_dense_map_handle.hpp" /* DenseMapHandle<T> */
#include "bifrost_component_list.hpp"                      /* ComponentPack     */

namespace bf
{
  template<typename T>
  using ComponentHandleImpl = DenseMapHandle<T, 12u, 20u>;

  template<typename T>
  struct ComponentHandle  // NOLINT(hicpp-member-init)
  {
    ComponentHandleImpl<T> handle;
  };

  template<typename T>
  struct ComponentActive  // NOLINT(hicpp-member-init)
  {
    bool is_active;
  };

  template<typename... Args>
  using DenseMapHandleTuple    = ContainerTuple<ComponentHandle, Args...>;
  using ComponentHandleStorage = ComponentPack::apply<DenseMapHandleTuple>;

  template<typename... Args>
  using ComponentActiveTuple   = ContainerTuple<ComponentActive, Args...>;
  using ComponentActiveStorage = ComponentPack::apply<ComponentActiveTuple>;
}  // namespace bf

#endif /* BIFROST_COMPONENT_HANDLE_STORAGE_HPP */
