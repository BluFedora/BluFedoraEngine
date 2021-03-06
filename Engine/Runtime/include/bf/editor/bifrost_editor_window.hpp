/*!
 * @file   bifrost_editor_window.hpp
 * @author Shareef Abdoul-Raheem (https://blufedora.github.io/)
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

#include "bf/PlatformFwd.h"                   // bfEvent
#include "bf/bf_non_copy_move.hpp"            // bfNonCopyMoveable<T>
#include "bf/data_structures/bf_variant.hpp"  // Variant<Ts...>

#include <imgui/imgui.h> /* ImGuiID */

namespace bf
{
  using Event = struct ::bfEvent;
  class IMemoryManager;
  class Entity;
  class IBaseObject;
}  // namespace bf

namespace bf::editor
{
  class EditorOverlay;

  using Selectable = Variant<IBaseAsset*, Entity*>;

  // TODO(SR):
  //   The title ID stuff could be more efficient since it assumes changing titles but most likely case is that the title string stays the same...

  //
  // This unique type ID system may not work across dll boundaries.
  // The solution works for me right now since I do not plan
  // on having EditorWindows across dlls but that would be a
  // thing to remember if I make a change to this system...
  //

  using EditorWindowID = int;

  class BaseEditorWindow : private NonCopyMoveable<BaseEditorWindow>
  {
   protected:
    static EditorWindowID s_TypeIDCounter;

   private:
    bool    m_IsOpen;
    bool    m_IsFocused;
    bool    m_IsVisible;
    ImGuiID m_DockID;
    int     m_InstanceID;

   protected:
    BaseEditorWindow();

   public:
    bool                   isOpen() const { return m_IsOpen; }
    bool                   isFocused() const { return m_IsFocused; }
    bool                   isVisible() const { return m_IsVisible; }
    const char*            fullImGuiTitle(IMemoryManager& memory) const;
    void                   handleEvent(EditorOverlay& editor, Event& event);
    void                   update(EditorOverlay& editor, float delta_time);
    void                   uiShow(EditorOverlay& editor);
    virtual EditorWindowID windowID() const = 0;

    virtual void onCreate(EditorOverlay& editor) {}
    virtual void onDraw(EditorOverlay& editor, Engine& engine, RenderView& camera, float alpha) {}
    virtual void onDestroy(EditorOverlay& editor) {}

    virtual ~BaseEditorWindow() = default;

   protected:
    virtual const char* title() const = 0;
    virtual void        onPreDrawGUI(EditorOverlay& editor) {}
    virtual void        onDrawGUI(EditorOverlay& editor) {}
    virtual void        onPostDrawGUI(EditorOverlay& editor) {}
    virtual void        onEvent(EditorOverlay& editor, Event& event) {}
    virtual void        onUpdate(EditorOverlay& editor, float dt) {}
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
}  // namespace bf::editor

#endif /* BIFROST_EDITOR_WINDOW_HPP */
