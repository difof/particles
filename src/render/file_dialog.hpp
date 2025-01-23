#ifndef __FILE_DIALOG_HPP
#define __FILE_DIALOG_HPP

#include <imgui.h>
#include <string>
#include <vector>

// Simple ImGui-based file dialog using tinydir. Not a full-featured dialog.
// Supports: full path, file name, going up/opening dir, creating dir,
// open/save, cancel.
class FileDialog {
  public:
    enum class Mode { Open, Save };

    struct Entry {
        std::string name;
        bool is_dir = false;
    };

  public:
    FileDialog() = default;

    void open(Mode mode, const std::string &title,
              const std::string &start_dir = "");
    // Returns true when the modal is closed this frame (either selected or
    // canceled)
    bool render();

    bool has_result() const { return m_has_result; }
    bool canceled() const { return m_canceled; }
    const std::string &selected_path() const { return m_selected_path; }

    void set_filename(const std::string &name) { m_file_name = name; }
    const std::string &current_dir() const { return m_current_dir; }

  private:
    void list_directory();
    void go_up_dir();
    void enter_dir(const std::string &name);
    void ensure_current_dir();

  private:
    bool m_open = false;
    Mode m_mode = Mode::Open;
    std::string m_title;
    std::string m_current_dir;
    std::vector<Entry> m_entries;
    std::string m_file_name;
    std::string m_new_dir_name;

    bool m_has_result = false;
    bool m_canceled = false;
    std::string m_selected_path;
};

#endif
