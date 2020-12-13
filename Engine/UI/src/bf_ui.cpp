//
// Shareef Abdoul-Raheem
//
// References:
//   [https://www.youtube.com/watch?v=Z1qyvQsjK5Y]
//   [https://www.youtube.com/watch?v=UUfXWzp0-DU]
//   [https://mortoray.com/topics/writing-a-ui-engine/]
//
#include "bf/bf_ui.hpp"

#include "bf/FreeListAllocator.hpp"
#include "bf/Platform.h"
#include "bf/Text.hpp"
#include "bf/bf_hash.hpp"

#include <algorithm>
#include <array>

namespace bf
{
  template<typename Key, typename T>
  struct SortedArrayTable
  {
    struct ArrayItem
    {
      Key key;
      T   value;
    };

    Array<ArrayItem> table;

    SortedArrayTable(IMemoryManager& memory) :
      table{memory}
    {
    }

    void insert(Key key, const T& value)
    {
      const auto it = std::lower_bound(
       table.begin(), table.end(), key, [](const ArrayItem& b, const Key& key) {
         return b.key < key;
       });

      if (it == table.end() || it->key != key)
      {
        table.insert(it, ArrayItem{key, value});
      }
      else
      {
        it->value = value;
      }
    }

    T find(Key key) const
    {
      const auto it = std::lower_bound(
       table.begin(), table.end(), key, [](const ArrayItem& b, const Key& key) {
         return b.key < key;
       });

      if (it == table.end() || it->key != key)
      {
        return nullptr;
      }

      return it->value;
    }

    // TODO(SR): Add remove when need be.
  };

  struct UIContext
  {
    static constexpr int k_WidgetMemorySize = bfMegabytes(10);

    // Input State

    Vector2f      mouse_pos       = {0.0f, 0.0f};
    bfButtonFlags old_mouse_state = 0x0;
    bfButtonFlags new_mouse_state = 0x0;
    float         delta_time      = 0.0f;

    // Widget Memory

    std::array<char, k_WidgetMemorySize>   widget_freelist_backing = {'\0'};
    FreeListAllocator                      widget_freelist         = {widget_freelist_backing.data(), widget_freelist_backing.size()};
    SortedArrayTable<UIElementID, Widget*> widgets                 = {widget_freelist};

    // State Tracking

    Array<UIElementID> id_stack         = Array<UIElementID>{widget_freelist};
    Array<Widget*>     root_widgets     = Array<Widget*>{widget_freelist};
    Array<Widget*>     root_widgets_old = Array<Widget*>{widget_freelist};
    Widget*            current_widget   = nullptr;

    // Interaction

    std::uint32_t next_zindex     = 1u;
    Widget*       next_hover_root = nullptr;
    Widget*       hovered_widgets = nullptr;
    const Widget* hot_widget      = nullptr;
    const Widget* active_widget   = nullptr;
    Vector2f      drag_offset     = {0.0f, 0.0f};  // MousePos - WidgetPos
  };

}  // namespace bf

namespace bf::UI
{
  static constexpr float k_Inf = FLT_MAX;

  static UIContext g_UI = {};

  static IMemoryManager& CurrentAllocator()
  {
    return g_UI.widget_freelist;
  }

  static PainterFont* const TEST_FONT = new PainterFont(CurrentAllocator(), "assets/fonts/Abel.ttf", 22.0f);

  static void BringToFront(Widget* widget)
  {
    widget->zindex = ++g_UI.next_zindex;
  }

  static bool IsFocusedWindow(const Widget* widget)
  {
    return !g_UI.root_widgets.isEmpty() && g_UI.root_widgets.back() == widget;
  }

  static Rect2f WidgetBounds(const Widget* widget)
  {
    return Rect2f(
     widget->position_from_parent,
     widget->position_from_parent + widget->realized_size);
  }

  static void SetZIndexContainer(Widget* widget)
  {
    if (WidgetBounds(widget).intersects(g_UI.mouse_pos) &&
        (!g_UI.next_hover_root || g_UI.next_hover_root->zindex <= widget->zindex))
    {
      g_UI.next_hover_root = widget;
    }
  }

  static float& WidgetParam(Widget* widget, WidgetParams param)
  {
    return widget->params[int(param)];
  }

  static float MutateFloat(float value, float delta)
  {
    if (value != k_Inf)
    {
      value += delta;
    }

    return value;
  }

