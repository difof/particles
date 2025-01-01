#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <lua.h>
#ifdef __cplusplus
}
#endif

#include "script_container.hpp"

extern "C" int luaopen_support(lua_State *L);
extern "C" int luaopen_rl(lua_State *L);
extern "C" int luaopen_imgui(lua_State *L);

int main(int argc, char *argv[])
{
    ScriptContainer *script = nullptr;
    try
    {
        script = new ScriptContainer(true);
        auto state = script->getLuaState();
        luaopen_support(state);
        luaopen_rl(state);
        luaopen_imgui(state);

        script->doScriptFromFile("test_data/basic_sand.moon");
    }
    catch (const std::exception &e)
    {
        fprintf(stderr, "Error: %s\n", e.what());
    }

    if (script != nullptr)
    {
        delete script;
    }

    return 0;
}
