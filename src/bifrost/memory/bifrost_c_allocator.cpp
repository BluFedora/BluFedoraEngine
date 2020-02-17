/******************************************************************************/
/*!
  @file   bifrost_c_allocator.cpp
  @author Shareef Abdoul-Raheem
  @par    email: shareef.a\@digipen.edu
  @brief
      This allocator is a wrapper around the built in memory allocator.
      Implemented using "malloc / calloc" and "free" respectively.
*/
/******************************************************************************/
#include "bifrost/memory/bifrost_c_allocator.hpp"

#include <cstdlib> /* calloc, malloc, free */

namespace bifrost
{
  CAllocator::CAllocator() :
    IMemoryManager()
  {
  }

  void* CAllocator::allocate(std::size_t size)
  {
#if 0
    return std::calloc(1, size);
#else
    return std::malloc(size);
#endif
  }

  void CAllocator::deallocate(void* ptr)
  {
    std::free(ptr);
  }
}  // namespace bifrost
