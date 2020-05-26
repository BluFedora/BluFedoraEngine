/******************************************************************************/
/*!
* @file   bifrost_editor_overlay.hpp
* @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
* @brief
*
* @version 0.0.1
* @date    2020-03-16
*
* @copyright Copyright (c) 2020
*/
/******************************************************************************/
#ifndef BIFROST_EDITOR_OVERLAY_HPP
#define BIFROST_EDITOR_OVERLAY_HPP

#include "bifrost/asset_io/bifrost_material.hpp"
#include "bifrost/bifrost.hpp"
#include "bifrost/math/bifrost_rect2.hpp"
#include "bifrost_editor_filesystem.hpp"
#include "bifrost_editor_inspector.hpp"

#include <memory> /* unique_ptr<T> */

namespace bifrost::editor
{
  class EditorOverlay;

  // Memory

  IMemoryManager& allocator();

  template<typename T, typename... Args>
  T* make(Args&&... args)
  {
    return allocator().allocateT<T>(std::forward<Args&&>(args)...);
  }

  template<typename T>
  void deallocateT(T* ptr)
  {
    allocator().deallocateT(ptr);
  }

  template<typename T>
  using UniquePtr = std::unique_ptr<T, meta::function_caller<&deallocateT<T>>>;

  // StringPool

  class StringPool;

  struct StringPoolRef final
  {
    StringPool* pool;
    std::size_t entry_idx;

    StringPoolRef() :
      pool{nullptr},
      entry_idx{k_StringNPos}
    {
    }

    StringPoolRef(const StringPoolRef& rhs) noexcept;
    StringPoolRef(StringPoolRef&& rhs) noexcept;

    StringPoolRef& operator=(const StringPoolRef& rhs) noexcept;
    StringPoolRef& operator=(StringPoolRef&& rhs) noexcept;

    const char* string() const noexcept;
    std::size_t length() const noexcept;

    void clear() noexcept;

    ~StringPoolRef() noexcept;

   private:
    friend class StringPool;

    StringPoolRef(StringPool* pool, std::size_t entry) :
      pool{pool},
      entry_idx{entry}
    {
    }
  };

  class StringPool final
  {
    friend struct StringPoolRef;

   private:
    struct StringPoolEntry final
    {
      union
      {
        struct
        {
          StringRange  data;  // Really a nul terminated string, I just want to keep track of the size.
          unsigned int ref_count;
        };

        std::size_t free_list_next;
      };

      // clang-format off
      // ReSharper disable CppNonExplicitConvertingConstructor
      StringPoolEntry(StringRange data) : // NOLINT(hicpp-member-init)
        data{data},
        ref_count{1}
      {
      }
      // ReSharper restore CppNonExplicitConvertingConstructor
      // clang-format on
    };

   private:
    Array<StringPoolEntry>                   m_EntryStorage;
    HashTable<StringRange, StringPoolEntry*> m_Table;
    std::size_t                              m_EntryStorageFreeList;

   public:
    explicit StringPool(IMemoryManager& memory) :
      m_EntryStorage{memory},
      m_Table{},
      m_EntryStorageFreeList{k_StringNPos}
    {
    }

    StringPoolRef intern(const StringRange& string)
    {
      auto it = m_Table.find(string);

      if (it == m_Table.end())
      {
        StringPoolEntry* const new_entry = grabNewEntry(string);

        it = m_Table.insert(new_entry->data, new_entry);
      }
      else
      {
        ++it->value()->ref_count;
      }

      return StringPoolRef{this, m_EntryStorage.indexOf(it->value())};
    }

   private:
    StringPoolEntry* grabNewEntry(const StringRange& string)
    {
      StringRange      cloned_data = string_utils::clone(m_EntryStorage.memory(), string);
      StringPoolEntry* result;

      if (m_EntryStorageFreeList != k_StringNPos)
      {
        result = &m_EntryStorage[m_EntryStorageFreeList];

        result->data      = cloned_data;
        result->ref_count = 1;

        m_EntryStorageFreeList = m_EntryStorage[m_EntryStorageFreeList].free_list_next;
      }
      else
      {
        result = &m_EntryStorage.emplace(cloned_data);
      }

      return result;
    }
  };

  struct ActionContext final
  {
    EditorOverlay* editor;

    bool actionButton(const char* name) const;
  };

  class Action
  {
   public:
    virtual void execute(const ActionContext& ctx) = 0;

    virtual bool isActive(const ActionContext& ctx) const
    {
      return true;
    }

    virtual ~Action() = default;
  };

  namespace ui
  {
    class Dialog
    {
     protected:
      bool        m_WantsToClose;
      const char* m_Name;

     public:
      explicit Dialog(const char* name) :
        m_WantsToClose{false},
        m_Name{name}
      {
      }

      [[nodiscard]] bool        wantsToClose() const { return m_WantsToClose; }
      [[nodiscard]] const char* name() const { return m_Name; }

      virtual void show(const ActionContext& ctx) = 0;

      void close()
      {
        m_WantsToClose = true;
      }

      virtual ~Dialog() = default;
    };

    class BaseMenuItem
    {
     private:
      StringPoolRef m_Name;

     protected:
      explicit BaseMenuItem(const StringPoolRef& name) :
        m_Name{name}
      {
      }

     public:
      virtual bool beginItem(const ActionContext& ctx) = 0;
      virtual void doAction(const ActionContext& ctx)  = 0;
      virtual void endItem()                           = 0;

      virtual ~BaseMenuItem() = default;
      [[nodiscard]] const StringPoolRef& name() const { return m_Name; }
    };

    class MenuDropdown : public BaseMenuItem
    {
     private:
      Array<BaseMenuItem*> m_SubItems;

