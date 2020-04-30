/*!
 * @file   bifrost_editor_window.hpp
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   This is the base interface for all windows the editor has.
 *   If you want to extend the editor this is the place to start with.
 *
 * @version 0.0.1
 * @date    25-04-2020
 *
 * @copyright Copyright (c) 2020
 */
#ifndef BIFROST_EDITOR_WINDOW_HPP
#define BIFROST_EDITOR_WINDOW_HPP

#include "bifrost/asset_io/bifrost_asset_handle.hpp"    // BaseAssetHandle
#include "bifrost/data_structures/bifrost_variant.hpp"  // Variant<Ts...>
#include "bifrost/utility/bifrost_non_copy_move.hpp"    // bfNonCopyMoveable<T>

#include <imgui/imgui.h>

namespace bifrost
{
  class IMemoryManager;
  class Entity;
  class IBaseObject;
}  // namespace bifrost

namespace bifrost::editor
{
  class EditorOverlay;

  using Selectable = Variant<IBaseObject*, Entity*, BaseAssetHandle>;

  // TODO(SR): The title ID stuff could be more efficient since it assumes changing titles but most likely case is that the title string stays the same...

  //
  // This unique type ID system may not work across dll boundaries.
  // This solution works for me right now since I do not plan
  // on having EditorWindows across dlls but that would be a
  // thing to remeber to change this system...
  //

  using EditorWindowID = int;

  class BaseEditorWindow : private bfNonCopyMoveable<BaseEditorWindow>
  {
   protected:
    static EditorWindowID s_TypeIDCounter;

   private:
    bool    m_IsOpen;
    ImGuiID m_DockID;
    int     m_InstanceID;

   protected:
    BaseEditorWindow();

   public:
    bool                   isOpen() const { return m_IsOpen; }
    const char*            fullImGuiTitle(IMemoryManager& memory) const;
    void                   uiShow(EditorOverlay& editor);
    void                   selectionChange(const Selectable& selectable);
    virtual EditorWindowID windowID() const = 0;

    virtual ~BaseEditorWindow() = default;

   protected:
    virtual const char* title() const = 0;
    virtual void        onDrawGUI(EditorOverlay& editor) {}
    virtual void        onSelectionChanged(const Selectable& selectable) {}
  };

  template<typename T>
  class EditorWindow : public BaseEditorWindow
  {
   public:
    static EditorWindowID typeID()
    {
      static EditorWindowID s_ID = BaseEditorWindow::s_TypeIDCounter++;
      return s_ID;
    }

    EditorWindowID windowID() const override
    {
      return typeID();
    }
  };
}  // namespace bifrost::editor

#endif /* BIFROST_EDITOR_WINDOW_HPP */