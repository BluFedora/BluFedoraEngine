/*!
 * @file   bifrost_string.hpp
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   C++ Utilities for manipulating strings.
 *
 * @version 0.0.1
 * @date 2019-12-22
 *
 * @copyright Copyright (c) 2019
 */
#ifndef BIFROST_STRING_HPP
#define BIFROST_STRING_HPP

#include "bifrost/bifrost_std.h"    /* bfStringRange      */
#include "bifrost_dynamic_string.h" /* ConstBifrostString */

#include <cstddef>   /* size_t        */
#include <cstring>   /* strncmp       */
#include <stdexcept> /* runtime_error */

namespace bifrost
{
  static constexpr std::size_t k_StringNPos = std::numeric_limits<std::size_t>::max();

  class IMemoryManager;

  struct StringRange : public bfStringRange
  {
    constexpr StringRange(const bfStringRange& rhs) :
      bfStringRange{rhs}
    {
    }

    constexpr StringRange(const char* bgn, const char* end) :
      bfStringRange{bgn, end}
    {
    }

    constexpr StringRange(const char* bgn, std::size_t length) :
      bfStringRange{bgn, bgn + length}
    {
    }

    constexpr StringRange(const char* cstring) :
      bfStringRange{bfMakeStringRangeC(cstring)}
    {
    }

    constexpr StringRange() :
      StringRange{nullptr, nullptr}
    {
    }

    constexpr StringRange(std::nullptr_t) :
      StringRange{nullptr, nullptr}
    {
    }

    [[nodiscard]] std::size_t length() const
    {
      // bfStringRange_length();
      return bfStringRange::end - bgn;
    }

    [[nodiscard]] bool operator==(const StringRange& rhs) const
    {
      const std::size_t lhs_length = length();
      const std::size_t rhs_length = rhs.length();
      // TODO(Shareef): Can this be optimized by not using 'strncmp' since 'strncmp' checks for 'NUL' aswell?
      return lhs_length == rhs_length && std::strncmp(bgn, rhs.bgn, lhs_length) == 0;
    }

    // Assumes a nul terminated string. If you only want to compare with a piece then use StringRange.
    [[nodiscard]] bool operator==(const char* rhs) const
    {
      return std::strncmp(bgn, rhs, length()) == 0 && rhs[length()] == '\0';
    }

    [[nodiscard]] bool operator!=(const StringRange& rhs) const
    {
      return !(*this == rhs);
    }

    const char* begin() const
    {
      return bgn;
    }

    const char* end() const
    {
      return bfStringRange::end;
    }

    std::size_t find(char character, std::size_t pos = 0) const
    {
      const std::size_t len = length();

      for (std::size_t i = pos; i < len; ++i)
      {
        if (bgn[i] == character)
        {
          return i;
        }
      }

      return k_StringNPos;
    }
  };

  class String final
  {
   private:
    BifrostString m_Handle;

   public:
    String() :
      m_Handle{nullptr}
    {
    }

    String(const char* data) :
      m_Handle{String_new(data)}
    {
    }

    String(const char* bgn, std::size_t length) :
      m_Handle{length ? String_newLen(bgn, length) : nullptr}
    {
    }

    String(const char* bgn, const char* end) :
      String(bgn, end - bgn)
    {
    }

    template<typename T>
    String(T bgn, const T& end) :
      String()
    {
      while (bgn != end)
      {
        append(*bgn);
        ++bgn;
      }
    }

    String(const bfStringRange range) :
      String(range.bgn, range.end)
    {
    }

    String(const String& rhs) :
      m_Handle{rhs.m_Handle ? String_clone(rhs.m_Handle) : nullptr}
    {
    }

    String(String&& rhs) noexcept :
      m_Handle{rhs.m_Handle}
    {
      rhs.m_Handle = nullptr;
    }

    String& operator=(const String& rhs)
    {
      if (this != &rhs)
      {
        deleteHandle();
        m_Handle = rhs.m_Handle ? String_clone(rhs.m_Handle) : nullptr;
      }

      return *this;
    }

