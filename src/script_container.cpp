#include "script_container.hpp"
#include "script_errors.hpp"
#include "utility.hpp"

#include <fmt/core.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "static/lulpeg.h"
#include "static/moonscript.h"
#include "static/moonc.h"
#include "static/utility.h"
#include "static/init.h"

static int _luaF_customPackageLoader(lua_State *L)
{
    const char *packageName = lua_tostring(L, 1);

    // Get the 'this' pointer from the Lua state's registry
    lua_getfield(L, LUA_REGISTRYINDEX, "ScriptContainer");
    ScriptContainer *self = static_cast<ScriptContainer *>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    return self->_luaF_moonPackageLoader(L);
}

ScriptContainer::ScriptContainer(bool openLibs)
    : _state(luaL_newstate())
{

    if (openLibs)
    {
        luaL_openlibs(_state);
    }

    // TODO: do it in lua
    {
        lua_pushlightuserdata(_state, this);
        lua_setfield(_state, LUA_REGISTRYINDEX, "ScriptContainer");

        lua_pushcfunction(_state, ::_luaF_customPackageLoader);
        lua_getglobal(_state, "package");
        lua_getfield(_state, -1, "loaders");

        this->luaX_tableInsert(2, -3);
        lua_pop(_state, 3);
    }

    this->_luaX_loadBaseScripts();
    this->setSearchPath("./init.lua;./init.moon");
}

ScriptContainer::~ScriptContainer()
{
    if (_state)
    {
        lua_close(_state);
    }
}

void ScriptContainer::doScriptFromFile(const std::string &filename)
{
    lua_State *L = this->_state;
    std::string ext = std::filesystem::path(filename).extension();

    if (ext == ".moon")
    {
        this->_luaX_compileMoonscriptIfNecessary(filename);
        auto newFilename = std::string(filename) + ".lua";
        return this->doScriptFromFile(newFilename);
    }
    else if (ext == ".lua")
    {
        auto buffer = readFileIntoBuffer(filename);
        if (luaL_loadbuffer(L, buffer.c_str(), buffer.size(), filename.c_str()) != LUA_OK)
        {
            throw ScriptContainerError(fmt::format("failed to load buffer for script {}", filename));
        }
    }

    // add to path
    {
        auto filepath = std::filesystem::absolute(filename);
        auto parentDir = filepath.parent_path();
        auto searchPath = fmt::format("{0}/?.lua;{0}/?/init.lua;{0}/?/init.moon;{0}/?.moon", parentDir.string());
        if (!this->isInSearchPath(searchPath))
        {
            this->addSearchPath(searchPath);
        }
    }

    // run the script
    if (lua_pcall(L, 0, 0, 0) != LUA_OK)
    {
        throw LuaRuntimeError("script file runtime error", lua_tostring(L, -1));
    }
}

lua_State *ScriptContainer::getLuaState()
{
    return this->_state;
}

void ScriptContainer::addSearchPath(const std::string &path)
{
    lua_State *L = this->_state;

    // Get the package table
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");

    // Get the current package path
    std::string currentPath = lua_tostring(L, -1);

    // Append the new path to the current package path
    std::string newPath = currentPath + ";" + path;

    // Set the new package path
    lua_pushstring(L, newPath.c_str());
    lua_setfield(L, -3, "path");

    // Pop the package table from the stack
    lua_pop(L, 2);
}

std::string ScriptContainer::getSearchPath()
{
    lua_State *L = this->_state;

    // Get the package table
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");

    // Get the current package path
    const char *path = lua_tostring(L, -1);

    // Pop the package table from the stack
    lua_pop(L, 1);

    return path;
}

void ScriptContainer::setSearchPath(const std::string &path)
{
    lua_State *L = this->_state;

    // Get the package table
    lua_getglobal(L, "package");

    // Set the new package path
    lua_pushstring(L, path.c_str());
    lua_setfield(L, -2, "path");

    // Pop the package table from the stack
    lua_pop(L, 1);
}

bool ScriptContainer::isInSearchPath(const std::string &path)
{
    lua_State *L = this->_state;

    // Get the package table
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");

    // Get the current package path
    const char *currentPath = lua_tostring(L, -1);

    // Check if the path is in the current package path
    bool found = currentPath && std::string(currentPath).find(path) != std::string::npos;

    // Pop the package table from the stack
    lua_pop(L, 2);

    return found;
}

void ScriptContainer::registerPackage(const std::string &name, const std::string &script)
{
    this->_luaX_loadPackage(name, script);
}

void ScriptContainer::registerEmbeddedPackage(const char *name, const char *script, size_t len)
{
    this->_luaX_loadPackage(name, std::string(script, len));
}

void ScriptContainer::printLuaStack()
{
    lua_State *L = this->_state;

    int top = lua_gettop(L);
    for (int i = 1; i <= top; i++)
    {
        int t = lua_type(L, i);
        switch (t)
        {
        case LUA_TSTRING:
            printf("%02d string '%s'\n", i, lua_tostring(L, i));
            break;
        case LUA_TBOOLEAN:
            printf("%02d bool %s\n", i, lua_toboolean(L, i) ? "true" : "false");
            break;
        case LUA_TNUMBER:
            printf("%02d number %g\n", i, lua_tonumber(L, i));
            break;
        default:
            printf("%02d %s\n", i, lua_typename(L, t));
            break;
        }
    }
}

