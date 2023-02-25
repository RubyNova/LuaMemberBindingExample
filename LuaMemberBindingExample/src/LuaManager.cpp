// Copyright © Matt Jones and Contributors. Licensed under the MIT Licence (MIT). See LICENCE.md in the repository root
// for more information.

#include <stdexcept>
#include <LuaManager.hpp>
#include <LuaTypeRegistry.hpp>

LuaManager::LuaManager() : L(luaL_newstate())
{
    luaL_openlibs(L);
}

LuaManager::~LuaManager()
{
    lua_close(L);
}

void LuaManager::Execute(std::string code)
{
    luaL_dostring(L, code.c_str());
    // TODO: error handling
}

void LuaManager::SetGlobal(std::string name)
{
    lua_setglobal(L, name.c_str());
}
