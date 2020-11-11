#ifndef BIFROST_EDITOR_SELECTION_HPP
#define BIFROST_EDITOR_SELECTION_HPP

#include "bf/bf_function_view.hpp"    // FunctionView<R(Args...)>
#include "bifrost_editor_window.hpp"  // Selectable

namespace bf::editor
{
  class Selection;

  using SelectionOnChangeFn = FunctionView<void(Selection&)>;

  class Selection final
  {
   private:
    Array<Selectable>          m_Selectables;
    Array<SelectionOnChangeFn> m_OnChangeCallbacks;

   public:
    explicit Selection(IMemoryManager& memory);

    const Array<Selectable>& selectables() const { return m_Selectables; }

    template<typename T>
    bool hasType() const
    {
      for (const Selectable& selectable : m_Selectables)
      {
        if (selectable.is<T>())
        {
          return true;
        }
      }

      return false;
    }

    template<typename T, typename F>
    void firstOfType(F&& callback) const
    {
      for (const Selectable& selectable : m_Selectables)
      {
        if (selectable.is<T>())
        {
          callback(selectable.as<T>());
          break;
        }
      }
    }

    template<typename T, typename F>
    void lastOfType(F&& callback) const
    {
      for (const Selectable& selectable : ReverseLoop(m_Selectables))
      {
        if (selectable.is<T>())
        {
          callback(selectable.as<T>());
          break;
        }
      }
    }

    template<typename T, typename F>
    void forEachOfType(F&& callback) const
    {
      for (const Selectable& selectable : m_Selectables)
      {
        if (selectable.is<T>())
        {
          callback(selectable.as<T>());
        }
      }
    }

    bool contains(const Selectable& object);
    void select(const Selectable& object);
    void deselect(const Selectable& object);
    void clear();

    void addOnChangeListener(const SelectionOnChangeFn& callback);
    void removeOnChangeListener(const SelectionOnChangeFn& callback);

   private:
    bool find(const Selectable& object, std::size_t& out_index);
    bool findListener(const SelectionOnChangeFn& callback, std::size_t& out_index);
    void notifyOnChange();
  };
}  // namespace bf::editor

#endif /* BIFROST_EDITOR_SELECTION_HPP */