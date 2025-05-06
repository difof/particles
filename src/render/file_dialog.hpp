#pragma once

#include <imgui.h>
#include <string>
#include <vector>

/**
 * @brief Simple ImGui-based file dialog using tinydir
 *
 * Not a full-featured dialog. Supports: full path, file name, going up/opening
 * dir, creating dir, open/save, cancel.
 */
class FileDialog {

  public:
    /**
     * @brief Dialog operation mode
     */
    enum class Mode {
        Open, ///< Open file mode
        Save  ///< Save file mode
    };

    /**
     * @brief Directory entry information
     */
    struct Entry {
        std::string name;    ///< Entry name
        bool is_dir = false; ///< Whether this entry is a directory
    };

  public:
    /**
     * @brief Default constructor
     */
    FileDialog() = default;

    /**
     * @brief Open the file dialog
     * @param mode Dialog mode (Open or Save)
     * @param title Dialog window title
     * @param start_dir Starting directory path
     */
    void open(Mode mode, const std::string &title,
              const std::string &start_dir = "");

    /**
     * @brief Render the dialog and return true when closed
     * @return true when the modal is closed this frame (either selected or
     * canceled)
     */
    bool render();

    /**
     * @brief Check if dialog has a result
     * @return true if dialog has completed with a result
     */
    bool has_result() const { return m_has_result; }

    /**
     * @brief Check if dialog was canceled
     * @return true if dialog was canceled
     */
    bool canceled() const { return m_canceled; }

    /**
     * @brief Get the selected file path
     * @return selected file path
     */
    const std::string &selected_path() const { return m_selected_path; }

    /**
     * @brief Set the filename
     * @param name filename to set
     */
    void set_filename(const std::string &name) { m_file_name = name; }

    /**
     * @brief Get current directory
     * @return current directory path
     */
    const std::string &current_dir() const { return m_current_dir; }

  protected:
    /**
     * @brief List directory contents
     */
    void list_directory();

    /**
     * @brief Navigate to parent directory
     */
    void go_up_dir();

    /**
     * @brief Enter a subdirectory
     * @param name directory name to enter
     */
    void enter_dir(const std::string &name);

    /**
     * @brief Ensure current directory is valid
     */
    void ensure_current_dir();
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