  static float RealizeSizeUnit(const SizeUnit& su, float parent_size, float flex_size)
  {
    // clang-format off
    switch (su.type)
    {
      case SizeUnitType::Absolute: return su.value;
      case SizeUnitType::Relative: return parent_size * su.value;
      case SizeUnitType::Flex:     return flex_size;
      bfInvalidDefaultCase();
    }
    // clang-format on

    return {0.0f};
  }

  //
  // Small little article on the words Actualize vs Realize:
  //   [https://cohering.net/blog/2010/09/realization_vs_actualization.html]
  // I could be using the wrong one here but the differnce is subtle and
  // not of much importance since there is no gramatical incorrectness.
  //
  // Honorabled Mentions: "crystalize" and "materialize". :)
  //

  static Vector2f RealizeSize(const Widget* widget, const LayoutConstraints& constraints)
  {
    assert(widget->parent && "Only windows have no parent and they do not call this function.");

    const Vector2f parent_size = widget->parent->realized_size;
    const Size&    size        = widget->desired_size;

    return {
     RealizeSizeUnit(size.width, parent_size.x, constraints.max_size.x),
     RealizeSizeUnit(size.height, parent_size.y, constraints.max_size.y),
    };
  }

  static LayoutOutput WidgetDoLayout(Widget* widget, const LayoutConstraints& constraints)
  {
    const auto&  layout = widget->layout;
    LayoutOutput layout_result;

    switch (layout.type)
    {
        // Single Child Layouts

      case LayoutType::Default:
      {
        widget->ForEachChild([&constraints](Widget* child) {
          WidgetDoLayout(child, constraints);
        });

        layout_result.desired_size = RealizeSize(widget, constraints);
        break;
      }
      case LayoutType::Padding:
      {
        const float padding = 10.0f;  // TODO(SR): Real padding value.

        LayoutConstraints child_constraints;

        child_constraints.max_size.x = MutateFloat(constraints.max_size.x, -padding * 2.0f);
        child_constraints.max_size.y = MutateFloat(constraints.max_size.y, -padding * 2.0f);
        child_constraints.min_size   = vec::min(constraints.min_size, child_constraints.max_size);

        Vector2f max_child_size = child_constraints.min_size;

        widget->ForEachChild([&max_child_size, &child_constraints](Widget* child) {
          const auto child_layout = WidgetDoLayout(child, child_constraints);

          max_child_size = vec::max(max_child_size, child_layout.desired_size);
        });

        layout_result.desired_size = max_child_size + Vector2f{padding * 2.0f};
        break;
      }
      case LayoutType::Fixed:
      {
        const float padding = 0.0f;

        LayoutConstraints child_constraints = constraints;

        child_constraints.min_size   = child_constraints.min_size;
        child_constraints.max_size.x = MutateFloat(constraints.max_size.x, -padding * 2.0f);
        child_constraints.max_size.y = MutateFloat(constraints.max_size.y, -padding * 2.0f);

        widget->ForEachChild([&layout_result, &child_constraints](Widget* child) {
          WidgetDoLayout(child, child_constraints);
        });

        layout_result.desired_size = constraints.max_size;
        break;
      }

        // Multi-Child Layouts

      case LayoutType::Row:
      {
        layout_result.desired_size.x = 0.0f;
        layout_result.desired_size.y = constraints.min_size.y;

        LayoutConstraints my_constraints = {};

        my_constraints.min_size   = constraints.min_size;
        my_constraints.max_size   = constraints.max_size;
        my_constraints.min_size.x = 0.0f;
        my_constraints.max_size.x = constraints.max_size.x;

        float total_flex_factor = 0.0f;

        widget->ForEachChild([&layout_result, &total_flex_factor, &my_constraints](Widget* child) {
          if (child->desired_size.width.type != SizeUnitType::Flex)
          {
            LayoutOutput child_size = WidgetDoLayout(child, my_constraints);

            layout_result.desired_size.x += child_size.desired_size.x;
            layout_result.desired_size.y = std::max(layout_result.desired_size.y, child_size.desired_size.y);
          }
          else
          {
            total_flex_factor += child->desired_size.width.value;
          }
        });

        if (total_flex_factor > 0.0f)
        {
          float flex_space_unit = std::max(constraints.max_size.x - layout_result.desired_size.x, 0.0f) / total_flex_factor;

          widget->ForEachChild([flex_space_unit, &constraints, &layout_result](Widget* child) {
            if (child->desired_size.width.type == SizeUnitType::Flex)
            {
              float             child_width      = flex_space_unit * child->desired_size.width.value;
              LayoutConstraints flex_constraints = {
               {child_width, 0.0f},
               {child_width, constraints.max_size.y},
              };
              LayoutOutput child_size = WidgetDoLayout(child, flex_constraints);

              layout_result.desired_size.x += child_size.desired_size.x;
              layout_result.desired_size.y = std::max(layout_result.desired_size.y, child_size.desired_size.y);

              if (child_size.desired_size.y == k_Inf)
              {
                child_size.desired_size.y = layout_result.desired_size.y;
              }
              else
              {
                layout_result.desired_size.y = std::max(layout_result.desired_size.y, child_size.desired_size.y);
              }

              child->realized_size = child_size.desired_size;
            }
          });
        }
        break;
      }
      case LayoutType::Column:
      {
        layout_result.desired_size.x = constraints.min_size.x;
        layout_result.desired_size.y = 0.0f;

        LayoutConstraints my_constraints = {};

        my_constraints.min_size   = constraints.min_size;
        my_constraints.max_size   = constraints.max_size;
        my_constraints.min_size.y = 0.0f;
        my_constraints.max_size.y = constraints.max_size.y;

        float total_flex_factor = 0.0f;

        widget->ForEachChild([&layout_result, &total_flex_factor, &my_constraints](Widget* child) {
          if (child->desired_size.height.type != SizeUnitType::Flex)
          {
            LayoutOutput child_size = WidgetDoLayout(child, my_constraints);

            layout_result.desired_size.x = std::max(layout_result.desired_size.x, child_size.desired_size.x);
            layout_result.desired_size.y += child_size.desired_size.y;
          }
          else
          {
            total_flex_factor += child->desired_size.height.value;
          }
        });

        if (total_flex_factor > 0.0f)
        {
          float flex_space_unit = std::max(constraints.max_size.y - layout_result.desired_size.y, 0.0f) / total_flex_factor;

          widget->ForEachChild([flex_space_unit, &constraints, &layout_result](Widget* child) {
            if (child->desired_size.height.type == SizeUnitType::Flex)
            {
              float             child_height     = flex_space_unit * child->desired_size.height.value;
              LayoutConstraints flex_constraints = {
               {0.0f, child_height},
               {constraints.max_size.x, child_height},
              };
              LayoutOutput child_size = WidgetDoLayout(child, flex_constraints);

              layout_result.desired_size.x = std::max(layout_result.desired_size.x, child_size.desired_size.x);
              layout_result.desired_size.y += child_size.desired_size.y;

              if (child_size.desired_size.x == k_Inf)
              {
                child_size.desired_size.x = layout_result.desired_size.x;
              }
              else
              {
                layout_result.desired_size.x = std::max(layout_result.desired_size.x, child_size.desired_size.x);
              }

              child->realized_size = child_size.desired_size;
            }
          });
        }
        break;
      }
      case LayoutType::Grid:
      {
        assert(false);

        break;
      }
      case LayoutType::Stack:
      {
        assert(false);
        break;
      }
      case LayoutType::Custom:
      {
        layout_result = layout.custom.layout(widget, constraints);
        break;
      }

        bfInvalidDefaultCase();
    }

    widget->realized_size = layout_result.desired_size;

    return layout_result;
  }