    String& operator=(String&& rhs) noexcept
    {
      if (this != &rhs)
      {
        deleteHandle();
        m_Handle     = rhs.m_Handle;
        rhs.m_Handle = nullptr;
      }

      return *this;
    }

    [[nodiscard]] bool operator==(const char* rhs) const
    {
      return m_Handle && String_ccmp(m_Handle, rhs) == 0;
    }

    [[nodiscard]] bool operator==(const String& rhs) const
    {
      return m_Handle && rhs.m_Handle && ::String_cmp(m_Handle, rhs.m_Handle) == 0;
    }

    [[nodiscard]] bool operator!=(const char* rhs) const
    {
      return !(*this == rhs);
    }

    [[nodiscard]] bool operator!=(const String& rhs) const
    {
      return !(*this == rhs);
    }

    operator StringRange() const
    {
      return {cstr(), cstr() + length()};
    }

    [[nodiscard]] BifrostString handle() const { return m_Handle; }

    void reserve(const std::size_t new_capacity)
    {
      if (!m_Handle)
      {
        m_Handle = String_new("");
      }

      ::String_reserve(&m_Handle, new_capacity);
    }

    void resize(const std::size_t new_size)
    {
      if (!m_Handle)
      {
        m_Handle = String_new("");
      }

      if (new_size)
      {
        ::String_resize(&m_Handle, new_size);
      }
    }

    const char* begin() const
    {
      return m_Handle;
    }

    const char* end() const
    {
      return m_Handle + length();
    }

    [[nodiscard]] std::size_t length() const
    {
      return m_Handle ? ::String_length(m_Handle) : 0u;
    }

    [[nodiscard]] std::size_t isEmpty() const
    {
      return length() == 0u;
    }

    [[nodiscard]] std::size_t size() const
    {
      return length();
    }

    [[nodiscard]] std::size_t capacity() const
    {
      return m_Handle ? ::String_capacity(m_Handle) : 0u;
    }

    [[nodiscard]] const char* cstr() const
    {
      return String_cstr(m_Handle);
    }

    // For STL Compat.
    [[nodiscard]] const char* c_str() const
    {
      return String_cstr(m_Handle);
    }

    void set(const char* str)
    {
      if (m_Handle)
      {
        String_cset(&m_Handle, str);
      }
      else
      {
        m_Handle = String_new(str);
      }
    }

    void append(const char character)
    {
      const char null_terminated_character[] = {character, '\0'};
      append(null_terminated_character, 1);
    }

    void append(const char* str)
    {
      if (m_Handle)
      {
        String_cappend(&m_Handle, str);
      }
      else
      {
        m_Handle = String_new(str);
      }
    }

    void append(const StringRange& str)
    {
      append(str.begin(), str.length());
    }

    void append(const String& str)
    {
      append(str.cstr(), str.length());
    }

    void append(const char* str, const std::size_t len)
    {
      if (m_Handle)
      {
        String_cappendLen(&m_Handle, str, len);
      }
      else
      {
        m_Handle = String_newLen(str, len);
      }
    }

    void insert(const std::size_t index, const char* str)
    {
      if (m_Handle)
      {
        String_cinsert(&m_Handle, index, str);
      }
      else if (index == 0)
      {
        m_Handle = String_new(str);
      }
      else
      {
        throw std::runtime_error("Inserting into an empty string.");
      }
    }

    // This is non-const by design.
    void unescape()  // const
    {
      String_unescape(m_Handle);
    }

    [[nodiscard]] std::size_t hash() const
    {
      // ReSharper disable CppUnreachableCode
      if constexpr (sizeof(std::size_t) == 4)
      {
        return bfString_hash(cstr());
      }
      else
      {
        return bfString_hash64(cstr());
      }
      // ReSharper restore CppUnreachableCode
    }

    void clear()
    {
      if (m_Handle)
      {
        ::String_clear(&m_Handle);
      }
    }

    ~String()
    {
      deleteHandle();
    }

