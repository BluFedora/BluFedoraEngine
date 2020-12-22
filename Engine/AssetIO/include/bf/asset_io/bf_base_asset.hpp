/******************************************************************************/
/*!
 * @file   bf_base_asset.hpp
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   Interface for creating asset type the engine can use.
 *
 * @version 0.0.1
 * @date    2020-12-19
 *
 * @copyright Copyright (c) 2019-2020
 */
/******************************************************************************/
#ifndef BF_BASE_ASSET_HPP
#define BF_BASE_ASSET_HPP

#include "bf/bf_core.h"                     // bfBit, bfPureInterface
#include "bf/bf_non_copy_move.hpp"          // NonCopyMoveable<T>
#include "bf/core/bifrost_base_object.hpp"  // IBaseObject

#include <atomic> /* atomic_uint16_t */

namespace bf
{
  class Assets;
  class Engine;
  class ISerializer;

  enum class AssetStatus
  {
    UNLOADED,  //!< (RefCount == 0)                                : Asset is not loaded.
    FAILED,    //!< (RefCount == 0) && (Flags & FailedToLoad) != 0 : Asset tried to load but could not.
    LOADING,   //!< (RefCount != 0) && (Flags & IsLoaded) == 0     : Asset loading on background thread.
    LOADED,    //!< (RefCount != 0) && (Flags & IsLoaded) != 0     : Asset is fully loaded.
  };

  namespace AssetFlags
  {
    enum
    {
      DEFAULT        = 0x0,       //!< No flags are set by default.
      IS_LOADED      = bfBit(0),  //!< Marks that the asset has been successfully loaded.
      IS_SUBASSET    = bfBit(1),  //!< This asset only lives in memory.
      FAILED_TO_LOAD = bfBit(2),  //!< Failed to load asset, this flag is set do that we do not continuously try to load it.
      IS_DIRTY       = bfBit(3),  //!< This asset wants to be saved.
    };
  }

  // clang-format off
  class bfPureInterface(IBaseAsset) : public IBaseObject
  // clang-format on
  {
    friend class Assets;

    // TODO(SR): Having data members in an interface probably breaks some of my codebase guidelines.
   private:
    bfUUIDNumber         m_UUID;
    String               m_FilePathAbs;  //!< The full path to an asset.
    StringRange          m_FilePathRel;  //!< Indexes into `IBaseAsset::m_FilePathAbs` for the relative path.
    std::atomic_uint16_t m_RefCount;     //!< The number of live references there are to this asset.
    std::atomic_uint16_t m_Flags;        //!< Various flags about the current state of the asset.

   public:
    IBaseAsset();

    // Accessors //

    [[nodiscard]] const bfUUIDNumber& uuid() const { return m_UUID; }
    [[nodiscard]] const String&       fullPath() const { return m_FilePathAbs; }
    [[nodiscard]] StringRange         relativePath() const { return m_FilePathRel; }
    [[nodiscard]] std::uint16_t       refCount() const { return m_RefCount; }
    AssetStatus                       status() const;

    // IO and Ref Count //

    void acquire();
    void reload();
    void release();
    void saveAssetContent(ISerializer & serializer);
    void saveAssetMeta(ISerializer & serializer);

   private:
    void setup(const String& full_path, std::size_t length_of_root_path, const bfUUIDNumber& uuid);

    // Interface That Must Be Implemented By Subclasses. //

    virtual void onLoad()   = 0;  // Called when the asset should be loaded. (Will never be called if the asset is already loaded)
    virtual void onUnload() = 0;  // Called when the asset should be unloaded from memory. (Will never be called if the asset is not loaded)

    // These have default implementations.

    virtual void onReload();                             // By Default calls "unload" then "load" but allows for subclasses to optimize the "reload" operation.
    virtual void onSaveAsset(ISerializer & serializer);  // Called when the asset should save to the source asset file, default does nothing.
    virtual void onSaveMeta(ISerializer & serializer);   // Called to save some extra information in the meta file, default does nothing.

   protected:
    // Helpers methods for sub classes //

    void markFailedToLoad()
    {
      m_Flags |= AssetFlags::FAILED_TO_LOAD;
      m_RefCount = 0;
    }

    void markIsLoaded()
    {
      m_Flags |= AssetFlags::IS_LOADED;
    }
  };

