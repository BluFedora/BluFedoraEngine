/******************************************************************************/
/*!
  @file   bifrost_linear_allocator.cpp
  @author Shareef Abdoul-Raheem
  @par    email: shareef.a\@digipen.edu
  @brief
      This allocator is very good for temporary memory allocations throughout
      the frame. There is no invidual deallocation but a whole clear operation
      that should happen at the beginning of each frame.
*/
/******************************************************************************/
#include "bifrost/memory/bifrost_linear_allocator.hpp"

// #include <new>    /* bad_alloc  */
#include <memory> /* align      */

namespace bifrost
{
  const char* linear_allocator_free::what() const noexcept
  {
    return "LinearAllocator::dealloc was called but that may never happen all data must be free at once with LinearAllocator::clear.";
  }

  LinearAllocator::LinearAllocator(char* memory_block, const std::size_t memory_block_size) :
    MemoryManager(memory_block, memory_block_size),
    m_MemoryOffset(0)
  {
  }

  void LinearAllocator::clear()
  {
#ifdef DEBUG_MEMORY
    if (m_MemoryOffset)
    {
      std::memset(begin(), DEBUG_MEMORY_SIGNATURE, m_MemoryOffset);
    }
#endif
    m_MemoryOffset = 0;
  }

  void* LinearAllocator::alloc(const std::size_t size, const std::size_t alignment)
  {
    if (m_MemoryOffset < this->size())
    {
      const std::size_t old_loc = end() - currentBlock();
      std::size_t memory_left = old_loc;

      void* ptr = currentBlock();

      if (std::align(alignment, size, ptr, memory_left))
      {
        m_MemoryOffset += size + (old_loc - memory_left);
        return ptr;
      }
    }

    // throw std::bad_alloc();
    return nullptr;
  }

  void LinearAllocator::dealloc(void*)
  {
    throw linear_allocator_free();
  }

  char* LinearAllocator::currentBlock() const
  {
    return begin() + m_MemoryOffset;
  }

  LinearAllocatorScope::LinearAllocatorScope(LinearAllocator& allocator) :
    m_Allocator(allocator),
    m_OldOffset(allocator.m_MemoryOffset)
  {
  }

  LinearAllocatorScope::~LinearAllocatorScope()
  {
    m_Allocator.m_MemoryOffset = m_OldOffset;
  }
}