     public:
      bool beginItem(const ActionContext& ctx) override;
      void doAction(const ActionContext& ctx) override;
      void endItem() override;

      MenuDropdown* findDropdown(const StringRange& name)
      {
        for (BaseMenuItem* item : m_SubItems)
        {
          if (name == item->name().string())
          {
            return static_cast<MenuDropdown*>(item);
          }
        }

        return nullptr;
      }

     public:
      MenuDropdown(const StringPoolRef& name, IMemoryManager& memory) :
        BaseMenuItem{name},
        m_SubItems{memory}
      {
      }

      MenuDropdown* addItem(BaseMenuItem* item);

      ~MenuDropdown() override = default;
    };

    class MainMenu final : public MenuDropdown
    {
     public:
      bool beginItem(const ActionContext& ctx) override;
      void endItem() override;

      MainMenu(const StringPoolRef& name, IMemoryManager& memory) :
        MenuDropdown{name, memory}
      {
      }
    };

    class MenuAction : public BaseMenuItem
    {
     public:
      bool beginItem(const ActionContext& ctx) override;
      void doAction(const ActionContext& ctx) override;
      void endItem() override;

     private:
      Action* m_Action;

     public:
      MenuAction(const StringPoolRef& name, Action* action) :
        BaseMenuItem{name},
        m_Action{action}
      {
      }

      ~MenuAction() override = default;
    };

  }  // namespace ui

  class Project final
  {
   private:
    String m_Name;
    String m_ProjectFilePath;
    String m_Path;  // TODO: Make it a StringRange of 'Project::m_ProjectFilePath'
    String m_MetaPath;

   public:
    explicit Project(String&& name, String&& project_file, String&& path, String&& meta_path) :
      m_Name{name},
      m_ProjectFilePath{project_file},
      m_Path{path},
      m_MetaPath{meta_path}
    {
    }

    String&       name() { return m_Name; }
    String&       projectFilePath() { return m_ProjectFilePath; }
    const String& path() const { return m_Path; }
    const String& metaPath() const { return m_MetaPath; }
  };

  using ActionPtr           = UniquePtr<Action>;
  using ProjectPtr          = UniquePtr<Project>;
  using BaseEditorWindowPtr = UniquePtr<BaseEditorWindow>;

  class ARefreshAsset;

  using ActionMap  = HashTable<String, ActionPtr>;
  using WindowList = Array<BaseEditorWindowPtr>;

  class EditorOverlay final : public IGameStateLayer
  {
    friend class ARefreshAsset;

   private:
    ui::Dialog*        m_CurrentDialog;
    bool               m_OpenNewDialog;
    ActionMap          m_Actions;
    StringPool         m_MenuNameStringPool;
    ui::MainMenu       m_MainMenu;
    Engine*            m_Engine;
    ProjectPtr         m_OpenProject;
    float              m_FpsTimer;
    int                m_CurrentFps;
    int                m_CurrentMs;
    AssetTextureHandle m_TestTexture;
    FileSystem         m_FileSystem;
    Rect2i             m_SceneViewViewport;  // Global Window Coordinates
    bool               m_IsSceneViewHovered;
    Vec2f              m_MousePosition;  // TODO(SR): This should be stored in a shared Engine Input Module.
    WindowList         m_OpenWindows;

   protected:
    void onCreate(Engine& engine) override;
    void onLoad(Engine& engine) override;
    void onEvent(Engine& engine, Event& event) override;
    void onUpdate(Engine& engine, float delta_time) override;
    void onUnload(Engine& engine) override;
    void onDestroy(Engine& engine) override;

   public:
    EditorOverlay();

    const ProjectPtr& currentlyOpenProject() const { return m_OpenProject; }
    const char*       name() override { return "Bifrost Editor"; }
    Engine&           engine() const { return *m_Engine; }
    FileSystem&       fileSystem() { return m_FileSystem; }

    Action* findAction(const char* name) const;
    void    enqueueDialog(ui::Dialog* dlog);
    bool    openProjectDialog();
    bool    openProject(StringRange path);
    void    saveProject();
    void    closeProject();
    void    assetRefresh();
    void    viewAddInspector();
    bool    isPointOverSceneView(const Vector2i& point) const;

    template<typename T, typename... Args>
    T& getWindow(Args&&... args)
    {
      const EditorWindowID type_id = T::typeID();

      for (BaseEditorWindowPtr& window : m_OpenWindows)
      {
        if (type_id == window->windowID())
        {
          return *reinterpret_cast<T*>(window.get());
        }
      }

      return addWindow<T>(std::forward<decltype(args)>(args)...);
    }

    template<typename T, typename... Args>
    T& addWindow(Args&&... args)
    {
      T* window = allocator().allocateT<T>(std::forward<decltype(args)>(args)...);

      m_OpenWindows.emplace(window);

      return *window;
    }

    template<typename T>
    void select(T&& selectable)
    {
      for (BaseEditorWindowPtr& window : m_OpenWindows)
      {
        window->selectionChange(selectable);
      }
    }

   private:
    void buttonAction(const ActionContext& ctx, const char* action_name) const;
    void buttonAction(const ActionContext& ctx, const char* action_name, const char* custom_label, const ImVec2& size = ImVec2(0.0f, 0.0f)) const;
    void selectableAction(const ActionContext& ctx, const char* action_name) const;
    void selectableAction(const ActionContext& ctx, const char* action_name, const char* custom_label) const;
    void addMenuItem(const StringRange& menu_path, const char* action_name);
  };
}  // namespace bifrost::editor

#endif /* BIFROST_EDITOR_OVERLAY_HPP */