  //
  // Widget Positioning is Separate From the Layout
  // Since Positioning requires knowledge of one own
  // `Widget::position_from_parent` to be relative to.
  //
  // When you do it within the `WidgetDoLayout` function
  // there is a noticable frame delay of the children not
  // keeping up with parents when quick motion happens.
  //

  static void WidgetDoLayoutPositioning(Widget* widget)
  {
    const auto& layout = widget->layout;

    switch (layout.type)
    {
        // Single Child Layouts

      case LayoutType::Default:
      {
        break;
      }
      case LayoutType::Padding:
      {
        const float padding = 10.0f;  // TODO(SR): Real padding value.

        widget->ForEachChild([widget, position_offset = Vector2f{padding}](Widget* child) {
          child->position_from_parent = widget->position_from_parent + position_offset;
          WidgetDoLayoutPositioning(child);
        });
        break;
      }
      case LayoutType::Fixed:
      {
        const float padding = 0.0f;

        widget->ForEachChild([widget, position_offset = Vector2f{padding}](Widget* child) {
          child->position_from_parent = widget->position_from_parent + position_offset;
          WidgetDoLayoutPositioning(child);
        });
        break;
      }

        // Multi-Child Layouts

      case LayoutType::Row:
      {
        float current_x = widget->position_from_parent.x;

        widget->ForEachChild([&current_x, widget](Widget* child) {
          child->position_from_parent.x = current_x;
          child->position_from_parent.y = widget->position_from_parent.y;
          WidgetDoLayoutPositioning(child);

          current_x += child->realized_size.x;
        });
        break;
      }
      case LayoutType::Column:
      {
        float current_y = widget->position_from_parent.y;

        widget->ForEachChild([&current_y, widget](Widget* child) {
          child->position_from_parent.x = widget->position_from_parent.x;
          child->position_from_parent.y = current_y;
          WidgetDoLayoutPositioning(child);

          current_y += child->realized_size.y;
        });
        break;
      }
      case LayoutType::Grid:
      {
        assert(false);

        break;
      }
      case LayoutType::Stack:
      {
        assert(false);
        break;
      }
      case LayoutType::Custom:
      {
        layout.custom.position_children(widget);
        break;
      }

        bfInvalidDefaultCase();
    }
  }

