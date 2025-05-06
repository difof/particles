#include <catch_amalgamated.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "render/file_dialog.hpp"

/**
 * @brief Test helper class to access FileDialog internals for testing
 */
class FileDialogTestHelper : public FileDialog {
  public:
    /**
     * @brief Get the current mode of a FileDialog
     * @return Current mode
     */
    FileDialog::Mode get_mode() const { return m_mode; }

    /**
     * @brief Get the current title of a FileDialog
     * @return Current title
     */
    const std::string &get_title() const { return m_title; }

    /**
     * @brief Get the current filename of a FileDialog
     * @return Current filename
     */
    const std::string &get_filename() const { return m_file_name; }

    /**
     * @brief Get the new directory name of a FileDialog
     * @return New directory name
     */
    const std::string &get_new_dir_name() const { return m_new_dir_name; }

    /**
     * @brief Get the entries list of a FileDialog
     * @return Reference to entries vector
     */
    const std::vector<FileDialog::Entry> &get_entries() const {
        return m_entries;
    }

    /**
     * @brief Check if dialog is open
     * @return true if dialog is open
     */
    bool is_open() const { return m_open; }

    /**
     * @brief Set the filename for testing
     * @param filename Filename to set
     */
    void set_filename(const std::string &filename) { m_file_name = filename; }

    /**
     * @brief Set the new directory name for testing
     * @param dir_name Directory name to set
     */
    void set_new_dir_name(const std::string &dir_name) {
        m_new_dir_name = dir_name;
    }

    /**
     * @brief Manually trigger list_directory for testing
     */
    void trigger_list_directory() { list_directory(); }

    /**
     * @brief Manually trigger go_up_dir for testing
     */
    void trigger_go_up_dir() { go_up_dir(); }

    /**
     * @brief Manually trigger enter_dir for testing
     * @param name Directory name to enter
     */
    void trigger_enter_dir(const std::string &name) { enter_dir(name); }
};

TEST_CASE("FileDialog - Basic functionality", "[file_dialog]") {
    FileDialogTestHelper dialog;

    SECTION("Initial state") {
        REQUIRE_FALSE(dialog.has_result());
        REQUIRE_FALSE(dialog.canceled());
        REQUIRE(dialog.selected_path().empty());
        REQUIRE(dialog.current_dir().empty());
    }

    SECTION("Open dialog") {
        dialog.open(FileDialog::Mode::Open, "Test Dialog", "/tmp");

        // Dialog should be open but no result yet
        REQUIRE_FALSE(dialog.has_result());
        REQUIRE_FALSE(dialog.canceled());
        REQUIRE(dialog.current_dir() == "/tmp");

        // Test internal state using helper
        REQUIRE(dialog.is_open());
        REQUIRE(dialog.get_mode() == FileDialog::Mode::Open);
        REQUIRE(dialog.get_title() == "Test Dialog");
    }

    SECTION("Set filename") {
        dialog.set_filename("test.txt");

        // Test internal state using helper
        REQUIRE(dialog.get_filename() == "test.txt");

        // Test setting via helper
        dialog.set_filename("another_file.txt");
        REQUIRE(dialog.get_filename() == "another_file.txt");
    }
}

TEST_CASE("FileDialog - Mode handling", "[file_dialog]") {
    FileDialogTestHelper dialog;

    SECTION("Open mode") {
        dialog.open(FileDialog::Mode::Open, "Open File", "/tmp");
        // Test that open mode is set correctly
        REQUIRE(dialog.get_mode() == FileDialog::Mode::Open);
        REQUIRE(dialog.get_title() == "Open File");
        REQUIRE_FALSE(dialog.has_result());
    }

    SECTION("Save mode") {
        dialog.open(FileDialog::Mode::Save, "Save File", "/tmp");
        // Test that save mode is set correctly
        REQUIRE(dialog.get_mode() == FileDialog::Mode::Save);
        REQUIRE(dialog.get_title() == "Save File");
        REQUIRE_FALSE(dialog.has_result());
    }
}

TEST_CASE("FileDialog - Directory operations", "[file_dialog]") {
    FileDialogTestHelper dialog;

    SECTION("Open with valid directory") {
        const std::string test_dir = "/tmp";
        if (std::filesystem::exists(test_dir)) {
            dialog.open(FileDialog::Mode::Open, "Test", test_dir);
            REQUIRE(dialog.current_dir() == test_dir);
        }
    }

    SECTION("Open with empty directory") {
        dialog.open(FileDialog::Mode::Open, "Test", "");
        // Should default to current directory
        REQUIRE_FALSE(dialog.current_dir().empty());
    }
}

TEST_CASE("FileDialog - Entry structure", "[file_dialog]") {
    SECTION("Entry default values") {
        FileDialog::Entry entry;
        REQUIRE(entry.name.empty());
        REQUIRE_FALSE(entry.is_dir);
    }

    SECTION("Entry with values") {
        FileDialog::Entry entry;
        entry.name = "test_file.txt";
        entry.is_dir = false;

        REQUIRE(entry.name == "test_file.txt");
        REQUIRE_FALSE(entry.is_dir);
    }

    SECTION("Entry for directory") {
        FileDialog::Entry entry;
        entry.name = "test_dir";
        entry.is_dir = true;

        REQUIRE(entry.name == "test_dir");
        REQUIRE(entry.is_dir);
    }
}