void ScriptContainer::luaX_tableInsert(int pos, int valueIndex)
{
    lua_State *L = this->_state;

    int len = lua_objlen(L, -1); // Get the length of the table.

    // Shift elements upwards from pos to len (inclusive)
    for (int i = len; i >= pos; --i)
    {
        lua_rawgeti(L, -1, i);     // Push t[i] onto the stack.
        lua_rawseti(L, -2, i + 1); // t[i+1] = t[i]; pops the value.
    }

    // Insert the new element at position pos.
    lua_pushvalue(L, valueIndex); // Push the value to be inserted.
    lua_rawseti(L, -2, pos);      // Set t[pos] = value; pops the value.
}

void ScriptContainer::_luaX_loadBaseScripts()
{
    this->registerEmbeddedPackage("lpeg", (const char *)___extlib_lulpeg_lulpeg_lua, ___extlib_lulpeg_lulpeg_lua_len);
    this->registerEmbeddedPackage("moonscript", (const char *)___build_moonscript_lua, ___build_moonscript_lua_len);
    this->registerEmbeddedPackage("moonc", (const char *)___scripts_moonc_lua, ___scripts_moonc_lua_len);
    this->registerEmbeddedPackage("utility", (const char *)___scripts_utility_lua, ___scripts_utility_lua_len);
    // this->registerEmbeddedPackage("init", (const char *)___scripts_init_lua, ___scripts_init_lua_len);
}

void ScriptContainer::_luaX_loadPackage(const std::string &name, const std::string &lib)
{
    lua_State *L = this->_state;

    if (luaL_loadbuffer(L, lib.c_str(), lib.size(), name.c_str()) != LUA_OK)
    {
        throw ScriptContainerError(fmt::format("failed to load package {}", name));
    }

    if (lua_pcall(L, 0, 1, 0) != LUA_OK)
    {
        auto err = lua_tostring(L, -1);
        lua_pop(L, 2);
        throw LuaRuntimeError(fmt::format("failed to run package {}", name), err);
    }

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_pushvalue(L, -3);              // copy loaded buffer
    lua_setfield(L, -2, name.c_str()); // package.loaded[name] = buffer
    lua_pop(L, 3);                     // pop the package and loaded buffer
}

void ScriptContainer::_luaX_compileMoonscriptIfNecessary(const std::string &filename)
{
    lua_State *L = this->_state;
    auto newFilename = std::string(filename) + ".lua";

    // check if exists and newFilename is newer than filename, otherwise compile
    if (std::filesystem::exists(newFilename) && std::filesystem::last_write_time(newFilename) > std::filesystem::last_write_time(filename))
    {
        return;
    }

    auto buffer = readFileIntoBuffer(filename);
    auto top = lua_gettop(L);

    lua_getglobal(L, "require");
    lua_pushstring(L, "moonc");

    // require('moonc')
    if (lua_pcall(L, 1, 1, 0) != LUA_OK)
    {
        lua_settop(L, top);
        throw ScriptContainerError("failed to require('moonc')");
    }

    lua_pushlstring(L, buffer.c_str(), buffer.size());

    // moonc(buffer)
    if (lua_pcall(L, 1, 1, 0) != LUA_OK)
    {
        const char *err = lua_tostring(L, -1);
        lua_settop(L, top);
        throw MoonCompileError(err, filename);
    }

    auto compiled = lua_tostring(this->_state, -1);
    writeFile(newFilename, compiled);
    lua_settop(L, top);
}

int ScriptContainer::_luaF_moonPackageLoader(lua_State *L)
{
    auto name = lua_tostring(L, 1);
    auto pathParts = split(this->getSearchPath(), ';');
    bool found = false;
    std::string filename = "";
    auto pathedName = replace_all(name, ".", "/");
    for (auto &path : pathParts)
    {
        if (!ends_with(path, ".moon"))
        {
            continue;
        }

        filename = replace_all(path, "?", pathedName);
        if (!std::filesystem::exists(filename))
        {
            continue;
        }

        found = true;
        break;
    }

    if (!found)
    {
        lua_pushstring(L, fmt::format("module '{}' not found", name).c_str());
        return 1;
    }

    try
    {
        this->_luaX_compileMoonscriptIfNecessary(filename);
    }
    catch (const std::exception &e)
    {
        lua_pushstring(L, e.what());
        return 1;
    }

    auto newFilename = std::string(filename) + ".lua";
    auto buffer = readFileIntoBuffer(newFilename);

    if (luaL_loadbuffer(L, buffer.c_str(), buffer.size(), filename.c_str()) != LUA_OK)
    {
        lua_pushstring(L, fmt::format("failed to load buffer for script {}", filename).c_str());
        return 1;
    }

    // set package.loaded[name] = stack top which is loaded buffer
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_pushvalue(L, -3);
    lua_setfield(L, -2, name);

    lua_pushvalue(L, -3);

    return 1;
}