  static void WidgetDoRender(Widget* self, Gfx2DPainter& painter)
  {
    self->render(self, painter);
  }

  static void DefaultRender(Widget* self, Gfx2DPainter& painter)
  {
    if (self->flags & Widget::IsWindow)
    {
      if (IsFocusedWindow(self))
      {
        painter.pushRectShadow(10.0f, self->position_from_parent, self->realized_size.x, self->realized_size.y, 4.0f, BIFROST_COLOR_BEIGE);
      }

      if (self->flags & Widget::DrawBackground)
      {
        painter.pushRect(
         self->position_from_parent,
         self->realized_size.x,
         self->realized_size.y,
         BIFROST_COLOR_BURLYWOOD);

        painter.pushRect(
         self->position_from_parent + Vector2f{1.0f},
         self->realized_size.x - 2.0f,
         self->realized_size.y - 2.0f,
         BIFROST_COLOR_BROWN);
      }
      else
      {
        painter.pushRect(
         self->position_from_parent,
         self->realized_size.x,
         self->realized_size.y,
         BIFROST_COLOR_FLORALWHITE);
      }
    }

    if (self->flags & Widget::DrawName)
    {
      painter.pushText(
       self->position_from_parent + Vector2f{1.0f, 16.0f},
       self->name,
       TEST_FONT);
    }

    self->ForEachChild([&painter](Widget* const child) {
      WidgetDoRender(child, painter);
    });
  }

  static UIElementID CalcID(StringRange name)
  {
    const auto hash_seed    = g_UI.id_stack.back();
    const auto current_hash = hash::addBytes(hash_seed, name.begin(), name.length());

    return current_hash;
  }

  static UIElementID CalcID(UIElementID local_id)
  {
    const auto hash_seed    = g_UI.id_stack.back();
    const auto current_hash = hash::addU64(hash_seed, local_id);

    return current_hash;
  }

  static void PushWidget(Widget* widget)
  {
    if (g_UI.current_widget)
    {
      g_UI.current_widget->AddChild(widget);
    }

    g_UI.current_widget = widget;
  }

  static Widget* CreateWidget(StringRange name, LayoutType layout_type = LayoutType::Default)
  {
    const auto id     = CalcID(name);
    Widget*    widget = g_UI.widgets.find(id);

    if (!widget)
    {
      const std::size_t name_buffer_len = name.length();
      char* const       name_buffer     = (char*)CurrentAllocator().allocate(name_buffer_len + 1u);

      std::strncpy(name_buffer, name.begin(), name_buffer_len);
      name_buffer[name_buffer_len] = '\0';

      widget = CurrentAllocator().allocateT<Widget>();

      widget->layout.type = layout_type;
      widget->name        = name_buffer;
      widget->name_len    = name_buffer_len;
      widget->render      = &DefaultRender;
      widget->hash        = id;

      g_UI.widgets.insert(id, widget);
    }

    assert(widget->hash == id);

    widget->Reset();

    return widget;
  }

  struct WidgetBehaviorResult
  {
    enum
    {
      IsClicked = (1 << 0),
      IsHovered = (1 << 1),
      IsActive  = (1 << 2),
      IsPressed = (1 << 3),
    };

    std::uint8_t flags = 0x0;