TEST_CASE("FileDialog - File operations", "[file_dialog]") {
    FileDialogTestHelper dialog;
    const std::string test_dir = "/tmp";

    if (std::filesystem::exists(test_dir)) {
        dialog.open(FileDialog::Mode::Open, "Test", test_dir);

        SECTION("Set and get filename") {
            const std::string filename = "test_file.txt";
            dialog.set_filename(filename);

            // Test internal state using helper
            REQUIRE(dialog.get_filename() == filename);

            // Test setting via helper
            dialog.set_filename("another_file.txt");
            REQUIRE(dialog.get_filename() == "another_file.txt");
        }
    }
}

TEST_CASE("FileDialog - Error handling", "[file_dialog]") {
    FileDialogTestHelper dialog;

    SECTION("Open with non-existent directory") {
        const std::string non_existent_dir =
            "/non/existent/directory/that/does/not/exist";
        dialog.open(FileDialog::Mode::Open, "Test", non_existent_dir);

        // Should not crash, but may not work as expected
        REQUIRE_NOTHROW(
            dialog.open(FileDialog::Mode::Open, "Test", non_existent_dir));
    }

    SECTION("Multiple opens") {
        dialog.open(FileDialog::Mode::Open, "First", "/tmp");
        dialog.open(FileDialog::Mode::Save, "Second", "/tmp");

        // Should not crash with multiple opens
        REQUIRE_NOTHROW(dialog.open(FileDialog::Mode::Open, "Third", "/tmp"));
    }
}

TEST_CASE("FileDialog - State management", "[file_dialog]") {
    FileDialogTestHelper dialog;

    SECTION("Initial state consistency") {
        REQUIRE_FALSE(dialog.has_result());
        REQUIRE_FALSE(dialog.canceled());
        REQUIRE(dialog.selected_path().empty());
    }

    SECTION("State after open") {
        dialog.open(FileDialog::Mode::Open, "Test", "/tmp");

        // Should still have no result immediately after opening
        REQUIRE_FALSE(dialog.has_result());
        REQUIRE_FALSE(dialog.canceled());
    }
}

TEST_CASE("FileDialog - Internal directory operations", "[file_dialog]") {
    FileDialogTestHelper dialog;

    SECTION("Directory navigation") {
        dialog.open(FileDialog::Mode::Open, "Test", "/tmp");

        // Test initial state
        REQUIRE(dialog.is_open());
        REQUIRE(dialog.current_dir() == "/tmp");

        // Test go_up_dir
        dialog.trigger_go_up_dir();
        // Should go to parent directory
        REQUIRE(dialog.current_dir() != "/tmp");

        // Test enter_dir
        dialog.trigger_enter_dir("tmp");
        // Should enter the tmp subdirectory
        REQUIRE(dialog.current_dir().find("tmp") != std::string::npos);
    }

    SECTION("New directory name handling") {
        dialog.open(FileDialog::Mode::Open, "Test", "/tmp");

        // Test setting new directory name
        dialog.set_new_dir_name("test_dir");
        REQUIRE(dialog.get_new_dir_name() == "test_dir");

        // Test clearing new directory name
        dialog.set_new_dir_name("");
        REQUIRE(dialog.get_new_dir_name().empty());
    }
}

TEST_CASE("FileDialog - Integration with filesystem", "[file_dialog]") {
    FileDialogTestHelper dialog;

    // Create a temporary directory for testing
    const std::string temp_dir = "/tmp/particles_test";
    std::filesystem::create_directories(temp_dir);

    // Create some test files
    std::ofstream file1(temp_dir + "/test1.txt");
    file1 << "test content 1";
    file1.close();

    std::ofstream file2(temp_dir + "/test2.txt");
    file2 << "test content 2";
    file2.close();

    // Create a subdirectory
    std::filesystem::create_directories(temp_dir + "/subdir");

    SECTION("Open dialog in test directory") {
        dialog.open(FileDialog::Mode::Open, "Test", temp_dir);
        REQUIRE(dialog.current_dir() == temp_dir);

        // Test that entries are populated
        dialog.trigger_list_directory();
        const auto &entries = dialog.get_entries();
        REQUIRE_FALSE(entries.empty());

        // Should have at least our test files
        bool found_test1 = false;
        bool found_test2 = false;
        bool found_subdir = false;

        for (const auto &entry : entries) {
            if (entry.name == "test1.txt" && !entry.is_dir) {
                found_test1 = true;
            }
            if (entry.name == "test2.txt" && !entry.is_dir) {
                found_test2 = true;
            }
            if (entry.name == "subdir" && entry.is_dir) {
                found_subdir = true;
            }
        }

        REQUIRE(found_test1);
        REQUIRE(found_test2);
        REQUIRE(found_subdir);
    }

    // Clean up
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("FileDialog - Edge cases", "[file_dialog]") {
    FileDialogTestHelper dialog;

    SECTION("Empty title") {
        REQUIRE_NOTHROW(dialog.open(FileDialog::Mode::Open, "", "/tmp"));
    }

    SECTION("Very long title") {
        const std::string long_title(1000, 'A');
        REQUIRE_NOTHROW(
            dialog.open(FileDialog::Mode::Open, long_title, "/tmp"));
    }

    SECTION("Special characters in title") {
        const std::string special_title = "Test!@#$%^&*()_+-=[]{}|;':\",./<>?";
        REQUIRE_NOTHROW(
            dialog.open(FileDialog::Mode::Open, special_title, "/tmp"));
    }
}
