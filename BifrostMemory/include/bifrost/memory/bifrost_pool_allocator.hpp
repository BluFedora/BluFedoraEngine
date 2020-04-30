/******************************************************************************/
/*!
* @file   bifrost_linear_allocator.hpp
* @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
* @brief
*   This allocator is a designed for static (known at compile time)
*   pools of objects. Features O(1) allocation and O(1) deletion.
*
* @version 0.0.1
* @date    2019-12-26
*
* @copyright Copyright (c) 2019-2020
*/
/******************************************************************************/
#ifndef BIFROST_POOL_ALLOCATOR_HPP
#define BIFROST_POOL_ALLOCATOR_HPP

#include "bifrost_imemory_manager.hpp"

#include <cassert> /* assert  */
#include <cstdint> /* uint8_t */

namespace bifrost
{
  namespace detail
  {
    template<std::size_t size_of_t, std::size_t alignment>
    static constexpr std::size_t aligned_size()
    {
      return ((size_of_t + alignment - 1) / alignment) * alignment;
    }
  }  // namespace detail

  template<typename T, std::size_t num_elements>
  class PoolAllocator final : public IMemoryManager
  {
   private:
    template<size_t arg1, size_t... others>
    struct static_max;

    template<size_t arg>
    struct static_max<arg>
    {
      static constexpr size_t value = arg;
    };

    template<size_t arg1, size_t arg2, size_t... others>
    struct static_max<arg1, arg2, others...>
    {
      static constexpr size_t value = (arg1 >= arg2) ? static_max<arg1, others...>::value : static_max<arg2, others...>::value;
    };

    class PoolHeader
    {
     public:
      PoolHeader* next;
    };

   public:
    static constexpr std::size_t header_size       = sizeof(PoolHeader);
    static constexpr std::size_t alignment_req     = static_max<alignof(T), alignof(PoolHeader)>::value;
    static constexpr std::size_t allocation_size   = static_max<sizeof(T), header_size>::value;
    static constexpr std::size_t pool_stride       = detail::aligned_size<allocation_size, alignment_req>();
    static constexpr std::size_t memory_block_size = pool_stride * num_elements;

   private:
    char        m_AllocBlock[memory_block_size];
    PoolHeader* m_PoolStart;

   public:
    PoolAllocator();

    void* allocate(std::size_t size) override;
    void  deallocate(void* ptr) override;

    char*       begin() { return m_AllocBlock; }
    const char* begin() const { return m_AllocBlock; }
    char*       end() { return m_AllocBlock + size(); }
    const char* end() const { return m_AllocBlock + size(); }
    std::size_t size() const { return sizeof(m_AllocBlock); }

   private:
    void init() const
    {
      PoolHeader* header = m_PoolStart;

      for (std::size_t i = 0; i < num_elements - 1; ++i)
      {
        header->next = reinterpret_cast<PoolHeader*>(reinterpret_cast<std::uint8_t*>(header) + pool_stride);
        header       = header->next;
      }

      header->next = nullptr;
    }

   public:
    void reset()
    {
      m_PoolStart = reinterpret_cast<PoolHeader*>(begin());
      init();
    }
  };

  template<typename T, std::size_t num_elements>
  PoolAllocator<T, num_elements>::PoolAllocator(void) :
    IMemoryManager(),
    m_PoolStart(reinterpret_cast<PoolHeader*>(begin()))
  {
    init();
  }

  template<typename T, std::size_t num_elements>
  void* PoolAllocator<T, num_elements>::allocate(const std::size_t size)
  {
    assert(size == sizeof(T) && "This Allocator is made for Objects of one size!");

    PoolHeader* const header = m_PoolStart;

    if (header)
    {
      m_PoolStart = header->next;
      return reinterpret_cast<void*>(header);
    }

    // throw std::bad_alloc();
    return nullptr;
  }

  template<typename T, std::size_t num_elements>
  void PoolAllocator<T, num_elements>::deallocate(void* ptr)
  {
    PoolHeader* const header = reinterpret_cast<PoolHeader*>(ptr);

#ifdef BIFROST_MEMORY_DEBUG_WIPE_MEMORY
    std::memset(ptr, BIFROST_MEMORY_DEBUG_SIGNATURE, pool_stride);
#endif

    header->next = m_PoolStart;
    m_PoolStart  = header;
  }
}  // namespace bifrost

#endif /* BIFROST_POOL_ALLOCATOR_HPP */