    bool Is(std::uint8_t f) const
    {
      return flags & f;
    }
  };

  static bool ClickedDownThisFrame(bfButtonFlags buttons)
  {
    const bool is_mouse_down_old = g_UI.old_mouse_state & buttons;
    const bool is_mouse_down_new = g_UI.new_mouse_state & buttons;

    return (is_mouse_down_old != is_mouse_down_new) && is_mouse_down_new;
  }

  static bool IsActiveWidget(const Widget* widget)
  {
    return g_UI.active_widget == widget;
  }

  static void WidgetsUnderPointHelper(Widget* widget, Widget*& result_list, const Vector2f& point)
  {
    const auto bounds = WidgetBounds(widget);

    if (bounds.intersects(point))
    {
      if (widget->flags & (Widget::Clickable | Widget::BlocksInput))
      {
        widget->hit_test_list = result_list;
        result_list           = widget;
      }

      widget->ForEachChild([&](Widget* child) {
        WidgetsUnderPointHelper(child, result_list, point);
      });
    }
  }

  static Widget* WidgetsUnderPoint(const Vector2f& point)
  {
    Widget* result = nullptr;

    for (Widget* window : g_UI.root_widgets_old)
    {
      WidgetsUnderPointHelper(window, result, point);
    }

    return result;
  }

  static WidgetBehaviorResult WidgetBehavior(const Widget* widget)
  {
    WidgetBehaviorResult result          = {};
    const bool           button_released = !(g_UI.new_mouse_state & BIFROST_BUTTON_LEFT) &&
                                 (g_UI.old_mouse_state & BIFROST_BUTTON_LEFT);

    if (g_UI.hovered_widgets == widget)
    {
      result.flags |= WidgetBehaviorResult::IsHovered;

      if (g_UI.new_mouse_state & BIFROST_BUTTON_LEFT)
      {
        result.flags |= WidgetBehaviorResult::IsPressed;
      }
    }

    if (widget->flags & Widget::Clickable)
    {
      if (result.Is(WidgetBehaviorResult::IsHovered))
      {
        if (button_released && IsActiveWidget(widget))
        {
          result.flags |= WidgetBehaviorResult::IsClicked;
        }

        if (ClickedDownThisFrame(BIFROST_BUTTON_LEFT))
        {
          g_UI.active_widget = widget;
          g_UI.drag_offset   = g_UI.mouse_pos - widget->position_from_parent;
        }
      }

      if (g_UI.active_widget == widget && button_released)
      {
        g_UI.active_widget = nullptr;
      }
    }

    if (IsActiveWidget(widget))
    {
      result.flags |= WidgetBehaviorResult::IsActive;
    }

    return result;
  }

  UIElementID PushID(UIElementID local_id)
  {
    const auto current_hash = CalcID(local_id);

    g_UI.id_stack.push(current_hash);

    return current_hash;
  }

  UIElementID PushID(StringRange string_value)
  {
    const auto current_hash = CalcID(string_value);

    g_UI.id_stack.push(current_hash);

    return current_hash;
  }

  void PopID()
  {
    g_UI.id_stack.pop();
  }

  static Widget* CreateButton(const char* name, const Size& size)
  {
    Widget* const button = CreateWidget(name);

    button->desired_size = size;
    button->flags |= Widget::DrawName | Widget::Clickable;

    return button;
  }

  bool BeginWindow(const char* title)
  {
    Widget* const window = CreateWidget(title, LayoutType::Column);

    window->flags |= Widget::BlocksInput | Widget::IsWindow;

    g_UI.root_widgets.push(window);

    SetZIndexContainer(window);

    PushID(window->hash);

    PushWidget(window);

    Widget* const titlebar = CreateWidget("__WindowTitlebar__", LayoutType::Row);

    titlebar->desired_size.width  = {SizeUnitType::Flex, 1.0f};
    titlebar->desired_size.height = {SizeUnitType::Absolute, 32.0f};
    titlebar->flags |= Widget::Clickable;

    const auto behavior = WidgetBehavior(titlebar);

    if (behavior.Is(WidgetBehaviorResult::IsActive))
    {
      window->position_from_parent = g_UI.mouse_pos - g_UI.drag_offset;
    }

    PushWidget(titlebar);
    {
      Widget* const title_spacing = CreateWidget(title);

      title_spacing->desired_size.width  = {SizeUnitType::Flex, 1.0f};
      title_spacing->desired_size.height = titlebar->desired_size.height;
      title_spacing->flags |= Widget::DrawName;

      PushWidget(title_spacing);
      PopWidget();

      Widget* const x_button = CreateButton(
       window->flags & Widget::IsExpanded ? "  --  " : "OPEN",
       {titlebar->desired_size.height, titlebar->desired_size.height});

      x_button->flags |= Widget::DrawBackground | Widget::IsWindow;

      PushWidget(x_button);
      PopWidget();

      const auto behavior = WidgetBehavior(x_button);

      if (behavior.flags & WidgetBehaviorResult::IsClicked)
      {
        window->flags ^= Widget::IsExpanded;  // Toggle
      }
    }
    PopWidget();

    const bool is_expanded = window->flags & Widget::IsExpanded;

    if (!is_expanded)
    {
      EndWindow();
    }

    return is_expanded;
  }

