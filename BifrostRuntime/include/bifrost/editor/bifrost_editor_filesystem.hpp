/******************************************************************************/
/*!
* @file   bifrost_editor_filesystem.hpp
* @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
* @brief
*   Filesystem managment for the editor.
*
* @version 0.0.1
* @date    2020-03-09
*
* @copyright Copyright (c) 2020
*/
/******************************************************************************/
#ifndef BIFROST_EDITOR_FILESYSTEM_HPP
#define BIFROST_EDITOR_FILESYSTEM_HPP

#include "bifrost/data_structures/bifrost_intrusive_list.hpp"  // Node
#include "bifrost/data_structures/bifrost_string.hpp"          // String

namespace bifrost::editor
{
  class EditorOverlay;

  struct FileEntry final
  {
    String                         name;  // TODO: Make Name Just a StringRange into 'FileEntry::full_path'
    String                         full_path;
    StringRange                    file_extension;  //!< Backed by FileEntry::full_path.
    bool                           is_file;
    BifrostUUID                    uuid;
    intrusive::ListView<FileEntry> children;
    intrusive::Node<FileEntry>     next;

   public:
    FileEntry(String&& name, const String& full_path, bool is_file);
  };

  class FileSystem final : bfNonCopyMoveable<FileSystem>
  {
   private:
    IMemoryManager&   m_Memory;
    Array<FileEntry*> m_AllNodes;
    FileEntry*        m_Root;
    FileEntry*        m_RenamedNode;
    bool              m_HasBeenModified;

   public:
    explicit FileSystem(IMemoryManager& memory);

    FileEntry& root() const { return *m_Root; }

    void        clear(String&& name, const String& path);
    FileEntry&  makeNode(String&& name, const String& path, bool is_file);
    StringRange relativePath(const FileEntry& entry) const;
    void        rename(EditorOverlay& editor, FileEntry& entry, const StringRange& new_name) const;
    void        remove(FileEntry& entry);
    void        uiShow(EditorOverlay& editor);

    ~FileSystem();

   private:
    void uiShowImpl(EditorOverlay& editor, FileEntry& entry);
    void clearImpl();
  };
}  // namespace bifrost::editor

#endif /* BIFROST_EDITOR_FILESYSTEM_HPP */