   private:
    void deleteHandle() const
    {
      if (m_Handle)
      {
        String_delete(m_Handle);
      }
    }
  };

  inline String operator+(const String& lhs, const char* rhs)
  {
    String result = lhs;
    result.append(rhs);
    return result;
  }

  inline String operator+(const StringRange& lhs, const String& rhs)
  {
    String result = lhs;
    result.append(rhs);
    return result;
  }

  struct StringLink final
  {
    StringRange string;
    StringLink* next;

    explicit StringLink(StringRange data, StringLink*& head, StringLink*& tail);
  };

  struct TokenizeResult final
  {
    StringLink* head;
    StringLink* tail;
    std::size_t size;
  };
}  // namespace bifrost

namespace std
{
  template<>
  struct hash<bifrost::String>
  {
    std::size_t operator()(bifrost::String const& s) const noexcept
    {
      return s.hash();
    }
  };

  template<>
  struct hash<bifrost::StringRange>
  {
    std::size_t operator()(bifrost::StringRange const& s) const noexcept
    {
      // ReSharper disable CppUnreachableCode
      if constexpr (sizeof(std::size_t) == 4)
      {
        return bfString_hashN(s.begin(), s.length());
      }
      else
      {
        return bfString_hashN64(s.begin(), s.length());
      }
      // ReSharper restore CppUnreachableCode
    }
  };
}  // namespace std

namespace bifrost::string_utils
{
  // Helper Structs //

  struct StringHasher final
  {
    std::size_t operator()(const ConstBifrostString input) const
    {
      return bfString_hash(input);
    }
  };

  struct StringComparator final
  {
    bool operator()(const ConstBifrostString lhs, const char* const rhs) const
    {
      return String_ccmp(lhs, rhs) == 0;
    }
  };

  // String Formatting Functions //

  // The caller is responsible for freeing any memory this allocates.
  // Use 'fmtFree' to deallocate memory.
  char* fmtAlloc(IMemoryManager& allocator, std::size_t* out_size, const char* fmt, ...);
  // Deallocates memory from 'fmtAlloc'
  void fmtFree(IMemoryManager& allocator, char* ptr);
  // Uses the passed in buffer for the formatting.
  // Returns true if the buffer was big enough.
  bool fmtBuffer(char* buffer, size_t buffer_size, std::size_t* out_size, const char* fmt, ...);

  // String Tokenizing //

  //
  // If the strig ends in the delimter then you will get an empty string as the last element.
  //

  // Callers job to either free each 'StringLink' themselves or call 'tokenizeFree' with the same allocator.
  TokenizeResult tokenizeAlloc(IMemoryManager& allocator, const StringRange& string, char delimiter = '/');
  // Deallocates memory from 'tokenizeAlloc'
  void tokenizeFree(IMemoryManager& allocator, const TokenizeResult& tokenized_list);

  //
  // The callback is passed in a 'StringRange' for each tokenized element.
  // The StringRange does not include the delimiter character.
  // Except for the first call you can assume the StringRange is preceeded by the delimiter.
  //
  template<typename F>
  void tokenize(const StringRange& string, const char delimiter, F&& callback)
  {
    std::size_t last_pos = 0u;

    while (true)
    {
      const std::size_t slash_pos = string.find(delimiter, last_pos);
      const char* const bgn       = string.begin() + last_pos;

      if (slash_pos == k_StringNPos)
      {
        callback(StringRange{bgn, string.length() - last_pos});
        break;
      }

      callback(StringRange{bgn, slash_pos - last_pos});

      last_pos = slash_pos + 1;  // NOTE(Shareef): The + 1 accounts for the delimiting character.
    }
  }

  // Misc //

  // Caller is responsible for freeing memory from this.
  // TODO(SR) FIX-ME: This is a bad abstraction since it returns a const view into a string that is OWNED by that view.
  StringRange clone(IMemoryManager& allocator, const StringRange& string);
}  // namespace bifrost::string_utils

#endif /* BIFROST_STRING_HPP */