  void EndWindow()
  {
    PopWidget();
    PopID();
  }

  bool Button(const char* name)
  {
    Widget* const button = CreateButton(
     name,
     {{SizeUnitType::Flex, 1.0f}, {SizeUnitType::Absolute, 20.0f}});

    button->flags |= Widget::DrawName | Widget::Clickable;

    const auto behavior = WidgetBehavior(button);

    PushWidget(button);
    PopWidget();

    if (behavior.Is(WidgetBehaviorResult::IsHovered))
    {
      WidgetParam(button, WidgetParams::HoverTime) += g_UI.delta_time;
    }
    else
    {
      WidgetParam(button, WidgetParams::HoverTime) -= g_UI.delta_time;

      if (WidgetParam(button, WidgetParams::HoverTime) < 0.0f)
      {
        WidgetParam(button, WidgetParams::HoverTime) = 0.0f;
      }
    }

    button->render = [](Widget* self, Gfx2DPainter& painter) {
      const float max_hover_time    = 0.3f;
      const float hover_lerp_factor = math::clamp(0.0f, WidgetParam(self, WidgetParams::HoverTime) / max_hover_time, 1.0f);

      const bfColor4u  normal_color = bfColor4u_fromUint32(BIFROST_COLOR_AQUAMARINE);
      const bfColor4u  hover_color  = bfColor4u_fromUint32(BIFROST_COLOR_CYAN);
      const bfColor32u final_color  = bfColor4u_toUint32(bfMathLerpColor4u(normal_color, hover_color, hover_lerp_factor));

      painter.pushRectShadow(3.0f, self->position_from_parent, self->realized_size.x, self->realized_size.y, 10.0f, final_color);
      painter.pushFillRoundedRect(self->position_from_parent, self->realized_size.x, self->realized_size.y, 10.0f, final_color);

      auto text_size = calculateTextSize(self->name, TEST_FONT);
      auto text_pos  = self->position_from_parent + Vector2f{(self->realized_size.x - text_size.x) * 0.5f, 16.0f};

      painter.pushText(
       text_pos,
       self->name,
       TEST_FONT);
    };

    return behavior.Is(WidgetBehaviorResult::IsClicked);
  }

  void PushColumn()
  {
    Widget* const widget = CreateWidget("__PushColumn__", LayoutType::Column);

    widget->desired_size.width  = {SizeUnitType::Flex, 1.0f};
    widget->desired_size.height = {SizeUnitType::Flex, 1.0f};

    PushWidget(widget);
  }

  void PushRow()
  {
    Widget* const widget = CreateWidget("__PushRow__", LayoutType::Row);

    widget->desired_size.width  = {SizeUnitType::Flex, 1.0f};
    widget->desired_size.height = {SizeUnitType::Flex, 1.0f};

    PushWidget(widget);
  }

  void PushFixedSize(SizeUnit width, SizeUnit height)
  {
    Widget* const widget = CreateWidget("__FixedSize__", LayoutType::Fixed);

    widget->desired_size = {width, height};

    PushWidget(widget);
  }

  void PushPadding(float value)
  {
    Widget* const padding = CreateWidget("__PADDING__", LayoutType::Padding);

    PushWidget(padding);
  }

  void PopWidget()
  {
    g_UI.current_widget = g_UI.current_widget->parent;
  }

  void Init()
  {
    g_UI.id_stack.push(0x0);  // Root ID Seed
  }

