#ifndef SCRIPT_CONTAINER_HPP
#define SCRIPT_CONTAINER_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#ifdef __cplusplus
}
#endif

#include <string>

/// @brief Container for loading and managing scripts (luajit state, lua, moonscript)
class ScriptContainer
{
public:
    enum Language
    {
        LUA,
        MOONSCRIPT
    };

    /// @brief Constructor
    /// @param[in] openLibs If true, open the lua libraries
    ScriptContainer(bool openLibs);

    /// @brief Destructor
    ~ScriptContainer();

    /// @brief Load and run a script from a file
    /// @param[in] filename The filename of the script to load
    void doScriptFromFile(const std::string &filename);

    /// @brief Get the lua state
    /// @return The lua state
    lua_State *getLuaState();

    /// @brief Add a new search path for package imports
    /// @param[in] path The path to add
    void addSearchPath(const std::string &path);

    /// @brief Get the search path
    /// @return The search path
    std::string getSearchPath();

    /// @brief Overwrite the search path
    /// @param[in] path The new search path
    void setSearchPath(const std::string &path);

    /// @brief Checks if a path is in the search path
    /// @param[in] path The path to check
    /// @return True if the path is in the search path, false otherwise
    bool isInSearchPath(const std::string &path);

    /// @brief Register a lua package
    /// @param[in] name The name of the package
    /// @param[in] script The script to register
    void registerPackage(const std::string &name, const std::string &script);

    /// @brief Register an embedded lua package
    /// @param[in] name The name of the package
    /// @param[in] script The script to register
    /// @param[in] len The length of the script
    void registerEmbeddedPackage(const char *name, const char *script, size_t len);

    /// @brief Print the lua stack
    void printLuaStack();

    /// @brief Insert a value into a table at a specific position. The table must be on top of stack.
    /// @param[in] pos The position to insert the value
    /// @param[in] valueIndex The index of the value to insert
    void luaX_tableInsert(int pos, int valueIndex);

    /// @brief Internal function for custom package loader
    int _luaF_moonPackageLoader(lua_State *L);

private:
    void _luaX_compileMoonscriptIfNecessary(const std::string &filename);
    void _luaX_loadBaseScripts();
    void _luaX_loadPackage(const std::string &name, const std::string &lib);

private:
    /// @brief The lua state
    lua_State *_state;
};

#endif // !SCRIPT_CONTAINER_HPP