  namespace detail
  {
    // clang-format off
    class BaseAssetT : public IBaseAsset, public meta::AutoRegisterType<BaseAssetT>, private NonCopyMoveable<BaseAssetT>
    {
     protected:
      explicit BaseAssetT(PrivateCtorTag) :
        IBaseAsset(),
        AutoRegisterType<BaseAssetT>(),
        NonCopyMoveable<BaseAssetT>()
      {}
    };
    // clang-format on
  }  // namespace detail

  // clang-format off
  template<typename T>
  class BaseAsset : public detail::BaseAssetT::Base<T>
  // clang-format on
  {
   public:
    using Base = BaseAsset<T>;

    BaseAsset() = default;

    meta::BaseClassMetaInfo* type() const final override
    {
      return meta::typeInfoGet<T>();
    }
  };

  //
  // ARC = Automatic Reference Count
  //

  // clang-format off
  
  //
  // This interface exists so that you can manipulate an
  // `ARC` handle generically particularly in serialization
  // and editor code.
  //
  class bfPureInterface(IARCHandle)
  // clang-format on
  {
   public:
    virtual bool                     isValid() const noexcept   = 0;
    virtual meta::BaseClassMetaInfo* typeInfo() const noexcept  = 0;
    virtual void                     assign(IBaseAsset * asset) = 0;
    virtual IBaseAsset*              handle() const noexcept    = 0;
    virtual ~IARCHandle()                                       = default;
  };

  //
  // Automatically handles calling acquire and release on the associated IBaseAsset pointer.
  //

  template<typename T>
  class ARC final : public IARCHandle
  {
    // using T = IBaseAsset;

    static_assert(!std::is_pointer_v<T>, "T must not be a pointer.");
    static_assert(std::is_convertible_v<T*, IBaseAsset*>, "The type must implement the IBaseAsset interface.");

   private:
    T* m_Handle;

   public:
    ARC(T* handle = nullptr) :
      IARCHandle(),
      m_Handle{handle}
    {
      acquire();
    }

    // NOTE(Shareef): Useful to set a handle to nullptr to represent null.
    ARC(std::nullptr_t) :
      IARCHandle(),
      m_Handle{nullptr}
    {
    }

    ARC(const ARC& rhs) :
      IARCHandle(),
      m_Handle{rhs.m_Handle}
    {
      acquire();
    }

    // clang-format off

    ARC(ARC&& rhs) noexcept :
      IARCHandle(),
      m_Handle{std::exchange(rhs.m_Handle, nullptr)}
    {
    }

    // clang-format on

    ARC& operator=(const ARC& rhs)  // NOLINT(bugprone-unhandled-self-assignment)
    {
      if (this != &rhs)
      {
        reassign(rhs.m_Handle);
      }

      return *this;
    }

    ARC& operator=(ARC&& rhs) noexcept
    {
      m_Handle = std::exchange(rhs.m_Handle, m_Handle);

      return *this;
    }

    ARC& operator=(std::nullptr_t)
    {
      release();
      m_Handle = nullptr;

      return *this;
    }

    operator bool() const noexcept { return isValid(); }
    T&   operator*() const noexcept { return *m_Handle; }
    T*   operator->() const noexcept { return m_Handle; }
    bool operator==(const ARC& rhs) const noexcept { return m_Handle == rhs.m_Handle; }
    bool operator!=(const ARC& rhs) const noexcept { return m_Handle != rhs.m_Handle; }

    ~ARC()
    {
      release();
    }

    // IARCHandle Interface

    bool                     isValid() const noexcept override { return m_Handle != nullptr; }
    meta::BaseClassMetaInfo* typeInfo() const noexcept override { return meta::typeInfoGet<T>(); }

    void assign(IBaseAsset* asset) override
    {
      assert(!asset || asset->type() == typeInfo() && "Either must be assigning nullptr or the types must match.");

      reassign(asset);
    }

    IBaseAsset* handle() const noexcept override
    {
      return m_Handle;
    }

   private:
    void reassign(IBaseAsset* asset)
    {
      if (m_Handle != asset)
      {
        release();
        m_Handle = static_cast<T*>(asset);
        acquire();
      }
    }

    void acquire() const
    {
      if (m_Handle)
      {
        m_Handle->acquire();
      }
    }

    // NOTE(SR):
    //   This function does not set 'm_Handle' to nullptr.
    //   That is because it is a useless assignment in the case of the copy assignment.
    void release() const
    {
      if (m_Handle)
      {
        m_Handle->release();
      }
    }
  };
}  // namespace bf

#endif /* BF_BASE_ASSET_HPP */

/******************************************************************************/
/*
  MIT License

  Copyright (c) 2020 Shareef Abdoul-Raheem

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
/******************************************************************************/