  void PumpEvents(bfEvent* event)
  {
    switch (event->type)
    {
      case BIFROST_EVT_ON_MOUSE_DOWN:
      case BIFROST_EVT_ON_MOUSE_UP:
      {
        g_UI.new_mouse_state = event->mouse.button_state;
        break;
      }
      case BIFROST_EVT_ON_MOUSE_MOVE:
      {
        const auto& mouse = event->mouse;

        g_UI.mouse_pos.x = float(mouse.x);
        g_UI.mouse_pos.y = float(mouse.y);

        break;
      }
    }
  }

  void BeginFrame()
  {
    g_UI.hovered_widgets = WidgetsUnderPoint(g_UI.mouse_pos);
  }

  void Update(float delta_time)
  {
    g_UI.delta_time = delta_time;
  }

  void Render(Gfx2DPainter& painter)
  {
    // Test Code

    if (BeginWindow("Test Window"))
    {
      g_UI.current_widget->flags |= Widget::DrawBackground;

      PushFixedSize({SizeUnitType::Flex, 1.0f}, {SizeUnitType::Flex, 1.0f});
      PushPadding(20.0f);
      if (Button("Hello"))
      {
        std::printf("\nHello was pressed.\n");
      }
      PopWidget();
      PopWidget();

      PushColumn();
      if (Button("Button 2"))
      {
        std::printf("\nButton2 was pressed.\n");
      }

      PushID("__FIXME__");
      PushFixedSize({SizeUnitType::Flex, 1.0f}, {SizeUnitType::Flex, 1.0f});
      PopWidget();
      PopID();

      PushID("__FIXME2__");
      PushPadding(5.0f);
      if (Button("Button 3"))
      {
        std::printf("\nButton3 was pressed.\n");
      }
      PopWidget();
      PopID();

      PopWidget();

      EndWindow();
    }

    if (BeginWindow("Test Window2"))
    {
      PushFixedSize({SizeUnitType::Flex, 1.0f}, {SizeUnitType::Flex, 1.0f});
      PushPadding(20.0f);
      if (Button("Hello22222222"))
      {
        std::printf("\nHelloffffffff was pressed.\n");
      }
      PopWidget();
      PopWidget();

      PushColumn();
      if (Button("Button 2222222222"))
      {
        std::printf("\nButton2ffffffff was pressed.\n");
      }

      PushID("__FIXME__");
      PushFixedSize({SizeUnitType::Flex, 1.0f}, {SizeUnitType::Flex, 1.0f});
      PopWidget();
      PopID();

      PushID("__FIXME2__");
      PushPadding(5.0f);
      if (Button("Button 32222222"))
      {
        std::printf("\nButton3fffffffff was pressed.\n");
      }
      PopWidget();
      PopID();

      PopWidget();

      EndWindow();
    }

    // Test End

    // Layout

    LayoutConstraints main_constraints = {
     {0.0f, 0.0f},
     //{std::min(g_UI.mouse_pos.x, 600.0f), std::min(g_UI.mouse_pos.y, 500.0f)},
     {600.0f, 500.0f},
    };

    std::stable_sort(
     g_UI.root_widgets.begin(),
     g_UI.root_widgets.end(),
     [](const Widget* a, const Widget* b) {
       return a->zindex < b->zindex;
     });

    //
    // TODO(SR): These two loops may be able to be comined since each window is independent.
    //

    for (Widget* const window : g_UI.root_widgets)
    {
      WidgetDoLayout(window, main_constraints);
      WidgetDoLayoutPositioning(window);
    }

    // Render

    for (Widget* const window : g_UI.root_widgets)
    {
      WidgetDoRender(window, painter);
    }

    auto     hw  = g_UI.hovered_widgets;
    Vector2f pos = g_UI.mouse_pos;

    while (hw)
    {
      painter.pushText(
       pos,
       hw->name,
       TEST_FONT);

      pos.y += fontNewlineHeight(TEST_FONT->font);

      hw = hw->hit_test_list;
    }

    if (ClickedDownThisFrame(BIFROST_BUTTON_LEFT) &&
        g_UI.next_hover_root &&
        g_UI.next_hover_root->zindex < g_UI.next_zindex)
    {
      BringToFront(g_UI.next_hover_root);
    }
    g_UI.next_hover_root = nullptr;

    // Reset Some State

    g_UI.root_widgets_old.clear();
    std::swap(g_UI.root_widgets, g_UI.root_widgets_old);

    g_UI.old_mouse_state = g_UI.new_mouse_state;
    g_UI.current_widget  = nullptr;
  }
}  // namespace bf::UI
